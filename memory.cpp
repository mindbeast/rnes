#include "mmc.h"
#include "memory.h"

namespace Rnes {

uint8_t CpuMemory::load(uint16_t addr) const 
{
    if (addr < cpuSramSize) {
        return cpuSram[addr];
    }
    else if ((addr >= prgSramBase) and
             (addr < prgSramBase + prgSramSize) and 
             mmc->isPrgSramEnabled()) {
        return prgSram[addr - prgSramBase];
    }
    else {
        return mmc->cpuMemRead(addr);
    }
}

void CpuMemory::store(uint16_t addr, uint8_t data)
{
    if (addr < cpuSramSize) {
        cpuSram[addr] = data;
    }
    else if ((addr >= prgSramBase) and
             (addr < prgSramBase + prgSramSize) and
             mmc->isPrgSramWriteable()) {
        prgSram[addr - prgSramBase] = data;
    }
    else {
        mmc->cpuMemWrite(addr, data);
    }
}

uint8_t VideoMemory::load(uint16_t addr) const 
{
    if (addr >= 0x2000 and addr < 0x2000 + 0x1000) {
        addr = mmc->vidAddrTranslate(addr);
        return nameTableMemory[addr - 0x2000];
    }
    else if (addr >= 0x3f00 and addr <= 0x3f1f) {
        return paletteMemory[addr - 0x3f00];
    }
    else {
        return mmc->vidMemRead(addr);
    }
}

void VideoMemory::store(uint16_t addr, uint8_t data)
{
    if (addr >= 0x2000 and addr < 0x2000 + 0x1000) {
        nameTableMemory[addr - 0x2000] = data;    
    }
    else if (addr >= 0x3f00 and addr <= 0x3f1f) {
        paletteMemory[addr - 0x3f00] = data;    
    }
    else {
        mmc->vidMemWrite(addr, data);
    }
}

}
