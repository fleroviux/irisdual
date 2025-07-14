
#pragma once

#include <array>
#include <atomic>
#include <atom/bit.hpp>
#include <atom/float.hpp>
#include <atom/integer.hpp>
#include <atom/vector_n.hpp>
#include <dual/arm/memory.hpp>
#include <dual/common/scheduler.hpp>
#include <dual/audio_driver.hpp>
#include <functional>
#include <memory>

namespace dual::nds::arm7 {

  class APU {
    public:
      APU(Scheduler& scheduler, arm::Memory& bus);
     ~APU();

      void Reset();

      AudioDriverBase* GetAudioDriver();
      void SetAudioDriver(std::shared_ptr<AudioDriverBase> audio_driver);

      bool GetEnableOutput() const;
      void SetEnableOutput(bool enable);

      u32   Read_SOUNDxCNT(int id) const;
      void Write_SOUNDxCNT(int id, u32 value, u32 mask);
      void Write_SOUNDxSAD(int id, u32 value, u32 mask);
      void Write_SOUNDxTMR(int id, u16 value, u16 mask);
      void Write_SOUNDxPNT(int id, u16 value, u16 mask);
      void Write_SOUNDxLEN(int id, u32 value, u32 mask);
      u32   Read_SOUNDCNT() const;
      void Write_SOUNDCNT(u32 value, u32 mask);
      u32   Read_SOUNDBIAS() const;
      void Write_SOUNDBIAS(u32 value, u32 mask);

    private:
      static constexpr int k_cycles_per_sample = 1024;

      enum class RepeatMode {
        Manual,
        Loop,
        OneShot,
        Prohibited
      };

      enum class SampleFormat {
        PCM8,
        PCM16,
        ADPCM,
        PSG
      };

      enum class OutputSource {
        Mixer,
        Channel1,
        Channel3,
        Channel1And3
      };

      void SampleMixers(int cycles_late);
      void SampleChannel(int id, int cycles_late);
      void SampleChannelPSG(int id);
      template<SampleFormat sample_format> void SampleChannelPCM(int id);
      void StartChannel(int id);
      void ScheduleSampleChannel(int id, int cycles_lates = 0);
      void CancelSampleChannel(int id);
      void RecomputeChannelSamplingInterval(int id);

      union SOUNDxCNT {
        atom::Bits< 0, 7, u32> volume_mul;
        atom::Bits< 8, 2, u32> volume_div;
        atom::Bits<15, 1, u32> hold_last_sample;
        atom::Bits<16, 7, u32> panning;
        atom::Bits<24, 3, u32> psg_wave_duty;
        atom::Bits<27, 2, u32> repeat_mode;
        atom::Bits<29, 2, u32> sample_format;
        atom::Bits<31, 1, u32> running;

        u32 word = 0u;
      };

      std::array<SOUNDxCNT, 16> m_soundxcnt{};
      std::array<u32, 16> m_soundxsad{};
      std::array<u16, 16> m_soundxtmr{};
      std::array<u16, 16> m_soundxpnt{};
      std::array<u32, 16> m_soundxlen{};

      union SOUNDCNT {
        atom::Bits< 0, 7, u32> master_volume;
        atom::Bits< 8, 2, u32> left_output_source;
        atom::Bits<10, 2, u32> right_output_source;
        atom::Bits<12, 1, u32> do_not_output_ch1_to_mixer;
        atom::Bits<13, 1, u32> do_not_output_ch3_to_mixer;
        atom::Bits<15, 1, u32> master_enable;

        u32 word = 0u;
      } m_soundcnt{};

      u32 m_soundbias{};

      struct Channel {
        int sampling_interval{};
        Scheduler::Event* sampling_event{};
        std::function<void(int)> sampling_event_fn{};
        f32 current_sample{0.f};
        u32 current_address{};
        SampleFormat sample_format{};

        struct ADPCM {
          i16 current_pcm16;
          int current_table_index;
          i16 loop_pcm16;
          int loop_table_index;
        } adpcm{};

        u16 noise_lfsr{};

        int samples_left{};
        u32 samples_pipe{};
      };

      std::array<Channel, 16> m_channels;

      Scheduler& m_scheduler;
      arm::Memory& m_bus;

      std::shared_ptr<AudioDriverBase> m_audio_driver;
      atom::Vector_N<i16, 1024> m_audio_buffer;
      bool m_output_enable{true};
  };

} // namespace dual::nds::arm7
