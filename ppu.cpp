//
//  ppu.cpp
//  rnes
//
//  Created by Riley Andrews on 7/6/13.
//  Copyright (c) 2013 Riley Andrews. All rights reserved.
//

#include "ppu.h"
#include "nes.h"

static uint32_t timerGetMs()
{
    return SDL_GetTicks();
}

static void sleepMs(uint32_t ms)
{
    SDL_Delay(ms);
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
            memcpy(regs[SPR_ADDR_REG]+ (uint8_t*)&spriteRam[0], &val, 1);
            regs[SPR_ADDR_REG]++;
            break;
        case VRAM_ADDR_REG1:
            if (vramToggle == 0) {
                vramTempAddr = (0xffe0 & vramTempAddr) | (val >> 3) ;
                vramFineXScroll = val & 0x7;
            }
            else {
                vramTempAddr = (0xc1f & vramTempAddr) | (((uint16_t)val & 0x7) << 12) | (((uint16_t)val & 0xf8) << 2);
            }
            vramToggle = not vramToggle;
            break;
        case VRAM_ADDR_REG2:
            //regs[CONTROL1_REG] &= ~0x3;
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
            vramCurrentAddr += vramAddrInc();
            break;
        default:
            break;
    }
}

uint8_t Ppu::readReg(uint32_t reg)
{
    uint8_t ret;
    switch (reg) {
        case CONTROL1_REG:
        case CONTROL2_REG:
            // Write only registers.
            return 0; 
            break;
        case STATUS_REG:
            // vblank status bit is clear on read
            regs[STATUS_REG] |= regs[STATUS_REG] & ~STATUS_VBLANK_HIT;
            // reset the scroll/vram machine flip flops
            vramToggle = 0;
            // and return the reg value 
            return regs[reg]; 
            break;
        case SPR_ADDR_REG:
        case SPR_DATA_REG:
            // Write only registers.
            return 0; 
            break;
        case VRAM_ADDR_REG1:
            assert(0);
            break;
        case VRAM_ADDR_REG2:
            assert(0);
            break;
        case VRAM_DATA_REG:
            ret = vramReadLatch;
            vramReadLatch = nes->vidMemRead(vramCurrentAddr);
            vramCurrentAddr += vramAddrInc();
            return ret;
            break;
        default:
            assert(0);
            break;
    }
    return 0;
}

uint32_t Ppu::getNameTableXOffset()
{
    return (regs[CONTROL1_REG] & 0x1) * 256;
}

uint32_t Ppu::getNameTableYOffset()
{
    return ((regs[CONTROL1_REG] & 0x2) >> 1) * 256;
}

uint8_t Ppu::getColorFromPatternTable(uint16_t patternTable, bool is8x8, int offset, uint32_t x, uint32_t y)
{
    assert(x < 8);
    uint32_t entrySize = is8x8 ? 16 : 32;
    if (!is8x8) {
        assert(y < 16);
        if ((offset % 2) == 0) {
            patternTable = 0x0000;
        }
        else {
            patternTable = 0x1000;
        }
    }
    else {
        assert(y < 8);
    }
    x = 8 - 1 - x;
    uint16_t byteAddrLower = patternTable + 16 * offset + y;
    uint16_t byteAddrUpper = patternTable + 16 * offset + y + entrySize / 2 ;
    uint8_t color = ((load(byteAddrLower) >> x) & 0x1) | (((load(byteAddrUpper) >> (x)) & 0x1) << 1);
    return color;
}

uint8_t Ppu::getNameTableColor(uint16_t nameTableAddr, uint16_t patternTableAddr, bool is8x8, uint32_t x, uint32_t y)
{
    assert(x < renderWidth);
    assert(y < renderHeight);
    
    uint32_t yPatternSize = is8x8 ? 8 : 16;
    uint16_t entryAddr = nameTableAddr + (y / yPatternSize) * nameTableWidth + x / 8;
    uint8_t entry = load(entryAddr);
    uint8_t color = getColorFromPatternTable(patternTableAddr, is8x8, entry, x % 8, y % yPatternSize);
    
    return color;
}

uint8_t Ppu::getAttributeTablePalette(uint16_t nameTableAddr, uint32_t x, uint32_t y)
{
    assert(x < renderWidth);
    assert(y < renderHeight);
    
    uint16_t entryAddr = nameTableAddr + nameTableWidth * nameTableHeight + x / 32 + y / 32 * attrTableWidth;
    uint8_t entry = load(entryAddr);
    uint32_t tileOffsetX = (x % 32) / 16;
    uint32_t tileOffsetY = (y % 32) / 16;
    uint32_t subNibble = tileOffsetX + tileOffsetY * 2;
    
    return (entry >> (2 * subNibble)) & 0x3;
}

uint32_t Ppu::renderBackgroundPixel(int x, int y, bool& transparent)
{
    uint32_t patternTableAddr = getBgPatternTableAddr();
    
    uint32_t globalOffsetX = (x + xScrollOrigin + getNameTableXOffset()) % (renderWidth * 2);
    uint32_t globalOffsetY = (y + yScrollOrigin + getNameTableYOffset()) % (renderHeight * 2);
    
    uint32_t nameTableOffset = (globalOffsetY / renderHeight) * 2 + (globalOffsetX / renderWidth);
    uint32_t nameTableAddr = 0x2000 + 0x400 * nameTableOffset;
    
    uint8_t palette = getAttributeTablePalette(nameTableAddr, globalOffsetX % renderWidth, globalOffsetY % renderHeight);
    uint8_t color = getNameTableColor(nameTableAddr, patternTableAddr, isSpriteSize8x8(), globalOffsetX % renderWidth, globalOffsetY % renderHeight);
    
    if (color == 0) {
        transparent = true;
    }
    else {
        transparent = false;
    }
    
    return getColor(palette, color, false);
}

void Ppu::render(uint32_t scanline)
{
    bool spriteSize8x8 = isSpriteSize8x8();
    uint32_t spriteSize = spriteSize8x8 ? 8 : 16;
    
    // Indicate that all bg elements are transparent
    memset(pixelBgInFront, 0, sizeof(pixelBgInFront));
    memset(pixelWritten, 0, sizeof(pixelWritten));
    memset(scanlineBuffer, 0, sizeof(scanlineBuffer));
    
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
                uint32_t color = getColorFromPatternTable(patternTableAddr, spriteSize8x8, spriteRam[sprite].tileIndex, xOffset, spriteLine);
                
                // Need to do some bounds checking bullshit here
                uint32_t xCoordinate = spriteRam[sprite].xCoord + j;
                if (color and (xCoordinate < renderWidth)) {
                    scanlineBuffer[xCoordinate] = getColor(spriteRam[sprite].attr & 0x3, color, true);
                    pixelBgInFront[xCoordinate] = (spriteRam[sprite].attr & (1u << 5)) != 0;
                    pixelWritten[xCoordinate]   = true;
                }
                // Do sprite 0 collision detection
                if (sprite == 0 and color != 0 and (xCoordinate < renderWidth - 1)) {
                    bool transparent;
                    renderBackgroundPixel(xCoordinate, scanline, transparent);
                    if (!transparent) {
                        setSprite0Hit();
                    }
                }
            }
            renderedSprites++;
            if (renderedSprites >= maxRenderedSpritePerScanline) {
                setLostSprites();
                break;
            }
        }
    }
    
    // render bg
    if (renderBackgroundEnabled()) {
        
        // render background
        for (uint32_t i = 0; i < renderWidth; i+=1) {
            bool transparent = false;


            uint32_t pixel = renderBackgroundPixel(i, scanline, transparent);
            
            if (!pixelWritten[i]) {
                scanlineBuffer[i] = pixel;
            }
            else if (pixelBgInFront[i] and not transparent) {
                scanlineBuffer[i] = pixel;
            }
        }
    }
    
    // dump pixels to display
    preRender();
    for (uint32_t i = 0; i < renderWidth; i++) {
        setPixel(i, scanline, scanlineBuffer[i]);
    }
    postRender();
}

void Ppu::renderv2(uint32_t scanline)
{
    bool spriteSize8x8 = isSpriteSize8x8();
    uint32_t spriteSize = spriteSize8x8 ? 8 : 16;
    
    // Indicate that all bg elements are transparent
    memset(pixelBgInFront, 0, sizeof(pixelBgInFront));
    memset(pixelWritten, 0, sizeof(pixelWritten));
    memset(scanlineBuffer, 0, sizeof(scanlineBuffer));
    
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
                uint32_t color = getColorFromPatternTable(patternTableAddr, spriteSize8x8, spriteRam[sprite].tileIndex, xOffset, spriteLine);
                
                // Need to do some bounds checking bullshit here
                uint32_t xCoordinate = spriteRam[sprite].xCoord + j;
                if (color and (xCoordinate < renderWidth)) {
                    scanlineBuffer[xCoordinate] = getColor(spriteRam[sprite].attr & 0x3, color, true);
                    pixelBgInFront[xCoordinate] = (spriteRam[sprite].attr & (1u << 5)) != 0;
                    pixelWritten[xCoordinate]   = true;
                }
                // Do sprite 0 collision detection
                if (sprite == 0 and color != 0 and (xCoordinate < renderWidth - 1)) {
                    bool transparent;
                    renderBackgroundPixel(xCoordinate, scanline, transparent);
                    if (!transparent) {
                        setSprite0Hit();
                    }
                }
            }
            renderedSprites++;
            if (renderedSprites >= maxRenderedSpritePerScanline) {
                setLostSprites();
                break;
            }
        }
    }
    
    // render bg
    if (renderBackgroundEnabled()) {
        uint32_t patternTableAddr = getBgPatternTableAddr();
        
        // render background
        for (uint32_t i = 0; i < renderWidth; i+=8) {
            uint16_t tileAddr = getTileAddr(vramCurrentAddr);
            uint16_t attrAddr = getAttrAddr(vramCurrentAddr);
            uint16_t tile = loadNameTile(load(tileAddr) * 16 + patternTableAddr) + getFineY(vramCurrentAddr);
            printf("%x ", (tile));
            uint8_t attr = load(attrAddr);
            uint32_t y = scanline;
            uint32_t tileOffsetY = (y % 32) / 16;

            for (uint32_t j = 0; j < 8; j++) {
                uint32_t x = i + j;
                uint32_t color = (tile >> ((7 - j) * 2)) & 0x3;
                uint32_t tileOffsetX = (x % 32) / 16;
                uint32_t subNibbleAttr = tileOffsetX + tileOffsetY * 2;
                uint32_t palette = (attr >> (2 * subNibbleAttr)) & 0x3;
                bool transparent = (color == 0);

                uint32_t pixel = getColor(palette, color, false);

                if (!pixelWritten[i + j]) {
                    scanlineBuffer[i + j] = pixel;
                }
                else if (pixelBgInFront[i + j] and not transparent) {
                    scanlineBuffer[i + j] = pixel;
                }
                vramFineXScroll += 1;
            }
            vramCoarseXInc();
        }
        vramCoarseXInc();
        vramCoarseXInc();
        printf("\n");
    }
    
    // dump pixels to display
    preRender();
    for (uint32_t i = 0; i < renderWidth; i++) {
        setPixel(i, scanline, scanlineBuffer[i]);
    }
    postRender();
}


void Ppu::tick()
{
    uint32_t scanline = getScanline();
    uint32_t lineClock = getScanlineOffset();
    bool isVblank = scanline >= 240 and scanline <= 260;
    
    /*
    if (scanline < 240 and lineClock == 0) {
        render(scanline);
    }
    */

    if (!isVblank) {
        // Increment vertical position in vramCurrentAddr
        if (lineClock == 255 and scanline < 240) {
            renderv2(scanline);
        }
        if (lineClock == 256) {
            vramYInc();
        }
        if (lineClock == 257) {
            vramXReset();
        }
        /*
        if ((lineClock >= 328) and (lineClock % 8 == 0)) {
            coarseXInc();
        }
        */
        if (scanline == 261 and lineClock >= 280 and lineClock <= 304) {
            vramYReset();
            printf("\n");
        }
    }
    
    // set and unset vblank register
    if (scanline == vblankScanline and lineClock == 0) {        
        sdl->renderSync();
        uint32_t currentTime = timerGetMs();
        if (currentTime - lastFrameTimeMs < frameTimeMs) {
            sleepMs(frameTimeMs - (currentTime - lastFrameTimeMs));
        }
        lastFrameTimeMs = timerGetMs();
        
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
    cycle += 1;
}

void Ppu::preRender()
{
    sdl->preRender();
}

void Ppu::postRender()
{
    sdl->postRender();
}

void Ppu::setPixel(int x, int y, uint32_t color)
{
    uint8_t r = 0xff & (color >> 16);
    uint8_t g = 0xff & (color >> 8);
    uint8_t b = 0xff & (color);
    sdl->setPixel(x, y, r, g, b);
}
