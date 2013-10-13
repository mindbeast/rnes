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

static void fillSilent(std::vector<uint8_t>& ref, uint32_t reqSamples)
{
    for (uint32_t i = 0; i < reqSamples; i++) {
        ref[i] = 0;
    }
}

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

uint8_t Pulse::getVolume()
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
 
void Pulse::generateFrame(std::vector<uint8_t>& ref, uint32_t sampleRate, uint32_t reqSamples)
{
    const float toneFrequency = getToneFrequency();
    const float toneWaveLength = 1.0f / toneFrequency;
    const float timePerSample = 1.0f / float(sampleRate);
    const uint16_t volume = getVolume();
    float threshhold = 0.0f;
    const DutyCycle cycle = getDutyCycle();
    float currentTime = time;
    time += timePerSample * reqSamples;

    // length has run out, channel is silenced
    if (!isNonZeroLength()) {
        fillSilent(ref, reqSamples);
        return;
    }
    // channel is silenced when period is < 8
    const uint16_t timerPeriod = getTimerPeriod();
    if (timerPeriod < 8) {
        fillSilent(ref, reqSamples);
        return;
    }
    // channel silenced when target is above 0x7ff
    const uint16_t targetPeriod = computeSweepTarget();
    if (targetPeriod > 0x7ff) {
        fillSilent(ref, reqSamples);
        return;
    }
    
    switch (cycle) {
        case CYCLE_12_5:   threshhold = 0.125f; break;
        case CYCLE_25:     threshhold = 0.25f;  break;
        case CYCLE_50:     threshhold = 0.50f;  break;
        case CYCLE_25_NEG: threshhold = 0.75f;  break;
        default: assert(0); break;
    }

    assert(ref.size() >= reqSamples); 
    for (uint32_t i = 0; i < reqSamples; i++) {
        float offsetInWavelengths = currentTime / toneWaveLength; 
        float offsetInWave = offsetInWavelengths - truncf(offsetInWavelengths);
        uint16_t sampleVolume = (offsetInWave <= threshhold) ? volume : 0;
        ref[i] = sampleVolume; 
        currentTime += timePerSample;
    }
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

void Triangle::generateFrame(std::vector<uint8_t>& ref, uint32_t sampleRate, uint32_t reqSamples)
{
    const float toneFrequency = getToneFrequency();
    const float toneWaveLength = 1.0f / toneFrequency;
    const float timePerSample = 1.0f / float(sampleRate);
    float currentTime = time;
    time += timePerSample * reqSamples;

    // length has run out or linear counter out, channel is silenced
    if (!isNonZeroLength() || !isNonZeroLinearCounter()) {
        fillSilent(ref, reqSamples);
        return;
    }

    assert(ref.size() >= reqSamples); 
    for (uint32_t i = 0; i < reqSamples; i++) {
        float offsetInWavelengths = currentTime / toneWaveLength; 
        float offsetInWave = 32.0f * (offsetInWavelengths - truncf(offsetInWavelengths));
        uint32_t sequence = (uint32_t)truncf(offsetInWave);
        assert(sequence < 32);
        if (sequence < 16) {
            ref[i] = 15 - sequence;
        }
        else {
            ref[i] = sequence - 1 - 15;
        }
        assert(ref[i] <= 15);
        currentTime += timePerSample;
    }    
}


uint16_t Noise::getNextShiftReg(uint16_t reg) const
{
    uint16_t newBit = 0;
    if (isShortMode()) {
        newBit = ((reg) ^ (reg >> 6)) << 15;
    }
    else {
        newBit = ((reg) ^ (reg >> 1)) << 15;
    }
    return newBit | (reg >> 1);
}

uint8_t Noise::getVolume()
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
 
void Noise::generateFrame(std::vector<uint8_t>& ref, uint32_t sampleRate, uint32_t reqSamples)
{
    const float toneFrequency = getToneFrequency();
    const float toneWaveLength = 1.0f / toneFrequency;
    const float timePerSample = 1.0f / float(sampleRate);
    const uint16_t volume = getVolume();
    float currentTime = time;
    time += timePerSample * reqSamples;

    // length has run out, channel is silenced
    if (!isNonZeroLength()) {
        fillSilent(ref, reqSamples);
        return;
    }
    
    uint32_t prevClock = 0;
    assert(ref.size() >= reqSamples); 
    for (uint32_t i = 0; i < reqSamples; i++) {
        float offsetInWavelengths = currentTime / toneWaveLength; 
        uint32_t currentClock = offsetInWavelengths; 
        for (uint32_t i = 0; i < currentClock - prevClock; i++) {
            shiftRegister = getNextShiftReg(shiftRegister);
        }
        ref[i] = (shiftRegister & 1) ? volume : 0; 
        currentTime += timePerSample;
        prevClock = currentClock;
    }
}

void apuSdlCallback(void *data, uint8_t *stream, int len)
{
    RingBuffer<int16_t> *rb = (RingBuffer<int16_t>*)data;
    uint32_t items = len / sizeof(int16_t); 
    int16_t *outData = (int16_t*)stream;
    uint32_t rbCount = rb->getCount(); 
    
    if (rbCount >= items) {
        rb->getData(outData, items);
    }
    else {
        rb->getData(outData, rbCount);
        memset(&outData[rbCount], 0, sizeof(int16_t) * (items - rbCount));
    }
}

Apu::Apu(Nes *parent, Sdl *audio) :
    nes{parent},
    audio{audio},
    cycle{0},
    step{0},
    regs{0},
    fourFrameCount{0},
    fiveFrameCount{0},
    pulseA{&regs[CHANNEL1_VOLUME_DECAY], this, true},
    pulseB{&regs[CHANNEL2_VOLUME_DECAY], this, false},
    triangle{&regs[CHANNEL3_LINEAR_COUNTER], this},
    noise{&regs[CHANNEL4_VOLUME_DECAY], this},
    rb{1 << 15}
{
    sampleRate = audio->getSampleRate(); 
    audio->registerAudioCallback(apuSdlCallback, &rb);    
}

Apu::~Apu()
{
    audio->unregisterAudioCallback();
}
