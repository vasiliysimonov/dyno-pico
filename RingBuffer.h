#ifndef __RING_BUFFER_H__
#define __RING_BUFFER_H__

#include <stdio.h>
#include <stdlib.h>
#include "pico/mutex.h"

#define SIZE 64 // power of 2

struct RingBuffer {
    int32_t head;
    int32_t tail;
    bool full;
    uint32_t deltas[SIZE];
    uint32_t times[SIZE];
    mutex_t mutex;

    RingBuffer() :
        head(),
        tail(),
        full(false) 
    {
        mutex_init(&mutex);
    }

    void clear() {
        mutex_enter_blocking(&mutex);
        head = 0;
        tail = 0;
        full = false;
        mutex_exit(&mutex);
    }

    void push(uint32_t time, uint32_t delta) {
        mutex_enter_blocking(&mutex);
        deltas[tail] = delta;
        times[tail] = time;
        tail = next(tail);
        if (full) { // move head forward like popping
            head = tail;
        } else {
            full = (tail == head);
        }
        mutex_exit(&mutex);
    }

    bool pop(uint32_t& time, uint32_t& delta) { 
        mutex_enter_blocking(&mutex);
        bool result = false;
        if (full || head != tail) {
            auto nextHead = 0;
            time = times[head];
            delta = deltas[head];
            head = next(head);
            full = false;
            result = true;
        }
        mutex_exit(&mutex);
        return result;
    }

    uint32_t size() {
        if (full) return SIZE;
        int32_t diff = tail - head;
        return (diff >= 0) ? diff : diff + SIZE;
    }

    inline int32_t next(int32_t i) {
        return (i + 1) & (SIZE - 1);
    }
};

#endif