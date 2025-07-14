
#include <atom/logger/logger.hpp>
#include <atom/panic.hpp>

#include "sdl2_audio_driver.hpp"

void SDL2AudioDriver_AudioCallback(SDL2AudioDriver* self, i16* stream, int length);

bool SDL2AudioDriver::Open(uint sample_rate, uint buffer_size) {
  SDL_AudioSpec want{};

  if(SDL_Init(SDL_INIT_AUDIO) < 0) {
    ATOM_ERROR("SDL_Init(SDL_INIT_AUDIO) failed.");
    return false;
  }

  // We want SDL2 to pull at most half of our buffer at one time.
  buffer_size /= 2u;

  want.freq = (int)sample_rate;
  want.samples = buffer_size;
  want.format = AUDIO_S16;
  want.channels = k_channel_count;
  want.callback = (SDL_AudioCallback)SDL2AudioDriver_AudioCallback;
  want.userdata = this;

  m_audio_device = SDL_OpenAudioDevice(nullptr, 0, &want, &m_have, 0);

  if(m_audio_device == 0) {
    ATOM_ERROR("SDL_OpenAudioDevice: failed to open audio: %s\n", SDL_GetError());
    return false;
  }

  m_audio_buffer.resize(m_have.samples * 2u * k_channel_count);
  m_rd_position = 0u;
  m_wr_position = 0u;
  m_current_buffer_size = 0u;

  SDL_PauseAudioDevice(m_audio_device, 0);
  return true;
}

void SDL2AudioDriver::Close() {
  SDL_CloseAudioDevice(m_audio_device);
  m_current_buffer_size = 0;
}

uint SDL2AudioDriver::GetBufferSize() const {
  return m_audio_buffer.size() / k_channel_count;
}

void SDL2AudioDriver::QueueSamples(std::span<i16> buffer) {
  for(size_t i = 0; i < buffer.size(); i += k_channel_count) {
    for(size_t channel = 0; channel < k_channel_count; channel++) {
      m_audio_buffer[m_wr_position++] = buffer[i + channel];
    }
    if(m_wr_position == m_audio_buffer.size()) {
      m_wr_position = 0u;
    }
    m_current_buffer_size++;

    if(m_wr_position == m_rd_position) {
      break;
    }
  }
}

uint SDL2AudioDriver::GetNumberOfQueuedSamples() const {
  return m_current_buffer_size;
}

void SDL2AudioDriver::WaitBufferHalfEmpty() const {
  m_buffer_half_empty_semaphore.acquire();
}

void SDL2AudioDriver_AudioCallback(SDL2AudioDriver* self, i16* stream, int length) {
  const size_t sample_count = length / sizeof(i16) / 2;

  for(int i = 0; i < sample_count; i++) {
    if(self->m_current_buffer_size > 0) {
      for(size_t channel = 0; channel < SDL2AudioDriver::k_channel_count; channel++) {
        *stream++ = self->m_audio_buffer[self->m_rd_position++];
      }
      if(self->m_rd_position == self->m_audio_buffer.size()) {
        self->m_rd_position = 0u;
      }
      self->m_current_buffer_size--;
      if(self->m_current_buffer_size * 2u <= self->GetBufferSize()) {
        self->m_buffer_half_empty_semaphore.release();
      }
    } else {
      for(size_t channel = 0; channel < SDL2AudioDriver::k_channel_count; channel++) {
        *stream++ = 0u;
      }
    }
  }
}
