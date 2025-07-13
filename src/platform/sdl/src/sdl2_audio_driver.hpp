
#include <atom/integer.hpp>
#include <atomic>
#include <dual/audio_driver.hpp>
#include <vector>

#include <SDL.h>

class SDL2AudioDriver final : public dual::AudioDriverBase {
  public:
    bool Open(uint sample_rate, uint buffer_size) override;
    void Close() override;
    uint GetBufferSize() const override;
    void QueueSamples(std::span<i16> buffer) override;
    uint GetNumberOfQueuedSamples() const override;
    void WaitBufferHalfEmpty() const override;

  private:
    static constexpr size_t k_channel_count = 2u;

    SDL_AudioDeviceID m_audio_device{};
    SDL_AudioSpec m_have{};

    std::vector<i16> m_audio_buffer{};
    size_t m_rd_position{};
    size_t m_wr_position{};
    std::atomic_size_t m_current_buffer_size{};

    friend void SDL2AudioDriver_AudioCallback(SDL2AudioDriver* audio_driver, i16* stream, int length);
};