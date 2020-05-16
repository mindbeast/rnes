//
//  ppu.h
//  rnes
//
//

#ifndef __PPU_H__
#define __PPU_H__

#include <cstdint>

namespace Rnes {

class Nes;
class Sdl;
class PpuState;

class Ppu {
public:
  static const bool debug = false;
  static const uint32_t ticksPerScanline = 341;
  static const uint32_t totalScanlines = 262;

  // bits in control register
  enum Control1Reg {
    CONTROL_VRAM_ADDR_INC = 1 << 2,
    CONTROL_PATTERN_TABLE_ADDR_SPR = 1 << 3,
    CONTROL_PATTERN_TABLE_ADDR_SCR = 1 << 4,
    CONTROL_SPRITE_SIZE = 1 << 5,
    CONTROL_MASTER_SLAVE = 1 << 6,
    CONTROL_NMI_ON_VBLANK = 1 << 7,
  };

  // bits in second control register
  enum Control2Reg {
    CONTROL2_SPRITE_VISIBLE = 1 << 4,
    CONTROL2_BKGD_VISIBLE = 1 << 3,
    CONTROL2_SPRITE_CLIPPING = 1 << 2,
    CONTROL2_BKGD_CLIPPING = 1 << 1,
    CONTROL2_MONOCHROME_MODE = 1 << 0
  };

  // bits in status register
  enum StatusReg {
    STATUS_LOST_SPRITES = 1 << 5,
    STATUS_SPRITE0_HIT = 1 << 6,
    STATUS_VBLANK_HIT = 1 << 7
  };

  // ppu registers
  enum {
    CONTROL1_REG = 0,
    CONTROL2_REG = 1,
    STATUS_REG = 2,
    SPR_ADDR_REG = 3,
    SPR_DATA_REG = 4,
    VRAM_ADDR_REG1 = 5,
    VRAM_ADDR_REG2 = 6,
    VRAM_DATA_REG = 7,
    REG_COUNT = 8,
  };

private:
  uint32_t getBgColor();
  uint32_t getColor(uint32_t palette, uint32_t color, bool sprite);

  uint16_t getScanline() const { return cycle / ticksPerScanline % totalScanlines; }
  uint16_t getScanlineOffset() const { return cycle % ticksPerScanline; }
  uint16_t getSpritePatternTableAddr() const {
    return (regs[CONTROL1_REG] & CONTROL_PATTERN_TABLE_ADDR_SPR) ? 0x1000 : 0x0;
  }
  uint16_t getBgPatternTableAddr() const {
    return (regs[CONTROL1_REG] & CONTROL_PATTERN_TABLE_ADDR_SCR) ? 0x1000 : 0x0;
  }
  bool nmiOnVblank() const { return (regs[CONTROL1_REG] & CONTROL_NMI_ON_VBLANK) != 0; }
  bool isSpriteSize8x8() const { return (regs[CONTROL1_REG] & CONTROL_SPRITE_SIZE) == 0; }
  bool isMonochromeMode() const { return (regs[CONTROL2_REG] & CONTROL2_MONOCHROME_MODE) != 0; }
  bool renderBackgroundEnabled() const { return (regs[CONTROL2_REG] & CONTROL2_BKGD_VISIBLE) != 0; }
  bool renderSpritesEnabled() const { return (regs[CONTROL2_REG] & CONTROL2_SPRITE_VISIBLE) != 0; }
  void setSprite0Hit() { regs[STATUS_REG] |= STATUS_SPRITE0_HIT; }
  void clearSprite0Hit() { regs[STATUS_REG] &= ~STATUS_SPRITE0_HIT; }
  void setLostSprites() { regs[STATUS_REG] |= STATUS_LOST_SPRITES; }
  void clearLostSprites() { regs[STATUS_REG] &= ~STATUS_LOST_SPRITES; }
  void setVblankFlag() { regs[STATUS_REG] |= STATUS_VBLANK_HIT; }
  void clearVblankFlag() { regs[STATUS_REG] &= ~STATUS_VBLANK_HIT; }
  void vramCoarseXInc();
  void vramYInc();
  void vramXReset();
  void vramYReset();

  uint16_t getTileAddr(uint16_t vramCurrent) const { return 0x2000 | (vramCurrent & 0xfff); }
  uint32_t getFineY(uint16_t vramCurrent) const { return (0x7000 & vramCurrent) >> 12; }
  uint16_t getAttrAddr(uint16_t vramCurrent) const {
    return 0x23c0 | (vramCurrent & 0xc00) | ((vramCurrent >> 4) & 0x38) |
           ((vramCurrent >> 2) & 0x7);
  }
  uint16_t loadPatternTile(uint16_t addr);

  template <bool is8x8>
  uint8_t getColorFromPatternTable(uint16_t patternTable, int offset, uint32_t x, uint32_t y);
  void render(uint32_t scanline);
  void tick();
  uint16_t getVramAddrInc() const;
  uint8_t load(uint16_t addr);
  void store(uint16_t addr, uint8_t val);
  void setPixel(int x, int y, uint32_t color);

public:
  void run(uint32_t cpuCycle);
  bool isRequestingNmi();
  void writeReg(uint32_t reg, uint8_t val);
  uint8_t readReg(uint32_t reg);

  void save(PpuState &pb);
  void restore(const PpuState &pb);

  Ppu(Nes *parent, Sdl *disp);
  Ppu() = delete;
  Ppu(const Ppu &) = delete;
  ~Ppu() {}

private:
  bool nmiRequested = false;
  Nes *nes;
  Sdl *sdl;
  uint64_t cycle = 0;
  uint64_t frame = 0;
  uint8_t regs[REG_COUNT] = {0};
  enum SpriteAttr {
    ATTR_VERT_FLIP = 1 << 7,
    ATTR_HORIZ_FLI = 1 << 6,
    ATTR_BG_PRIOR = 1 << 5,
    ATTR_COLOR_MASK = 3 << 0,
  };

  static const int spriteRamSize = 64;
  struct {
    uint8_t yCoordMinus1;
    uint8_t tileIndex;
    uint8_t attr;
    uint8_t xCoord;
  } spriteRam[spriteRamSize] = {{0}};

  uint32_t vramToggle = 0;
  uint32_t vramFineXScroll = 0;
  uint16_t vramCurrentAddr = 0;
  uint16_t vramTempAddr = 0;

  uint16_t vramMachineAddr = 0;
  uint8_t vramReadLatch = 0;

  uint32_t scrollingMachineState = 0;
  uint8_t xScrollOrigin = 0;
  uint8_t yScrollOrigin = 0;

  float lastFrameTimeMs = 0.0f;

  bool pixelWritten[256];
  uint32_t scanlineBuffer[256];
};

}; // namespace Rnes

#endif
