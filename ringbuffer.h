//
//  ringbuffer.h
//  rnes
//
//  Created by Riley Andrews on 7/6/13.
//  Copyright (c) 2013 Riley Andrews. All rights reserved.
//

#ifndef __RINGBUFFER_H__
#define __RINGBUFFER_H__

#include <cstdint>
#include <vector>
#include <cassert>
#include <iostream>
#include <mutex>
#include <condition_variable>

namespace Rnes {

static bool isPow2(uint32_t i) {
    return ((i - 1) & i) == 0;
}

template <typename T> class RingBuffer {
    std::vector<T> buffer;
    uint32_t size;
    volatile uint64_t get, put;
    std::mutex mutex;
    std::condition_variable consumeCv;
    std::condition_variable produceCv;
    bool hasData(uint32_t count) const {
        return (get + count) <= put;
    }
    bool hasEmptySpace(uint32_t count) const {
        return (put + count - get) <= size;
    }
public:
    RingBuffer(uint32_t sz) :
        buffer(sz, 0), size{sz}, get{0}, put{0}, mutex{}, consumeCv{}, produceCv{} {
        assert(isPow2(sz));
    }
    ~RingBuffer() {}
    void putData(const T *in, uint32_t count) {
        {
            std::unique_lock<std::mutex> lock(mutex);
            if (!hasEmptySpace(count)) {
                produceCv.wait(lock, [&]{ return this->hasEmptySpace(count); });
            }
            for (uint32_t i = 0; i < count; i++) {
                buffer[put & (size - 1)] = in[i];
                put += 1;
            }
        }
        consumeCv.notify_one();
    }
    void getData(T *out, uint32_t count) {
        {
            std::unique_lock<std::mutex> lock(mutex);
            if (!hasData(count)) {
                consumeCv.wait(lock, [&]{ return this->hasData(count); });
            }
            for (uint32_t i = 0; i < count; i++) {
                out[i] = buffer[get & (size - 1)];
                get += 1;
            }
        }
        produceCv.notify_one();
    }
};

};
#endif
