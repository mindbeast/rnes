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
    
    if (rb->hasData(items)) {

        rb->getData(outData, items);
        /*
        for (int i = 0; i < items; i++) {
            printf("%d ", (int)outData[i]);
        }
        printf("\n");
        */
    }
    else {
        
        memset(outData, 0, sizeof(int16_t) * items);
        printf("not here %d\n", rb->getCount());
    }
}

Apu::Apu(Nes *parent, Sdl *audio) :
    nes{parent},
    audio{audio},
    cycle{0},
    step{0},
    regs{0},
    pulseA{&regs[CHANNEL1_VOLUME_DECAY]},
    pulseB{&regs[CHANNEL2_VOLUME_DECAY]},
    rb{1 << 15}
{
    sampleRate = audio->getSampleRate(); 
    audio->registerAudioCallback(apuSdlCallback, &rb);    
}

Apu::~Apu()
{
    audio->unregisterAudioCallback();
}
 
void Pulse::generateFrame(std::vector<uint8_t>& ref, uint32_t sampleRate, uint32_t reqSamples)
{
    float toneFrequency = getToneFrequency();
    //float toneFrequency = 400.0f;

    float toneWaveLength = 1.0f / toneFrequency;
    float timePerSample = 1.0f / float(sampleRate);
    uint16_t volume = getVolume();
    DutyCycle cycle = getDutyCycle();
    float t = time;
    float threshhold = 0.0f;
    
    assert(ref.size() >= reqSamples);
    switch (cycle) {
        case CYCLE_12_5:   threshhold = 0.125f; break;
        case CYCLE_25:     threshhold = 0.25f;  break;
        case CYCLE_50:     threshhold = 0.50f;  break;
        case CYCLE_25_NEG: threshhold = 0.75f;  break;
        default: assert(0); break;
    }
    for (uint32_t i = 0; i < reqSamples; i++) {
        float offsetInWavelengths = t / toneWaveLength; 
        float offsetInWave = offsetInWavelengths - float(int(offsetInWavelengths));
        uint16_t sampleVolume = (offsetInWave <= threshhold) ? volume : 0;
        sampleVolume = lengthCounter != 0 ? sampleVolume : 0;
        ref[i] = sampleVolume; 
        t += timePerSample;
    }
    //printf("obj: %p freq: %f samples: %d vol: %d lengthCounter: %d\n", this, toneFrequency, reqSamples, volume, lengthCounter);
   // printf("%d\n", cycle);
    //for (int i = 0; i < reqSamples; i++) {
    //    printf("%d ", (int)ref[i]);
    //}
    //printf("\n");
   
    time += timePerSample * reqSamples;
}
