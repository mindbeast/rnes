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

class Sdl;
class Nes;

class Apu {
    static const uint32_t cycleDivider = 7457;
    
    Nes *nes;
    Sdl *display;
    uint64_t cycle;
    uint32_t step;

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
    void clockLengthAndSweep() {
        
    }
    void clockEnvAndTriangle() {
        
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
    
    enum {
        // pulse wave channel a
        CHANNEL1_VOLUME_DECAY,
        CHANNEL1_SWEEP,
        CHANNEL1_FREQUENCY,
        CHANNEL1_LENGTH,
        
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
        STATUS_CHANNEL1_ENABLED    = 1 << 0,
        STATUS_CHANNEL2_ENABLED    = 1 << 1,
        STATUS_CHANNEL3_ENABLED    = 1 << 2,
        STATUS_CHANNEL4_ENABLED    = 1 << 3,
        STATUS_CHANNEL5_ENABLED    = 1 << 4,
        STATUS_FRAME_IRQ_REQUESTED = 1 << 6,
        STATUS_DMC_IRQ_REQUESTED   = 1 << 7,
    };
    
    uint8_t regs[REG_COUNT] = {0};
    void writeReg(uint32_t reg, uint8_t val) {
        if (reg == SOFTCLOCK) {
            resetFrameCounter();
        }
        else if (reg == CONTROL_STATUS) {
            clearRequestDmcIrq();
        }
        regs[reg] = val;
    }
    uint8_t readReg(uint32_t reg) {
        if (reg == CONTROL_STATUS) {
            clearRequestFrameIrq();
        }
        return regs[reg];
    }
    
    Apu(Nes *parent, Sdl *disp) : nes{parent}, display{disp} {}
    Apu() = delete;
    Apu(const Apu&) = delete;
    ~Apu() {}
    
};
#endif
