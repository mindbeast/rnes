//
//  apu.cpp
//  rnes
//
//  Created by Riley Andrews on 7/6/13.
//  Copyright (c) 2013 Riley Andrews. All rights reserved.
//

#include "apu.h"
#include "sdl.h"

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
    pulseA{&regs[CHANNEL1_VOLUME_DECAY], this, true},
    pulseB{&regs[CHANNEL2_VOLUME_DECAY], this, false},
    triangle{&regs[CHANNEL3_LINEAR_COUNTER], this},
    rb{1 << 15}
{
    sampleRate = audio->getSampleRate(); 
    audio->registerAudioCallback(apuSdlCallback, &rb);    
}

Apu::~Apu()
{
    audio->unregisterAudioCallback();
}

static void fillSilent(std::vector<uint8_t>& ref, uint32_t reqSamples)
{
    for (uint32_t i = 0; i < reqSamples; i++) {
        ref[i] = 0;
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
    
    assert(ref.size() >= reqSamples); 
    switch (cycle) {
        case CYCLE_12_5:   threshhold = 0.125f; break;
        case CYCLE_25:     threshhold = 0.25f;  break;
        case CYCLE_50:     threshhold = 0.50f;  break;
        case CYCLE_25_NEG: threshhold = 0.75f;  break;
        default: assert(0); break;
    }

    for (uint32_t i = 0; i < reqSamples; i++) {
        float offsetInWavelengths = currentTime / toneWaveLength; 
        float offsetInWave = offsetInWavelengths - float(int(offsetInWavelengths));
        uint16_t sampleVolume = (offsetInWave <= threshhold) ? volume : 0;
        ref[i] = sampleVolume; 
        currentTime += timePerSample;
    }
}
