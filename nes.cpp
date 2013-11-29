//
//  nes.cpp
//  rnes
//
//  Created by Riley Andrews on 7/6/13.
//  Copyright (c) 2013 Riley Andrews. All rights reserved.
//

#include "nes.h"
#include "mmc.h"
#include <unistd.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <sys/stat.h>

void Controller::setShiftReg()
{
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

void Controller::write(uint8_t val)
{
    if ((control & 0x1) and !(val & 0x1)) {
        setShiftReg();
    }
    control = val;
}

uint8_t Controller::read()
{
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

uint16_t Nes::translateCpuWindows(uint16_t addr) const
{
    // Deal with 3 mirrors of 2k internal RAM: 0x0 - 0x7ff.
    if (addr >= 0x800 && addr < 0x2000) {
        addr = (addr & (0x800 - 1)) + 0x800;
    }
    // Deal with 1023 mirrors of 8 bytes of PPU registers.
    if (addr >= 0x2000 && addr < 0x4000) {
        addr = (addr & (0x8 - 1)) + 0x2000;
    }
    return addr;
}

uint16_t Nes::translatePpuWindows(uint16_t addr) const
{
    // Name table mirrors.
    if (addr >= 0x3000 && addr < 0x3f00) {
        addr = (addr & (0xf00 - 1)) + 0x2000;
    }
    // ??
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
    if (addr < 0x800) {
        cpuMemory[addr] = val;
    }
    else if (addr >= ppuRegBase && addr <= ppuRegEnd) {
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
        mmc->cpuMemWrite(addr, val);
    }
}

uint8_t Nes::cpuMemRead(uint16_t addr)
{
    addr = translateCpuWindows(addr);
    if (addr < 0x800) {
        return cpuMemory[addr];
    }
    else if (addr >= ppuRegBase && addr <= ppuRegEnd) {
        return ppu.readReg(addr - ppuRegBase);
    }
    else if (addr == joypadAddr) {
        return pad.read();
    }
    else if (addr >= apuRegBase && addr <= apuRegEnd) {
        return apu.readReg(addr - apuRegBase);
    }
    else {
        return mmc->cpuMemRead(addr);
    }
}

void Nes::vidMemWrite(uint16_t addr, uint8_t val)
{
    addr = translatePpuWindows(addr);
    assert(addr < videoMemorySize);
    mmc->vidMemWrite(addr, val);
}

uint8_t Nes::vidMemRead(uint16_t addr)
{
    addr = translatePpuWindows(addr);
    assert(addr < videoMemorySize);
    return mmc->vidMemRead(addr);
}

void Nes::notifyScanlineComplete()
{
    mmc->notifyScanlineComplete();
}

bool Nes::isRequestingNmi()
{
    return ppu.isRequestingNmi();
}

bool Nes::isRequestingInt()
{
    return apu.isRequestingIrq() || mmc->isRequestingIrq();
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
    }
}

struct NesHeader {
    char str[4]; // ? "NES^Z"
    uint8_t numRomBanks;   // number of 16kb ROM banks
    uint8_t numVRomBanks;  // num of vrom banks
    uint8_t info[2];
    uint8_t numPrgRamBanks;
    uint8_t info2;
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
    std::cerr << "ram banks: " << (int)header->numPrgRamBanks << std::endl;
    if (header->info[0] & (1 << 2)) {
        std::cerr << "trainer present" << std::endl;
    }
    uint8_t mapper = (header->info[0] & 0xf0) >> 4;
    mapper |= (header->info[1] & 0xf0);
    std::cerr << "mapper: " << (int)mapper << std::endl;


    std::vector<uint8_t*> prgRoms;
    std::vector<uint8_t*> chrRoms;

    uint8_t *prgRomBase = (uint8_t*)rom + sizeof(NesHeader);
    for (int i = 0; i < header->numRomBanks; i++) {
        std::cerr << "loading rom section " << std::endl;
        uint8_t *prgRom  = prgRomBase + i * prgRomSize;
        prgRoms.push_back(prgRom);
    }

    uint8_t *chrRomBase = prgRomBase + header->numRomBanks * prgRomSize;
    for (int i = 0; i < header->numVRomBanks; i++) {
        uint8_t *chrRom = chrRomBase + i * chrRomSize;
        chrRoms.push_back(chrRom);
    }

    bool verticalMirroring = (header->info[0] & 1) != 0;
    switch (mapper) {
        case 0:
            mmc = new MmcNone(prgRoms, chrRoms, header->numPrgRamBanks, verticalMirroring);
            break;
        case 1:
            mmc = new Mmc1(prgRoms, chrRoms, header->numPrgRamBanks, verticalMirroring);
            break;
        case 4:
            mmc = new Mmc3(prgRoms, chrRoms, header->numPrgRamBanks, verticalMirroring);
            break;
        default:
            assert(0);
            break;
    }
    return 0;
}

Nes::~Nes()
{
    if (rom) {
        munmap(rom, romSize);
    }
}

