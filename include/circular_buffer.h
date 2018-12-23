#pragma once

#include <semaphore.h>

class circular_buffer
{
    unsigned int buf_size;
    unsigned int head_ = 0;
    unsigned int tail_ = 0;
    bool         full_ = 0;    
    sem_t*       sem = nullptr; // points to shared memory for semaphores

public:

    /// Reset the buffer, mark everything empty.
    void reset()
    {
      head_ = tail_;
      full_ = false;
    }

    /// Returns wether the buffer is empty or not.
    bool empty() const
    {
        return (!full_ && (head_ == tail_));
    }

    /// Returns wether the buffer is full or not.
    bool full() const
    {
        return full_;
    }

    /// Returns how many valid items are there in the buffer.
    unsigned int size() const
    {
        unsigned int sz = buf_size;

        if (!full_) {
            if (head_ >= tail_) {
                sz = head_ - tail_;
            } else {
                sz = buf_size + head_ - tail_;
            }
        }

        return sz;
    }

    /// Get the index on the buffer of the next empty element.
    /// This function blocks if the buffer is full
    unsigned int get_next_empty()
    {
        if (full()) {
            // TODO: Block
        }
        return head_;
    }

    /// This function will add an item at the top
    void publish()
    {
        if (full()) {
            // TODO: Block
        }
        // TODO: mutex for this?
        head_ = (head_ + 1) % buf_size;
        // Update full, see if tail and head meet
        full_ = head_ == tail_;
    }

    unsigned int get_next_full()
    {
        if (empty()) {
            // TODO: Block
        }
        return tail_;
    }

    void release()
    {
        if (empty())
        {
            // TODO: Block
        }
        full_ = false;
        tail_ = (tail_ + 1) % buf_size;
    }

};