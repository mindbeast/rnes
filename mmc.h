//
//  mmc.h
//  rnes
//
//  Created by Riley Andrews on 11/10/13.
//  Copyright (c) 2013 Riley Andrews. All rights reserved.
//

#ifndef __MMC_H__
#define __MMC_H__

#include <cstdint>
#include <vector>

static const uint16_t mmcCpuAddrBase = 0x6000;
static const uint16_t mmcCpuAddrSize = 0xa000;

static const uint16_t mmcVidAddrBase = 0x0;
static const uint16_t mmcVidAddrSize = 0x2000;

static const uint16_t prgSramBase = 0x6000;
static const uint16_t prgSramSize = 0x2000;

class Mmc {
public:
    Mmc() {}
    virtual ~Mmc() {}
    virtual void cpuMemWrite(uint16_t addr, uint8_t val) = 0;
    virtual uint8_t cpuMemRead(uint16_t addr) = 0;
    virtual void vidMemWrite(uint16_t addr, uint8_t val) = 0;
    virtual uint8_t vidMemRead(uint16_t addr) = 0;
    virtual bool isVerticalMirror() = 0;
};

class MmcNone : public Mmc {
    uint8_t cpuSram[prgSramSize] = {0};
    uint8_t vidSram[0x1000] = {0};
    std::vector<uint8_t*> progRoms;
    std::vector<uint8_t*> charRoms;
    uint32_t numPrgRam;
    bool verticalMirror;

public:
    MmcNone() = delete;
    MmcNone(const std::vector<uint8_t*>& prgRoms,
            const std::vector<uint8_t*>& chrRoms,
            uint32_t prgRam,
            bool verticalMirror);
    ~MmcNone();
    void cpuMemWrite(uint16_t addr, uint8_t val);
    uint8_t cpuMemRead(uint16_t addr);
    void vidMemWrite(uint16_t addr, uint8_t val);
    uint8_t vidMemRead(uint16_t addr);
    bool isVerticalMirror();
};

class Mmc1 : public Mmc {
    uint8_t cpuSram[prgSramSize] = {0};
    uint8_t vidSram[0x1000 + 0x2000] = {0};
    std::vector<uint8_t*> progRoms;
    std::vector<uint8_t*> charRoms;
    uint32_t numPrgRam;
    bool verticalMirror;

    // mmc1 internal registers
    uint8_t controlReg = 0x1c;
    uint8_t chr0Bank = 0;
    uint8_t chr1Bank = 0;
    uint8_t prgBank = 0;
    uint8_t shiftRegister = 0;
    uint32_t writeNumber;

    static const uint16_t shiftWriteAddr = 0x8000;
    static const uint16_t shiftWriteAddrLimit = 0xffff;
    static const uint8_t shiftInit = 1 << 4;

    void updateMmcRegister(uint16_t addr, uint8_t shiftRegister);
    uint32_t getPrgRomMode() const {
        return (controlReg >> 2) & 0x3;
    }
    uint32_t getMirroringMode() const {
        return controlReg & 0x3;
    }
    uint32_t getChrRomMode() const {
        return (controlReg >> 4) & 0x1;
    }
    bool isPrgSramEnabled() const {
        return (controlReg & (1 << 4)) != 0;
    }
public:
    Mmc1() = delete;
    Mmc1(const std::vector<uint8_t*>& prgRoms,
            const std::vector<uint8_t*>& chrRoms,
            uint32_t prgRam,
            bool verticalMirror);
    ~Mmc1();
    void cpuMemWrite(uint16_t addr, uint8_t val);
    uint8_t cpuMemRead(uint16_t addr);
    void vidMemWrite(uint16_t addr, uint8_t val);
    uint8_t vidMemRead(uint16_t addr);
    bool isVerticalMirror();
};

#endif 
