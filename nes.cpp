//
//  nes.cpp
//  rnes
//
//  Created by Riley Andrews on 7/6/13.
//  Copyright (c) 2013 Riley Andrews. All rights reserved.
//

#include "nes.h"
#include <unistd.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <sys/stat.h>

uint16_t Nes::translateCpuWindows(uint16_t addr)
{
    if (addr >= 0x800 && addr < 0x2000) {
        addr = (addr & (0x800 - 1)) + 0x800;
    }
    if (addr >= 0x2000 && addr < 0x4000) {
        addr = (addr & (0x8 - 1)) + 0x2000;
    }
    return addr;
}

uint16_t Nes::translatePpuWindows(uint16_t addr)
{
    if (addr >= 0x3000 && addr < 0x3f00) {
        addr = (addr & (0xf00 - 1)) + 0x2000;
    }
    if (addr >= 0x4000) {
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

uint32_t Nes::spriteDmaExecute()
{
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

void Nes::spriteDmaSetup(uint8_t val)
{
    spriteDmaMode = true;
    spriteDmaCycle = 0;
    spriteDmaSourceAddr = (uint16_t)val * 0x100;
}

void Nes::cpuMemWrite(uint16_t addr, uint8_t val)
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

uint8_t Nes::cpuMemRead(uint16_t addr)
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

void Nes::vidMemWrite(uint16_t addr, uint8_t val)
{
    assert(translatePpuWindows(addr) < videoMemorySize);
    vidMemory[translatePpuWindows(addr)] = val;
}

uint8_t Nes::vidMemRead(uint16_t addr)
{
    assert(translateCpuWindows(addr) < cpuMemorySize);
    return vidMemory[translatePpuWindows(addr)];
}

void Nes::run()
{
    uint32_t inputCycles = 1 << 16;
    cpu.reset();
    while (1) {
        uint32_t cpuCycles;
        if (!spriteDmaMode) {
            cpuCycles = cpu.runInst();
        }
        else {
            cpuCycles = spriteDmaExecute();
        }
        apu.run(cpuCycles);
        ppu.run(cpuCycles);

        cycles += cpuCycles;
        if ((cycles % inputCycles) == 0) {
            sdl.parseInput();
        }
        /*
        sdl->renderSync();
        uint32_t currentTime = timerGetMs();
        if (currentTime - lastFrameTimeMs < frameTimeMs+1) {
            sleepMs(frameTimeMs +1 - (currentTime - lastFrameTimeMs));
        }
        lastFrameTimeMs = timerGetMs();
        */
    }
}

struct NesHeader {
    char str[4]; // ? "NES^Z"
    uint8_t numRomBanks;   // number of 16kb ROM banks
    uint8_t numVRomBanks;  // num of vrom banks
    uint8_t info[4];
    uint8_t reserved[6];
} __attribute__((packed));

static_assert(sizeof(NesHeader) == 16, "NesHeader is wrong size");

int Nes::mapRom(const std::string &filename)
{
    struct stat stats;
    int fd;
    int result = open(filename.c_str(), O_RDWR);
    if (result == -1) {
        return result;
    }
    fd = result;
    
    result = fstat(fd, &stats);
    if (result == -1) {
        perror(nullptr);
        return result;
    }
    
    rom = mmap(nullptr, stats.st_size, PROT_READ, MAP_FILE | MAP_PRIVATE, fd, 0);
    if (rom == MAP_FAILED) {
        perror(nullptr);
        return -1;
    }
    romSize = stats.st_size;
    
    close(fd);
    return 0;
}

int Nes::loadRom(const std::string &filename)
{
    int result = mapRom(filename);
    if (result == -1) {
        return -1;
    }
    const NesHeader *header = (const NesHeader*)rom;
    std::cerr << "Loading rom.. " << std::endl;
    std::cerr << "rom banks: " << (int)header->numRomBanks << std::endl;
    std::cerr << "vrom banks: " << (int)header->numVRomBanks << std::endl;
    if (header->info[0] & (1 << 2)) {
        std::cerr << "trainer present" << std::endl;
    }
    uint8_t mapper = (header->info[0] & 0xf0) >> 4;
    mapper |= (header->info[1] & 0xf0);
    std::cerr << "mapper: " << (int)mapper << std::endl;
    
    assert(header->numRomBanks == 1 || header->numRomBanks == 2);
    uint16_t romBase = (header->numRomBanks == 2) ? cartridgeRomBase : cartridgeRomBase + cartridgeRomSize;
    for (int i = 0; i < header->numRomBanks && i < 2; i++) {
        std::cerr << "loading rom section " << std::endl;
        uint8_t *dest = &cpuMemory[romBase + i * cartridgeRomSize];
        uint8_t *src = (uint8_t*)rom + sizeof(NesHeader) + i * cartridgeRomSize;
        memcpy(dest, src, cartridgeRomSize);
    }
    
    assert(header->numVRomBanks == 1 || header->numVRomBanks == 0);
    if (header->numVRomBanks == 1)
    {
        uint8_t *dest = &vidMemory[chrRomBase + chrRomSize];
        uint8_t *src = (uint8_t*)rom + sizeof(NesHeader) + header->numRomBanks * cartridgeRomSize;
        memcpy(dest, src, chrRomSize);
        memcpy(dest - chrRomSize, src, chrRomSize);
        
    }
    
    // set nametable mirroring
    verticalMirroring = (header->info[0] & 1) != 0;
    
    
    return 0;
}

Nes::~Nes()
{
    if (rom) {
        munmap(rom, romSize);
    }
}

