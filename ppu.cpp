//
//  ppu.cpp
//  rnes
//
//  Created by Riley Andrews on 7/6/13.
//  Copyright (c) 2013 Riley Andrews. All rights reserved.
//

#include "ppu.h"
#include "nes.h"
#include "time.h"
#include "sys/time.h"
#include <assert.h>

namespace Rnes {

static void sleepMs(float ms)
{
    struct timespec wait = {
        0, (long)(ms * 1000000)
    };
    nanosleep(&wait, NULL);
}

static float timerGetMs()
{
    struct timespec ts;
    clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &ts);
    return ts.tv_sec * 1000.0f + ts.tv_nsec / 1000000.0f;
}

Ppu::Ppu(Nes *parent, Sdl *disp) : nes{parent}, sdl{disp}
{
    lastFrameTimeMs = timerGetMs();
}

uint8_t Ppu::load(uint16_t addr)
{
    return nes->vidMemRead(addr);
}

void Ppu::store(uint16_t addr, uint8_t val)
{
    nes->vidMemWrite(addr, val);
}

void Ppu::writeReg(uint32_t reg, uint8_t val)
{
    // CONTROL1_REG            = 0,
    // CONTROL2_REG            = 1,
    // STATUS_REG              = 2,
    // SPR_ADDR_REG            = 3,
    // SPR_DATA_REG            = 4,
    // VRAM_ADDR_REG1          = 5,
    // VRAM_ADDR_REG2          = 6,
    // VRAM_DATA_REG           = 7,
    switch (reg) {
        case CONTROL1_REG:
            vramTempAddr = (vramTempAddr & 0xf3ff) | ((uint16_t)val & 0x3) << 10;
        case CONTROL2_REG:
            // Write only registers.
            regs[reg] = val;
            break;
        case STATUS_REG:
            // Read only register.
            break;
        case SPR_ADDR_REG:
            regs[reg] = val;
            break;
        case SPR_DATA_REG:
            memcpy(regs[SPR_ADDR_REG] + (uint8_t*)&spriteRam[0], &val, 1);
            regs[SPR_ADDR_REG]++;
            break;
        case VRAM_ADDR_REG1:
            if (vramToggle == 0) {
                vramTempAddr = (0xffe0 & vramTempAddr) | (val >> 3);
                vramFineXScroll = val & 0x7;
            }
            else {
                vramTempAddr = (0xc1f & vramTempAddr) | (((uint16_t)val & 0x7) << 12) | (((uint16_t)val & 0xf8) << 2);
            }
            vramToggle = not vramToggle;
            break;
        case VRAM_ADDR_REG2:
            if (vramToggle == 0) {
                vramTempAddr = (((uint16_t)val & 0x3f) << 8) | (vramTempAddr & 0xff);
            }
            else {
                vramTempAddr = val | (vramTempAddr & 0xff00);
                vramCurrentAddr = vramTempAddr;
            }
            vramToggle = not vramToggle;
            break;
        case VRAM_DATA_REG:
            nes->vidMemWrite(vramCurrentAddr, val);
            vramCurrentAddr += getVramAddrInc();
            break;
        default:
            assert(0);
            break;
    }
}

uint8_t Ppu::readReg(uint32_t reg)
{
    uint8_t ret;
    switch (reg) {
        case CONTROL1_REG:
            assert(0); 
            // GUESS: Write only registers.
            return regs[reg];
            break;
        case CONTROL2_REG:
            assert(0); 
            // GUESS: Write only registers.
            return regs[reg];
            break;
        case STATUS_REG:
            ret = regs[reg];
            // vblank status bit is clear on read
            clearVblankFlag();
            // reset the scroll/vram machine flip flops
            vramToggle = 0;
            // and return the reg value 
            return ret;
            break;
        case SPR_ADDR_REG:
        case SPR_DATA_REG:
            // Write only registers.
            assert(0);
            return 0; 
            break;
        case VRAM_ADDR_REG1:
            //assert(0); 
            // This is a guess, we aren't expecting reads here.
            return vramCurrentAddr;
            break;
        case VRAM_ADDR_REG2:
            assert(0); 
            // This is a guess, we aren't expecting reads here.
            return vramTempAddr;
            break;
        case VRAM_DATA_REG:
            ret = vramReadLatch;
            vramReadLatch = nes->vidMemRead(vramCurrentAddr);
            vramCurrentAddr += getVramAddrInc();
            return ret;
            break;
        default:
            assert(0);
            break;
    }
    return 0;
}


template<bool is8x8> uint8_t Ppu::getColorFromPatternTable(uint16_t patternTable, int offset, uint32_t x, uint32_t y)
{
    assert(x < 8);
    if (!is8x8) {
        assert(y < 16);
        if ((offset % 2) == 0) {
            patternTable = 0x0000;
        }
        else {
            patternTable = 0x1000;
            offset--;
        }
        if (y >= 8) {
            offset++;
            y -= 8;
        }
    }
    else {
        assert(y < 8);
    }
    x = 8 - 1 - x;
    uint16_t byteAddrLower = patternTable + 16 * offset + y;
    uint16_t byteAddrUpper = patternTable + 16 * offset + y + 8;
    uint8_t color = ((load(byteAddrLower) >> x) & 0x1) | (((load(byteAddrUpper) >> (x)) & 0x1) << 1);
    return color;
}

void Ppu::render(uint32_t scanline)
{
    bool spriteSize8x8 = isSpriteSize8x8();
    uint32_t spriteSize = spriteSize8x8 ? 8 : 16;
    
    // Indicate that all bg elements are transparent
    memset(pixelWritten, 0, sizeof(pixelWritten));
    memset(scanlineBuffer, 0, sizeof(scanlineBuffer));
    
    // render bg
    if (renderBackgroundEnabled()) {
        uint32_t patternTableAddr = getBgPatternTableAddr();
        uint32_t tempFineXScroll = vramFineXScroll;
        
        // render background
        for (uint32_t xBgOffset = 0; xBgOffset < renderWidth;) {
            // Get next tile and attribute addresses
            uint16_t nameAddr = getTileAddr(vramCurrentAddr);
            uint16_t attrAddr = getAttrAddr(vramCurrentAddr);

            // Load attribute and pattern
            uint16_t patternAddr = load(nameAddr) * 16 + patternTableAddr + getFineY(vramCurrentAddr);
            assert(patternAddr >= patternTableAddr and patternAddr < (patternTableAddr + patternTableSize));
            uint16_t pattern = loadPatternTile(patternAddr);
            uint8_t attr = load(attrAddr);
            uint32_t tileOffsetY = ((nameAddr & 0x3ff) >> 5 >> 1) % 2;

            // Render the pattern 
            for (uint32_t tileXOffset = tempFineXScroll;
                 tileXOffset < 8 and xBgOffset < renderWidth;
                 tileXOffset++, xBgOffset++) {

                uint32_t color = (pattern >> ((7 - tileXOffset) * 2)) & 0x3;
                uint32_t tileOffsetX = (((nameAddr & 0x3ff) & 0x1f) >> 1) % 2;
                uint32_t subNibble = tileOffsetX + tileOffsetY * 2;
                assert(subNibble >= 0 and subNibble < 4);
                uint32_t palette = (attr >> (2 * subNibble)) & 0x3;
                bool transparent = (color == 0);
                uint32_t pixel = getColor(palette, color, false);
            
                scanlineBuffer[xBgOffset] = pixel;
                if (!transparent) {
                    pixelWritten[xBgOffset] = true;
                }
                tempFineXScroll++;
                tempFineXScroll &= 0x7;
            }
            vramCoarseXInc();
        }
    }
    
    // render sprites
    if (renderSpritesEnabled()) {
        // Render up to 8 colliding sprites
        uint32_t patternTableAddr = getSpritePatternTableAddr();
        uint32_t renderedSprites = 0;
        for (uint32_t i = 0; i < maxSpriteCount; i++) {
            
            // Determine if the provide collides with the scanline
            uint32_t sprite = maxSpriteCount - 1 - i;
            if ((scanline >= (spriteRam[sprite].yCoordMinus1 + 1u)) and
                (scanline <= (spriteRam[sprite].yCoordMinus1 + 1u + spriteSize - 1u))) {
            }
            else {
                continue;
            }
            
            // Render the sprite
            bool spriteVerticalFlip = (spriteRam[sprite].attr & (1u << 7)) != 0;
            bool spriteHorizontalFlip = (spriteRam[sprite].attr & (1u << 6)) != 0;
            for (int j = 0; j < 8; j++) {
                uint32_t spriteLine = scanline - (spriteRam[sprite].yCoordMinus1 + 1);
                if (spriteVerticalFlip) {
                    spriteLine = spriteSize - 1 - spriteLine;
                }
                
                uint32_t xOffset = j;
                if (spriteHorizontalFlip) {
                    xOffset = 8 - 1 - xOffset;
                }
                uint32_t color;
                if (spriteSize8x8) {
                    color = getColorFromPatternTable<true>(patternTableAddr, spriteRam[sprite].tileIndex, xOffset, spriteLine);
                }
                else {
                    color = getColorFromPatternTable<false>(patternTableAddr, spriteRam[sprite].tileIndex, xOffset, spriteLine);
                }
                
                // Need to do some bounds checking bullshit here
                uint32_t xCoordinate = spriteRam[sprite].xCoord + j;
                if ((color != 0) and (xCoordinate < renderWidth)) {
                    bool bgPixelInFront = (spriteRam[sprite].attr & (1u << 5)) != 0;
                    if ((bgPixelInFront and !pixelWritten[xCoordinate]) or !bgPixelInFront) {
                        scanlineBuffer[xCoordinate] = getColor(spriteRam[sprite].attr & 0x3, color, true);
                    }
                }
                // Do sprite 0 collision detection
                if ((sprite == 0) and (color != 0) and (xCoordinate < renderWidth - 1)) {
                    if (pixelWritten[xCoordinate]) {
                        setSprite0Hit();
                    }
                }
            }
            renderedSprites++;
            if (renderedSprites > maxRenderedSpritePerScanline) {
                setLostSprites();
                break;
            }
        }
    }
    
    
    // dump pixels to display
    for (uint32_t i = 0; i < renderWidth; i++) {
        setPixel(i, scanline, scanlineBuffer[i]);
    }
}


void Ppu::tick()
{
    uint32_t scanline = getScanline();
    uint32_t lineClock = getScanlineOffset();
    bool isVblank = scanline >= 240 and scanline <= 260;
    
    // Update vram registers
    if (!isVblank and renderBackgroundEnabled()) {
        if (lineClock == 255 and scanline < 240) {
            render(scanline);
            nes->notifyScanlineComplete();
        }
        if (lineClock == 256) {
            vramYInc();
        }
        if (lineClock == 257) {
            vramXReset();
        }
        if (scanline == 261 and lineClock >= 280 and lineClock <= 304) {
            vramYReset();
        }
    }
    
    // set and unset vblank register
    if (scanline == vblankScanline and lineClock == 0) {        
        setVblankFlag();
        if (nmiOnVblank()) {
            nmiRequested = true;
        }
    }
    else if (scanline == vblankScanelineEnd and lineClock == 0) {
        clearVblankFlag();
        clearSprite0Hit();
        clearLostSprites();
    }
    else if (scanline == vblankScanelineEnd and lineClock == (ticksPerScanline - 1)) {
        sdl->renderSync();
        /*
        float currentTime = timerGetMs();
        float diffTime = currentTime - lastFrameTimeMs;
        if (diffTime < frameTimeMs) {
            float waitTime = frameTimeMs - diffTime;
            sleepMs(waitTime);
        }
        lastFrameTimeMs = timerGetMs();
        */
        frame += 1;
    }
    cycle += 1;
}

void Ppu::run(uint32_t cpuCycle)
{
    for (uint32_t i = 0; i < cpuCycle * 3; i++) {
        tick();
    }
}

void Ppu::setPixel(int x, int y, uint32_t color)
{
    uint8_t r = 0xff & (color >> 16);
    uint8_t g = 0xff & (color >> 8);
    uint8_t b = 0xff & (color);
    sdl->setPixel(x, y, r, g, b);
}

};
