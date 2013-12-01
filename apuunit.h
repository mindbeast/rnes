//
//  apuunit.h
//  rnes
//
//  Created by Riley Andrews on 12/1/13.
//  Copyright (c) 2013 Riley Andrews. All rights reserved.
//

#ifndef __APUUNIT_H__
#define __APUUNIT_H__

#include <cstdint>
#include <vector>
#include <cassert>
#include <iostream>
#include <memory>

namespace Rnes {

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

    // length logic
    uint8_t lengthCounter;
    
    // envelope logic
    uint8_t envelope;
    uint8_t envelopeDivider;
    bool resetEnvelopeAndDivider;

    // sweep logic
    bool resetSweepDivider;
    uint8_t sweepDivider;

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
    Pulse(uint8_t *argRegs, bool primaryPulse) : 
        regs{argRegs},
        primary{primaryPulse},
        lengthCounter{0},
        envelope{0},
        envelopeDivider{0},
        resetEnvelopeAndDivider{true},
        resetSweepDivider{true},
        sweepDivider{0},
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
        lengthCounter = lengthCounterLut[getLengthIndex()];
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
    uint8_t getCurrentSample() const;
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
    
    // length logic
    uint8_t lengthCounter;
    
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
        lengthCounter = lengthCounterLut[getLengthIndex()];
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
    Triangle(uint8_t *argRegs) : 
        regs{argRegs},
        lengthCounter{0},
        linearCounterHalt{false},
        linearCounter{0},
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
    
    // length logic
    uint8_t lengthCounter;
    
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
        lengthCounter = lengthCounterLut[getLengthIndex()];
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
    uint8_t getCurrentSample() const;
    Noise(uint8_t *argRegs) : 
        regs{argRegs},
        lengthCounter{0},
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

};
#endif
