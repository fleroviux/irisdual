
#include <atom/panic.hpp>
#include <dual/common/scheduler.hpp>

namespace dual {

  Scheduler::Scheduler() {
    for(int i = 0; i < k_event_limit; i++) {
      heap[i] = new Event();
      heap[i]->handle = i;
    }
    Reset();
  }

  Scheduler::~Scheduler() {
    for(int i = 0; i < k_event_limit; i++) {
      delete heap[i];
    }
  }

  void Scheduler::Reset() {
    heap_size = 0;
    timestamp_now = 0;
  }

  void Scheduler::Step() {
    const u64 now = GetTimestampNow();

    while(heap_size > 0 && heap[0]->timestamp <= now) {
      auto event = heap[0];

      event->callback(int(now - event->timestamp));

      // @note: the handle may have changed due to the event callback.
      Remove(event->handle);
    }
  }

  auto Scheduler::Add(u64 delay, std::function<void(int)> callback) -> Event* {
    int n = heap_size++;
    int p = Parent(n);

    if(heap_size > k_event_limit) {
      ATOM_PANIC("exceeded maximum number of scheduler events.");
    }

    auto event = heap[n];
    event->timestamp = GetTimestampNow() + delay;
    event->callback = callback;

    while(n != 0 && heap[p]->timestamp > heap[n]->timestamp) {
      Swap(n, p);
      n = p;
      p = Parent(n);
    }

    return event;
  }

  void Scheduler::Remove(int n) {
    Swap(n, --heap_size);

    int p = Parent(n);

    if(n != 0 && heap[p]->timestamp > heap[n]->timestamp) {
      do {
        Swap(n, p);
        n = p;
        p = Parent(n);
      } while (n != 0 && heap[p]->timestamp > heap[n]->timestamp);
    } else {
      Heapify(n);
    }
  }

  void Scheduler::Swap(int i, int j) {
    auto tmp = heap[i];
    heap[i] = heap[j];
    heap[j] = tmp;
    heap[i]->handle = i;
    heap[j]->handle = j;
  }

  void Scheduler::Heapify(int n) {
    const int l = LeftChild(n);
    const int r = RightChild(n);

    if(l < heap_size && heap[l]->timestamp < heap[n]->timestamp) {
      Swap(l, n);
      Heapify(l);
    }

    if(r < heap_size && heap[r]->timestamp < heap[n]->timestamp) {
      Swap(r, n);
      Heapify(r);
    }
  }

} // namespace dual