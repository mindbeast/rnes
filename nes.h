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
            return (sdl->getButtonState(Sdl::BUTTON_A) ? 0u : 1u)    << 0;
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
    static const uint32_t videoMemorySize = (1 << 11) + (1 << 13);
    
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
    
    uint8_t vidMemory[videoMemorySize] = {0};
    uint8_t cpuMemory[cpuMemorySize] = {0};
    
    bool verticalMirroring;
    void *rom;
    size_t romSize;
    
    bool spriteDmaMode = true;
    uint32_t spriteDmaCycle = 0;
    uint16_t spriteDmaSourceAddr = 0;
    
    Cpu cpu;
    Ppu ppu;
    Apu apu;
    Sdl sdl;
    Controller pad;
    
    uint16_t translateCpuWindows(uint16_t addr)
    {
        if (addr >= 0x800 && addr < 0x2000) {
            addr = (addr & (0x800 - 1)) + 0x800;
        }
        if (addr >= 0x2000 && addr < 0x4000) {
            addr = (addr & (0x8 - 1)) + 0x2000;
        }
        return addr;
    }
    uint16_t translatePpuWindows(uint16_t addr)
    {
        if (addr >= 0x3000 && addr < 0x3f00) {
            addr = (addr & (0xf00 - 1)) + 0x2000;
        }
        if (addr >= 0x4000 && addr < (0x4000 + 0xc000)) {
            addr = addr - 0x4000;
        }
        
        // palette mirroring
        if (addr == 0x3f10) {
            addr = 0x3f00;
        }
        else if (addr == 0x3f14) {
            addr = 0x3f04;
        }
        else if (addr == 0x3f18) {
            addr = 0x3f08;
        }
        else if (addr == 0x3f1c) {
            addr = 0x3f0c;
        }
        
        // Nametable mappings are rom dependent
        if (!verticalMirroring) {
            if (addr >= nameTable1 && addr < nameTable2) {
                addr = (addr & (nameTableSize - 1)) + nameTable0;
            }
            else if (addr >= nameTable3 && addr < (nameTable3 + nameTableSize)) {
                addr = (addr & (nameTableSize - 1)) + nameTable2;
            }
        }
        else {
            if (addr >= nameTable3 && addr < (nameTable3 + nameTableSize)) {
                addr = (addr & (nameTableSize - 1)) + nameTable1;
            }
            else if (addr >= nameTable2 && addr < nameTable3) {
                addr = (addr & (nameTableSize - 1)) + nameTable0;
            }
        }
        
        return addr;
    }
    
    uint32_t spriteDmaExecute() {
        const uint32_t cyclesPerItr = 2;
        assert(spriteDmaMode);
        cpuMemWrite(ppuRegBase + Ppu::SPR_DATA_REG, cpuMemRead(spriteDmaSourceAddr));
        spriteDmaSourceAddr++;
        spriteDmaCycle += cyclesPerItr;
        if (spriteDmaCycleEnd == spriteDmaCycle) {
            spriteDmaMode = false;
        }
        return cyclesPerItr;
    }
    void spriteDmaSetup(uint8_t val) {
        spriteDmaMode = true;
        spriteDmaCycle = 0;
        spriteDmaSourceAddr = (uint16_t)val * 0x100;
    }
    
    static const uint16_t ppuRegBase = 0x2000;
    static const uint16_t ppuRegEnd = ppuRegBase + Ppu::REG_COUNT - 1;
    
    static const uint16_t apuRegBase = 0x4000;
    static const uint16_t apuRegEnd = apuRegBase + Apu::REG_COUNT - 1;
    
public:
    void cpuMemWrite(uint16_t addr, uint8_t val)
    {
        addr = translateCpuWindows(addr);
        if (addr >= ppuRegBase && addr <= ppuRegEnd) {
            ppu.writeReg(addr - ppuRegBase, val);
        }
        else if (addr == spriteDmaAddr) {
            spriteDmaSetup(val);
        }
        else if (addr == joypadAddr) {
            pad.write(val);
        }
        else if (addr >= apuRegBase && addr <= apuRegEnd) {
            apu.writeReg(addr - apuRegBase, val);
        }
        else {
            cpuMemory[addr] = val;
        }
    }
    uint8_t cpuMemRead(uint16_t addr)
    {
        addr = translateCpuWindows(addr);
        if (addr >= ppuRegBase && addr <= ppuRegEnd) {
            return ppu.readReg(addr - ppuRegBase);
        }
        else if (addr == joypadAddr) {
            return pad.read();
        }
        else if (addr >= apuRegBase && addr <= apuRegEnd) {
            return apu.readReg(addr - apuRegBase);
        }
        else {
            return cpuMemory[addr];
        }
    }
    void vidMemWrite(uint16_t addr, uint8_t val)
    {
        vidMemory[translatePpuWindows(addr)] = val;
    }
    uint8_t vidMemRead(uint16_t addr)
    {
        return vidMemory[translatePpuWindows(addr)  ];
    }
    
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
    void run() {
        sdl.init();
        cpu.reset();
        while (1) {
            uint32_t cpuCycles;
            if (!spriteDmaMode) {
                cpuCycles = cpu.runInst();
            }
            else {
                cpuCycles = spriteDmaExecute();
            }
            ppu.run(cpuCycles);
            apu.run(cpuCycles);
        }
    }
    
    Nes() : cpu{this}, ppu{this,&sdl}, apu{this, &sdl}, sdl{}, pad{&sdl} {}
    ~Nes();
};


#endif
