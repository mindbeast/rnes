//
//  nes.h
//  rnes
//
//  Created by Riley Andrews on 7/6/13.
//  Copyright (c) 2013 Riley Andrews. All rights reserved.
//

#ifndef __NES_H__
#define __NES_H__

#include <memory>

namespace Rnes {

class Mmc;
class Sdl;
class Cpu;
class Ppu;
class Apu;
class CpuMemory;
class VideoMemory;
class SaveState;
class ControllerState;

class Controller {
    Sdl *sdl;
    uint8_t control = 0;
    uint8_t shiftReg =  0;
    const bool debug = false;
    
public:
    Controller(Sdl *inpt) : sdl{inpt} {}
    ~Controller() {}

    void save(ControllerState &pb);
    void restore(const ControllerState &pb);

    void setShiftReg();
    void write(uint8_t val);
    uint8_t read();
    
    Controller(const Controller&) = delete;
    Controller operator=(const Controller&) = delete;
};

class Nes {
    void *rom;
    size_t romSize;
    
    bool spriteDmaMode = false;
    uint32_t spriteDmaCycle = 0;
    uint16_t spriteDmaSourceAddr = 0;

    uint64_t cycles = 0;
    
    std::unique_ptr<Sdl> sdl;
    std::unique_ptr<Cpu> cpu;
    std::unique_ptr<Ppu> ppu;
    std::unique_ptr<Apu> apu;
    std::unique_ptr<Controller> pad;
    std::unique_ptr<CpuMemory> cpuMemory;
    std::unique_ptr<VideoMemory> videoMemory;
    std::unique_ptr<Mmc> mmc;
    
    uint32_t spriteDmaExecute();
    void spriteDmaSetup(uint8_t val);
    
public:
    void cpuMemWrite(uint16_t addr, uint8_t val);
    uint8_t cpuMemRead(uint16_t addr);
    void vidMemWrite(uint16_t addr, uint8_t val);
    uint8_t vidMemRead(uint16_t addr);

    void notifyScanlineComplete();

    bool isRequestingNmi();
    bool isRequestingInt();
    
    int mapRom(const std::string &filename);
    int loadRom(const std::string &filename);
    void run();

    void save(SaveState &pb);
    void restore(const SaveState &pb);
    
    Nes();
    ~Nes();
    Nes& operator=(const Nes&) = delete;
    Nes(const Nes&) = delete;
};

};

#endif
