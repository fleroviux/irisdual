
#include <algorithm>
#include <atom/panic.hpp>
#include <dual/nds/arm7/apu.hpp>

namespace dual::nds::arm7 {

  static constexpr int k_adpcm_index_tab[8] { -1, -1, -1, -1, 2, 4, 6, 8 };

  static constexpr i16 k_adpcm_adpcm_tab[89] {
    0x0007, 0x0008, 0x0009, 0x000A, 0x000B, 0x000C, 0x000D, 0x000E,
    0x0010, 0x0011, 0x0013, 0x0015, 0x0017, 0x0019, 0x001C, 0x001F,
    0x0022, 0x0025, 0x0029, 0x002D, 0x0032, 0x0037, 0x003C, 0x0042,
    0x0049, 0x0050, 0x0058, 0x0061, 0x006B, 0x0076, 0x0082, 0x008F,
    0x009D, 0x00AD, 0x00BE, 0x00D1, 0x00E6, 0x00FD, 0x0117, 0x0133,
    0x0151, 0x0173, 0x0198, 0x01C1, 0x01EE, 0x0220, 0x0256, 0x0292,
    0x02D4, 0x031C, 0x036C, 0x03C3, 0x0424, 0x048E, 0x0502, 0x0583,
    0x0610, 0x06AB, 0x0756, 0x0812, 0x08E0, 0x09C3, 0x0ABD, 0x0BD0,
    0x0CFF, 0x0E4C, 0x0FBA, 0x114C, 0x1307, 0x14EE, 0x1706, 0x1954,
    0x1BDC, 0x1EA5, 0x21B6, 0x2515, 0x28CA, 0x2CDF, 0x315B, 0x364B,
    0x3BB9, 0x41B2, 0x4844, 0x4F7E, 0x5771, 0x602F, 0x69CE, 0x7462,
    0x7FFF
  };

  APU::APU(Scheduler& scheduler, arm::Memory& bus) : m_scheduler{scheduler}, m_bus{bus} {}

  void APU::Reset() {
    m_soundxcnt.fill({});
    m_soundxsad.fill(0u);
    m_soundxtmr.fill(0u);
    m_soundxpnt.fill(0u);
    m_soundxlen.fill(0u);
    m_soundcnt = {};
    m_soundbias = 0u;
    m_channels.fill({});

    for(int id = 0; id < 16; id++) {
      m_channels[id].sampling_event_fn = [this, id](int cycles_late) {
        SampleChannel(id, cycles_late);
      };

      RecomputeChannelSamplingInterval(id);
    }

    m_scheduler.Add(k_cycles_per_sample, this, &APU::SampleMixers);
  }

  AudioDriverBase* APU::GetAudioDriver() {
    return m_audio_driver.get();
  }

  void APU::SetAudioDriver(std::shared_ptr<AudioDriverBase> audio_driver) {
    if(m_audio_driver) {
      m_audio_driver->Close();
    }
    m_audio_driver = std::move(audio_driver);
    m_audio_driver->Open(nullptr, nullptr, 32768u, 1024u);
  }

  bool APU::GetEnableOutput() const {
    return m_output_enable;
  }

  void APU::SetEnableOutput(bool enable) {
    m_output_enable = enable;
  }

  void APU::SampleMixers(int cycles_late) {
    f32 samples[2] {0.f, 0.f};

    if(m_soundcnt.master_enable) {
      static constexpr f32 k_volume_div_tab[4] { 1.f, 0.5f, 0.25f, 0.0625f };

      const f32 master_volume = (f32)m_soundcnt.master_volume * (1.f / 127.f);

      for(int id = 0; id < 16; id++) {
        const SOUNDxCNT& control = m_soundxcnt[id];
        const Channel &channel = m_channels[id];

        if(!control.running && !control.hold_last_sample) {
          continue;
        }

        const f32 volume  = (f32)control.volume_mul * (1.f / 127.f) * k_volume_div_tab[control.volume_div];
        const f32 panning = (f32)control.panning * (1.f / 128.f);
        samples[0] += channel.current_sample * volume * (1.f - panning);
        samples[1] += channel.current_sample * volume * panning;
      }

      for(f32& sample : samples) sample = 0.0f;//std::clamp(sample * master_volume, -1.f, +1.f);
    }

    m_audio_buffer.PushBack((i16)(samples[0] * 32767));
    m_audio_buffer.PushBack((i16)(samples[1] * 32767));

    if(m_audio_buffer.Full()) {
      if(m_output_enable && m_audio_driver) {
        m_audio_driver->QueueSamples(m_audio_buffer);
      }
      m_audio_buffer.Clear();
    }

    m_scheduler.Add(k_cycles_per_sample - cycles_late, this, &APU::SampleMixers);
  }

  void APU::SampleChannel(int id, int cycles_late) {
    switch(m_channels[id].sample_format) {
      case SampleFormat::ADPCM: SampleChannelPCM<SampleFormat::ADPCM>(id); break;
      case SampleFormat::PCM8:  SampleChannelPCM<SampleFormat::PCM8 >(id); break;
      case SampleFormat::PCM16: SampleChannelPCM<SampleFormat::PCM16>(id); break;
      case SampleFormat::PSG:   SampleChannelPSG(id); break;
    }

    if(m_soundxcnt[id].running) {
      ScheduleSampleChannel(id, cycles_late);
    }
  }

  void APU::SampleChannelPSG(int id) {
    Channel& channel = m_channels[id];

    if(id >= 14) {
      const u16 lfsr = channel.noise_lfsr;

      if(lfsr & 1u) {
        channel.noise_lfsr = (lfsr >> 1) ^ 0x6000u;
        channel.current_sample = -1.f;
      } else {
        channel.noise_lfsr = lfsr >> 1;
        channel.current_sample = +1.f;
      }
    } else if(id >= 8) {
      const u32 current_address = channel.current_address;

      if((current_address ^ 7u) > m_soundxcnt[id].psg_wave_duty) {
        channel.current_sample = +1.f;
      } else {
        channel.current_sample = -1.f;
      }

      channel.current_address = (current_address + 1u) & 7u;
    } else {
      channel.current_sample = 0.f;
    }
  }

  template<APU::SampleFormat sample_format>
  void APU::SampleChannelPCM(int id) {
    Channel& channel = m_channels[id];
    Channel::ADPCM& adpcm = channel.adpcm;

    if(channel.samples_left == 0) {
      const u32 loop_point   = m_soundxpnt[id] + (sample_format == SampleFormat::ADPCM ? 1 : 0);
      const u32 loop_address = m_soundxsad[id] + loop_point * sizeof(u32);
      const u32 stop_address = loop_address + m_soundxlen[id] * sizeof(u32);

      if constexpr(sample_format == SampleFormat::ADPCM) {
        if(channel.current_address == loop_address) {
          adpcm.loop_pcm16 = adpcm.current_pcm16;
          adpcm.loop_table_index = adpcm.current_table_index;
        }
      }

      if(channel.current_address == stop_address) {
        switch((RepeatMode)m_soundxcnt[id].repeat_mode) {
          case RepeatMode::Manual: {
            ATOM_PANIC("Unimplemented manual repeat mode");
          }
          case RepeatMode::Loop: {
            channel.current_address = loop_address;

            if constexpr(sample_format == SampleFormat::ADPCM) {
              adpcm.current_pcm16 = adpcm.loop_pcm16;
              adpcm.current_table_index = adpcm.loop_table_index;
            }
            break;
          }
          case RepeatMode::OneShot: {
            m_soundxcnt[id].running = false;
            break;
          }
          case RepeatMode::Prohibited: {
            ATOM_PANIC("Playing channel was in prohibited repeat mode");
          }
        }
      }

      channel.samples_pipe = m_bus.ReadWord(channel.current_address, arm::Memory::Bus::System);
      channel.samples_left = 8;
      channel.current_address += sizeof(u32);
    }

    switch(sample_format) {
      case SampleFormat::PCM8: {
        const i8 sample = (i8)(u8)channel.samples_pipe;
        channel.samples_pipe >>= 8;
        channel.samples_left -= 2;

        channel.current_sample = (f32)sample * (1.f / 127.f);
        break;
      }
      case SampleFormat::PCM16: {
        const i16 sample = (i16)(u16)channel.samples_pipe;
        channel.samples_pipe >>= 16;
        channel.samples_left -= 4;

        channel.current_sample = (f32)sample * (1.f / 32767.f);
        break;
      }
      case SampleFormat::ADPCM: {
        const uint sample = channel.samples_pipe & 0xFu;
        channel.samples_pipe >>= 4;
        channel.samples_left--;

        const int table_index = adpcm.current_table_index;
        const i32 diff = (i32)(k_adpcm_adpcm_tab[table_index] * (((sample & 7) << 1) | 1) >> 3);

        if(sample & 8u) {
          adpcm.current_pcm16 = (i16)std::max(adpcm.current_pcm16 - diff, -32767);
        } else {
          adpcm.current_pcm16 = (i16)std::min(adpcm.current_pcm16 + diff, +32767);
        }

        adpcm.current_table_index = std::clamp(table_index + k_adpcm_index_tab[sample & 7], 0, 88);

        channel.current_sample = (f32)adpcm.current_pcm16 * (1.f / 32767.f);
        break;
      }
      default: {
        ATOM_UNREACHABLE();
      }
    }
  }

  void APU::StartChannel(int id) {
    Channel& channel = m_channels[id];

    const auto sample_format = (SampleFormat)m_soundxcnt[id].sample_format;
    const u32  src_address = m_soundxsad[id] & ~3u;

    if(sample_format == SampleFormat::ADPCM) {
      const u32 adpcm_header = m_bus.ReadWord(src_address, arm::Memory::Bus::System);

      channel.adpcm.current_pcm16 = (i16)(u16)adpcm_header;
      channel.adpcm.current_table_index = std::clamp((int)(adpcm_header >> 16) & 0x7F, 0, 88);
      channel.adpcm.loop_pcm16 = channel.adpcm.current_pcm16;
      channel.adpcm.loop_table_index = channel.adpcm.current_table_index;
      channel.current_address = src_address + sizeof(u32);
    } else if(sample_format == SampleFormat::PSG) {
      channel.current_address = 0u;
      channel.noise_lfsr = 0x7FFFu;
    } else {
      channel.current_address = src_address;
    }

    channel.samples_left = 0;
    channel.sample_format = sample_format;

    ScheduleSampleChannel(id);
  }

  void APU::ScheduleSampleChannel(int id, int cycles_late) {
    Channel& channel = m_channels[id];

    channel.sampling_event = m_scheduler.Add(channel.sampling_interval - cycles_late, channel.sampling_event_fn);
  }

  void APU::CancelSampleChannel(int id) {
    m_scheduler.Cancel(m_channels[id].sampling_event);
  }

  void APU::RecomputeChannelSamplingInterval(int id) {
    m_channels[id].sampling_interval = (int)((0x10000u - (u32)m_soundxtmr[id]) << 1);
  }

  u32 APU::Read_SOUNDxCNT(int id) const {
    return m_soundxcnt[id].word;
  }

  void APU::Write_SOUNDxCNT(int id, u32 value, u32 mask) {
    const u32 write_mask = 0xFF7F837Fu & mask;

    const bool was_running = m_soundxcnt[id].running;

    m_soundxcnt[id].word = (m_soundxcnt[id].word & ~write_mask) | (value & write_mask);

    if(m_soundxcnt[id].running) {
      if(!was_running) StartChannel(id);
    } else {
      if( was_running) CancelSampleChannel(id);
    }
  }

  void APU::Write_SOUNDxSAD(int id, u32 value, u32 mask) {
    const u32 write_mask = 0x07FFFFFFu & mask;

    m_soundxsad[id] = (m_soundxsad[id] & ~write_mask) | (value & write_mask);
  }

  void APU::Write_SOUNDxTMR(int id, u16 value, u16 mask) {
    m_soundxtmr[id] = (m_soundxtmr[id] & ~mask) | (value & mask);

    RecomputeChannelSamplingInterval(id);
  }

  void APU::Write_SOUNDxPNT(int id, u16 value, u16 mask) {
    m_soundxpnt[id] = (m_soundxpnt[id] & ~mask) | (value & mask);
  }

  void APU::Write_SOUNDxLEN(int id, u32 value, u32 mask) {
    const u32 write_mask = 0x003FFFFF & mask;

    m_soundxlen[id] = (m_soundxlen[id] & ~write_mask) | (value & write_mask);
  }

  u32 APU::Read_SOUNDCNT() const {
    return m_soundcnt.word;
  }

  void APU::Write_SOUNDCNT(u32 value, u32 mask) {
    const u32 write_mask = 0x0000BF7Fu & mask;

    m_soundcnt.word = (m_soundcnt.word & ~write_mask) | (value & write_mask);
  }

  u32 APU::Read_SOUNDBIAS() const {
    return m_soundbias;
  }

  void APU::Write_SOUNDBIAS(u32 value, u32 mask) {
    const u32 write_mask = 0x000003FFu & mask;

    m_soundbias = (m_soundbias & ~write_mask) | (value & write_mask);
  }

} // namespace dual::nds::arm7
