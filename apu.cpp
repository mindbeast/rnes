//
//  apu.cpp
//  rnes
//
//  Created by Riley Andrews on 7/6/13.
//  Copyright (c) 2013 Riley Andrews. All rights reserved.
//

#include "apu.h"
#include "sdl.h"
#include <cmath>

uint16_t Pulse::computeSweepTarget() const
{
    uint16_t period = getTimerPeriod(); 
    uint16_t shiftedPeriod = period >> getSweepShift();
    if (isSweepNegative()) {
        shiftedPeriod = isFirstPulse() ? -shiftedPeriod : -shiftedPeriod + 1;
    }
    uint16_t targetPeriod = period + shiftedPeriod;
    return targetPeriod;
}

void Pulse::sweepPeriod()
{
    uint16_t targetPeriod = computeSweepTarget();
    uint16_t period = getTimerPeriod(); 
    if (targetPeriod > 0x7ff or period < 8) {
        return;
    }
    setTimerPeriod(targetPeriod); 
}

uint8_t Pulse::getVolume() const
{
    if (isDisabled()) {
        return getEnvelopeN();
    } 
    else {
        return envelope;
    }
}

void Pulse::envelopeDividerClock()
{
    if (envelope) {
        envelope--;
    }
    else if (isEnvelopeLoopSet()) {
        envelope = 15; 
    }
}

void Pulse::clockEnvelope()
{
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

void Pulse::clockLengthAndSweep()
{
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

void Pulse::updateSample()
{
    const DutyCycle cycle = getDutyCycle();

    currentSample = (sequences[cycle] & (1 << sequencerOffset)) != 0;
    sequencerOffset = (sequencerOffset + 1) % 8;
}

void Pulse::clockTimer()
{
    if (timerDivider == 0) {
        updateSample();
    }
    timerDivider = (timerDivider + 1) % getTimerPeriod();
}

void Triangle::clockLength() {
    // Length
    if (!isHalted() and lengthCounter) {
       lengthCounter--; 
    }   
}

void Triangle::clockLinearCounter() {
    if (linearCounterHalt) {
        linearCounter = getLinearCounterReload();
    }
    else if (linearCounter) {
        linearCounter--;
    }
    if (!getControlFlag()) {
        linearCounterHalt = false; 
    } 
}

void Triangle::updateSample()
{
    // length has run out or linear counter out, channel is silenced
    currentSample = sequence[sequencerOffset];
    sequencerOffset = (sequencerOffset + 1) % 32;
}

void Triangle::clockTimer()
{
    if (timerDivider == 0 and isNonZeroLength() && isNonZeroLinearCounter()) {
        updateSample();
    }
    timerDivider = (timerDivider + 1) % (getTimerPeriod());
}

uint16_t Noise::getNextShiftReg(uint16_t reg) const
{
    uint16_t newBit = 0;
    if (isShortMode()) {
        newBit = (0x1 & ((reg) ^ (reg >> 6))) << 14;
    }
    else {
        newBit = (0x1 & ((reg) ^ (reg >> 1))) << 14;
    }
    return newBit | (reg >> 1);
}

uint8_t Noise::getVolume() const
{
    if (isDisabled()) {
        return getEnvelopeN();
    } 
    else {
        return envelope;
    }
}

void Noise::envelopeDividerClock()
{
    if (envelope) {
        envelope--;
    }
    else if (isEnvelopeLoopSet()) {
        envelope = 15; 
    }
}

void Noise::clockEnvelope()
{
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

void Noise::clockLength()
{
    // Length
    if (!isHalted() and lengthCounter) {
       lengthCounter--; 
    }   
}
 
void Noise::updateSample()
{
    shiftRegister = getNextShiftReg(shiftRegister);
    currentSample = (shiftRegister & 1) ? 1 : 0;
}

void Noise::clockTimer()
{
    if (timerDivider == 0) {
        updateSample();
    }
    timerDivider = (timerDivider + 1) % getTimerPeriod();
}

/*
static float timerGetMs()
{
    struct timespec ts;
    clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &ts);
    return ts.tv_sec * 1000.0f + ts.tv_nsec / 1000000.0f;
}
*/

void apuSdlCallback(void *data, uint8_t *stream, int len)
{
    //float a = timerGetMs();
    RingBuffer<int16_t> *rb = (RingBuffer<int16_t>*)data;
    uint32_t items = len / sizeof(int16_t); 
    int16_t *outData = (int16_t*)stream;
    
    //std::cout << "items - rbCount: " << (int)items << " - " << (int)rbCount << std::endl;
    rb->getData(outData, items);
    //float b = timerGetMs();
    //std::cout << "time :" << b - a << std::endl;
}

void Apu::clockLengthAndSweep()
{
    pulseA.clockLengthAndSweep();
    pulseB.clockLengthAndSweep();
    triangle.clockLength();
    noise.clockLength();
} 

void Apu::clockEnvAndTriangle()
{
    pulseA.clockEnvelope(); 
    pulseB.clockEnvelope();
    triangle.clockLinearCounter();
}

void Apu::resetFrameCounter()
{
    frameDivider = 0;
    step = 0;
    if (!isFourStepFrame())  {
        stepAdvance();
    }
}

void Apu::stepAdvance()
{
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

void Apu::stepFastTimers() 
{
    triangle.clockTimer();
}

void Apu::stepSlowTimers()
{
    pulseA.clockTimer();
    pulseB.clockTimer();
    noise.clockTimer();
}



void Apu::generateSample() 
{
    //float sample = (samples[sampleOffset % sampleBufferSize] + samples[(sampleOffset - 32) % sampleBufferSize]) / 2;
    float sample = 0.0f;
    sample += 0.02051777 * (samples[sampleOffset % sampleBufferSize]);
    sample += 0.06532911 * (samples[(sampleOffset - 1) % sampleBufferSize]);
    sample += 0.16640572 * (samples[(sampleOffset  - 2)% sampleBufferSize]);
    sample += 0.2477474 * (samples[(sampleOffset  - 3)% sampleBufferSize]);
    sample += 0.2477474 * (samples[(sampleOffset  - 4)% sampleBufferSize]);
    sample += 0.16640572 * (samples[(sampleOffset  - 5) % sampleBufferSize]);
    sample += 0.06532911 * (samples[(sampleOffset  - 6) % sampleBufferSize]);
    sample += 0.02051777 * (samples[(sampleOffset - 7 )% sampleBufferSize]);

    int16_t truncSample = (int16_t)((sample) * (1 << 14));

    const unsigned int bufferedSamples = 128;
    sampleBuffer.push_back(truncSample);
    if (sampleBuffer.size() == bufferedSamples) {
        rb.putData(sampleBuffer.data(), bufferedSamples);
        sampleBuffer.clear();
    }
}

void Apu::tick()
{
    // Divider logic for frame
    if (frameDivider == 0) {
        stepAdvance();
    }
    frameDivider = (frameDivider + 1) % frameCycles;

    // Divider logic for timers
    stepFastTimers();
    if (halfTimerDivider == 0) {
        stepSlowTimers();
    }
    halfTimerDivider = !halfTimerDivider;

    // Sample capture
    sampleOffset += 1;
    /*
    std::cerr << "pulseA: " << (int)pulseA.getCurrentSample() << " pulseB: " << (int)pulseB.getCurrentSample()
              << " triangle: " << (int)triangle.getCurrentSample() << " noise: " << (int)noise.getCurrentSample() << std::endl;
    */
    float sample = 95.88f  / ((8128.0f  / (float(pulseA.getCurrentSample() + pulseB.getCurrentSample()))) + 100.0f);
    sample += 159.79f / (100.0f + (1.0f / ((float(triangle.getCurrentSample()) / 8227.0f) + float(noise.getCurrentSample()) / 12241.0f)));
    samples[sampleOffset % sampleBufferSize] = sample;

    // Divider logic for sampling 
    if (samplerDivider == 0) {
        generateSample();     
    } 
    samplerDivider = (samplerDivider + 1) % uint32_t(cpuClk/ sampleRate);

}

void Apu::run(int cycles)
{
    for (int i = 0; i < cycles; i++) {
        tick();
    }
}

void Apu::writeReg(uint32_t reg, uint8_t val)
{
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
        if (~val & STATUS_CHANNEL4_LENGTH) {
            noise.zeroLength();
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
        triangle.setHaltFlag();
    }
    else if (reg == CHANNEL4_LENGTH) {
        noise.resetLength(); 
        noise.resetEnvelope();
    }
}

uint8_t Apu::readReg(uint32_t reg)
{
    uint8_t result = regs[reg];
    if (reg == CONTROL_STATUS) {
        clearRequestFrameIrq();
        result = result & (STATUS_FRAME_IRQ_REQUESTED | STATUS_DMC_IRQ_REQUESTED);
        result |= pulseA.isNonZeroLength() ? STATUS_CHANNEL1_LENGTH : 0;
        result |= pulseB.isNonZeroLength() ? STATUS_CHANNEL2_LENGTH : 0;
        result |= triangle.isNonZeroLength() ? STATUS_CHANNEL3_LENGTH : 0;
        result |= noise.isNonZeroLength() ? STATUS_CHANNEL4_LENGTH : 0;
    }
    return result;
}

Apu::Apu(Nes *parent, Sdl *audio) :
    nes{parent},
    audio{audio},
    frameDivider{0},
    step{0},
    samplerDivider{0},
    regs{0},
    fourFrameCount{0},
    fiveFrameCount{0},
    pulseA{&regs[CHANNEL1_VOLUME_DECAY], this, true},
    pulseB{&regs[CHANNEL2_VOLUME_DECAY], this, false},
    triangle{&regs[CHANNEL3_LINEAR_COUNTER], this},
    noise{&regs[CHANNEL4_VOLUME_DECAY], this},
    rb{1 << 16},
    sampleBuffer{}
{
    sampleRate = audio->getSampleRate(); 
    audio->registerAudioCallback(apuSdlCallback, &rb);    
}

Apu::~Apu()
{
    audio->unregisterAudioCallback();
}

