#include <cassert>
#include <iostream>

#include "mmc.h"
#include "memory.h"
#include "save.pb.h"

namespace Rnes {

static const uint16_t mmcCpuAddrBase = 0x6000;
static const uint16_t mmcCpuAddrSize = 0xa000;

static const uint16_t mmcVidAddrBase = 0x0;
static const uint16_t mmcVidAddrSize = 0x2000;

static const uint16_t nameTable0 = 0x2000;
static const uint16_t nameTable1 = 0x2400;
static const uint16_t nameTable2 = 0x2800;
static const uint16_t nameTable3 = 0x2c00;
static const uint16_t nameTableSize = 0x400;

//
// Shared MMC logic.
//

static uint16_t translateHorizMirror(uint16_t addr)
{
    if (addr >= nameTable1 && addr < (nameTable2)) {
        addr = (addr & (nameTableSize - 1)) + nameTable0;
    }
    else if (addr >= nameTable3 && addr < (nameTable3 + nameTableSize)) {
        addr = (addr & (nameTableSize - 1)) + nameTable2;
    }
    return addr;
}

static uint16_t translateVerticalMirror(uint16_t addr)
{
    if (addr >= nameTable3 && addr < (nameTable3 + nameTableSize)) {
        addr = (addr & (nameTableSize - 1)) + nameTable1;
    }
    else if (addr >= nameTable2 && addr < nameTable3) {
        addr = (addr & (nameTableSize - 1)) + nameTable0;
    }
    return addr;
}

static uint16_t translateSingleMirror(uint16_t addr)
{
    if (addr >= nameTable0 and addr < nameTable3 + nameTableSize) {
        addr = (addr & (nameTableSize - 1)) + nameTable0; 
    }
    return addr;
}

//
// No MMC logic.
//

MmcNone::MmcNone(const std::vector<uint8_t*> &prgRoms,
                 const std::vector<uint8_t*> &chrRoms,
                 uint32_t prgRam,
                 bool vertMirror) :
    progRoms{prgRoms},
    charRoms{chrRoms},
    numPrgRam{prgRam},
    verticalMirror{vertMirror}
{
    assert(numPrgRam == 0 || numPrgRam == 1);
    assert(prgRoms.size() <= 2);
    assert(chrRoms.size() <= 1);
}

MmcNone::~MmcNone()
{}

void MmcNone::cpuMemWrite(uint16_t addr, uint8_t val)
{
    assert(addr >= mmcCpuAddrBase);
}

uint8_t MmcNone::cpuMemRead(uint16_t addr)
{
    assert(addr >= mmcCpuAddrBase);
    // prg rom 1
    if ((addr >= 0x8000) and
        (addr < 0x8000 + 0x4000) and
        (progRoms.size() == 2)) {
        return *(progRoms[0] + addr - 0x8000);
    }
    // prg rom 2
    if ((addr >= 0xc000) and
        (progRoms.size() >= 1)) {
        if (progRoms.size() == 1) {
            return *(progRoms[0] + addr - 0xc000);
        }
        else {
            return *(progRoms[1] + addr - 0xc000);
        }
    }
    return 0;
}

uint16_t MmcNone::vidAddrTranslate(uint16_t addr) 
{
    if (verticalMirror) {
        return translateVerticalMirror(addr);
    }
    else {
        return translateHorizMirror(addr);
    }
}

void MmcNone::vidMemWrite(uint16_t addr, uint8_t val)
{
}

uint8_t MmcNone::vidMemRead(uint16_t addr)
{
    if ((charRoms.size() == 1) and (addr < 0x2000)) {
        return *(charRoms[0] + addr);
    }
    return 0;
}

void MmcNone::save(MmcState &pb) {}

void MmcNone::restore(MmcState &pb) {}

//
// MMC1 logic
//

Mmc1::Mmc1(const std::vector<uint8_t*> &prgRoms,
           const std::vector<uint8_t*> &chrRoms,
           uint32_t prgRam,
           bool vertMirror,
           CpuMemory *cpuMemoryRef,
           VideoMemory *videoMemoryRef) :
    progRoms{prgRoms},
    charRoms{chrRoms},
    numPrgRam{prgRam},
    cpuMemory{cpuMemoryRef},
    videoMemory{videoMemoryRef}
{
    assert(numPrgRam == 0 || numPrgRam == 1);
}

Mmc1::~Mmc1() {}

void Mmc1::updateMmcRegister(uint16_t addr, uint8_t shiftRegister)
{
    switch ((addr >> 13) & 0x7) {
        case 4:
            controlReg = shiftRegister;
            if (debug) {
                std::cerr << "mmc1 " << std::hex << addr << ": control reg: " << std::hex << (int)controlReg << std::endl;
            }
            break;
        case 5:
            chr0Bank = shiftRegister;
            if (debug) {
                std::cerr << "mmc1 " << std::hex << addr << ": chr0Bank reg: " << std::hex << (int)chr0Bank << std::endl;
            }
            break;
        case 6:
            chr1Bank = shiftRegister;
            if (debug) {
                std::cerr << "mmc1 " << std::hex << addr << ": chr1Bank reg: " << std::hex << (int)chr1Bank << std::endl;
            }
            break;
        case 7:
            prgBank = shiftRegister;
            if (debug) {
                std::cerr << "mmc1 " << std::hex << addr << ": prgBank reg: " << std::hex << (int)prgBank << std::endl;
            }
            break;
        default:
            break;
    }
    
}

void Mmc1::cpuMemWrite(uint16_t addr, uint8_t val)
{
    // shift register writes
    if ((addr >= shiftWriteAddr) and (addr <= shiftWriteAddrLimit)) {
        // initialize shift register when 7th bit is set.
        if (val & (1 << 7)) {
            shiftRegister = shiftInit;
            controlReg |= 0x3 << 2;
            if (debug) {
                std::cerr << "mmc1: control reg reset: " << std::hex << (int)controlReg << std::endl;
            }
        }
        else {
            uint8_t oldShiftRegister = shiftRegister;
            shiftRegister >>= 1;    
            shiftRegister |= (val & 0x1) << 4;
            // lowest bit is set on last write.
            if (oldShiftRegister & 0x1) {
                updateMmcRegister(addr, shiftRegister);
                shiftRegister = shiftInit;
            }
        }
    }
}

uint8_t Mmc1::cpuMemRead(uint16_t addr)
{
    assert(addr >= mmcCpuAddrBase);
    // prg rom @ 0x8000
    uint32_t prgRomMode = getPrgRomMode();
    uint8_t bank = prgBank & 0xf;
    if ((addr >= 0x8000) and (addr < 0x8000 + 0x4000)) {
        switch (prgRomMode) {
            case 0:
            case 1:
                return *(progRoms[bank & ~0x1] + addr - 0x8000);
                break;
            case 2:
                return *(progRoms[0] + addr - 0x8000);
                break;
            case 3:
                return *(progRoms[bank] + addr - 0x8000);
                break;
            default:
                assert(0);
                break;
        }
    }
    // prg rom @ 0xc000
    if (addr >= 0xc000) {
        switch (prgRomMode) {
            case 0:
            case 1:
                return *(progRoms[(bank & ~0x1) + 1] + addr - 0xc000);
                break;
            case 2:
                return *(progRoms[bank] + addr - 0xc000);
                break;
            case 3:
                return *(progRoms[progRoms.size()-1] + addr - 0xc000);
                break;
            default:
                assert(0);
                break;
        }
    }
    return 0;
}

void Mmc1::vidMemWrite(uint16_t addr, uint8_t val)
{
    addr = vidAddrTranslate(addr);
    bool chr8kMode = getChrRomMode() == 0;
    if ((charRoms.size() == 0) and (addr < 0x1000)) {
        if (chr8kMode) {
            videoMemory->patternTableMemory[addr] = val;
        }
        else {
            videoMemory->patternTableMemory[addr + ((chr0Bank & 0x1) ? 0x1000 : 0)] = val;
        }
    }
    else if ((charRoms.size() == 0) and (addr >= 0x1000) and (addr <= 0x1fff)) {
        if (chr8kMode) {
            videoMemory->patternTableMemory[addr] = val;
        }
        else {
            videoMemory->patternTableMemory[addr - 0x1000 + ((chr1Bank & 0x1) ? 0x1000 : 0)] = val;
        }

    }
}

uint8_t Mmc1::vidMemRead(uint16_t addr)
{
    bool chr8kMode = getChrRomMode() == 0;
    if (charRoms.size() > 0) {
        // chr rom @ 0x0
        if (addr < 0x1000) {
            if (chr8kMode) {
                return *(charRoms[chr0Bank >> 1] + addr);
            }
            else {
                return *(charRoms[chr0Bank >> 1] + addr + ((chr0Bank & 0x1) ? 0x1000 : 0));
            }
        }
        // chr rom @ 0x1000
        if ((addr >= 0x1000) and (addr <= 0x1fff)) {
            if (chr8kMode) {
                return *(charRoms[chr0Bank >> 1] + addr);
            }
            else {
                return *(charRoms[chr1Bank >> 1] + addr + ((chr1Bank & 0x1) ? 0x1000 : 0) - 0x1000);
            }
        }
    } 
    else {
        if (chr8kMode) {
            if (addr < 0x2000) {
                return videoMemory->patternTableMemory[addr];
            }
        }
        else {
            if (addr < 0x1000) {
                return videoMemory->patternTableMemory[addr + (chr0Bank & 0x1) ? 0x1000 : 0];
            }
            if ((addr >= 0x1000) and (addr <= 0x1fff)) {
                return videoMemory->patternTableMemory[addr - 0x1000 + (chr1Bank & 0x1) ? 0x1000 : 0];
            }
        }
    }

    return 0;
}

uint16_t Mmc1::vidAddrTranslate(uint16_t addr) 
{
    uint32_t mirrorMode = getMirroringMode();
    switch (mirrorMode) {
        case 0: return translateSingleMirror(addr);
        case 1: return translateSingleMirror(addr) + nameTableSize;
        case 2: return translateVerticalMirror(addr);
        case 3: return translateHorizMirror(addr);
        default: assert(0); 
    }
    return 0; 
}

void Mmc1::save(MmcState &pb)
{
    MmcState_Mmc1State *mmc1 = pb.mutable_mmc1();
    mmc1->set_controlreg(controlReg);
    mmc1->set_chr0bank(chr0Bank);
    mmc1->set_chr1bank(chr1Bank);
    mmc1->set_prgbank(prgBank);
    mmc1->set_shiftregister(shiftRegister);
}

void Mmc1::restore(MmcState &pb)
{
    const MmcState_Mmc1State &mmc1 = pb.mmc1();
    controlReg = mmc1.controlreg();
    chr0Bank = mmc1.chr0bank();
    chr1Bank = mmc1.chr1bank();
    prgBank = mmc1.prgbank();
    shiftRegister = mmc1.shiftregister();
}

//
// MMC3 logic
//

Mmc3::Mmc3(const std::vector<uint8_t*> &prgRoms,
           const std::vector<uint8_t*> &chrRoms,
           uint32_t prgRam,
           bool vertMirror, 
           CpuMemory *cpuMemoryRef,
           VideoMemory *videoMemoryRef) :
    progRoms{prgRoms},
    charRoms{chrRoms},
    numPrgRam{prgRam},
    cpuMemory{cpuMemoryRef},
    videoMemory{videoMemoryRef}
{
    assert(numPrgRam == 0 || numPrgRam == 1);
}

Mmc3::~Mmc3() {}

void Mmc3::updateBankRegister(uint8_t val)
{
    uint8_t bankSelect = getBankSelect();
    bankRegister[bankSelect] = val;
    if (debug) {
        std::cerr << "bank " << std::hex << (int)bankSelect << ": " << (int)val << std::endl;
    }
}

uint8_t *Mmc3::get1kChrBank(uint32_t bank) {
    if (charRoms.size() == 0) {
        return &videoMemory->patternTableMemory[bank * 1024];
    }
    else {
        return charRoms[bank >> 3] + (bank & 0x7) * 1024;
    }
}

uint16_t Mmc3::vidAddrTranslate(uint16_t addr) 
{
    if (isHorizMirroring()) {
        return translateHorizMirror(addr);
    }
    else {
        return translateVerticalMirror(addr);
    }
}

void Mmc3::cpuMemWrite(uint16_t addr, uint8_t val)
{
    assert(addr >= mmcCpuAddrBase);

    // mmc control register writes
    if (addr >= 0x8000 and addr <= 0x9fff) {
        if (addr & 0x1) {
            updateBankRegister(val); 
        }
        else {
            bankSelectReg = val;
        }          
    }
    else if (addr >= 0xa000 and addr <= 0xbfff) {
        if (addr & 0x1) {
            prgRamReg = val;
        }
        else {
            mirrorReg = val;
        }          
    }
    else if (addr >= 0xc000 and addr <= 0xdfff) {
        if (addr & 0x1) {
            irqCounterReg = 0; 
        }
        else {
            irqReloadReg = val;
        }          
    }
    else if (addr >= 0xe000 and addr <= 0xffff) {
        if (addr & 0x1) {
            irqEnabled = true; 
        }
        else {
            irqEnabled = false;
            irqPending = false;
        }          
    }
}

uint8_t Mmc3::cpuMemRead(uint16_t addr)
{
    assert(addr >= mmcCpuAddrBase);

    // prg rom @ 0x8000
    if ((addr >= 0x8000) and (addr < 0x8000 + 0x2000)) {
        uint8_t bank;
        if (isLowerPrgRomSwappable())
            bank = bankRegister[6];
        else
            bank = get8kPrgBankCount() - 2;
        return *(get8kPrgBank(bank) + addr - 0x8000);
    }
    // prg rom @ 0xa000
    if ((addr >= 0xa000) and (addr < 0xa000 + 0x2000)) {
        uint8_t bank = bankRegister[7];
        return *(get8kPrgBank(bank) + addr - 0xa000);
    }

    // prg rom @ 0xc000
    if ((addr >= 0xc000) and (addr <= 0xdfff)) {
        uint8_t bank;
        if (isLowerPrgRomSwappable())
            bank = get8kPrgBankCount() - 2;
        else
            bank = bankRegister[6];
        return *(get8kPrgBank(bank) + addr - 0xc000);
    }
    // prg rom @ 0xe000
    if ((addr >= 0xe000) and (addr <= 0xffff)) {
        return *(get8kPrgBank(get8kPrgBankCount()-1) + addr - 0xe000);
    }
    return 0;
}

uint8_t *Mmc3::getChrPointer(uint16_t addr)
{
    bool a12Invert = isChrA12Inverted(); 
    assert(addr <= 0x1fff);
    
    if (a12Invert) {
        addr ^= (1 << 12);
    }
    if (addr <= 0x7ff) {
        return get2kChrBank(bankRegister[0]) + addr;
    }
    else if (addr >= 0x800 and addr <= 0xfff) {
        return get2kChrBank(bankRegister[1]) + addr - 0x800;
    }
    else if (addr >= 0x1000 and addr <= 0x13ff) {
        return get1kChrBank(bankRegister[2]) + addr - 0x1000;
    }
    else if (addr >= 0x1400 and addr <= 0x17ff) {
        return get1kChrBank(bankRegister[3]) + addr - 0x1400;
    }
    else if (addr >= 0x1800 and addr <= 0x1bff) {
        return get1kChrBank(bankRegister[4]) + addr - 0x1800;
    }
    else if (addr >= 0x1c00 and addr <= 0x1fff) {
        return get1kChrBank(bankRegister[5]) + addr - 0x1c00;
    }
    return 0;
}

void Mmc3::vidMemWrite(uint16_t addr, uint8_t val)
{
    // chr banks
    if (charRoms.size() == 0 and addr <= 0x1fff) {
        *getChrPointer(addr) = val;
    }
}

uint8_t Mmc3::vidMemRead(uint16_t addr)
{
    // chr banks
    if (addr <= 0x1fff) {
        return *getChrPointer(addr);
    }
    return 0;
}

void Mmc3::notifyScanlineComplete()
{
    if (irqCounterReg == 0) {
        irqCounterReg = irqReloadReg;
    }
    else {
        irqCounterReg--;
        if (irqCounterReg == 0 and irqEnabled) {
            irqPending = true; 
        } 
        
    }
}

bool Mmc3::isRequestingIrq()
{
    return irqEnabled and irqPending;
}

void Mmc3::save(MmcState &pb)
{
    /*
    // internal control registers
    uint8_t bankSelectReg = 1 << 6;
    uint8_t mirrorReg = 0;
    uint8_t prgRamReg = 0;
    uint8_t bankRegister[8] = {0};
    uint8_t irqReloadReg = 0;
    uint8_t irqCounterReg = 0;
    bool irqEnabled = false;
    bool irqPending = false;
    */

    MmcState_Mmc3State *mmc3 = pb.mutable_mmc3();
    mmc3->set_bankselectreg(bankSelectReg);
    mmc3->set_mirrorreg(mirrorReg);
    mmc3->set_prgramreg(prgRamReg);
    mmc3->set_bankregister(bankRegister, 8);
    mmc3->set_irqreloadreg(irqReloadReg);
    mmc3->set_irqcounterreg(irqCounterReg);
    mmc3->set_irqenabled(irqEnabled);
    mmc3->set_irqpending(irqPending);

}

void Mmc3::restore(MmcState &pb)
{
    const MmcState_Mmc3State &mmc3 = pb.mmc3();
    bankSelectReg = mmc3.bankselectreg();
    mirrorReg = mmc3.mirrorreg();
    prgRamReg = mmc3.prgramreg();
    const std::string &bytes = mmc3.bankregister();
    for (unsigned i = 0; i < bytes.size(); i++) {
        bankRegister[i] = bytes[i];
    }
    irqReloadReg = mmc3.irqreloadreg();
    irqCounterReg = mmc3.irqcounterreg();
    irqEnabled = mmc3.irqenabled();
    irqPending = mmc3.irqpending();
}

};
