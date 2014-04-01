//
//  apu.cpp
//  rnes
//
//  Created by Riley Andrews on 7/6/13.
//  Copyright (c) 2013 Riley Andrews. All rights reserved.
//

#include <cmath>

#include "apuunit.h"
#include "save.pb.h"

namespace Rnes {

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

void Pulse::save(PulseState &pb)
{
    // length logic
    pb.set_lengthcounter(lengthCounter);
    
    // envelope logic
    pb.set_envelope(envelope);
    pb.set_envelopedivider(envelopeDivider);
    pb.set_resetenvelopeanddivider(resetEnvelopeAndDivider);

    // sweep logic
    pb.set_resetsweepdivider(resetSweepDivider);
    pb.set_sweepdivider(sweepDivider);

    // current sample
    pb.set_currentsample(currentSample);
    
    // timer
    pb.set_timerdivider(timerDivider);

    // sequencer offset 
    pb.set_sequenceroffset(sequencerOffset);
}

void Pulse::restore(const PulseState &pb)
{
    lengthCounter = pb.lengthcounter();
    envelope = pb.envelope();
    envelopeDivider = pb.envelopedivider();
    resetEnvelopeAndDivider = pb.resetenvelopeanddivider();
    
    resetSweepDivider = pb.resetsweepdivider();
    sweepDivider = pb.sweepdivider();
    currentSample = pb.currentsample();
    timerDivider = pb.timerdivider();
    sequencerOffset = pb.sequenceroffset();
}

uint8_t Pulse::getCurrentSample() const
{
    // channel is silenced when period is < 8
    const uint16_t timerPeriod = getTimerPeriod();
    if (timerPeriod < 8) {
        return 0;
    }
    // channel silenced when target is above 0x7ff
    const uint16_t targetPeriod = computeSweepTarget();
    if (targetPeriod > 0x7ff) {
        return 0;
    }
    if (!isNonZeroLength()) {
        return 0;
    }
    if (!currentSample) {
        return 0;
    }
    return getVolume();
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

void Triangle::save(TriangleState &pb)
{
    // length logic
    pb.set_lengthcounter(lengthCounter);
    
    // linear counter logic
    pb.set_linearcounterhalt(linearCounterHalt);
    pb.set_linearcounter(linearCounter);

    // current sample
    pb.set_currentsample(currentSample);

    // timer
    pb.set_timerdivider(timerDivider);

    // sequencer offset 
    pb.set_sequenceroffset(sequencerOffset);
}

void Triangle::restore(const TriangleState &pb)
{
    lengthCounter = pb.lengthcounter();
    linearCounterHalt = pb.linearcounterhalt();
    linearCounter = pb.linearcounter();
    
    currentSample = pb.currentsample();
    timerDivider = pb.timerdivider();
    
    sequencerOffset = pb.sequenceroffset();
}

void Triangle::clockLength()
{
    // Length
    if (!isHalted() and lengthCounter) {
       lengthCounter--; 
    }   
}

void Triangle::clockLinearCounter()
{
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

uint8_t Noise::getCurrentSample() const
{
    if (!currentSample) {
        return 0;
    }
    if (!isNonZeroLength()) {
        return 0;
    }
    return getVolume();
}


void Noise::save(NoiseState &pb)
{
    // length logic
    pb.set_lengthcounter(lengthCounter);
    
    // noise shift register
    pb.set_shiftregister(shiftRegister);
    
    // envelope logic
    pb.set_envelope(envelope);
    pb.set_envelopedivider(envelopeDivider);
    pb.set_resetenvelopeanddivider(resetEnvelopeAndDivider);

    // current sample
    pb.set_currentsample(currentSample);

    // timer
    pb.set_timerdivider(timerDivider);
}

void Noise::restore(const NoiseState &pb)
{
    lengthCounter = pb.lengthcounter();
    shiftRegister = pb.shiftregister();
    envelope = pb.envelope();
    envelopeDivider = pb.envelopedivider();
    resetEnvelopeAndDivider = pb.resetenvelopeanddivider();
    currentSample = pb.currentsample();
    timerDivider = pb.timerdivider();
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

};

