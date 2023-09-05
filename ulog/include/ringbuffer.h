#ifndef RINGBUFFER_H
#define RINGBUFFER_H

#include <unistd.h>

#include <array>
#include <atomic>
#include <cassert>
#include <condition_variable>
#include <iostream>
#include <mutex>
#include <optional>
#include <thread>
#include <unordered_map>

template <int Size>
class RingBuffer {
private:
  enum class AllocationType { Empty = 1, Dummy = 2, Populated = 3 };
  struct Allocation {
    uint32_t size_;
    uint32_t begin_;
    AllocationType type_;
    uint64_t id_;
    const char* metadata_ = nullptr;
    const char* type_name_ = nullptr;
    uint64_t topic_name_hash_ = 0;

    Allocation(uint32_t size, uint32_t begin, AllocationType type, uint64_t id, const char* metadata,
               const char* type_name, const uint64_t topic_name_hash)
        : size_(size)
        , begin_(begin)
        , type_(type)
        , id_(id)
        , metadata_(metadata)
        , type_name_(type_name)
        , topic_name_hash_(topic_name_hash) {}

    Allocation() {}
  };

  std::array<uint8_t, Size> m_buf;
  std::atomic<uint32_t> m_write;
  std::mutex lk;
  std::condition_variable freecv;
  std::condition_variable fullcv;
  std::atomic<uint32_t> m_sizeAllocated;
  std::unordered_map<uint32_t, Allocation> allocations;
  std::mutex alloclock;
  std::atomic<uint64_t> allocationId;
  std::atomic<uint64_t> lastUnfreedId;

public:
  struct Buffer {
    uint8_t* loc;
    uint32_t size;
    const char* metadata;
    const char* type_name;
    const uint64_t topic_name_hash;
  };
  RingBuffer()
      : m_buf()
      , m_write(0)
      , lk()
      , freecv()
      , fullcv()
      , m_sizeAllocated(0)
      , allocations()
      , alloclock()
      , allocationId(0)
      , lastUnfreedId(0) {}
  ~RingBuffer() {}

  uint32_t size() { return m_sizeAllocated.load(); }
  std::optional<Allocation> alloc_inner(int size, const char* metadata, const char* type_name,
                                        const uint64_t& topic_name_hash) {
    std::unique_lock<std::mutex> uniquelock(lk);

    freecv.wait(uniquelock, [&]() {
      if ((Size - int(m_sizeAllocated)) > size) {
        return true;
      } else {
        // std::cout << "can't push: " << Size - m_sizeAllocated << " " << size
        // << std::endl;
        return false;
      }
    });

    if (size != 0 && m_write + size <= Size) {
      Allocation a(size, m_write, AllocationType::Empty, allocationId++, metadata, type_name,
                   topic_name_hash);
      alloclock.lock();
      allocations[a.id_] = a;
      alloclock.unlock();
      m_sizeAllocated += a.size_;
      m_write += size;
      uniquelock.unlock();
      return std::optional<Allocation>(a);
    } else {
      Allocation a(Size - m_write, m_write, AllocationType::Dummy, allocationId++, nullptr, nullptr, 0);
      alloclock.lock();
      allocations[a.id_] = a;
      alloclock.unlock();
      // hack for teardown
      m_sizeAllocated += (a.size_ == 0 ? 1 : a.size_);
      m_write = 0;
      uniquelock.unlock();
      return std::optional<Allocation>();
    }
  }

  uint64_t alloc(int size, const char* metadata, const char* type_name, const uint64_t topic_name_hash = 0) {
    std::optional<Allocation> r;
    while (!(r = alloc_inner(size, metadata, type_name, topic_name_hash))) {
      usleep(100);
    }
    return (*r).id_;
  }

  bool populate(uint64_t dst, uint8_t* src = nullptr) {
    alloclock.lock();
    Allocation a = allocations[dst];
    alloclock.unlock();

    assert(a.type_ == AllocationType::Empty);
    if (src) {
      std::copy(src, src + a.size_, &m_buf[a.begin_]);
    }

    alloclock.lock();
    allocations[dst].type_ = AllocationType::Populated;
    alloclock.unlock();
    fullcv.notify_one();
    return true;
  }

  uint8_t* handleToAddress(uint64_t handle) {
    alloclock.lock();
    Allocation a = allocations[handle];
    alloclock.unlock();

    return &m_buf[a.begin_];
  }

  std::optional<Buffer> lastUnread() {
    std::unique_lock uniquelock(lk);

    fullcv.wait(uniquelock, [&]() {
      if (m_sizeAllocated > 0) {
        return true;
      } else {
        return false;
      }
    });

    alloclock.lock();
    if (allocations.size()) {
      if (allocations[lastUnfreedId].type_ != AllocationType::Populated &&
          allocations[lastUnfreedId].type_ != AllocationType::Dummy) {
        alloclock.unlock();
        uniquelock.unlock();
        return std::optional<Buffer>();
      } else {
        Allocation a = allocations[lastUnfreedId];
        alloclock.unlock();

        if (a.type_ == AllocationType::Populated) {
          uniquelock.unlock();
          return std::optional<Buffer>(
              {handleToAddress(lastUnfreedId), a.size_, a.metadata_, a.type_name_, a.topic_name_hash_});
        } else {
          uniquelock.unlock();
          return std::optional<Buffer>({nullptr, 0, nullptr, nullptr, 0});
        }
      }
    } else {
      alloclock.unlock();
      return std::optional<Buffer>();
    }
  }

  bool dequeue() {
    std::unique_lock uniquelock(lk);

    fullcv.wait(uniquelock, [&]() {
      if (m_sizeAllocated > 0) {
        return true;
      } else {
        return false;
      }
    });

    alloclock.lock();
    if (allocations.size()) {
      if (allocations[lastUnfreedId].type_ != AllocationType::Populated &&
          allocations[lastUnfreedId].type_ != AllocationType::Dummy) {
        alloclock.unlock();
        uniquelock.unlock();
        return false;
      } else {
        Allocation a = allocations[lastUnfreedId];
        allocations.erase(a.id_);
        alloclock.unlock();
        lastUnfreedId++;
        m_sizeAllocated -= a.size_;
        uniquelock.unlock();
        freecv.notify_one();
        return true;
      }
    } else {
      alloclock.unlock();
      uniquelock.unlock();
      return false;
    }
  }

  bool consume(uint8_t* dst, uint32_t& size, const char** metadata, const char** type_name) {
    std::unique_lock uniquelock(lk);

    fullcv.wait(uniquelock, [&]() {
      if (m_sizeAllocated > 0) {
        return true;
      } else {
        return false;
      }
    });

    alloclock.lock();
    if (allocations.size()) {
      if (allocations[lastUnfreedId].type_ != AllocationType::Populated &&
          allocations[lastUnfreedId].type_ != AllocationType::Dummy) {
        alloclock.unlock();
        uniquelock.unlock();
        return false;
      } else {
        Allocation a = allocations[lastUnfreedId];
        allocations.erase(a.id_);
        alloclock.unlock();
        lastUnfreedId++;
        m_sizeAllocated -= a.size_;

        if (a.type_ == AllocationType::Populated) {
          size = a.size_;
          *metadata = a.metadata_;
          *type_name = a.type_name_;
          std::copy(&m_buf[a.begin_], &m_buf[a.begin_ + a.size_], dst);
        } else {
          size = 0;
        }
        uniquelock.unlock();
        freecv.notify_one();
        return true;
      }
    } else {
      alloclock.unlock();
      uniquelock.unlock();
      return false;
    }
  }
};

#endif  // RINGBUFFER_H
