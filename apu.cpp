//
//  apu.cpp
//  rnes
//
//

#include <cmath>

#include "apu.h"
#include "apuunit.h"
#include "sdl.h"
#include "ringbuffer.h"
#include "save.pb.h"

namespace Rnes {

void apuSdlCallback(void *data, uint8_t *stream, int len)
{
    RingBuffer<int16_t> *rb = (RingBuffer<int16_t>*)data;
    uint32_t items = len / sizeof(int16_t); 
    int16_t *outData = (int16_t*)stream;
    
    rb->getData(outData, items);
}

void Apu::clockLengthAndSweep()
{
    pulseA->clockLengthAndSweep();
    pulseB->clockLengthAndSweep();
    triangle->clockLength();
    noise->clockLength();
} 

void Apu::clockEnvAndTriangle()
{
    pulseA->clockEnvelope(); 
    pulseB->clockEnvelope();
    triangle->clockLinearCounter();
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
    triangle->clockTimer();
}

void Apu::stepSlowTimers()
{
    pulseA->clockTimer();
    pulseB->clockTimer();
    noise->clockTimer();
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

    const unsigned int bufferedSamples = 32;
    sampleBuffer.push_back(truncSample);
    if (sampleBuffer.size() == bufferedSamples) {
        rb->putData(sampleBuffer.data(), bufferedSamples);
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

    // Compute output each cycle
    sampleOffset += 1;
    float sample = 95.88f  / ((8128.0f  / (float(pulseA->getCurrentSample() + pulseB->getCurrentSample()))) + 100.0f);
    sample += 159.79f / (100.0f + (1.0f / ((float(triangle->getCurrentSample()) / 8227.0f) + float(noise->getCurrentSample()) / 12241.0f)));
    samples[sampleOffset % sampleBufferSize] = sample;

    // Determine when to sample, and sample if needed.
    nextSampleCountdown--;
    if (!nextSampleCountdown) {
        float prevSampleClk = currentSampleClk;
        currentSampleClk += clksPerSample;
        nextSampleCountdown = (uint32_t)currentSampleClk - (uint32_t)prevSampleClk;
        if (currentSampleClk >= float(cpuClk)) {
            currentSampleClk = 0.0f;
        }
        generateSample();     
    }

    /*
    if (samplerDivider == 0) {
    } 
    samplerDivider = (samplerDivider + 1) % uint32_t(cpuClk/ sampleRate);
    */
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
            pulseA->zeroLength();
        }
        if (~val & STATUS_CHANNEL2_LENGTH) {
            pulseB->zeroLength();
        }
        if (~val & STATUS_CHANNEL3_LENGTH) {
            triangle->zeroLength();
        }
        if (~val & STATUS_CHANNEL4_LENGTH) {
            noise->zeroLength();
        }
    }
    else if (reg == CHANNEL1_LENGTH) {
        pulseA->resetLength();
        pulseA->resetSequencer();
        pulseA->resetEnvelope();
    }
    else if (reg == CHANNEL2_LENGTH) {
        pulseB->resetLength();
        pulseB->resetSequencer();
        pulseB->resetEnvelope();
    } 
    else if (reg == CHANNEL3_LENGTH) {
        triangle->resetLength();
        triangle->setHaltFlag();
    }
    else if (reg == CHANNEL4_LENGTH) {
        noise->resetLength(); 
        noise->resetEnvelope();
    }
}

uint8_t Apu::readReg(uint32_t reg)
{
    uint8_t result = regs[reg];
    if (reg == CONTROL_STATUS) {
        clearRequestFrameIrq();
        result = result & (STATUS_FRAME_IRQ_REQUESTED | STATUS_DMC_IRQ_REQUESTED);
        result |= pulseA->isNonZeroLength() ? STATUS_CHANNEL1_LENGTH : 0;
        result |= pulseB->isNonZeroLength() ? STATUS_CHANNEL2_LENGTH : 0;
        result |= triangle->isNonZeroLength() ? STATUS_CHANNEL3_LENGTH : 0;
        result |= noise->isNonZeroLength() ? STATUS_CHANNEL4_LENGTH : 0;
    }
    return result;
}

void Apu::save(ApuState &pb)
{
    // Save sub-units.
    pulseA->save(*pb.mutable_pulsea());
    pulseB->save(*pb.mutable_pulseb());
    triangle->save(*pb.mutable_triangle());
    noise->save(*pb.mutable_noise());

    /*
    // Apu register file.
    repeated uint32 reg = 1;
    optional uint32 sampleRate = 2;
    optional uint32 fourFrameCount = 3;
    optional uint32 fiveFrameCount = 4;
    optional uint32 frameDivider = 5;
    optional uint32 step = 6;
    optional uint32 halfTimerDivider = 7;
    optional uint32 samplerDivider = 8;
    optional float clksPerSample = 9;
    optional float currentSampleClk = 10;
    optional uint32 nextSampleCountdown = 11;
    */
    
    for (unsigned i = 0; i < REG_COUNT; i++) {
        pb.add_reg(regs[i]);
    }
    pb.set_samplerate(sampleRate);
    pb.set_fourframecount(fourFrameCount);
    pb.set_fiveframecount(fiveFrameCount);
    pb.set_framedivider(frameDivider);
    pb.set_step(step);
    pb.set_halftimerdivider(halfTimerDivider);
    pb.set_samplerdivider(samplerDivider);
    pb.set_clkspersample(clksPerSample);
    pb.set_currentsampleclk(currentSampleClk);
    pb.set_nextsamplecountdown(nextSampleCountdown);
}

void Apu::restore(const ApuState &pb)
{
    // Restore sub-units.
    pulseA->restore(pb.pulsea());    
    pulseB->restore(pb.pulseb());
    triangle->restore(pb.triangle());
    noise->restore(pb.noise());
    
    /*
    // Apu register file.
    repeated uint32 reg = 1;
    optional uint32 sampleRate = 2;
    optional uint32 fourFrameCount = 3;
    optional uint32 fiveFrameCount = 4;
    optional uint32 frameDivider = 5;
    optional uint32 step = 6;
    optional uint32 halfTimerDivider = 7;
    optional uint32 samplerDivider = 8;
    optional float clksPerSample = 9;
    optional float currentSampleClk = 10;
    optional uint32 nextSampleCountdown = 11;
    */
    
    for (int i = 0; i < pb.reg_size(); i++) {
        regs[i] = pb.reg(i);
    }

    sampleRate = pb.samplerate();
    fourFrameCount = pb.fourframecount();
    fiveFrameCount = pb.fiveframecount();
    frameDivider = pb.framedivider();
    step = pb.step();
    halfTimerDivider = pb.halftimerdivider();
    samplerDivider = pb.samplerdivider();
    clksPerSample = pb.clkspersample();
    currentSampleClk = pb.currentsampleclk();
    nextSampleCountdown = pb.nextsamplecountdown();
}

Apu::Apu(Nes *parent, Sdl *audio) :
    nes{parent},
    audio{audio},
    frameDivider{0},
    step{0},
    halfTimerDivider{0},
    samplerDivider{0},
    regs{0},
    fourFrameCount{0},
    fiveFrameCount{0},
    sampleBuffer{}
{
    // Create audio ringbuffer.
    std::unique_ptr<RingBuffer<uint16_t>> rbLocal(new RingBuffer<uint16_t>(1 << 12));
    rb = std::move(rbLocal);

    // Create apu units.
    std::unique_ptr<Pulse> pulseALocal(new Pulse{&regs[CHANNEL1_VOLUME_DECAY], true});
    pulseA = std::move(pulseALocal);

    std::unique_ptr<Pulse> pulseBLocal(new Pulse{&regs[CHANNEL2_VOLUME_DECAY], false});
    pulseB = std::move(pulseBLocal);

    std::unique_ptr<Triangle> triangleLocal(new Triangle{&regs[CHANNEL3_LINEAR_COUNTER]});
    triangle = std::move(triangleLocal);

    std::unique_ptr<Noise> noiseLocal(new Noise{&regs[CHANNEL4_VOLUME_DECAY]});
    noise = std::move(noiseLocal);

    // Get Current sample rate
    sampleRate = audio->getSampleRate(); 

    // Start audio callbacks.
    audio->registerAudioCallback(apuSdlCallback, rb.get());    

    // compute clocks per sample
    clksPerSample = float(cpuClk) / float(sampleRate);

    nextSampleCountdown = 1;
    currentSampleClk = 0.0f;
}

Apu::~Apu()
{
    audio->unregisterAudioCallback();
}

};

