//
//  nes.h
//  rnes
//
//  Created by Riley Andrews on 7/6/13.
//  Copyright (c) 2013 Riley Andrews. All rights reserved.
//

#ifndef __NES_H__
#define __NES_H__

#include "ppu.h"
#include "cpu.h"
#include "apu.h"
#include "sdl.h"
#include "sdl.h"


class Controller {
    Sdl *sdl;
    uint8_t control = 0;
    uint8_t shiftReg =  0;
    const bool debug = false;
    
public:
    Controller(Sdl *inpt) : sdl{inpt} {}
    ~Controller() {}
    void setShiftReg() {
        sdl->parseInput();
        shiftReg = 0;
        shiftReg |= (sdl->getButtonState(Sdl::BUTTON_A) ? 1u : 0u)      << 0;
        shiftReg |= (sdl->getButtonState(Sdl::BUTTON_B) ? 1u : 0u)      << 1;
        shiftReg |= (sdl->getButtonState(Sdl::BUTTON_SELECT) ? 1u : 0u) << 2;
        shiftReg |= (sdl->getButtonState(Sdl::BUTTON_START) ? 1u : 0u)  << 3;
        shiftReg |= (sdl->getButtonState(Sdl::BUTTON_UP) ? 1u : 0u)     << 4;
        shiftReg |= (sdl->getButtonState(Sdl::BUTTON_DOWN) ? 1u : 0u)   << 5;
        shiftReg |= (sdl->getButtonState(Sdl::BUTTON_LEFT) ? 1u : 0u)   << 6;
        shiftReg |= (sdl->getButtonState(Sdl::BUTTON_RIGHT) ? 1u : 0u)  << 7;
        
        if (debug) {
            std::cerr << "shiftReg: " << std::hex << (int)shiftReg << "\n";
        }
    }
    void write(uint8_t val) {
        if ((control & 0x1) and !(val & 0x1)) {
            setShiftReg();
        }
        control = val;
    }
    uint8_t read() {
        if (control & 0x1) {
            sdl->parseInput();
            return (sdl->getButtonState(Sdl::BUTTON_A) ? 1u : 0u)    << 0;
        }
        else {
            uint8_t ret = 0;
            ret |= shiftReg & 0x1;
            shiftReg >>= 1;
            shiftReg |= 1u << 7;
            return ret;
        }
    }
    
    Controller(const Controller&) = delete;
    Controller operator=(const Controller&) = delete;
};

class Nes {
    static const uint32_t cpuMemorySize = 1 << 16;
    static const uint32_t videoMemorySize = 1 << 14;
    
    static const uint16_t cartridgeRomBase = 0x8000;
    static const uint16_t cartridgeRomSize = 0x4000;
    
    static const uint16_t chrRomBase = 0x0000;
    static const uint16_t chrRomSize = 0x2000;
    
    static const uint32_t spriteDmaCycleEnd = 512;
    static const uint16_t spriteDmaAddr = 0x4014;
    
    static const uint16_t joypadAddr = 0x4016;
    
    static const uint16_t nameTable0 = 0x2000;
    static const uint16_t nameTable1 = 0x2400;
    static const uint16_t nameTable2 = 0x2800;
    static const uint16_t nameTable3 = 0x2c00;
    static const uint16_t nameTableSize = 0x400;
    
    static const uint16_t ppuRegBase = 0x2000;
    static const uint16_t ppuRegEnd = ppuRegBase + Ppu::REG_COUNT - 1;
    
    static const uint16_t apuRegBase = 0x4000;
    static const uint16_t apuRegEnd = apuRegBase + Apu::REG_COUNT - 1;
    
    uint8_t vidMemory[videoMemorySize] = {0};
    uint8_t cpuMemory[cpuMemorySize] = {0};
    
    bool verticalMirroring;
    void *rom;
    size_t romSize;
    
    bool spriteDmaMode = true;
    uint32_t spriteDmaCycle = 0;
    uint16_t spriteDmaSourceAddr = 0;

    uint64_t cycles = 0;
    
    Sdl sdl;
    Cpu cpu;
    Ppu ppu;
    Apu apu;
    Controller pad;
    
    uint16_t translateCpuWindows(uint16_t addr);
    uint16_t translatePpuWindows(uint16_t addr);
    
    uint32_t spriteDmaExecute();
    void spriteDmaSetup(uint8_t val);
    
public:
    void cpuMemWrite(uint16_t addr, uint8_t val);
    uint8_t cpuMemRead(uint16_t addr);
    void vidMemWrite(uint16_t addr, uint8_t val);
    uint8_t vidMemRead(uint16_t addr);

    bool isRequestingNmi()
    {
        return ppu.isRequestingNmi();
    }
    bool isRequestingInt()
    {
        return apu.isRequestingIrq();
    }
    
    int mapRom(const std::string &filename);
    int loadRom(const std::string &filename);
    void run();
    Nes() : sdl{}, cpu{this}, ppu{this,&sdl}, apu{this, &sdl}, pad{&sdl} {}
    ~Nes();
};


#endif
