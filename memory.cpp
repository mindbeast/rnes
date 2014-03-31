#include "mmc.h"
#include "memory.h"
#include "save.pb.h"

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

void CpuMemory::save(CpuMemoryState &pb)
{
    pb.set_cpusram(0, cpuSram, cpuSramSize); 
    pb.set_prgsram(0, prgSram, prgSramSize); 
}

void CpuMemory::restore(CpuMemoryState &pb)
{
    const std::string& bytes = pb.cpusram(0);
    for (int i = 0; i < pb.cpusram_size(); i++) {
        cpuSram[i] = bytes[i]; 
    }

    const std::string& prgBytes = pb.prgsram(0);
    for (int i = 0; i < pb.prgsram_size(); i++) {
        prgSram[i] = prgBytes[i]; 
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

void VideoMemory::save(VideoMemoryState &pb)
{
    pb.set_patterntablememory(0, patternTableMemory, patternTableSize); 
    pb.set_nametablememory(0, nameTableMemory, nameTableMemorySize); 
    pb.set_palettememory(0, paletteMemory, paletteSize);
}

void VideoMemory::restore(VideoMemoryState &pb)
{
    {
        const std::string& bytes = pb.patterntablememory(0);
        for (int i = 0; i < pb.patterntablememory_size(); i++) {
            patternTableMemory[i] = bytes[i]; 
        }
    }
    {
        const std::string& bytes = pb.nametablememory(0);
        for (int i = 0; i < pb.nametablememory_size(); i++) {
            nameTableMemory[i] = bytes[i]; 
        }
    {
        const std::string& bytes = pb.palettememory(0);
        for (int i = 0; i < pb.palettememory_size(); i++) {
            paletteMemory[i] = bytes[i]; 
        }
    }
    }
}

}
