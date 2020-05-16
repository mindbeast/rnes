//
//  memory.h
//  rnes
//
//

#ifndef __MEMORY_H__
#define __MEMORY_H__

#include <cstdint>

namespace Rnes {

class CpuMemoryState;
class VideoMemoryState;
class Mmc;

class CpuMemory {
    Mmc *mmc;
public:

    static const uint16_t cpuSramSize = 0x800;
    uint8_t cpuSram[cpuSramSize] = {0};

    static const uint16_t prgSramBase = 0x6000;
    static const uint16_t prgSramSize = 0x2000;
    uint8_t prgSram[prgSramSize] = {0};

    uint8_t load(uint16_t addr) const ;
    void store(uint16_t addr, uint8_t data);

    void setMmc(Mmc *mmcPtr) { mmc = mmcPtr; }

    void save(CpuMemoryState &pb);
    void restore(const CpuMemoryState &pb);
    
    CpuMemory() {} 
    CpuMemory(const CpuMemory&) = delete;
    ~CpuMemory() {} 
};

class VideoMemory {
    Mmc *mmc;
public:

    static const uint16_t patternTableSize = 0x2000;
    uint8_t patternTableMemory[patternTableSize] = {0};

    static const uint16_t nameTableMemorySize = 0x1000;
    uint8_t nameTableMemory[nameTableMemorySize] = {0};

    static const uint16_t paletteSize = 0x20;  
    uint8_t paletteMemory[paletteSize] = {0};

    uint8_t load(uint16_t addr) const;
    void store(uint16_t addr, uint8_t data);

    void setMmc(Mmc *mmcPtr) { mmc = mmcPtr; }

    void save(VideoMemoryState &pb);
    void restore(const VideoMemoryState &pb);
    
    VideoMemory() {} 
    VideoMemory(const VideoMemory&) = delete;
    ~VideoMemory() {} 
};

}

#endif 
