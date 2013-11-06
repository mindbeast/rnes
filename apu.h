//
//  apu.h
//  rnes
//
//  Created by Riley Andrews on 7/6/13.
//  Copyright (c) 2013 Riley Andrews. All rights reserved.
//

#ifndef __APU_H__
#define __APU_H__

#include <cstdint>
#include <vector>
#include <cassert>
#include <iostream>
#include <mutex>
#include <condition_variable>

class Sdl;
class Nes;

static bool isPow2(uint32_t i) {
    return ((i - 1) & i) == 0;
}
static const
uint8_t lengthCounterLut[] = {
     10, 254,  20,   2,
     40,   4,  80,   6,
    160,   8,  60,  10,
     14,  12,  26,  14,
     12,  16,  24,  18,
     48,  20,  96,  22,
    192,  24,  72,  26,
     16,  28,  32,  30
};

static uint16_t lengthIndexToValue(uint32_t index)
{
    assert(index < sizeof(lengthCounterLut) / sizeof(lengthCounterLut[0]));
    return lengthCounterLut[index];
}

template <typename T> class RingBuffer {
    std::vector<T> buffer;
    uint32_t size;
    volatile uint64_t get, put;
    std::mutex mutex;
    std::condition_variable cv;
    bool hasData(uint32_t count) const {
        return (get + count) <= put;
    }
    bool hasEmptySpace(uint32_t count) const {
        return (put + count - get) <= size;
    }
public:
    RingBuffer(uint32_t sz) :
        buffer(sz, 0), size{sz}, get{0}, put{0}, mutex{}, cv{} {
        assert(isPow2(sz));
    }
    ~RingBuffer() {}
    void putData(const T *in, uint32_t count) {
        {
            std::unique_lock<std::mutex> lock(mutex);
            for (uint32_t i = 0; i < count; i++) {
                buffer[put & (size - 1)] = in[i];
                put += 1;
            }
        }
        cv.notify_one();
    }
    /*
    void dumpSamples(uint32_t samples) {
        std::unique_lock<std::mutex> lock(mutex);
        get += samples;
    }
    */
    void getData(T *out, uint32_t count) {
        std::unique_lock<std::mutex> lock(mutex);
        if (!hasData(count)) {
            cv.wait(lock, [&]{ return this->hasData(count); });
        }
        for (uint32_t i = 0; i < count; i++) {
            out[i] = buffer[get & (size - 1)];
            get += 1;
        }
    }
};

class Apu;

class Pulse {
    enum {
        PULSE_VOLUME_DECAY, // ddlDnnnn (duty cycle, loop, Disable, n)
        PULSE_SWEEP,        // epppnsss (enable, period, negate, shift)
        PULSE_FREQUENCY,    // llllllll (lower 8 of period)
        PULSE_LENGTH,       // iiiiihhh (length index, upper 3 of period)
        PULSE_REG_COUNT
    };
    enum DutyCycle {
        CYCLE_12_5   = 0,
        CYCLE_25     = 1,
        CYCLE_50     = 2,
        CYCLE_25_NEG = 3,
    };
    uint8_t sequences[4] = {
        0x02, 0x06, 0x1e, 0xf9
    };

    uint8_t *regs;
    bool primary;
    Apu *apu;

    // length logic
    uint8_t lengthCounter;
    
    // envelope logic
    uint8_t envelope;
    uint8_t envelopeDivider;
    bool resetEnvelopeAndDivider;

    // sweep logic
    bool resetSweepDivider;
    uint8_t sweepDivider;
    
    float phase;
    static constexpr float cpuClk = 1.789773 * 1.0E6;

    // current sample
    uint8_t currentSample;
    
    // timer
    uint32_t timerDivider;

    // sequencer offset 
    uint32_t sequencerOffset;

    // Query functions
    bool isEnvelopeLoopSet() const {
        return (regs[PULSE_VOLUME_DECAY] & (1 << 5)) != 0;
    }
    bool isHalted() const {
        return (regs[PULSE_VOLUME_DECAY] & (1 << 5)) != 0;
    }
    bool isDisabled() const {
        return (regs[PULSE_VOLUME_DECAY] & (1 << 4)) != 0;
    }
    uint8_t getEnvelopeN() const {
        return (regs[PULSE_VOLUME_DECAY] & 0xf);
    }
    bool isSweepEnabled() const {
        return (regs[PULSE_SWEEP] & (1 << 7)) != 0;
    }
    bool isSweepNegative() const {
        return (regs[PULSE_SWEEP] & (1 << 3)) != 0;
    }
    bool isFirstPulse() const {
        return primary;
    }
    uint8_t getSweepP() const {
        return (regs[PULSE_SWEEP] >> 4) & 0x7;
    }
    uint8_t getSweepShift() const {
        return (regs[PULSE_SWEEP] & 0x7);
    }
    uint8_t getLengthIndex() const {
        return (regs[PULSE_LENGTH] >> 3);
    }
    uint16_t getTimerPeriod() const {
        return ((uint16_t)regs[PULSE_FREQUENCY] | (((uint16_t)regs[PULSE_LENGTH] & 0x7) << 8)) + 1;
    }
    float getToneFrequency() const {
        return cpuClk / (16.0f * (getTimerPeriod() + 1)); 
    }
    DutyCycle getDutyCycle() const {
        return (DutyCycle)(regs[PULSE_VOLUME_DECAY] >> 6);
    }
    void setTimerPeriod(uint16_t period) {
        regs[PULSE_FREQUENCY] = (uint8_t)period;
        regs[PULSE_LENGTH] = (regs[PULSE_LENGTH] & 0xf8) | ((uint8_t)(period >> 8) & 0x7);
    }
    uint16_t computeSweepTarget() const;
    void sweepPeriod();
    uint8_t getVolume() const;
    void envelopeDividerClock();

public:
    Pulse(uint8_t *argRegs, Apu *parent, bool primaryPulse) : 
        regs{argRegs},
        primary{primaryPulse},
        apu{parent},
        envelope{0},
        envelopeDivider{0},
        resetEnvelopeAndDivider{true},
        resetSweepDivider{true},
        sweepDivider{0},
        phase{0.0f},
        currentSample{0},
        timerDivider{0},
        sequencerOffset{0}
    {}
    Pulse(const Pulse&) = delete;
    ~Pulse() {}

    bool isNonZeroLength() const {
        return lengthCounter != 0;
    }
    void resetLength() {
        lengthCounter = lengthIndexToValue(getLengthIndex());
    }
    void zeroLength() {
        lengthCounter = 0;
    } 
    void resetSequencer() {
        sequencerOffset = 0;
    }
    void resetEnvelope() {
        resetEnvelopeAndDivider = true;
    }
    void resetSweep() {
        resetSweepDivider = true;
    }
    uint8_t getCurrentSample() const {
        // channel is silenced when period is < 8
        const uint16_t timerPeriod = getTimerPeriod();
        if (timerPeriod < 8) {
            return 0;
        }
        // channel silenced when target is above 0x7ff
        const uint16_t targetPeriod = computeSweepTarget();
        if (targetPeriod > 0x7ff) {
            return 0;
        }
        if (!isNonZeroLength()) {
            return 0;
        }
        if (!currentSample) {
            return 0;
        }
        return getVolume();
    }
    void clockEnvelope();
    void clockLengthAndSweep();
    void updateSample();
    void clockTimer();
};

class Triangle {
    enum {
        TRIANGLE_LINEAR_COUNTER, // crrrrrrr (control flag, reload)
        TRIANGLE_UNUSED,         // 
        TRIANGLE_FREQUENCY,      // llllllll (lower 8 of period)
        TRIANGLE_LENGTH,         // iiiiihhh (length index, upper 3 of period)
        TRIANGLE_REG_COUNT
    };
    uint8_t sequence[32] = {
        15, 14, 13, 12, 11, 10,  9,  8,  7,  6,  5,  4,  3,  2,  1,  0,
         0,  1,  2,  3,  4,  5,  6,  7,  8,  9, 10, 11, 12, 13, 14, 15
    };
    uint8_t *regs;
    Apu *apu;
    
    // length logic
    uint8_t lengthCounter;
    
    // sequencer logic 
    float phase;
    static constexpr float cpuClk = 1.789773 * 1.0E6;

    // linear counter logic
    bool linearCounterHalt;
    uint8_t linearCounter;

    // current sample
    uint8_t currentSample;

    // timer
    uint32_t timerDivider;

    // sequencer offset 
    uint32_t sequencerOffset;

    bool isHalted() const {
        return (regs[TRIANGLE_LINEAR_COUNTER] & (1 << 7)) != 0;
    }
    bool isNonZeroLinearCounter() const {
        return linearCounter != 0;
    }
    uint8_t getLengthIndex() const {
        return (regs[TRIANGLE_LENGTH] >> 3);
    }
    uint16_t getTimerPeriod() const {
        return ((uint16_t)regs[TRIANGLE_FREQUENCY] | (((uint16_t)regs[TRIANGLE_LENGTH] & 0x7) << 8)) + 1;
    }
    float getToneFrequency() const {
        return cpuClk / (32.0f * (getTimerPeriod() + 1)); 
    }
    uint8_t getLinearCounterReload() const {
        return (regs[TRIANGLE_LINEAR_COUNTER] & ~(1 << 7));
    }
    bool getControlFlag() const {
        return (regs[TRIANGLE_LINEAR_COUNTER] & (1 << 7)) != 0;
    }
public:   
    bool isNonZeroLength() const {
        return lengthCounter != 0;
    }
    void resetLength() {
        lengthCounter = lengthIndexToValue(getLengthIndex());
    }
    void zeroLength() {
        lengthCounter = 0;
    } 
    void setHaltFlag() {
        linearCounterHalt = true;
    }
    void resetSequencer() {
        sequencerOffset = 0;
        timerDivider = 0;
    }
    uint8_t getCurrentSample() const {
        return currentSample;
    }
    Triangle(uint8_t *argRegs, Apu *parent) : 
        regs{argRegs},
        apu{parent},
        lengthCounter{0},
        phase{0.0f},
        linearCounterHalt{false},
        currentSample{0},
        timerDivider{0},
        sequencerOffset{0}
    {}
    Triangle(const Pulse&) = delete;
    ~Triangle() {}
    void clockLength();
    void clockLinearCounter();
    void updateSample();
    void clockTimer();
};

class Noise {
    enum {
        NOISE_VOLUME_DECAY,   // --lennnn (loop env/disable length, env disable, vol/env period)
        NOISE_UNUSED,         // 
        NOISE_FREQUENCY,      // m---pppp (short mode, period index)
        NOISE_LENGTH,         // lllll--- (length index)
        NOISE_REG_COUNT
    };
    uint8_t *regs;
    Apu *apu;
    
    // length logic
    uint8_t lengthCounter;
    
    // sequencer logic 
    float phase;

    // noise shift register
    uint16_t shiftRegister;
    
    // envelope logic
    uint8_t envelope;
    uint8_t envelopeDivider;
    bool resetEnvelopeAndDivider;

    // current sample
    uint8_t currentSample;

    // timer
    uint32_t timerDivider;

    // constants 
    static constexpr float cpuClk = 1.789773 * 1.0E6;
    uint16_t periodTable[16] = {
        0x004, 0x008, 0x010, 0x020,
        0x040, 0x060, 0x080, 0x0a0,
        0x0ca, 0x0fe, 0x17c, 0x1fc,
        0x2fa, 0x3f8, 0x7f2, 0xfe4 
    };

    // Query functions
    uint8_t getLengthIndex() const {
        return (regs[NOISE_LENGTH] >> 3);
    }
    uint8_t getTimerPeriodIndex() const {
        return regs[NOISE_FREQUENCY] & 0xf;
    }
    uint16_t getTimerPeriod() const {
        uint8_t index = getTimerPeriodIndex();
        assert(index < sizeof(periodTable) / sizeof(periodTable[0]));
        return periodTable[index];
    }
    float getToneFrequency() const {
        return cpuClk / (16.0f * (getTimerPeriod() + 1)); 
    }
    bool isShortMode() const {
        return (regs[NOISE_FREQUENCY] & (1 << 7)) != 0;
    }
    bool isEnvelopeLoopSet() const {
        return (regs[NOISE_VOLUME_DECAY] & (1 << 5)) != 0;
    }
    bool isHalted() const {
        return (regs[NOISE_VOLUME_DECAY] & (1 << 5)) != 0;
    }
    bool isDisabled() const {
        return (regs[NOISE_VOLUME_DECAY] & (1 << 4)) != 0;
    }
    uint8_t getEnvelopeN() const {
        return (regs[NOISE_VOLUME_DECAY] & 0xf);
    }
    uint16_t getNextShiftReg(uint16_t reg) const;
public:   
    uint8_t getVolume() const;
    void envelopeDividerClock();
    bool isNonZeroLength() const {
        return lengthCounter != 0;
    }
    void resetLength() {
        lengthCounter = lengthIndexToValue(getLengthIndex());
    }
    void resetEnvelope() {
        resetEnvelopeAndDivider = true;
    }
    void zeroLength() {
        lengthCounter = 0;
    } 
    void resetSequencer() {
        shiftRegister = 1;
    }
    uint8_t getCurrentSample() const {
        if (!currentSample) {
            return 0;
        }
        if (!isNonZeroLength()) {
            return 0;
        }
        return getVolume();
    }
    Noise(uint8_t *argRegs, Apu *parent) : 
        regs{argRegs},
        apu{parent},
        lengthCounter{0},
        phase{0.0f},
        shiftRegister{1},
        envelope{0},
        envelopeDivider{0},
        resetEnvelopeAndDivider{true},
        currentSample{0},
        timerDivider{0}
    {}
    Noise(const Pulse&) = delete;
    ~Noise() {}
    void clockEnvelope();
    void clockLength();
    void updateSample();
    void clockTimer();
};

class Apu {
public:
    enum {
        // pulse wave channel a
        CHANNEL1_VOLUME_DECAY, // --ldnnnn (loop, disable, n)
        CHANNEL1_SWEEP,        // epppnsss (enable, period, negate, shift)
        CHANNEL1_FREQUENCY,    // llllllll (lower 8 of period)
        CHANNEL1_LENGTH,       // -----hhh (upper 3 of period)
        
        // pulse wave channel b
        CHANNEL2_VOLUME_DECAY,
        CHANNEL2_SWEEP,
        CHANNEL2_FREQUENCY,
        CHANNEL2_LENGTH,
        
        // triangle wave channel
        CHANNEL3_LINEAR_COUNTER,
        CHANNEL3_UNUSED_A,
        CHANNEL3_FREQUENCY,
        CHANNEL3_LENGTH,
        
        // white noise channel
        CHANNEL4_VOLUME_DECAY,
        CHANNEL4_UNUSED_B,
        CHANNEL4_FREQUENCY,
        CHANNEL4_LENGTH,
        
        // pcm channel
        CHANNEL5_PLAY_MODE,
        CHANNEL5_DELTA_COUNTER_LOAD_REGISTER,
        CHANNEL5_ADDR_LOAD_REGISTER,
        CHANNEL5_LENGTH_REGISTER,
        
        // this register is pushed to another module
        SPR_RAM_REG_UNUSED,
        
        // apu control register
        CONTROL_STATUS,
        
        // joypad register (not used here)
        JOYPAD_1,
        
        // softclock register
        SOFTCLOCK,
        
        // not a register
        REG_COUNT,
    };
    
    enum StatusReg {
        STATUS_CHANNEL1_LENGTH     = 1 << 0,
        STATUS_CHANNEL2_LENGTH     = 1 << 1,
        STATUS_CHANNEL3_LENGTH     = 1 << 2,
        STATUS_CHANNEL4_LENGTH     = 1 << 3,
        STATUS_CHANNEL5_LENGTH     = 1 << 4,
        STATUS_FRAME_IRQ_REQUESTED = 1 << 6,
        STATUS_DMC_IRQ_REQUESTED   = 1 << 7,
    };

private:
    static const uint32_t frameCycles = 7457;
    static const uint32_t timerCycles = 32;
    static const uint32_t frameRate = 240;

    static constexpr float cpuClk = 1.789773 * 1.0E6;
    
    Nes *nes;
    Sdl *audio;
    
    // Frame divider 
    uint32_t frameDivider;

    // Frame step
    uint32_t step;

    // Timer divider 
    uint32_t halfTimerDivider;

    // sampler divider
    uint32_t samplerDivider;

    // prior samples buffer
    static const uint32_t sampleBufferSize = 32;
    float samples[sampleBufferSize] = {0.0f};
    uint64_t sampleOffset;

    uint8_t regs[REG_COUNT] = {0};
    uint32_t sampleRate;
    uint64_t fourFrameCount;
    uint64_t fiveFrameCount;

    Pulse pulseA;
    Pulse pulseB;
    Triangle triangle;
    Noise noise;

    RingBuffer<int16_t> rb;
    std::vector<int16_t> sampleBuffer;

public:
    bool isRequestingFrameIrq() {
        return (regs[CONTROL_STATUS] & STATUS_FRAME_IRQ_REQUESTED) != 0;
    }
    bool isRequestingDmcIrq() {
        return (regs[CONTROL_STATUS] & STATUS_DMC_IRQ_REQUESTED) != 0;
    }
    bool isRequestingIrq() {
        return isRequestingDmcIrq() or isRequestingFrameIrq();
    }
    void setRequestFrameIrq() {
        regs[CONTROL_STATUS] |= STATUS_FRAME_IRQ_REQUESTED;
    }
    void clearRequestFrameIrq() {
        regs[CONTROL_STATUS] &= ~STATUS_FRAME_IRQ_REQUESTED;
    }
    void setRequestDmcIrq() {
        regs[CONTROL_STATUS] |= STATUS_DMC_IRQ_REQUESTED;
    }
    void clearRequestDmcIrq() {
        regs[CONTROL_STATUS] &= ~STATUS_DMC_IRQ_REQUESTED;
    }
    bool isFourStepFrame() {
        return (regs[SOFTCLOCK] & (1u << 7)) == 0;
    }
    bool isFrameIntEnabled() {
        return (regs[SOFTCLOCK] & (1u << 6)) == 0;
    }
    void clockLengthAndSweep();
    void clockEnvAndTriangle();
    void resetFrameCounter();
    void stepAdvance();
    void stepFastTimers();
    void stepSlowTimers();
    void generateSample();
    void tick();
    void run(int cycles);
    void writeReg(uint32_t reg, uint8_t val);
    uint8_t readReg(uint32_t reg);
    Apu(Nes *parent, Sdl *audio);
    Apu() = delete;
    Apu(const Apu&) = delete;
    ~Apu();
};
#endif
