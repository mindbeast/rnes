//
//  cpu.cpp
//  rnes
//
//  Created by Riley Andrews on 7/6/13.
//  Copyright (c) 2013 Riley Andrews. All rights reserved.
//

#include "cpu.h"
#include "nes.h"

uint8_t Cpu::load(uint16_t addr)
{
    return nes->cpuMemRead(addr);
}

void Cpu::store(uint16_t addr, uint8_t val)
{
    nes->cpuMemWrite(addr, val);
}

bool Cpu::intRequested() {
    return nes->isRequestingInt();
}
bool Cpu::nmiRequested() {
    return nes->isRequestingNmi();;
}

