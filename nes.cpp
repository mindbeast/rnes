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
