//
//  ppu.h
//  rnes
//
//  Created by Riley Andrews on 7/6/13.
//  Copyright (c) 2013 Riley Andrews. All rights reserved.
//

#ifndef __PPU_H__
#define __PPU_H__

#include <cstdint>
#include <unordered_map>
#include <string>
#include <iostream>
#include <sstream>
#include <iomanip>
#include <cassert>
#include <SDL/SDL.h>

class Nes;
class Sdl;

class Ppu {
public:
    static const uint32_t vblankScanline = 241;
    static const uint32_t vblankScanelineEnd = 261;
    
    static const uint32_t ticksPerScanline = 341;
    static const uint32_t totalScanlines = 262;
    static const uint32_t frameTime = totalScanlines * ticksPerScanline;
    
    static const uint32_t renderHeight = 240;
    static const uint32_t renderWidth = 256;
    
    static const uint32_t nameTableWidth = 32;
    static const uint32_t nameTableHeight = 30;
    
    static const uint32_t attrTableWidth = 8;
    static const uint32_t attrTableHeight = 8;
    
    static const uint16_t nameTableAddr = 0x2000;
    static const uint16_t backColorAddr = 0x3f00;
    
    static const uint32_t maxSpriteCount = 64;
    static const uint32_t maxRenderedSpritePerScanline = 8;

    static const uint32_t patternTableSize = 0x1000;
    
    static const uint32_t frameTimeMs = 1000 / 60;
    
    static const bool debug = false;
    
    const uint32_t nesPaletteLut[64] = {
        0x7C7C7C, 0x0000FC, 0x0000BC, 0x4428BC,
        0x940084, 0xA80020, 0xA81000, 0x881400,
        0x503000, 0x007800, 0x006800, 0x005800,
        0x004058, 0x000000, 0x000000, 0x000000,
        0xBCBCBC, 0x0078F8, 0x0058F8, 0x6844FC,
        0xD800CC, 0xE40058, 0xF83800, 0xE45C10,
        0xAC7C00, 0x00B800, 0x00A800, 0x00A844,
        0x008888, 0x000000, 0x000000, 0x000000,
        0xF8F8F8, 0x3CBCFC, 0x6888FC, 0x9878F8,
        0xF878F8, 0xF85898, 0xF87858, 0xFCA044,
        0xF8B800, 0xB8F818, 0x58D854, 0x58F898,
        0x00E8D8, 0x787878, 0x000000, 0x000000,
        0xFCFCFC, 0xA4E4FC, 0xB8B8F8, 0xD8B8F8,
        0xF8B8F8, 0xF8A4C0, 0xF0D0B0, 0xFCE0A8,
        0xF8D878, 0xD8F878, 0xB8F8B8, 0xB8F8D8,
        0x00FCFC, 0xF8D8F8, 0x000000, 0x000000,
    };
    
    // bits in control register
    enum Control1Reg {
        CONTROL_VRAM_ADDR_INC           = 1 << 2,
        CONTROL_PATTERN_TABLE_ADDR_SPR  = 1 << 3,
        CONTROL_PATTERN_TABLE_ADDR_SCR  = 1 << 4,
        CONTROL_SPRITE_SIZE             = 1 << 5,
        CONTROL_MASTER_SLAVE            = 1 << 6,
        CONTROL_NMI_ON_VBLANK           = 1 << 7,
    };
    
    // bits in second control register
    enum Control2Reg {
        CONTROL2_SPRITE_VISIBLE  = 1 << 4,
        CONTROL2_BKGD_VISIBLE    = 1 << 3,
        CONTROL2_SPRITE_CLIPPING = 1 << 2,
        CONTROL2_BKGD_CLIPPING   = 1 << 1,
        CONTROL2_MONOCHROME_MODE = 1 << 0
    };
    
    // bits in status register
    enum StatusReg {
        STATUS_LOST_SPRITES = 1 << 5,
        STATUS_SPRITE0_HIT  = 1 << 6,
        STATUS_VBLANK_HIT   = 1 << 7
    };
    
    // ppu registers
    enum {
        CONTROL1_REG            = 0,
        CONTROL2_REG            = 1,
        STATUS_REG              = 2,
        SPR_ADDR_REG            = 3,
        SPR_DATA_REG            = 4,
        VRAM_ADDR_REG1          = 5,
        VRAM_ADDR_REG2          = 6,
        VRAM_DATA_REG           = 7,
        REG_COUNT               = 8,
    };

public:   
    uint32_t getBgColor() {
        uint8_t memColor = load(backColorAddr);
        assert(memColor < sizeof(nesPaletteLut)/sizeof(nesPaletteLut[0]));
        if (isMonochromeMode()) {
            memColor &= 0x30;
        }
        else {
            memColor &= 0x3f;
        }
        return nesPaletteLut[memColor];
    }
    
    uint32_t getColor(uint32_t palette, uint32_t color, bool sprite) {
        uint8_t memColor;
        assert(color >= 0 && color <= 3);
        if (color == 0) {
            memColor = load(backColorAddr);
        }
        else {
            uint16_t paletteOffset = sprite ? 0x10 : 0;
            memColor = load(backColorAddr + paletteOffset + 4 * palette + color);
        }
        if (isMonochromeMode()) {
            memColor &= 0x30;
        }
        else {
            memColor &= 0x3f;
        }
        assert(memColor < sizeof(nesPaletteLut)/sizeof(nesPaletteLut[0]));
        return nesPaletteLut[memColor];
    }
    
    uint16_t getScanline() {
        return cycle / ticksPerScanline % totalScanlines;
    }
    uint16_t getScanlineOffset() {
        return cycle % ticksPerScanline;
    }
    uint16_t getNameTableAddr() {
        uint16_t addr;
        switch (regs[CONTROL1_REG] & 0x3) {
            case 0: addr = 0x2000; break;
            case 1: addr = 0x2400; break;
            case 2: addr = 0x2800; break;
            case 3: addr = 0x2c00; break;
        }
        return addr;
    }
    uint16_t getSpritePatternTableAddr() {
        return (regs[CONTROL1_REG] & CONTROL_PATTERN_TABLE_ADDR_SPR) ? 0x1000 : 0x0;
    }
    uint16_t getBgPatternTableAddr() {
        return (regs[CONTROL1_REG] & CONTROL_PATTERN_TABLE_ADDR_SCR) ? 0x1000 : 0x0;
    }
    bool nmiOnVblank() {
        return (regs[CONTROL1_REG] & CONTROL_NMI_ON_VBLANK) != 0;
    }
    bool isSpriteSize8x8() {
        return (regs[CONTROL1_REG] & CONTROL_SPRITE_SIZE) == 0;
    }
    bool isMonochromeMode() {
        return (regs[CONTROL2_REG] & CONTROL2_MONOCHROME_MODE) != 0;
    }
    void setSprite0Hit() {
        regs[STATUS_REG] |= STATUS_SPRITE0_HIT;
    }
    void clearSprite0Hit() {
        regs[STATUS_REG] &= ~STATUS_SPRITE0_HIT;
    }
    void setLostSprites() {
        regs[STATUS_REG] |= STATUS_LOST_SPRITES;
    }
    void clearLostSprites() {
        regs[STATUS_REG] &= ~STATUS_LOST_SPRITES;
    }
    bool isRequestingNmi() {
        bool ret = nmiRequested;
        nmiRequested = false;
        return ret;
    }
    bool renderBackgroundEnabled() {
        return (regs[CONTROL2_REG] & CONTROL2_BKGD_VISIBLE) != 0;
    }
    bool renderSpritesEnabled() {
        return (regs[CONTROL2_REG] & CONTROL2_SPRITE_VISIBLE) != 0;
    }
    void setVblankFlag() {
        regs[STATUS_REG] |= STATUS_VBLANK_HIT;
    }
    void clearVblankFlag() {
        regs[STATUS_REG] &= ~STATUS_VBLANK_HIT;
    }
    uint8_t getNameTable(uint32_t x, uint32_t y) {
        uint16_t nameTableAddr = getNameTableAddr();
        assert(x < 32);
        assert(y < 30);
        return load(nameTableAddr + nameTableWidth * y + x);
    }
    void vramCoarseXInc() {
        if ((vramCurrentAddr & 0x1f) == 31) {
            // coarse x = 0
            vramCurrentAddr &= ~0x001f;
            // switch horizontal nametable
            vramCurrentAddr ^= 0x0400;
        }  
        else {
            // increment coarse x
            vramCurrentAddr += 1;
        }
    }
    void vramYInc() {
        if ((vramCurrentAddr & 0x7000) != 0x7000) {
            vramCurrentAddr += 0x1000;
        }
        else {
            vramCurrentAddr &= ~0x7000;
            int y = (vramCurrentAddr & 0x03e0) >> 5;
            if (y == 29) {
                y = 0;
                vramCurrentAddr ^= 0x0800;
            }
            else if (y == 31) {
                y = 0;
            }
            else {
                y += 1;
            }
            vramCurrentAddr = (vramCurrentAddr & ~0x03e0) | (y << 5);
        }
    }
    void vramXReset() {
        vramCurrentAddr = (vramCurrentAddr & 0xfbe0) | (vramTempAddr & ~0xfbe0);
    }
    void vramYReset() {
        vramCurrentAddr = (vramCurrentAddr & ~0xfbe0) | (vramTempAddr & 0xfbe0);
    }
    uint16_t getTileAddr(uint16_t vramCurrent) {
        return 0x2000 | (vramCurrent & 0xfff);
    }
    uint32_t getFineY(uint16_t vramCurrent) {
        return (0x7000 & vramCurrent) >> 12;
    }
    uint16_t getAttrAddr(uint16_t vramCurrent) {
        return 0x23c0 | (vramCurrent & 0xc00) | ((vramCurrent >> 4) & 0x38) | ((vramCurrent >> 2) & 0x7);
    }
    uint16_t loadPatternTile(uint16_t addr) {
        uint16_t ret = 0;
        uint16_t a = load(addr);
        uint16_t b = load(addr + 8);
        for (uint32_t i = 0; i < 8; i++) {
            ret |= ((1u << i) & a) << i;
            ret |= ((1u << i) & b) << (i + 1);
        }
        return ret;
    }


    uint32_t getNameTableXOffset();
    uint32_t getNameTableYOffset();
    uint8_t getColorFromPatternTable(uint16_t patternTable, bool is8x8, int offset, uint32_t x, uint32_t y);
    uint8_t getNameTableColor(uint16_t nameTableAddr, uint16_t patternTableAddr, bool is8x8, uint32_t x, uint32_t y);
    uint8_t getAttributeTablePalette(uint16_t nameTableAddr, uint32_t x, uint32_t y);
    uint32_t renderBackgroundPixel(int x, int y, bool& transparent);
    void render(uint32_t scanline);
    void renderv2(uint32_t scanline);
    void tick();
    
    void run(uint32_t cpuCycle)
    {
        for (uint32_t i = 0; i < cpuCycle * 3; i++) {
            tick();
        }
    }
    uint16_t vramAddrInc() {
        if (regs[CONTROL1_REG] & CONTROL_VRAM_ADDR_INC) {
            return 32;
        }
        else {
            return 1;
        }
    }
    
    uint8_t load(uint16_t addr);
    void store(uint16_t addr, uint8_t val);
    
    void preRender();
    void postRender();
    void setPixel(int x, int y, uint32_t color);
    
    void writeReg(uint32_t reg, uint8_t val);
    uint8_t readReg(uint32_t reg);

public:
    
    Ppu(Nes *parent, Sdl *disp);
    Ppu() = delete;
    Ppu(const Ppu&) = delete;
    ~Ppu() {}
private:
    bool nmiRequested;
    Nes *nes;
    Sdl *sdl;
    uint64_t cycle;
    uint8_t regs[REG_COUNT] = {0};
    enum SpriteAttr {
        ATTR_VERT_FLIP  = 1 << 7,
        ATTR_HORIZ_FLI  = 1 << 6,
        ATTR_BG_PRIOR   = 1 << 5,
        ATTR_COLOR_MASK = 3 << 0,
    };
    struct {
        uint8_t yCoordMinus1;
        uint8_t tileIndex;
        uint8_t attr;
        uint8_t xCoord;
    } spriteRam[64] = {{0}};

    uint32_t vramToggle = 0;
    uint32_t vramFineXScroll = 0;
    uint16_t vramCurrentAddr = 0;
    uint16_t vramTempAddr = 0;


    uint16_t vramMachineAddr = 0;
    uint8_t vramReadLatch = 0;
    
    uint32_t scrollingMachineState = 0;
    uint8_t xScrollOrigin = 0;
    uint8_t yScrollOrigin = 0;
    
    uint32_t lastFrameTimeMs = 0;
    
    uint32_t bgScanlineBuffer[256];
    uint32_t spriteScanlineBuffer[256];
    bool pixelWritten[256];
    uint32_t scanlineBuffer[256];    

};


#endif 
