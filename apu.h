//
//  apu.h
//  rnes
//
//

#ifndef __APU_H__
#define __APU_H__

#include <cstdint>
#include <vector>
#include <memory>

namespace Rnes {

class Sdl;
class Nes;
class Pulse;
class Triangle;
class Noise;
class ApuState;
template <class T> class RingBuffer;

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

    // clocks per sample
    float clksPerSample;

    // next sample
    float currentSampleClk;
    uint32_t nextSampleCountdown;

    // prior samples buffer
    static const uint32_t sampleBufferSize = 32;
    float samples[sampleBufferSize] = {0.0f};
    uint64_t sampleOffset = 0;

    uint8_t regs[REG_COUNT] = {0};
    uint32_t sampleRate;
    uint64_t fourFrameCount;
    uint64_t fiveFrameCount;

    std::unique_ptr<Pulse> pulseA;
    std::unique_ptr<Pulse> pulseB;
    std::unique_ptr<Triangle> triangle;
    std::unique_ptr<Noise> noise;

    std::shared_ptr<RingBuffer<uint16_t>> rb;
    std::vector<uint16_t> sampleBuffer;

public:
    bool isRequestingFrameIrq() const {
        return (regs[CONTROL_STATUS] & STATUS_FRAME_IRQ_REQUESTED) != 0;
    }
    bool isRequestingDmcIrq() const {
        return (regs[CONTROL_STATUS] & STATUS_DMC_IRQ_REQUESTED) != 0;
    }
    bool isRequestingIrq() const {
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
    bool isFourStepFrame() const {
        return (regs[SOFTCLOCK] & (1u << 7)) == 0;
    }
    bool isFrameIntEnabled() const {
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

    void save(ApuState &pb);
    void restore(const ApuState &pb);

    Apu(Nes *parent, Sdl *audio);
    Apu() = delete;
    Apu(const Apu&) = delete;
    ~Apu();
};

};
#endif
