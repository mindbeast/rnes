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

template <typename T> struct RingBuffer {
    std::vector<T> buffer;
    uint32_t size;
    uint64_t get, put;
    RingBuffer(uint32_t sz) : buffer(sz, 0), size{sz}, get{0}, put{0} {
        assert(isPow2(sz));
    }
    ~RingBuffer() {}
    bool hasData(uint32_t count) {
        return (get + count) <= put;
    }
    bool hasEmptySpace(uint32_t count) {
        return (put + count - get) <= size;
    }
    void putData(const T *in, uint32_t count) {
        assert(hasEmptySpace(count));
        for (uint32_t i = 0; i < count; i++) {
            buffer[put & (size - 1)] = in[i];
            put += 1;
        }
    }
    void getData(T *out, uint32_t count) {
        assert(hasData(count)); 
        for (uint32_t i = 0; i < count; i++) {
            out[i] = buffer[get & (size - 1)];
            get += 1;
        }
    }
    uint32_t getCount() {
        return put - get;
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
    
    float time;
    static constexpr float cpuClk = 1.789773 * 1.0E6;

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
        return (uint16_t)regs[PULSE_FREQUENCY] | (((uint16_t)regs[PULSE_LENGTH] & 0x7) << 8);
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
    uint8_t getVolume();
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
        time(0.0f) {}
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
        time = 0.0f;
    }
    void resetEnvelope() {
        resetEnvelopeAndDivider = true;
    }
    void resetSweep() {
        resetSweepDivider = true;
    }
    void clockEnvelope();
    void clockLengthAndSweep();
    void generateFrame(std::vector<uint8_t>& ref, uint32_t sampleRate, uint32_t reqSamples);
};

class Triangle {
    enum {
        TRIANGLE_LINEAR_COUNTER, // crrrrrrr (control flag, reload)
        TRIANGLE_UNUSED,         // 
        TRIANGLE_FREQUENCY,      // llllllll (lower 8 of period)
        TRIANGLE_LENGTH,         // iiiiihhh (length index, upper 3 of period)
        TRIANGLE_REG_COUNT
    };
    uint8_t *regs;
    Apu *apu;
    
    // length logic
    uint8_t lengthCounter;
    
    // sequencer logic 
    float time;
    static constexpr float cpuClk = 1.789773 * 1.0E6;

    // linear counter logic
    bool linearCounterHalt;
    uint8_t linearCounter;

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
        return (uint16_t)regs[TRIANGLE_FREQUENCY] | (((uint16_t)regs[TRIANGLE_LENGTH] & 0x7) << 8);
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
        time = 0.0f; 
    }
    Triangle(uint8_t *argRegs, Apu *parent) : 
        regs{argRegs},
        apu{parent},
        lengthCounter{0},
        time{0.0f},
        linearCounterHalt{false}
    {}
    Triangle(const Pulse&) = delete;
    ~Triangle() {}
    void clockLength();
    void clockLinearCounter();
    void generateFrame(std::vector<uint8_t>& ref, uint32_t sampleRate, uint32_t reqSamples);
};

/*
class Noise {
    enum {
        NOISE_LINEAR_COUNTER, // crrrrrrr (control flag, reload)
        NOISE_UNUSED,         // 
        NOISE_FREQUENCY,      // llllllll (lower 8 of period)
        NOISE_LENGTH,         // iiiiihhh (length index, upper 3 of period)
        NOISE_REG_COUNT
    };
    uint8_t *regs;
    Apu *apu;
    
    // length logic
    uint8_t lengthCounter;
    
    // sequencer logic 
    float time;
    static constexpr float cpuClk = 1.789773 * 1.0E6;

    // linear counter logic
    bool linearCounterHalt;
    uint8_t linearCounter;

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
        return (uint16_t)regs[TRIANGLE_FREQUENCY] | (((uint16_t)regs[TRIANGLE_LENGTH] & 0x7) << 8);
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
        time = 0.0f; 
    }
    Noise(uint8_t *argRegs, Apu *parent) : 
        regs{argRegs},
        apu{parent},
        lengthCounter{0},
        time{0.0f},
        linearCounterHalt{false}
    {}
    Noise(const Pulse&) = delete;
    ~Noise() {}
    void clockLength();
    void clockLinearCounter();
    void generateFrame(std::vector<uint8_t>& ref, uint32_t sampleRate, uint32_t reqSamples);
};
*/

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
    static const uint32_t cycleDivider = 7457;
    static const uint32_t fiveStepRate = 192;
    static const uint32_t fourStepRate = 240;
    
    Nes *nes;
    Sdl *audio;
    
    uint64_t cycle;
    uint32_t step;
    uint8_t regs[REG_COUNT] = {0};
    uint32_t sampleRate;
    uint64_t fourFrameCount;
    uint64_t fiveFrameCount;

    Pulse pulseA;
    Pulse pulseB;
    Triangle triangle;

    std::vector<uint8_t> pulseAFrame;
    std::vector<uint8_t> pulseBFrame;
    std::vector<uint8_t> triangleFrame;
    std::vector<int16_t> fullFrame;

    RingBuffer<int16_t> rb;

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
    void generateFrameSamples() {
        const bool isFourStep = isFourStepFrame();
        uint32_t frameRate;
        if (isFourStep) {
            frameRate = fourStepRate;
        }
        else {
            frameRate = fiveStepRate;
        }
        
        uint32_t samples = sampleRate / frameRate;
        const uint32_t extraSamples = sampleRate - samples * frameRate;
        uint64_t &frameCount = isFourStep ? fourFrameCount : fiveFrameCount;
        if ((frameCount % frameRate) < extraSamples) {
            samples += 1;
        }
        frameCount++;

        pulseAFrame.resize(samples);     
        pulseA.generateFrame(pulseAFrame, sampleRate, samples);
        
        pulseBFrame.resize(samples);     
        pulseB.generateFrame(pulseBFrame, sampleRate, samples);

        triangleFrame.resize(samples);
        triangle.generateFrame(triangleFrame, sampleRate, samples);

        fullFrame.resize(samples);
        for (uint32_t i = 0; i < samples; i++) {
            float sample = 95.88f  / ((8128.0f  / (float(pulseAFrame[i] + pulseBFrame[i]))) + 100.0f);
            sample += 159.79f / (100.0f + (1.0f / ((float(triangleFrame[i]) / 8227.0f))));
            int16_t truncSamples = (int16_t)(sample * (1 << 14));
            fullFrame[i] = truncSamples;
        }
        if (rb.hasEmptySpace(samples)) {
            rb.putData(&fullFrame[0], samples);
        }
    }
    void clockLengthAndSweep() {
        pulseA.clockLengthAndSweep();
        pulseB.clockLengthAndSweep();
        triangle.clockLength();
    }
    void clockEnvAndTriangle() {
        pulseA.clockEnvelope(); 
        pulseB.clockEnvelope();
        triangle.clockLinearCounter();
        generateFrameSamples();
    }
    void resetFrameCounter() {
        cycle = 0;
        step = 0;
    }
    void stepAdvance() {
        uint32_t frameSteps = isFourStepFrame() ? 4 : 5;
        
        if (isFourStepFrame())  {
            if (step % 2) {
                clockLengthAndSweep();
            }
            clockEnvAndTriangle();
            if ((step == 3) and isFrameIntEnabled()) {
                setRequestFrameIrq();
            }
        }
        else {
            if (step < 4) {
                clockEnvAndTriangle();
            }
            if (step == 0 or step == 2) {
                clockLengthAndSweep();
            }
        }

        step += 1;
        if (step == frameSteps) {
            step = 0;
        }
    }
    void tick() {
        cycle += 1;
        if (cycle == cycleDivider) {
            cycle = 0;
            stepAdvance();
        }
    }
    void run(int cycles) {
        for (int i = 0; i < cycles; i++) {
            tick();
        }
    }
    void writeReg(uint32_t reg, uint8_t val) {
        regs[reg] = val;
        if (reg == SOFTCLOCK) {
            resetFrameCounter();
        }
        else if (reg == CONTROL_STATUS) {
            clearRequestDmcIrq();
            // xx what to do with the upper bits here
            if (~val & STATUS_CHANNEL1_LENGTH) {
                pulseA.zeroLength();
            }
            if (~val & STATUS_CHANNEL2_LENGTH) {
                pulseB.zeroLength();
            }
            if (~val & STATUS_CHANNEL3_LENGTH) {
                triangle.zeroLength();
            }
        }
        else if (reg == CHANNEL1_LENGTH) {
            pulseA.resetLength();
            pulseA.resetSequencer();
            pulseA.resetEnvelope();
        }
        else if (reg == CHANNEL2_LENGTH) {
            pulseB.resetLength();
            pulseB.resetSequencer();
            pulseB.resetEnvelope();
        } 
        else if (reg == CHANNEL3_LENGTH) {
            triangle.resetLength();
            triangle.resetSequencer();
            triangle.setHaltFlag();
        }
    }
    uint8_t readReg(uint32_t reg) {
        uint8_t result = regs[reg];
        if (reg == CONTROL_STATUS) {
            clearRequestFrameIrq();
            result = result & (STATUS_FRAME_IRQ_REQUESTED | STATUS_DMC_IRQ_REQUESTED);
            result |= pulseA.isNonZeroLength() ? STATUS_CHANNEL1_LENGTH : 0;
            result |= pulseB.isNonZeroLength() ? STATUS_CHANNEL2_LENGTH : 0;
            result |= triangle.isNonZeroLength() ? STATUS_CHANNEL3_LENGTH : 0;
        }
        return result;
    }
    
    Apu(Nes *parent, Sdl *audio);
    Apu() = delete;
    Apu(const Apu&) = delete;
    ~Apu();
};
#endif
