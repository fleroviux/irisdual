
#pragma once

#include <atom/integer.hpp>
#include <span>

namespace dual {

  class AudioDriverBase {
    public:
      using Callback = void (*)(void* user_data, i16* stream, int byte_len);

      virtual ~AudioDriverBase() = default;

      virtual bool Open(uint sample_rate, uint buffer_size) = 0;
      virtual void Close() = 0;
      virtual uint GetBufferSize() const = 0;
      virtual void QueueSamples(std::span<i16> buffer) = 0;
      virtual uint GetNumberOfQueuedSamples() const = 0;
      virtual void WaitBufferHalfEmpty() const = 0;
  };

} // namespace dual
