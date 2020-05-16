//
//  cpu.h
//  rnes
//
//

#ifndef __CPU_H__
#define __CPU_H__

#include <cstdint>
#include <unordered_map>
#include <functional>

namespace Rnes {

class Nes;
class CpuState;

struct OpcodeHash {
    size_t operator()(uint8_t opcode) const {
        return opcode;
    }
};

class Cpu {
private:
    // system object
    Nes *nes;
    
    // 6502 registers
    uint8_t a, x, y;
    
    // Program counter
    uint16_t pc;
    
    // stack pointer
    uint8_t sp;
    
    // cycle count ??
    uint64_t cycle;
    
    // processor status register
    uint8_t status;
    
    // Turn on debug prints
    static const bool debug = false;

    enum Flag {
        CARRY       = 1 << 0, // set when accumlator rolls over from 0xff -> 0x00
        ZERO        = 1 << 1, // set when the result of any operation is 0x00
        INT_DISABLE = 1 << 2, // processor ignores interrupts when set
        DECIMAL     = 1 << 3, // causes arithmetic to be in BCD
        BREAK       = 1 << 4, // set whenever a BRK instruction is executed
        ONE         = 1 << 5, // always one
        OVERFL      = 1 << 6, // ???
        NEGATIVE    = 1 << 7, // set when the result of an operation is negative
    };
    
    // Helper functions....
    uint8_t load(uint16_t addr);
    uint16_t load16(uint16_t addr);
    void store(uint16_t addr, uint8_t val);

    void setFlag(Flag f);
    void clearFlag(Flag f);
    bool getFlag(Flag f);
    void setZeroAndNeg(uint8_t val);
    void pushStack(uint8_t v);
    uint8_t popStack();
    void relativeBranchPenalty(uint16_t start, uint16_t end);

    // Are other parts of the system requesting interrupts.
    bool intRequested();
    bool nmiRequested();
    
    // Execute interrupt.
    uint32_t doInt();

    // Execute non-maskable interrupt.
    uint32_t doNmi();
    
    // instructions
    // adds value from memory/immediate to a
    void adcInst(uint16_t addr);
    void aslInstMem(uint16_t addr);
    void aslInstReg(uint16_t addr);

    // branch if carry clear/set. returns bool if branch taken to stop advance of pc.
    void bccInst(uint16_t addr);
    void bcsInst(uint16_t addr);
    void beqInst(uint16_t addr);
    void bneInst(uint16_t addr);
    void bitInst(uint16_t addr);
    void bmiInst(uint16_t addr);
    void bplInst(uint16_t addr);
    void brkInst(uint16_t);

    // branch if OVERFL clear
    void bvcInst(uint16_t addr);

    // branch if OVERFL set
    void bvsInst(uint16_t addr);

    // clear carry flag
    void clcInst(uint16_t);

    // clear decimal mode
    void cldInst(uint16_t);

    // clear interrupt disable flag
    void cliInst(uint16_t);

    // clear OVERFL flag
    void clvInst(uint16_t);

    // helper
    void cmp(uint16_t addr, uint8_t val);

    // compare accumulator to memory
    void cmpInst(uint16_t addr);

    // compare x to memory
    void cpxInst(uint16_t addr);

    // compare y to memory
    void cpyInst(uint16_t addr);
    
    // decrement memory helper
    void incdecmem(uint16_t addr, const std::function<uint8_t(uint8_t)>& f);
    void decInst(uint16_t addr);
    void incInst(uint16_t addr);

    // helper
    void incdec(uint8_t &reg, const std::function<uint8_t(uint8_t)>& f);
    
    // decrement x register
    void dexInst(uint16_t);
    void deyInst(uint16_t);
    void inxInst(uint16_t);
    void inyInst(uint16_t);
    
    // helper
    void accumAndMemOp(uint16_t addr, const std::function<uint8_t(uint8_t, uint8_t)> &f);
    
    // ands value from memory/immediate to a
    void andInst(uint16_t addr);

    // exclusive ors value from memory/immediate
    void eorInst(uint16_t addr);
    
    // jmp to program counter
    void jmpInst(uint16_t addr);

    // jmp to subroutine and push return addr
    void jsrInst(uint16_t addr);
    
    // return from subroutine
    void rtsInst(uint16_t);
    
    void ldaInst(uint16_t addr);
    void ldxInst(uint16_t addr);
    void ldyInst(uint16_t addr);
    void lsrInstMem(uint16_t addr);
    void lsrInstReg(uint16_t);
    
    // nop
    void nopInst(uint16_t);
    
    // Or value with memory in accumulator
    void oraInst(uint16_t addr);
    
    void phaInst(uint16_t);
    void phpInst(uint16_t);
    void plaInst(uint16_t);
    void plpInst(uint16_t);
    void rolInstReg(uint16_t);
    void rolInstMem(uint16_t addr);
    void rorInstReg(uint16_t);
    void rorInstMem(uint16_t addr);
    
    void rtiInst(uint16_t);
    void sbcInst(uint16_t addr);

    // undocumented instruction for zelda - no idea if this is correct.
    void dcpInst(uint16_t addr);

    // set carry flag
    void secInst(uint16_t);

    // set bcd mode
    void sedInst(uint16_t);

    // set interrupt disable
    void seiInst(uint16_t);
    
    // store accumulator to memory
    void staInst(uint16_t addr);

    // store x to memory
    void stxInst(uint16_t addr);

    // store y to memory
    void styInst(uint16_t addr);
    
    // move accum to x
    void taxInst(uint16_t);

    // move accum to y
    void tayInst(uint16_t);

    // move sp to x
    void tsxInst(uint16_t);

    // move x to accum
    void txaInst(uint16_t);

    // move x to sp
    void txsInst(uint16_t);

    // move y to accum
    void tyaInst(uint16_t);
    
    // addressing mode dispatch functions
    typedef void (Cpu::*InstFunc)(uint16_t);

    // instruction dispatch table
    struct Instruction;
    typedef void (Cpu::*InstDispatchFunc)(const Instruction&);
    
    struct Instruction {
        std::string nemonic;
        uint16_t cycles;
        InstDispatchFunc func;
    };
    
    // Templatized implementations of all instruction formats.
    template <InstFunc fx> void impliedFormat(const Instruction &ins);
    template <InstFunc fx> void accumFormat(const Instruction &ins);
    template <InstFunc fx> void immediateFormat(const Instruction &ins);
    template <InstFunc fx> void zeroPageFormat(const Instruction &ins);
    template <InstFunc fx> void zeroPageXFormat(const Instruction &ins);
    template <InstFunc fx> void zeroPageYFormat(const Instruction &ins);
    template <InstFunc fx> void relativeFormat(const Instruction &ins);
    template <InstFunc fx> void absoluteFormat(const Instruction &ins);
    template <InstFunc fx> void absoluteXFormat(const Instruction &ins);
    template <InstFunc fx> void absoluteYFormat(const Instruction &ins);
    template <InstFunc fx> void indirectFormat(const Instruction &ins);
    template <InstFunc fx> void indexedIndirectFormat(const Instruction &ins);
    template <InstFunc fx> void indirectIndexedFormat(const Instruction &ins);

    // Large look-up table associating opcode to instruction implementation.
    static const std::unordered_map<uint8_t, Cpu::Instruction, OpcodeHash> instTable;

    // Instruction trace logic.
    void dumpRegs();
    void dumpPc();
    void dumpInstructionBytes(int bytes);
    void dumpPPUTiming();
    void instTrace(const std::string& str, int bytes);
    
public:
    // Run a single 6502 instruction, return the number of cycles.
    uint32_t runInst();
    void reset();

    // Save/Restore from protobuf 
    void save(CpuState& pb);
    void restore(const CpuState& pb);
    
    Cpu(Nes *system);
    ~Cpu();
};

};

#endif 
