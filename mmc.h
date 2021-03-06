//  rnes
//
//

#ifndef __MMC_H__
#define __MMC_H__

#include <cstdint>
#include <vector>

namespace Rnes {

static constexpr uint32_t cpuMemorySize = 1 << 16;
static constexpr uint32_t videoMemorySize = 1 << 14;

static constexpr uint16_t prgSramBase = 0x6000;
static constexpr uint16_t prgSramSize = 0x2000;

static constexpr uint16_t nameTablesBase = 0x2000;
static constexpr uint16_t nameTablesSize = 0x1000;

static constexpr uint16_t paletteBase = 0x3f00;
static constexpr uint16_t paletteSize = 0x20;

class VideoMemory;
class CpuMemory;
class MmcState;

class Mmc {
public:
  Mmc() {}
  virtual ~Mmc() {}
  virtual void cpuMemWrite(uint16_t addr, uint8_t val) = 0;
  virtual uint8_t cpuMemRead(uint16_t addr) = 0;
  virtual void vidMemWrite(uint16_t addr, uint8_t val) = 0;
  virtual uint8_t vidMemRead(uint16_t addr) = 0;
  virtual void notifyScanlineComplete() {}
  virtual bool isRequestingIrq() { return false; }
  virtual bool isPrgSramEnabled() const = 0;
  virtual bool isPrgSramWriteable() const { return isPrgSramEnabled(); }
  virtual uint16_t vidAddrTranslate(uint16_t addr) = 0;
  virtual void save(MmcState &pb) = 0;
  virtual void restore(MmcState &pb) = 0;

protected:
  static const bool debug = false;
};

class MmcNone : public Mmc {
  std::vector<uint8_t *> progRoms;
  std::vector<uint8_t *> charRoms;
  uint32_t numPrgRam;
  bool verticalMirror;

public:
  bool isPrgSramEnabled() const { return numPrgRam == 1; }
  MmcNone() = delete;
  MmcNone(const std::vector<uint8_t *> &prgRoms, const std::vector<uint8_t *> &chrRoms,
          uint32_t prgRam, bool verticalMirror);
  ~MmcNone();
  void cpuMemWrite(uint16_t addr, uint8_t val);
  uint8_t cpuMemRead(uint16_t addr);
  void vidMemWrite(uint16_t addr, uint8_t val);
  uint8_t vidMemRead(uint16_t addr);
  uint16_t vidAddrTranslate(uint16_t addr);
  void save(MmcState &pb);
  void restore(MmcState &pb);
};

class Mmc1 : public Mmc {
  std::vector<uint8_t *> progRoms;
  std::vector<uint8_t *> charRoms;
  uint32_t numPrgRam;
  CpuMemory *cpuMemory;
  VideoMemory *videoMemory;

  // mmc1 internal registers
  uint8_t controlReg = 0x1c;
  uint8_t chr0Bank = 0;
  uint8_t chr1Bank = 0;
  uint8_t prgBank = 0;
  uint8_t shiftRegister = 0;

  static const uint16_t shiftWriteAddr = 0x8000;
  static const uint16_t shiftWriteAddrLimit = 0xffff;
  static const uint8_t shiftInit = 1 << 4;

  void updateMmcRegister(uint16_t addr, uint8_t shiftRegister);
  uint32_t getPrgRomMode() const { return (controlReg >> 2) & 0x3; }
  uint32_t getMirroringMode() const { return controlReg & 0x3; }
  uint32_t getChrRomMode() const { return (controlReg >> 4) & 0x1; }
  bool isPrgSramEnabled() const { return (prgBank & (1 << 4)) == 0; }

public:
  Mmc1() = delete;
  Mmc1(const std::vector<uint8_t *> &prgRoms, const std::vector<uint8_t *> &chrRoms,
       uint32_t prgRam, bool verticalMirror, CpuMemory *cpuMemoryRef, VideoMemory *videoMemoryRef);
  ~Mmc1();
  void cpuMemWrite(uint16_t addr, uint8_t val);
  uint8_t cpuMemRead(uint16_t addr);
  void vidMemWrite(uint16_t addr, uint8_t val);
  uint8_t vidMemRead(uint16_t addr);
  uint16_t vidAddrTranslate(uint16_t addr);
  void save(MmcState &pb);
  void restore(MmcState &pb);
};

class Mmc3 : public Mmc {
  std::vector<uint8_t *> progRoms;
  std::vector<uint8_t *> charRoms;
  uint32_t numPrgRam;
  CpuMemory *cpuMemory;
  VideoMemory *videoMemory;

  // internal control registers
  uint8_t bankSelectReg = 1 << 6;
  uint8_t mirrorReg = 0;
  uint8_t prgRamReg = 0;
  static const uint32_t bankRegisterCount = 8;
  uint8_t bankRegister[bankRegisterCount] = {0};
  uint8_t irqReloadReg = 0;
  uint8_t irqCounterReg = 0;
  bool irqEnabled = false;
  bool irqPending = false;

  bool isPrgSramEnabled() const { return (prgRamReg & (1 << 7)) != 0; }
  bool isPrgSramWriteable() const { return ((prgRamReg & (1 << 6)) == 0) and isPrgSramEnabled(); }
  bool isLowerPrgRomSwappable() const { return (bankSelectReg & (1 << 6)) == 0; }
  bool isChrA12Inverted() const { return (bankSelectReg & (1 << 7)) != 0; }
  bool isHorizMirroring() const { return (mirrorReg & 0x1) != 0; }
  uint8_t getBankSelect() const { return (bankSelectReg & 0x7); }
  uint8_t *get8kPrgBank(uint32_t bank) const {
    bank = bank & ~(1 << 6 | 1 << 7);
    return progRoms[bank >> 1] + ((bank & 1) ? 8192 : 0);
  }
  uint8_t *get2kChrBank(uint32_t bank) { return get1kChrBank(bank & ~0x1); }
  uint8_t *get1kChrBank(uint32_t bank);
  uint32_t get8kPrgBankCount() const { return progRoms.size() * 2; }
  uint32_t get2kChrBankCount() const { return charRoms.size() * 4; }
  uint32_t get1kChrBankCount() const { return charRoms.size() * 8; }
  uint16_t vidAddrTranslate(uint16_t addr);
  void updateBankRegister(uint8_t val);
  uint8_t *getChrPointer(uint16_t addr);

public:
  Mmc3() = delete;
  Mmc3(const std::vector<uint8_t *> &prgRoms, const std::vector<uint8_t *> &chrRoms,
       uint32_t prgRam, bool verticalMirror, CpuMemory *cpuMemoryRef, VideoMemory *videoMemoryRef);
  ~Mmc3();
  void cpuMemWrite(uint16_t addr, uint8_t val);
  uint8_t cpuMemRead(uint16_t addr);
  void vidMemWrite(uint16_t addr, uint8_t val);
  uint8_t vidMemRead(uint16_t addr);
  void notifyScanlineComplete();
  bool isRequestingIrq();
  void save(MmcState &pb);
  void restore(MmcState &pb);
};

}; // namespace Rnes

#endif
