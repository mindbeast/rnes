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

class Pulse {
public:
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
    Pulse(uint8_t *argRegs, bool primaryPulse) : 
        regs{argRegs},
        primary{primaryPulse},
        envelope{0},
        envelopeDivider{0},
        resetEnvelopeAndDivider{true},
        resetSweepDivider{true},
        sweepDivider{0},
        time(0.0f) {}
    Pulse(const Pulse&) = delete;
    ~Pulse() {}

    // Query functions
    bool isEnvelopeLoopSet() {
        return (regs[PULSE_VOLUME_DECAY] & (1 << 5)) != 0;
    }
    bool isHalted() {
        return (regs[PULSE_VOLUME_DECAY] & (1 << 5)) != 0;
    }
    bool isDisabled() {
        return (regs[PULSE_VOLUME_DECAY] & (1 << 4)) != 0;
    }
    bool isNonZeroLength() {
        return lengthCounter != 0;
    }
    uint8_t getEnvelopeN() {
        return (regs[PULSE_VOLUME_DECAY] & 0xf);
    }
    bool isSweepEnabled() {
        return (regs[PULSE_SWEEP] & (1 << 7)) != 0;
    }
    bool isSweepNegative() {
        return (regs[PULSE_SWEEP] & (1 << 3)) != 0;
    }
    bool isFirstPulse() {
        return primary;
    }
    uint8_t getSweepP() {
        return (regs[PULSE_SWEEP] >> 4) & 0x7;
    }
    uint8_t getSweepShift() {
        return (regs[PULSE_SWEEP] & 0x7);
    }
    uint8_t getLengthIndex() {
        return (regs[PULSE_LENGTH] >> 3);
    }
    uint16_t getTimerPeriod() {
        return (uint16_t)regs[PULSE_FREQUENCY] | (((uint16_t)regs[PULSE_LENGTH] & 0x7) << 8);
    }
    void setTimerPeriod(uint16_t period) {
        regs[PULSE_FREQUENCY] = (uint8_t)period;
        regs[PULSE_LENGTH] = (regs[PULSE_LENGTH] & 0xf8) | ((uint8_t)(period >> 8) & 0x7);
    }
    float getToneFrequency() {
        return cpuClk / (16.0f * (getTimerPeriod() + 1)); 
    }
    DutyCycle getDutyCycle() {
        return (DutyCycle)(regs[PULSE_VOLUME_DECAY] >> 6);
    }

    // Internal pulse unit functions
    void envelopeDividerClock() {
        if (envelope) {
            envelope--;
        }
        else if (isEnvelopeLoopSet()) {
            envelope = 15; 
        }
    }
    void clockEnvelope() {
        if (resetEnvelopeAndDivider) {
            envelope = 15;
            envelopeDivider = getEnvelopeN() + 1;
            resetEnvelopeAndDivider = false;
        } 
        else {
            if (envelopeDivider) {
                envelopeDivider--;
            }
            else {
                envelopeDividerClock();
                envelopeDivider = getEnvelopeN() + 1;
            }
        }
    }
    uint16_t computeSweepTarget() {
        uint16_t period = getTimerPeriod(); 
        uint16_t shiftedPeriod = period >> getSweepShift();
        if (isSweepNegative()) {
            shiftedPeriod = isFirstPulse() ? -shiftedPeriod : -shiftedPeriod + 1;
        }
        uint16_t targetPeriod = period + shiftedPeriod;
        return targetPeriod;
    }
    void sweepPeriod() {
        uint16_t targetPeriod = computeSweepTarget();
        uint16_t period = getTimerPeriod(); 
        if (targetPeriod > 0x7ff or period < 8) {
            return;
        }
        setTimerPeriod(targetPeriod); 
    }
    void clockLengthAndSweep() {
        // Length
        if (!isHalted() and lengthCounter) {
           lengthCounter--; 
        }   
        // Sweep
        if (resetSweepDivider) {
            sweepDivider = getSweepP() + 1;
            resetSweepDivider = false;
        }
        else {
            if (sweepDivider) {
                sweepDivider--; 
            } 
            else {
                if (isSweepEnabled()) {
                    sweepDivider = getSweepP() + 1;
                    sweepPeriod();
                }
            }
        }
    }
    void resetLength() {
        assert(getLengthIndex() < (sizeof(lengthCounterLut) / sizeof(lengthCounterLut[0])));
        lengthCounter = lengthCounterLut[getLengthIndex()];
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
    uint8_t getVolume() {
        if (isDisabled()) {
            return getEnvelopeN();
        } 
        else {
            return envelope;
        }
    }
    void generateFrame(std::vector<uint8_t>& ref, uint32_t sampleRate, uint32_t reqSamples);
private:
    uint8_t *regs;
    bool primary;

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
    uint8_t lengthCounterLut[32] = {
         10, 254, 20,  2,
         40,   4, 80,  6,
        160,   8, 60, 10,
         14,  12, 26, 14,
         12,  16, 24, 18,
         48,  20, 96, 22,
        192,  24, 72, 26,
         16,  28, 32, 30
    };
    // real cpu frequency (ntsc)
    static constexpr float cpuClk = 1.789773 * 1.0E6;
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
    static const uint32_t cycleDivider = 7457;
    static const uint32_t fiveStepRate = 192;
    static const uint32_t fourStepRate = 240;
    
    Nes *nes;
    Sdl *audio;
    
    uint64_t cycle;
    uint32_t step;
    uint8_t regs[REG_COUNT] = {0};
    uint32_t sampleRate;

    Pulse pulseA;
    Pulse pulseB;

    std::vector<uint8_t> pulseAFrame;
    std::vector<uint8_t> pulseBFrame;
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
        uint32_t rate;
        uint32_t samples;
        if (isFourStepFrame()) {
            rate = fourStepRate;
        }
        else {
            rate = fiveStepRate;
        }
        samples = sampleRate / rate;
        pulseAFrame.resize(samples);     
        pulseA.generateFrame(pulseAFrame, sampleRate, samples);
        
        pulseBFrame.resize(samples);     
        pulseB.generateFrame(pulseBFrame, sampleRate, samples);

        fullFrame.resize(samples);
        for (uint32_t i = 0; i < samples; i++) {
            float sample = (95.88f  / ((8128.0f  / (float(pulseAFrame[i] + pulseBFrame[i]))) + 100.0f));
            int16_t truncSamples = (int16_t)(sample * (1 << 14));
            //fullFrame[i] = (int16_t)(((int32_t)pulseAFrame[i] << 8) + (1 << 15));
            fullFrame[i] = truncSamples;
        }
        /*
        for (int i = 0; i < 20; i++) {
            printf("%d ", fullFrame[i]);
        }
        printf("\n");
        */
        if (rb.hasEmptySpace(samples)) {
            rb.putData(&fullFrame[0], samples);
        }
    }
    void clockLengthAndSweep() {
        pulseA.clockLengthAndSweep();
        pulseB.clockLengthAndSweep();
    }
    void clockEnvAndTriangle() {
        pulseA.clockEnvelope(); 
        pulseB.clockEnvelope();
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
    }
    uint8_t readReg(uint32_t reg) {
        uint8_t result = regs[reg];
        if (reg == CONTROL_STATUS) {
            clearRequestFrameIrq();
            result = result & (STATUS_FRAME_IRQ_REQUESTED | STATUS_DMC_IRQ_REQUESTED);
            result |= pulseA.isNonZeroLength() ? STATUS_CHANNEL1_LENGTH : 0;
            result |= pulseB.isNonZeroLength() ? STATUS_CHANNEL2_LENGTH : 0;
        }
        return result;
    }
    
    Apu(Nes *parent, Sdl *audio);
    Apu() = delete;
    Apu(const Apu&) = delete;
    ~Apu();
};
#endif
