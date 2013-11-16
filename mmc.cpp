#include "mmc.h"
#include <cassert>
#include <iostream>

/*
class MmcNone : public Mmc {
    MmcNone() = delete;
    MmcNone(const std::vector<uint8_t*>& prgRoms, const std::vector<uint8_t>& chrRoms, bool verticalMirror);
    ~MmcNone();
    void cpuMemWrite(uint16_t addr, uint8_t val) = 0;
    uint8_t cpuMemRead(uint16_t addr) = 0;
    void vidMemWrite(uint16_t addr, uint8_t val) = 0;
    uint8_t vidMemRead(uint16_t addr) = 0;
    bool isVerticalMirror() = 0;
};
*/

MmcNone::MmcNone(const std::vector<uint8_t*> &prgRoms,
                 const std::vector<uint8_t*> &chrRoms,
                 uint32_t prgRam,
                 bool vertMirror) :
    progRoms{prgRoms},
    charRoms{chrRoms},
    numPrgRam{prgRam},
    verticalMirror{vertMirror}
{
    assert(numPrgRam == 0 || numPrgRam == 1);
    assert(prgRoms.size() <= 2);
    assert(chrRoms.size() <= 1);
}

MmcNone::~MmcNone()
{}

void MmcNone::cpuMemWrite(uint16_t addr, uint8_t val)
{
    assert(addr >= mmcCpuAddrBase);
    if ((numPrgRam == 1) and
        (addr >= prgSramBase) and
        (addr < prgSramBase + prgSramSize)) {
        cpuSram[addr - prgSramBase] = val;                
    }
}

uint8_t MmcNone::cpuMemRead(uint16_t addr)
{
    assert(addr >= mmcCpuAddrBase);
    if ((numPrgRam == 1) and
        (addr >= prgSramBase) and
        (addr < prgSramBase + prgSramSize)) {
        return cpuSram[addr - prgSramBase];
    }
    if ((addr >= 0x8000) and
        (addr < 0x8000 + 0x4000) and
        (progRoms.size() == 2)) {
        return *(progRoms[0] + addr - 0x8000);
    }
    if ((addr >= 0xc000) and
        (progRoms.size() >= 1)) {
        if (progRoms.size() == 1) {
            return *(progRoms[0] + addr - 0xc000);
        }
        else {
            return *(progRoms[1] + addr - 0xc000);
        }
    }
    return 0;
}

void MmcNone::vidMemWrite(uint16_t addr, uint8_t val)
{
    //assert(addr < (mmcVidAddrBase + mmcVidAddrSize));
    if (addr >= 0x2000 and addr < 0x2000 + 0x1000) {
        vidSram[addr - 0x2000] = val;    
    }
}

uint8_t MmcNone::vidMemRead(uint16_t addr)
{
    //assert(addr < (mmcVidAddrBase + mmcVidAddrSize));
    if ((charRoms.size() == 1) and
        (addr < 0x2000)) {
        return *(charRoms[0] + addr);
        std::cerr << "reading from vidmem" << std::endl;
    }
    if ((addr >= 0x2000) and (addr < 0x2000 + 0x1000)) {
        return vidSram[addr - 0x2000];
    }
    return 0;
}

bool MmcNone::isVerticalMirror()
{
    return verticalMirror;
}
