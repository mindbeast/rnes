//
//  cpu.h
//  rnes
//
//  Created by Riley Andrews on 7/6/13.
//  Copyright (c) 2013 Riley Andrews. All rights reserved.
//

#ifndef __CPU_H__
#define __CPU_H__

#include <cstdint>
#include <unordered_map>
#include <string>
#include <iostream>
#include <sstream>
#include <iomanip>
#include <cassert>
#include <functional>

class Nes;

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

    // stack pointer base
    static const uint16_t base = 0x0100;
    static const uint8_t stackPointerStart = 0xfd;
    
    // nmi vector base
    static const uint16_t nmiBaseAddr = 0xfffa;
    
    // reset vector base
    static const uint16_t resetBaseAddr = 0xfffc;
    
    // irq vector base
    static const uint16_t irqBaseAddr = 0xfffe;
    
    // max instruction length
    static const int maxInstLength = 3;

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
    
    // helpers
    uint8_t load(uint16_t addr);
    uint16_t load16(uint16_t addr)
    {
        uint16_t result = load(addr);
        result |= (uint16_t)load(addr + 1) << 8;
        return result;
    }
    void store(uint16_t addr, uint8_t val);
    void setFlag(Flag f)
    {
        status |= (uint8_t)f;
    }
    void clearFlag(Flag f)
    {
        if (f != ONE) {
            status &= ~f;
        }
    }
    bool getFlag(Flag f)
    {
        return (f & status) != 0;
    }
    void cycleAdvance(uint64_t cycles);
    void setZeroAndNeg(uint8_t val)
    {
        if (val == 0) {
            setFlag(ZERO);
        }
        else {
            clearFlag(ZERO);
        }
        if (val & (1 << 7)) {
            setFlag(NEGATIVE);
        }
        else {
            clearFlag(NEGATIVE);
        }
    }
    
    void pushStack(uint8_t v) {
        store(sp + base, v);
        sp--;
    }
    uint8_t popStack() {
        sp++;
        uint8_t ret = load(sp + base);
        return ret;
    }
    
    bool isNeg(uint8_t val) {
        if (val & (1 << 7)) {
            return true;
        }
        else {
            return false;
        }
    }
    bool isPos(uint8_t val) {
        return !isNeg(val);
    }
    void relativeBranchPenalty(uint16_t start, uint16_t end)
    {
        // one cycle penalty if dst of branch is same page
        if ((start & 0xff00) == (end & 0xff00)) {
            cycle++;
        }
        // two if not
        else {
            cycle +=2;
        }
    }
    
    bool intRequested();
    bool nmiRequested();
    
    uint32_t doInt() {
        //std::cerr << "INTERRUPT" << std::endl;
        pushStack((uint8_t)(pc >> 8));
        pushStack((uint8_t)pc);
        pushStack(status);
        setFlag(INT_DISABLE);
        pc = load(irqBaseAddr) | ((uint16_t)load(irqBaseAddr + 1) << 8);
        cycle += 7;
        return 7;
    }
    
    uint32_t doNmi() {
        //std::cerr << "INTERRUPT" << std::endl;
        pushStack((uint8_t)(pc >> 8));
        pushStack((uint8_t)pc);
        pushStack(status);
        setFlag(INT_DISABLE);
        pc = load(nmiBaseAddr) | ((uint16_t)load(nmiBaseAddr + 1) << 8);
        cycle += 7;
        return 7;
    }
    
    // instructions
    // adds value from memory/immediate to a
    void adcInst(uint16_t addr)
    {
        uint8_t v = load(addr);
        uint16_t result = a + v + (getFlag(CARRY) ? 1 : 0);
        if (result & (1u << 8)) {
            setFlag(CARRY);
        }
        else {
            clearFlag(CARRY);
        }
        if (isNeg((uint8_t)result) && isPos(a) && isPos(v)) {
            setFlag(OVERFL);
        }
        else if (isPos((uint8_t)result) && isNeg((a) && isNeg(v))) {
            setFlag(OVERFL);
        }
        else {
            clearFlag(OVERFL);
        }
        a = (uint8_t)result;
        setZeroAndNeg(a);
    }
    void aslInstMem(uint16_t addr)
    {
        uint8_t src = load(addr);
        uint8_t result = src << 1;
        store(addr,result);
        setZeroAndNeg(result);
        if (src & (1 << 7)) {
            setFlag(CARRY);
        }
        else {
            clearFlag(CARRY);
        }
    }
    void aslInstReg(uint16_t addr)
    {
        if (a & (1 << 7)) {
            setFlag(CARRY);
        }
        else {
            clearFlag(CARRY);
        }
        
        a <<= 1;
        setZeroAndNeg(a);
    }
    // branch if carry clear/set. returns bool if branch taken to stop advance of pc.
    void bccInst(uint16_t addr) {
        if (!getFlag(CARRY)) {
            relativeBranchPenalty(pc, pc + addr);
            pc += addr;
        }
    }
    void bcsInst(uint16_t addr) {
        if (getFlag(CARRY)) {
            relativeBranchPenalty(pc, pc + addr);
            pc += addr;
        }
    }
    void beqInst(uint16_t addr) {
        if (getFlag(ZERO)) {
            relativeBranchPenalty(pc, pc + addr);
            pc += addr;
        }
    }
    void bneInst(uint16_t addr) {
        if (!getFlag(ZERO)) {
            relativeBranchPenalty(pc, pc + addr);
            pc += addr;
        }
    }
    void bitInst(uint16_t addr) {
        uint8_t v = load(addr);
        bool zero = (a & v) == 0;
        if (zero) {
            setFlag(ZERO);
        }
        else {
            clearFlag(ZERO);
        }
        if (v & (1 << 7)) {
            setFlag(NEGATIVE);
        }
        else {
            clearFlag(NEGATIVE);
        }
        if (v & (1 << 6)) {
            setFlag(OVERFL);
        }
        else {
            clearFlag(OVERFL);
        }
    }
    void bmiInst(uint16_t addr) {
        if (getFlag(NEGATIVE)) {
            relativeBranchPenalty(pc, pc + addr);
            pc += addr;
        }
    }
    void bplInst(uint16_t addr) {
        if (!getFlag(NEGATIVE)) {
            relativeBranchPenalty(pc, pc + addr);
            pc += addr;
        }
    }
    void brkInst(uint16_t) {
        pc++;
        pushStack((uint8_t)(pc >> 8));
        pushStack((uint8_t)pc);
        pushStack(status | BREAK);
        
        pc = load(irqBaseAddr) | ((uint16_t)load(irqBaseAddr + 1) << 8);
    }
    // branch if OVERFL clear
    void bvcInst(uint16_t addr) {
        if (!getFlag(OVERFL)) {
            relativeBranchPenalty(pc, pc + addr);
            pc += addr;
        }
    }
    // branch if OVERFL set
    void bvsInst(uint16_t addr) {
        if (getFlag(OVERFL)) {
            relativeBranchPenalty(pc, pc + addr);
            pc += addr;
        }
    }
    // clear carry flag
    void clcInst(uint16_t) {
        clearFlag(CARRY);
    }
    // clear decimal mode
    void cldInst(uint16_t) {
        clearFlag(DECIMAL);
    }
    // clear interrupt disable flag
    void cliInst(uint16_t) {
        clearFlag(INT_DISABLE);
    }
    // clear OVERFL flag
    void clvInst(uint16_t) {
        clearFlag(OVERFL);
    }
    // helper
    void cmp(uint16_t addr, uint8_t val) {
        uint8_t v = load(addr);
        uint8_t result;
        result = val - v;
        setZeroAndNeg(result);
        if (val >= v) {
            setFlag(CARRY);
        }
        else {
            clearFlag(CARRY);
        }
    }
    // compare accumulator to memory
    void cmpInst(uint16_t addr) {
        cmp(addr, a);
    }
    // compare x to memory
    void cpxInst(uint16_t addr) {
        cmp(addr, x);
    }
    // compare y to memory
    void cpyInst(uint16_t addr) {
        cmp(addr, y);
    }
    
    // decrement memory helper
    void incdecmem(uint16_t addr, const std::function<uint8_t(uint8_t)>& f) {
        uint8_t result = f(load(addr));
        setZeroAndNeg(result);
        store(addr, result);
    }
    void decInst(uint16_t addr) {
        incdecmem(addr, [](uint8_t v){ return v - 1; });
    }
    void incInst(uint16_t addr) {
        incdecmem(addr, [](uint8_t v){ return v + 1; });
    }
    
    // helper
    void incdec(uint8_t &reg, const std::function<uint8_t(uint8_t)>& f) {
        uint8_t result = f(reg);
        setZeroAndNeg(result);
        reg = result;
    }
    
    // decrement x register
    void dexInst(uint16_t) {
        incdec(x, [](uint8_t v){ return v - 1; });
    }
    void deyInst(uint16_t) {
        incdec(y, [](uint8_t v){ return v - 1; });
    }
    void inxInst(uint16_t) {
        incdec(x, [](uint8_t v){ return v + 1; });
    }
    void inyInst(uint16_t) {
        incdec(y, [](uint8_t v){ return v + 1; });
    }
    
    // helper
    void accumAndMemOp(uint16_t addr, const std::function<uint8_t(uint8_t, uint8_t)> &f) {
        a = f(a,load(addr));
        setZeroAndNeg(a);
    }
    
    // ands value from memory/immediate to a
    void andInst(uint16_t addr) {
        accumAndMemOp(addr, [](uint8_t a, uint8_t b) { return a & b; });
    }
    // exclusive ors value from memory/immediate
    void eorInst(uint16_t addr) {
        accumAndMemOp(addr, [](uint8_t a, uint8_t b) { return a ^ b; });
    }
    
    // jmp to program counter
    void jmpInst(uint16_t addr) {
        pc = addr;
    }
    // jmp to subroutine and push return addr
    void jsrInst(uint16_t addr) {
        uint16_t pcm = pc - 1;
        pushStack((uint8_t)(pcm >> 8));
        pushStack((uint8_t)pcm);
        pc = addr;
    }
    
    // return from subroutine
    void rtsInst(uint16_t) {
        uint16_t newpc;
        newpc = popStack();
        newpc |= (uint16_t)popStack() << 8;
        newpc++;
        pc = newpc;
    }
    
    void ldaInst(uint16_t addr) {
        a = load(addr);
        setZeroAndNeg(a);
    }
    void ldxInst(uint16_t addr) {
        x = load(addr);
        setZeroAndNeg(x);
    }
    void ldyInst(uint16_t addr) {
        y = load(addr);
        setZeroAndNeg(y);
    }
    void lsrInstMem(uint16_t addr) {
        uint8_t result = load(addr) >> 1;
        setZeroAndNeg(result);
        if (load(addr) & 1) {
            setFlag(CARRY);
        }
        else {
            clearFlag(CARRY);
        }
        store(addr, result);
    }
    void lsrInstReg(uint16_t) {
        uint8_t result = a >> 1;
        setZeroAndNeg(result);
        if (a & 1) {
            setFlag(CARRY);
        }
        else {
            clearFlag(CARRY);
        }
        a = result;
    }
    
    // nop
    void nopInst(uint16_t) {}
    
    // Or value with memory in accumulator
    void oraInst(uint16_t addr) {
        a |= load(addr);
        setZeroAndNeg(a);
    }
    
    void phaInst(uint16_t) {
        pushStack(a);
    }
    void phpInst(uint16_t) {
        pushStack(status | ONE);
    }
    void plaInst(uint16_t) {
        a = popStack();
        setZeroAndNeg(a);
    }
    void plpInst(uint16_t) {
        // BRK is never actually set in the flags register. WHAT?
        status = (popStack() & ~BREAK) | ONE;
    }
    void rolInstReg(uint16_t) {
        uint8_t result = a << 1 | (getFlag(CARRY) ? 1 : 0);
        if (a & (1 << 7)) {
            setFlag(CARRY);
        }
        else {
            clearFlag(CARRY);
        }
        a = result;
        setZeroAndNeg(a);
    }
    void rolInstMem(uint16_t addr) {
        uint8_t result = (load(addr) << 1) | (getFlag(CARRY) ? 1 : 0);
        if (load(addr) & (1 << 7)) {
            setFlag(CARRY);
        }
        else {
            clearFlag(CARRY);
        }
        store(addr, result);
        setZeroAndNeg(result);
    }
    void rorInstReg(uint16_t) {
        uint8_t result = (a >> 1) | (getFlag(CARRY) ? 1u << 7 : 0);
        if (a & 1) {
            setFlag(CARRY);
        }
        else {
            clearFlag(CARRY);
        }
        a = result;
        setZeroAndNeg(a);
    }
    void rorInstMem(uint16_t addr) {
        uint8_t result = (load(addr) >> 1) | (getFlag(CARRY) ? 1u << 7 : 0);
        if (load(addr) & 1) {
            setFlag(CARRY);
        }
        else {
            clearFlag(CARRY);
        }
        store(addr, result);
        setZeroAndNeg(result);
        
        /*
        int read = (int)load(0x4d);
        //std::cerr << std::hex << (int)((read & (1 << 7)) != 0) << "\n";
        std::cerr << std::hex << (int)(read) << "\n";
         */

    }
    
    void rtiInst(uint16_t) {
        uint16_t targetPc;
        status = (popStack() & ~BREAK) | ONE;
        
        targetPc = popStack();
        targetPc |= ((uint16_t)popStack() << 8);
        pc = targetPc;
    }
    
    void sbcInst(uint16_t addr) {
        uint8_t v = load(addr);
        int16_t sresult = (int16_t)(int8_t)a - (int16_t)(int8_t)v - (getFlag(CARRY) ? 0 : 1);
        int16_t uresult = (int16_t)a - (int16_t)v - (getFlag(CARRY) ? 0 : 1);
        
        if (uresult >= 0) {
            setFlag(CARRY);
        }
        else {
            clearFlag(CARRY);
        }
        if (sresult > 127 || sresult < -128) {
            setFlag(OVERFL);
        }
        else {
            clearFlag(OVERFL);
        }
        a = (uint8_t)uresult;
        setZeroAndNeg(a);
    }

    // undocumented instruction for zelda - no idea if this is correct.
    void dcpInst(uint16_t addr) {
        uint8_t result = load(addr) - 1;
        //setZeroAndNeg(result);
        store(addr, result);
        if (x >= result) {
            setFlag(CARRY);
        }
        else {
            clearFlag(CARRY);
        }
    }

    // set carry flag
    void secInst(uint16_t) {
        setFlag(CARRY);
    }
    // set bcd mode
    void sedInst(uint16_t) {
        setFlag(DECIMAL);
    }
    // set interrupt disable
    void seiInst(uint16_t) {
        setFlag(INT_DISABLE);
    }
    
    // store accumulator to memory
    void staInst(uint16_t addr) {
        store(addr, a);
    }
    // store x to memory
    void stxInst(uint16_t addr) {
        store(addr, x);
    }
    // store y to memory
    void styInst(uint16_t addr) {
        store(addr, y);
    }
    
    // move accum to x
    void taxInst(uint16_t) {
        x = a;
        setZeroAndNeg(x);
    }
    // move accum to y
    void tayInst(uint16_t) {
        y = a;
        setZeroAndNeg(y);
    }
    // move sp to x
    void tsxInst(uint16_t) {
        x = sp;
        setZeroAndNeg(x);
    }
    // move x to accum
    void txaInst(uint16_t) {
        a = x;
        setZeroAndNeg(a);
    }
    // move x to sp
    void txsInst(uint16_t) {
        sp = x;
    }
    // move y to accum
    void tyaInst(uint16_t) {
        a = y;
        setZeroAndNeg(a);
    }
    
    // addressing mode dispatch functions
    typedef void (Cpu::*InstFunc)(uint16_t);
    
    
    // instruction dispatch table
    struct Instruction;
    typedef void (Cpu::*InstDispatchFunc)(const Instruction&);
    
    struct Instruction {
        std::string nemonic;
        uint8_t opcode;
        uint16_t cycles;
        InstDispatchFunc func;
    };
    
    
    template <InstFunc fx> void impliedFormat(const Instruction &ins) {
        instTrace(ins.nemonic, 1);
        pc += 1;
        (this->*fx)(0);
    }
    template <InstFunc fx> void accumFormat(const Instruction &ins) {
        instTrace(ins.nemonic + " A", 1);
        pc += 1;
        (this->*fx)(0);
    }
    template <InstFunc fx> void immediateFormat(const Instruction &ins) {
        uint16_t immediateAddr = pc + 1;
        
        if (debug) {
            std::stringstream str;
            str << std::setfill('0') << std::right;
            str << ins.nemonic << " #$" << std::hex << std::uppercase << std::setw(2) << (int)load(immediateAddr);
            instTrace(str.str(), 2);
        }
        pc += 2;
        (this->*fx)(immediateAddr);
    }
    template <InstFunc fx> void zeroPageFormat(const Instruction &ins) {
        uint16_t zeroPageAddr = pc + 1;
        
        if (debug) {
            std::stringstream str;
            str << std::setfill('0') << std::right;
            str << ins.nemonic << " $" << std::hex << std::uppercase << std::setw(2) << (int)load(zeroPageAddr);
            instTrace(str.str(), 2);
        }
        pc += 2;
        (this->*fx)((uint16_t)load(zeroPageAddr));
    }
    template <InstFunc fx> void zeroPageXFormat(const Instruction &ins) {
        uint16_t zeroPageAddr = pc + 1;
        
        if (debug) {
            std::stringstream str;
            str << std::setfill('0') << std::right;
            str << ins.nemonic << " $" << std::hex << std::uppercase << std::setw(2) << (int)load(zeroPageAddr) << ",X";
            instTrace(str.str(), 2);
        }
        pc += 2;
        (this->*fx)((uint16_t)((load(zeroPageAddr) + x) & 0xff));
    }
    template <InstFunc fx> void zeroPageYFormat(const Instruction &ins) {
        uint16_t zeroPageAddr = pc + 1;
        
        if (debug) {
            std::stringstream str;
            str << std::setfill('0') << std::right;
            str << ins.nemonic << " $" << std::hex << std::uppercase << std::setw(2) << (int)load(zeroPageAddr) << ",Y";
            instTrace(str.str(), 2);
        }
        pc += 2;
        (this->*fx)((uint16_t)((load(zeroPageAddr) + y)) & 0xff);
    }
    template <InstFunc fx> void relativeFormat(const Instruction &ins) {
        uint16_t immedAddr = pc + 1;
        int8_t relativeAddr = load(immedAddr);
        
        if (debug) {
            std::stringstream str;
            str << std::setfill('0') << std::right;
            str << ins.nemonic << " $" << std::hex << std::uppercase << std::setw(4) << relativeAddr + pc + 2;
            instTrace(str.str(), 2);
        }
        pc+=2;
        
        (this->*fx)((int16_t)relativeAddr);
    }
    template <InstFunc fx> void absoluteFormat(const Instruction &ins) {
        uint16_t immediateAddr = pc + 1;
        uint16_t absAddr = load(immediateAddr);
        absAddr |= ((uint16_t)load(immediateAddr + 1)) << 8;
        
        if (debug) {
            std::stringstream str;
            str << std::setfill('0') << std::right;
            str << ins.nemonic << " $" << std::hex << std::uppercase << std::setw(4) << (int)load16(immediateAddr);
            instTrace(str.str(), 3);
        }
        
        pc += 3;
        (this->*fx)(absAddr);
    }
    template <InstFunc fx> void absoluteXFormat(const Instruction &ins) {
        uint16_t relativeAddr = pc + 1;
        uint16_t absAddr = load(relativeAddr);
        absAddr |= ((uint16_t)load(relativeAddr + 1)) << 8;
        
        if (debug) {
            std::stringstream str;
            str << std::setfill('0') << std::right;
            str << ins.nemonic << " $" << std::hex <<  std::uppercase << std::setw(4) << (int)absAddr << ",X";
            instTrace(str.str(), 3);
        }
        
        // instructions that have a base cycle count of 4, will take a 1 cycle
        // penalty when they cross
        if (ins.cycles == 4) {
            if ((absAddr & 0xff00) != ((absAddr + x) & 0xff00)) {
                cycle++;
            }
        }
        
        absAddr += x;
        pc += 3;
        (this->*fx)(absAddr);
    }
    template <InstFunc fx> void absoluteYFormat(const Instruction &ins) {
        uint16_t relativeAddr = pc + 1;
        uint16_t absAddr = load(relativeAddr);
        absAddr |= ((uint16_t)load(relativeAddr + 1)) << 8;
        
        if (debug) {
            std::stringstream str;
            str << std::setfill('0') << std::right;
            str << ins.nemonic << " $" << std::hex <<  std::uppercase <<  std::setw(4) << (int)absAddr << ",Y";
            instTrace(str.str(), 3);
        }
        
        // instructions that have a base cycle count of 4, will take a 1 cycle
        // penalty when they cross
        if (ins.cycles == 4) {
            if ((absAddr & 0xff00) != ((absAddr + y) & 0xff00)) {
                cycle++;
            }
        }
        
        absAddr += y;
        pc += 3;
        (this->*fx)(absAddr);
    }
    template <InstFunc fx> void indirectFormat(const Instruction &ins) {
        uint16_t immediateAddr = pc + 1u;
        uint16_t absAddr = load(immediateAddr);
        absAddr |= ((uint16_t)load(immediateAddr + 1u)) << 8;
        
        uint16_t finalAddr = load(absAddr);
        // bug in jmp (yes, this needs to work)
        if ((absAddr & 0xff) == 0xff) {
            finalAddr |= ((uint16_t)load(absAddr & 0xff00)) << 8;
        }
        else {
            finalAddr |= ((uint16_t)load(absAddr + 1u)) << 8;
        }
        
        if (debug) {
            std::stringstream str;
            str << std::setfill('0') << std::right;
            str << ins.nemonic << " ($" << std::hex <<  std::uppercase <<  std::setw(4) << (int)absAddr << ")";
            instTrace(str.str(), 3);
        }
        
        pc += 3;
        (this->*fx)(finalAddr);
    }
    template <InstFunc fx> void indexedIndirectFormat(const Instruction &ins) {
        uint16_t immediateAddr = pc + 1;
        uint8_t immed = load(immediateAddr);
        uint8_t tableAddr = immed + x;
        uint16_t finalAddr = load(tableAddr);
        finalAddr |= (uint16_t)load((tableAddr + 1) & 0xff) << 8;
        
        std::stringstream str;
        str << std::setfill('0') << std::right;
        str << ins.nemonic << " ($" << std::hex << std::uppercase << std::setw(2) << (int)immed << ",X)";
        
        instTrace(str.str(), 2);
        
        pc += 2;
        (this->*fx)(finalAddr);
    }
    
    template <InstFunc fx> void indirectIndexedFormat(const Instruction &ins) {
        uint16_t immediateAddr = pc + 1;
        uint8_t immed = load(immediateAddr);
        
        uint16_t tableAddr = load(immed);
        tableAddr |= (uint16_t)load((immed + 1) & 0xff) << 8;
        
        uint16_t finalAddr = tableAddr + y;
        
        if (debug) {
            std::stringstream str;
            str << std::setfill('0') << std::right;
            str << ins.nemonic << " ($" << std::hex << std::uppercase << std::setw(2) << (int)immed << "),Y";
            instTrace(str.str(), 2);
        }
        
        // Add a penalty when y causes the table addr and final address to
        // not be on the same page
        if (ins.cycles == 5) {
            if ((tableAddr & 0xff00) != ((tableAddr + y) & 0xff00)) {
                cycle++;
            }
        }
        
        pc += 2;
        (this->*fx)(finalAddr);
    }
    
    std::unordered_map<uint8_t, Cpu::Instruction, OpcodeHash> instTable = {{
        {0x00, {"BRK", 0x00, 7, &Cpu::impliedFormat<&Cpu::brkInst>}},
        {0x01, {"ORA", 0x01, 6, &Cpu::indexedIndirectFormat<&Cpu::oraInst>}},
        {0x05, {"ORA", 0x05, 3, &Cpu::zeroPageFormat<&Cpu::oraInst>}},
        {0x06, {"ASL", 0x06, 5, &Cpu::zeroPageFormat<&Cpu::aslInstMem>}},
        {0x08, {"PHP", 0x08, 3, &Cpu::impliedFormat<&Cpu::phpInst>}},
        {0x09, {"ORA", 0x09, 2, &Cpu::immediateFormat<&Cpu::oraInst>}},
        {0x0a, {"ASL", 0x0a, 2, &Cpu::accumFormat<&Cpu::aslInstReg>}},
        {0x0d, {"ORA", 0x0d, 4, &Cpu::absoluteFormat<&Cpu::oraInst>}},
        {0x0e, {"ASL", 0x0e, 6, &Cpu::absoluteFormat<&Cpu::aslInstMem>}},
        {0x10, {"BPL", 0x10, 2, &Cpu::relativeFormat<&Cpu::bplInst>}},
        {0x11, {"ORA", 0x11, 5, &Cpu::indirectIndexedFormat<&Cpu::oraInst>}},
        {0x15, {"ORA", 0x15, 4, &Cpu::zeroPageXFormat<&Cpu::oraInst>}},
        {0x16, {"ASL", 0x16, 6, &Cpu::zeroPageXFormat<&Cpu::aslInstMem>}},
        {0x18, {"CLC", 0x18, 2, &Cpu::impliedFormat<&Cpu::clcInst>}},
        {0x19, {"ORA", 0x19, 4, &Cpu::absoluteYFormat<&Cpu::oraInst>}},
        {0x1a, {"NOP2", 0x1a, 2, &Cpu::impliedFormat<&Cpu::nopInst>}},        // undocumented;p
        {0x1c, {"NOP2", 0x1c, 4, &Cpu::absoluteXFormat<&Cpu::nopInst>}},      // undocumented and wrong
        {0x1d, {"ORA", 0x1d, 4, &Cpu::absoluteXFormat<&Cpu::oraInst>}},
        {0x1e, {"ASL", 0x1e, 7, &Cpu::absoluteXFormat<&Cpu::aslInstMem>}},
        {0x20, {"JSR", 0x20, 6, &Cpu::absoluteFormat<&Cpu::jsrInst>}},
        {0x21, {"AND", 0x21, 6, &Cpu::indexedIndirectFormat<&Cpu::andInst>}},
        {0x24, {"BIT", 0x24, 3, &Cpu::zeroPageFormat<&Cpu::bitInst>}},
        {0x25, {"AND", 0x25, 3, &Cpu::zeroPageFormat<&Cpu::andInst>}},
        {0x26, {"ROL", 0x26, 5, &Cpu::zeroPageFormat<&Cpu::rolInstMem>}},
        {0x28, {"PLP", 0x28, 4, &Cpu::impliedFormat<&Cpu::plpInst>}},
        {0x29, {"AND", 0x29, 2, &Cpu::immediateFormat<&Cpu::andInst>}},
        {0x2a, {"ROL", 0x2a, 2, &Cpu::accumFormat<&Cpu::rolInstReg>}},
        {0x2c, {"BIT", 0x2c, 4, &Cpu::absoluteFormat<&Cpu::bitInst>}},
        {0x2d, {"AND", 0x2d, 4, &Cpu::absoluteFormat<&Cpu::andInst>}},
        {0x2e, {"ROL", 0x2e, 6, &Cpu::absoluteFormat<&Cpu::rolInstMem>}},
        {0x30, {"BMI", 0x30, 2, &Cpu::relativeFormat<&Cpu::bmiInst>}},
        {0x31, {"AND", 0x31, 5, &Cpu::indirectIndexedFormat<&Cpu::andInst>}},
        {0x35, {"AND", 0x35, 4, &Cpu::zeroPageXFormat<&Cpu::andInst>}},
        {0x36, {"ROL", 0x36, 6, &Cpu::zeroPageXFormat<&Cpu::rolInstMem>}},
        {0x38, {"SEC", 0x38, 2, &Cpu::impliedFormat<&Cpu::secInst>}},
        {0x39, {"AND", 0x39, 4, &Cpu::absoluteYFormat<&Cpu::andInst>}},
        {0x3d, {"AND", 0x3d, 4, &Cpu::absoluteXFormat<&Cpu::andInst>}},
        {0x3e, {"ROL", 0x3e, 7, &Cpu::absoluteXFormat<&Cpu::rolInstMem>}},
        {0x40, {"RTI", 0x40, 6, &Cpu::impliedFormat<&Cpu::rtiInst>}},
        {0x41, {"EOR", 0x41, 6, &Cpu::indexedIndirectFormat<&Cpu::eorInst>}},
        {0x45, {"EOR", 0x45, 3, &Cpu::zeroPageFormat<&Cpu::eorInst>}},
        {0x46, {"LSR", 0x46, 5, &Cpu::zeroPageFormat<&Cpu::lsrInstMem>}},
        {0x48, {"PHA", 0x48, 3, &Cpu::impliedFormat<&Cpu::phaInst>}},
        {0x49, {"EOR", 0x49, 2, &Cpu::immediateFormat<&Cpu::eorInst>}},
        {0x4a, {"LSR", 0x4a, 2, &Cpu::accumFormat<&Cpu::lsrInstReg>}},
        {0x4c, {"JMP", 0x4c, 3, &Cpu::absoluteFormat<&Cpu::jmpInst>}},
        {0x4d, {"EOR", 0x4d, 4, &Cpu::absoluteFormat<&Cpu::eorInst>}},
        {0x4e, {"LSR", 0x4e, 6, &Cpu::absoluteFormat<&Cpu::lsrInstMem>}},
        {0x50, {"BVC", 0x50, 2, &Cpu::relativeFormat<&Cpu::bvcInst>}},
        {0x51, {"EOR", 0x51, 5, &Cpu::indirectIndexedFormat<&Cpu::eorInst>}},
        {0x55, {"EOR", 0x55, 4, &Cpu::zeroPageXFormat<&Cpu::eorInst>}},
        {0x56, {"LSR", 0x56, 6, &Cpu::zeroPageXFormat<&Cpu::lsrInstMem>}},
        {0x58, {"CLI", 0x58, 2, &Cpu::impliedFormat<&Cpu::cliInst>}},
        {0x59, {"EOR", 0x59, 4, &Cpu::absoluteYFormat<&Cpu::eorInst>}},
        {0x5d, {"EOR", 0x5d, 4, &Cpu::absoluteXFormat<&Cpu::eorInst>}},
        {0x5e, {"LSR", 0x58, 7, &Cpu::absoluteXFormat<&Cpu::lsrInstMem>}},
        {0x60, {"RTS", 0x60, 6, &Cpu::impliedFormat<&Cpu::rtsInst>}},
        {0x61, {"ADC", 0x61, 6, &Cpu::indexedIndirectFormat<&Cpu::adcInst>}},
        {0x65, {"ADC", 0x65, 3, &Cpu::zeroPageFormat<&Cpu::adcInst>}},
        {0x66, {"ROR", 0x66, 5, &Cpu::zeroPageFormat<&Cpu::rorInstMem>}},
        {0x68, {"PLA", 0x68, 4, &Cpu::impliedFormat<&Cpu::plaInst>}},
        {0x69, {"ADC", 0x69, 2, &Cpu::immediateFormat<&Cpu::adcInst>}},
        {0x6a, {"ROR", 0x6a, 2, &Cpu::accumFormat<&Cpu::rorInstReg>}},
        {0x6c, {"JMP", 0x6c, 5, &Cpu::indirectFormat<&Cpu::jmpInst>}},
        {0x6d, {"ADC", 0x6d, 4, &Cpu::absoluteFormat<&Cpu::adcInst>}},
        {0x6e, {"ROR", 0x68, 6, &Cpu::absoluteFormat<&Cpu::rorInstMem>}},
        {0x70, {"BVS", 0x70, 2, &Cpu::relativeFormat<&Cpu::bvsInst>}},
        {0x71, {"ADC", 0x71, 5, &Cpu::indirectIndexedFormat<&Cpu::adcInst>}},
        {0x75, {"ADC", 0x75, 4, &Cpu::zeroPageXFormat<&Cpu::adcInst>}},
        {0x76, {"ROR", 0x76, 6, &Cpu::zeroPageXFormat<&Cpu::rorInstMem>}},
        {0x78, {"SEI", 0x78, 2, &Cpu::impliedFormat<&Cpu::seiInst>}},
        {0x79, {"ADC", 0x79, 4, &Cpu::absoluteYFormat<&Cpu::adcInst>}},
        {0x7d, {"ADC", 0x7d, 4, &Cpu::absoluteXFormat<&Cpu::adcInst>}},
        {0x7e, {"ROR", 0x7e, 7, &Cpu::absoluteXFormat<&Cpu::rorInstMem>}},
        {0x81, {"STA", 0x81, 6, &Cpu::indexedIndirectFormat<&Cpu::staInst>}},
        {0x82, {"NOP2", 0x82, 2, &Cpu::immediateFormat<&Cpu::nopInst>}},
        {0x84, {"STY", 0x84, 3, &Cpu::zeroPageFormat<&Cpu::styInst>}},
        {0x85, {"STA", 0x85, 3, &Cpu::zeroPageFormat<&Cpu::staInst>}},
        {0x86, {"STX", 0x86, 3, &Cpu::zeroPageFormat<&Cpu::stxInst>}},
        {0x88, {"DEY", 0x88, 2, &Cpu::impliedFormat<&Cpu::deyInst>}},
        {0x8a, {"TXA", 0x8a, 2, &Cpu::impliedFormat<&Cpu::txaInst>}},
        {0x8c, {"STY", 0x8c, 4, &Cpu::absoluteFormat<&Cpu::styInst>}},
        {0x8d, {"STA", 0x8d, 4, &Cpu::absoluteFormat<&Cpu::staInst>}},
        {0x8e, {"STX", 0x8e, 4, &Cpu::absoluteFormat<&Cpu::stxInst>}},
        {0x90, {"BCC", 0x90, 2, &Cpu::relativeFormat<&Cpu::bccInst>}},
        {0x91, {"STA", 0x91, 6, &Cpu::indirectIndexedFormat<&Cpu::staInst>}},
        {0x94, {"STY", 0x94, 4, &Cpu::zeroPageXFormat<&Cpu::styInst>}},
        {0x95, {"STA", 0x95, 4, &Cpu::zeroPageXFormat<&Cpu::staInst>}},
        {0x96, {"STX", 0x96, 4, &Cpu::zeroPageYFormat<&Cpu::stxInst>}},
        {0x98, {"TYA", 0x98, 2, &Cpu::impliedFormat<&Cpu::tyaInst>}},
        {0x99, {"STA", 0x99, 5, &Cpu::absoluteYFormat<&Cpu::staInst>}},
        {0x9a, {"TXS", 0x9a, 2, &Cpu::impliedFormat<&Cpu::txsInst>}},
        {0x9d, {"STA", 0x9d, 5, &Cpu::absoluteXFormat<&Cpu::staInst>}},
        {0xa0, {"LDY", 0xa0, 2, &Cpu::immediateFormat<&Cpu::ldyInst>}},
        {0xa1, {"LDA", 0xa1, 6, &Cpu::indexedIndirectFormat<&Cpu::ldaInst>}},
        {0xa2, {"LDX", 0xa2, 2, &Cpu::immediateFormat<&Cpu::ldxInst>}},
        {0xa4, {"LDY", 0xa4, 3, &Cpu::zeroPageFormat<&Cpu::ldyInst>}},
        {0xa5, {"LDA", 0xa5, 3, &Cpu::zeroPageFormat<&Cpu::ldaInst>}},
        {0xa6, {"LDX", 0xa6, 3, &Cpu::zeroPageFormat<&Cpu::ldxInst>}},
        {0xa8, {"TAY", 0xa8, 2, &Cpu::impliedFormat<&Cpu::tayInst>}},
        {0xa9, {"LDA", 0xa9, 2, &Cpu::immediateFormat<&Cpu::ldaInst>}},
        {0xaa, {"TAX", 0xaa, 2, &Cpu::impliedFormat<&Cpu::taxInst>}},
        {0xac, {"LDY", 0xac, 4, &Cpu::absoluteFormat<&Cpu::ldyInst>}},
        {0xad, {"LDA", 0xad, 4, &Cpu::absoluteFormat<&Cpu::ldaInst>}},
        {0xae, {"LDX", 0xae, 4, &Cpu::absoluteFormat<&Cpu::ldxInst>}},
        {0xb0, {"BCS", 0xb0, 2, &Cpu::relativeFormat<&Cpu::bcsInst>}},
        {0xb1, {"LDA", 0xb1, 5, &Cpu::indirectIndexedFormat<&Cpu::ldaInst>}},
        {0xb4, {"LDY", 0xb4, 4, &Cpu::zeroPageXFormat<&Cpu::ldyInst>}},
        {0xb5, {"LDA", 0xb5, 4, &Cpu::zeroPageXFormat<&Cpu::ldaInst>}},
        {0xb6, {"LDX", 0xb6, 4, &Cpu::zeroPageYFormat<&Cpu::ldxInst>}},
        {0xb8, {"CLV", 0xb8, 2, &Cpu::impliedFormat<&Cpu::clvInst>}},
        {0xb9, {"LDA", 0xb9, 4, &Cpu::absoluteYFormat<&Cpu::ldaInst>}},
        {0xba, {"TSX", 0xba, 2, &Cpu::impliedFormat<&Cpu::tsxInst>}},
        {0xbc, {"LDY", 0xbc, 4, &Cpu::absoluteXFormat<&Cpu::ldyInst>}},
        {0xbd, {"LDA", 0xbd, 4, &Cpu::absoluteXFormat<&Cpu::ldaInst>}},
        {0xbe, {"LDX", 0xbe, 4, &Cpu::absoluteYFormat<&Cpu::ldxInst>}},
        {0xc0, {"CPY", 0xc0, 2, &Cpu::immediateFormat<&Cpu::cpyInst>}},
        {0xc1, {"CMP", 0xc1, 6, &Cpu::indexedIndirectFormat<&Cpu::cmpInst>}},
        {0xc4, {"CPY", 0xc4, 3, &Cpu::zeroPageFormat<&Cpu::cpyInst>}},
        {0xc5, {"CMP", 0xc5, 3, &Cpu::zeroPageFormat<&Cpu::cmpInst>}},
        {0xc6, {"DEC", 0xc6, 5, &Cpu::zeroPageFormat<&Cpu::decInst>}},
        {0xc8, {"INY", 0xc8, 2, &Cpu::impliedFormat<&Cpu::inyInst>}},
        {0xc9, {"CMP", 0xc9, 2, &Cpu::immediateFormat<&Cpu::cmpInst>}},
        {0xca, {"DEX", 0xca, 2, &Cpu::impliedFormat<&Cpu::dexInst>}},
        {0xcc, {"CPY", 0xcc, 4, &Cpu::absoluteFormat<&Cpu::cpyInst>}},
        {0xcd, {"CMP", 0xcd, 4, &Cpu::absoluteFormat<&Cpu::cmpInst>}},
        {0xce, {"DEC", 0xce, 6, &Cpu::absoluteFormat<&Cpu::decInst>}},
        {0xd0, {"BNE", 0xd0, 2, &Cpu::relativeFormat<&Cpu::bneInst>}},
        {0xd1, {"CMP", 0xd1, 5, &Cpu::indirectIndexedFormat<&Cpu::cmpInst>}},
        {0xd5, {"CMP", 0xd5, 4, &Cpu::zeroPageXFormat<&Cpu::cmpInst>}},
        {0xd6, {"DEC", 0xd6, 6, &Cpu::zeroPageXFormat<&Cpu::decInst>}},
        {0xd7, {"DCP", 0xd7, 6, &Cpu::zeroPageXFormat<&Cpu::dcpInst>}},
        {0xd8, {"CLD", 0xd8, 2, &Cpu::impliedFormat<&Cpu::cldInst>}},
        {0xd9, {"CMP", 0xd9, 4, &Cpu::absoluteYFormat<&Cpu::cmpInst>}},
        {0xdd, {"CMP", 0xdd, 4, &Cpu::absoluteXFormat<&Cpu::cmpInst>}},
        {0xde, {"DEC", 0xde, 7, &Cpu::absoluteXFormat<&Cpu::decInst>}},
        {0xe0, {"CPX", 0xe0, 2, &Cpu::immediateFormat<&Cpu::cpxInst>}},
        {0xe1, {"SBC", 0xe1, 6, &Cpu::indexedIndirectFormat<&Cpu::sbcInst>}},
        {0Xe4, {"CPX", 0xe4, 3, &Cpu::zeroPageFormat<&Cpu::cpxInst>}},
        {0xe5, {"SBC", 0xe5, 3, &Cpu::zeroPageFormat<&Cpu::sbcInst>}},
        {0xe6, {"INC", 0xe6, 5, &Cpu::zeroPageFormat<&Cpu::incInst>}},
        {0xe8, {"INX", 0xe8, 2, &Cpu::impliedFormat<&Cpu::inxInst>}},
        {0xe9, {"SBC", 0xe9, 2, &Cpu::immediateFormat<&Cpu::sbcInst>}},
        {0xea, {"NOP", 0xea, 2, &Cpu::impliedFormat<&Cpu::nopInst>}},
        {0xec, {"CPX", 0xec, 4, &Cpu::absoluteFormat<&Cpu::cpxInst>}},
        {0xed, {"SBC", 0xed, 4, &Cpu::absoluteFormat<&Cpu::sbcInst>}},
        {0xee, {"INC", 0xee, 6, &Cpu::absoluteFormat<&Cpu::incInst>}},
        {0xf0, {"BEQ", 0xf0, 2, &Cpu::relativeFormat<&Cpu::beqInst>}},
        {0xf1, {"SBC", 0xf1, 5, &Cpu::indirectIndexedFormat<&Cpu::sbcInst>}},
        {0xf5, {"SBC", 0xf5, 4, &Cpu::zeroPageXFormat<&Cpu::sbcInst>}},
        {0xf6, {"INC", 0xf6, 6, &Cpu::zeroPageXFormat<&Cpu::incInst>}},
        {0xf8, {"SED", 0xf8, 2, &Cpu::impliedFormat<&Cpu::sedInst>}},
        {0xf9, {"SBC", 0xf9, 4, &Cpu::absoluteYFormat<&Cpu::sbcInst>}},
        {0xfd, {"SBC", 0xfd, 4, &Cpu::absoluteXFormat<&Cpu::sbcInst>}},
        {0xfe, {"INC", 0xfe, 7, &Cpu::absoluteXFormat<&Cpu::incInst>}}
    }, 256, OpcodeHash()};
    
public:
    uint32_t runInst() {
        // check for nmi
        if (nmiRequested()) {
            return doNmi();
        }
        // check for lower priority maskable interrupts
        if (intRequested() && !getFlag(INT_DISABLE)) {
            return doInt();
        }
        
        // load opcode using current pc
        uint8_t opcode = load(pc);
        
        // look up instruction in the table
        auto itr = instTable.find(opcode);
        if (itr == instTable.end()) {
            assert(0);
            return 0;
        }
        
        // run the instruction
        const Instruction &ins = itr->second;
        (this->*(ins.func))(ins);
        
        // update the cycle count
        cycle += ins.cycles;
        return ins.cycles;
    }
    
    
    void reset() {
        setFlag(INT_DISABLE);
        setFlag(ONE);
        pc = load16(resetBaseAddr);
        sp = stackPointerStart;
    }
    
    void dumpRegs() {
        using namespace std;
        cerr << setfill('0') << right;
        cerr << "A:" << setw(2) << (int)a << " ";
        cerr << "X:" << setw(2) << (int)x << " ";
        cerr << "Y:" << setw(2) << (int)y << " ";
        cerr << "P:" << setw(2) << (int)status << " ";
        cerr << "SP:" << setw(2) << (int)sp << " ";
    }
    void dumpPc() {
        using namespace std;
        cerr << setfill('0') << std::right << setw(4) << hex << uppercase;
        cerr << (int)pc << "  ";
    }
    void dumpInstructionBytes(int bytes) {
        using namespace std;
        cerr << setfill('0') << right;
        for (int i = 0; i < bytes; i++) {
            cerr << setw(2) << uppercase << (int)load(pc + i) << " ";
        }
        for (int i = 0; i < maxInstLength - bytes; i++) {
            cerr << "   ";
        }
        cerr << " ";
    }
    void dumpPPUTiming() {
        using namespace std;
        //const int initScanLine = 0;
        cerr << setfill(' ') << right;
        cerr << "CYC:" << dec << setw(3)  << (3 * cycle) % 341;
    }
    inline void instTrace(const std::string& str, int bytes) {
        uint32_t instPadding = 32;
        if (debug) {
            dumpPc();
            dumpInstructionBytes(bytes);
            std::cerr << str;
            if (str.size() < instPadding) {
                for (uint32_t i = 0; i < instPadding - str.size(); i++) {
                    std::cerr << " ";
                }
            }
            
            dumpRegs();
            dumpPPUTiming();
            std::cerr << "\n";
        }
    }
    Cpu(Nes *system) {
        a = 0;
        x = 0;
        y = 0;
        pc = 0;
        status = 0;
        sp = 0;
        cycle = 0;
        nes = system;
    }
    
    ~Cpu() {}
};


#endif 
