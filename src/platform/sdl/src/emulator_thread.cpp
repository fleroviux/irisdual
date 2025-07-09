
#include <atom/panic.hpp>
#include <chrono>

#include "emulator_thread.hpp"

EmulatorThread::EmulatorThread() = default;

EmulatorThread::~EmulatorThread() {
  Stop();
}

void EmulatorThread::Start(std::unique_ptr<dual::nds::NDS> nds) {
  if(m_running) {
    ATOM_PANIC("Starting an already running emulator thread is illegal.");
  }
  m_nds = std::move(nds);
  m_frame_mailbox.available[0] = false;
  m_frame_mailbox.available[1] = false;
  m_nds->GetVideoUnit().SetPresentationCallback([this](const u32* fb_top, const u32* fb_bottom) {
    PresentCallback(fb_top, fb_bottom);
  });
  m_running = true;
  m_thread = std::thread{&EmulatorThread::ThreadMain, this};
}

std::unique_ptr<dual::nds::NDS> EmulatorThread::Stop() {
  if(!m_running) {
    return {};
  }
  m_running = false;
  m_thread.join();
  return std::move(m_nds);
}

void EmulatorThread::Reset() {
  PushMessage({.type = MessageType::Reset});
}

void EmulatorThread::DirectBoot() {
  PushMessage({.type = MessageType::DirectBoot});
}

void EmulatorThread::SetKeyState(dual::nds::Key key, bool pressed) {
  PushMessage({
    .type = MessageType::SetKeyState,
    .set_key_state = { .key = key, .pressed = (u8)(pressed ? 1 : 0) }
  });
}

void EmulatorThread::SetTouchState(bool pen_down, u8 x, u8 y) {
  PushMessage({
    .type = MessageType::SetTouchState,
    .set_touch_state = { .pen_down = (u8)(pen_down ? 1 : 0), .x = x, .y = y }
  });
}

bool EmulatorThread::GetFastForward() const {
  return m_fast_forward;
}

void EmulatorThread::SetFastForward(bool fast_forward) {
  if(fast_forward != m_fast_forward) {
    m_fast_forward = fast_forward;
    m_nds->GetAPU().SetEnableOutput(!fast_forward);
  }
}

void EmulatorThread::PushMessage(const Message& message) {
  std::lock_guard lock_guard{m_msg_queue_mutex};
  m_msg_queue.push(message); // @todo: can we use emplace?
}

void EmulatorThread::ProcessMessages() {
  // @todo: would it be thread-safe to do an initial check on empty() before locking
  // to avoid acquiring a lock when we do not have to?
  // idea: use a separate std::atomic_int to track the number of messages in the queue
  std::lock_guard lock_guard{m_msg_queue_mutex};

  while(!m_msg_queue.empty()) {
    const Message& message = m_msg_queue.front();

    switch(message.type) {
      case MessageType::Reset: {
        m_nds->Reset();
        break;
      }
      case MessageType::DirectBoot: {
        m_nds->DirectBoot();
        break;
      }
      case MessageType::SetKeyState: {
        m_nds->SetKeyState(message.set_key_state.key, message.set_key_state.pressed);
        break;
      }
      case MessageType::SetTouchState: {
        m_nds->SetTouchState(message.set_touch_state.pen_down, message.set_touch_state.x, message.set_touch_state.y);
        break;
      }
      default: ATOM_PANIC("unhandled message type {}", (int)message.type);
    }

    m_msg_queue.pop();
  }
}

void EmulatorThread::ThreadMain() {
  using namespace std::chrono_literals;

  constexpr int k_cycles_per_frame = 560190;

  dual::AudioDriverBase* audio_driver = m_nds->GetAPU().GetAudioDriver();

  if(!audio_driver) {
    ATOM_PANIC("An audio driver is required to synchronize to audio, but no audio driver is present.");
  }

  const uint full_buffer_size = audio_driver->GetBufferSize();
  const uint half_buffer_size = full_buffer_size >> 1;

  while(m_running) {
    // @todo: figure out how frequently we want to run this, especially when unthrottled.
    ProcessMessages();

    if(!m_fast_forward) {
      uint current_buffer_size = audio_driver->GetNumberOfQueuedSamples();

      if(current_buffer_size == 0) {
        fmt::print("Uh oh! Bad! Audio not synced anymore! Fix me!!\n");
      }

      // Sleep until the queue is less than half full
      while(current_buffer_size > half_buffer_size) {
        std::this_thread::sleep_for(1ms);

        current_buffer_size = audio_driver->GetNumberOfQueuedSamples();
      }

      // Run the emulator for as many cycles as is needed to fully fill the queue.
      const int cycles = (int)(full_buffer_size - current_buffer_size) * 1024;
      m_nds->Step(cycles);
    } else {
      m_nds->Step(k_cycles_per_frame);
    }
  }
}

std::optional<std::pair<const u32*, const u32*>> EmulatorThread::AcquireFrame() {
  int read_id = m_frame_mailbox.read_id;

  if(!m_frame_mailbox.available[read_id]) {
    m_frame_mailbox.read_id ^= 1;
    read_id ^= 1;
  }

  if(m_frame_mailbox.available[read_id]) {
    return std::make_pair<const u32*, const u32*>(
      m_frame_mailbox.frames[read_id][0], m_frame_mailbox.frames[read_id][1]);
  }

  return std::nullopt;
}

void EmulatorThread::ReleaseFrame() {
  const int read_id = m_frame_mailbox.read_id;

  m_frame_mailbox.available[read_id] = false;
}

void EmulatorThread::PresentCallback(const u32* fb_top, const u32* fb_bottom) {
  using namespace std::chrono_literals;

  int write_id = m_frame_mailbox.write_id;

  if(m_frame_mailbox.available[write_id]/* && !m_frame_mailbox.available[write_id ^ 1]*/) {
    m_frame_mailbox.write_id ^= 1;
    write_id ^= 1;
  }

  if(!m_frame_mailbox.available[write_id]) {
    std::memcpy(m_frame_mailbox.frames[write_id][0], fb_top, sizeof(u32) * 256 * 192);
    std::memcpy(m_frame_mailbox.frames[write_id][1], fb_bottom, sizeof(u32) * 256 * 192);
    m_frame_mailbox.available[write_id] = true;
  }
}
