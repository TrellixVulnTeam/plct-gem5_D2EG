/*
 * Copyright (c) 2022 PLCT Lab
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met: redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer;
 * redistributions in binary form must reproduce the above copyright
 * notice, this list of conditions and the following disclaimer in the
 * documentation and/or other materials provided with the distribution;
 * neither the name of the copyright holders nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef __ARCH_RISCV_INSTS_VECTOR_HH__
#define __ARCH_RISCV_INSTS_VECTOR_HH__

#include <string>

#include "arch/riscv/insts/static_inst.hh"
#include "arch/riscv/regs/misc.hh"
#include "arch/riscv/utility.hh"
#include "cpu/exec_context.hh"
#include "cpu/static_inst.hh"

namespace gem5
{

namespace RiscvISA
{

enum VectorRoundingMode {
    RoundToNearestUp = 0,
    RoundToNearestEven,
    RoundDown,
    RoundToOdd,
    InvalidRound,
};

void roundUnsignedInteger(__uint128_t &result, uint32_t xrm, int gb);
void roundSignedInteger(__int128_t &result, uint32_t xrm, int gb);

float
getVflmul(uint32_t vlmul_encoding);

inline uint32_t getSew(uint32_t vsew) {
    assert(vsew <= 3);
    return (8 << vsew);
}

uint32_t
getVlmax(VTYPE vtype, uint32_t vlen);

/**
 * Base class for Vector Config operations
 */
class VConfOp : public RiscvStaticInst
{
  protected:
    uint64_t bit30;
    uint64_t bit31;
    uint64_t zimm10;
    uint64_t zimm11;
    uint64_t uimm;
    VConfOp(const char *mnem, ExtMachInst _extMachInst, OpClass __opClass)
        : RiscvStaticInst(mnem, _extMachInst, __opClass),
          bit30(_extMachInst.bit30), bit31(_extMachInst.bit31),
          zimm10(_extMachInst.zimm_vsetivli),
          zimm11(_extMachInst.zimm_vsetvli),
          uimm(_extMachInst.uimm_vsetivli)
    {}

    std::string generateDisassembly(
        Addr pc, const loader::SymbolTable *symtab) const override;

    std::string generateZimmDisassembly() const;
};

inline uint8_t checked_vtype(bool vill, uint8_t vtype) {
    panic_if(vill, "vill has been set");
    const uint8_t vsew = bits(vtype, 5, 3);
    panic_if(vsew >= 0b100, "vsew: %#x not supported", vsew);
    const uint8_t vlmul = bits(vtype, 2, 0);
    panic_if(vlmul == 0b100, "vlmul: %#x not supported", vlmul);
    return vtype;
}

class VectorNonSplitInst : public RiscvStaticInst
{
  protected:
    uint32_t vl;
    uint8_t vtype;
    VectorNonSplitInst(const char* mnem, ExtMachInst _machInst,
                   OpClass __opClass)
        : RiscvStaticInst(mnem, _machInst, __opClass),
        vl(_machInst.vl),
        vtype(checked_vtype(_machInst.vill, _machInst.vtype8))
    {
        this->flags[IsVector] = true;
    }

    std::string generateDisassembly(
        Addr pc, const loader::SymbolTable *symtab) const override;
};

class VectorMacroInst : public RiscvMacroInst
{
  protected:
    uint32_t vl;
    uint8_t vtype;
    VectorMacroInst(const char* mnem, ExtMachInst _machInst,
                   OpClass __opClass)
        : RiscvMacroInst(mnem, _machInst, __opClass),
        vl(_machInst.vl),
        vtype(checked_vtype(_machInst.vill, _machInst.vtype8))
    {
        this->flags[IsVector] = true;
    }
};

class VectorMicroInst : public RiscvMicroInst
{
protected:
    uint8_t microVl;
    uint8_t microIdx;
    uint8_t vtype;
    VectorMicroInst(const char *mnem, ExtMachInst _machInst, OpClass __opClass,
                    uint8_t _microVl, uint8_t _microIdx)
        : RiscvMicroInst(mnem, _machInst, __opClass),
        microVl(_microVl),
        microIdx(_microIdx),
        vtype(_machInst.vtype8)
    {
        this->flags[IsVector] = true;
    }
};

class VectorArithMicroInst : public VectorMicroInst
{
protected:
    VectorArithMicroInst(const char *mnem, ExtMachInst _machInst,
                         OpClass __opClass, uint8_t _microVl,
                         uint8_t _microIdx)
        : VectorMicroInst(mnem, _machInst, __opClass, _microVl, _microIdx)
    {}

    std::string generateDisassembly(
            Addr pc, const loader::SymbolTable *symtab) const override;
};

class VectorArithMacroInst : public VectorMacroInst
{
  protected:
    VectorArithMacroInst(const char* mnem, ExtMachInst _machInst,
                         OpClass __opClass)
        : VectorMacroInst(mnem, _machInst, __opClass)
    {
        this->flags[IsVector] = true;
    }

    std::string generateDisassembly(
            Addr pc, const loader::SymbolTable *symtab) const override;
};

class VectorMemMicroInst : public VectorMicroInst
{
  protected:
    uint32_t offset; // Used to calculate EA.
    Request::Flags memAccessFlags;

    VectorMemMicroInst(const char* mnem, ExtMachInst _machInst,
                       OpClass __opClass, uint8_t _microVl, uint8_t _microIdx,
                       uint32_t _offset)
        : VectorMicroInst(mnem, _machInst, __opClass, _microVl, _microIdx)
        , offset(_offset)
        , memAccessFlags(0)
    {}
};

class VectorMemMacroInst : public VectorMacroInst
{
  protected:
    VectorMemMacroInst(const char* mnem, ExtMachInst _machInst,
                       OpClass __opClass)
        : VectorMacroInst(mnem, _machInst, __opClass)
    {}
};

class VleMacroInst : public VectorMemMacroInst
{
  protected:
    VleMacroInst(const char* mnem, ExtMachInst _machInst,
                   OpClass __opClass)
        : VectorMemMacroInst(mnem, _machInst, __opClass)
    {}

    std::string generateDisassembly(
            Addr pc, const loader::SymbolTable *symtab) const override;
};

class VseMacroInst : public VectorMemMacroInst
{
  protected:
    VseMacroInst(const char* mnem, ExtMachInst _machInst,
                   OpClass __opClass)
        : VectorMemMacroInst(mnem, _machInst, __opClass)
    {}

    std::string generateDisassembly(
            Addr pc, const loader::SymbolTable *symtab) const override;
};

class VleMicroInst : public VectorMicroInst
{
  protected:
    Request::Flags memAccessFlags;

    VleMicroInst(const char *mnem, ExtMachInst _machInst, OpClass __opClass,
                 uint8_t _microVl, uint8_t _microIdx)
        : VectorMicroInst(mnem, _machInst, __opClass, _microVl, _microIdx)
    {
        this->flags[IsLoad] = true;
    }

    std::string generateDisassembly(
        Addr pc, const loader::SymbolTable *symtab) const override;
};

class VseMicroInst : public VectorMicroInst
{
  protected:
    Request::Flags memAccessFlags;

    VseMicroInst(const char *mnem, ExtMachInst _machInst, OpClass __opClass,
                 uint8_t _microVl, uint8_t _microIdx)
        : VectorMicroInst(mnem, _machInst, __opClass, _microVl, _microIdx)
    {
        this->flags[IsStore] = true;
    }

    std::string generateDisassembly(
        Addr pc, const loader::SymbolTable *symtab) const override;
};

class VlWholeMacroInst : public VectorMemMacroInst
{
  protected:
    VlWholeMacroInst(const char *mnem, ExtMachInst _machInst,
                     OpClass __opClass)
      : VectorMemMacroInst(mnem, _machInst, __opClass)
    {}

    std::string generateDisassembly(
      Addr pc, const loader::SymbolTable *symtab) const override;
};

class VlWholeMicroInst : public VectorMicroInst
{
  protected:
    Request::Flags memAccessFlags;

    VlWholeMicroInst(const char *mnem, ExtMachInst _machInst,
                     OpClass __opClass, uint8_t _microVl, uint8_t _microIdx)
        : VectorMicroInst(mnem, _machInst, __opClass, _microVl, _microIdx)
    {}

    std::string generateDisassembly(
      Addr pc, const loader::SymbolTable *symtab) const override;
};

class VsWholeMacroInst : public VectorMemMacroInst
{
  protected:
    VsWholeMacroInst(const char *mnem, ExtMachInst _machInst,
                     OpClass __opClass)
        : VectorMemMacroInst(mnem, _machInst, __opClass)
    {}

    std::string generateDisassembly(
            Addr pc, const loader::SymbolTable *symtab) const override;
};

class VsWholeMicroInst : public VectorMicroInst
{
  protected:
    Request::Flags memAccessFlags;

    VsWholeMicroInst(const char *mnem, ExtMachInst _machInst,
                     OpClass __opClass, uint8_t _microVl, uint8_t _microIdx)
        : VectorMicroInst(mnem, _machInst, __opClass, _microIdx, _microIdx)
    {}

    std::string generateDisassembly(
        Addr pc, const loader::SymbolTable *symtab) const override;
};

class VlStrideMacroInst : public VectorMemMacroInst
{
  protected:
    VlStrideMacroInst(const char* mnem, ExtMachInst _machInst,
                   OpClass __opClass)
        : VectorMemMacroInst(mnem, _machInst, __opClass)
    {}

    std::string generateDisassembly(
            Addr pc, const loader::SymbolTable *symtab) const override;
};

class VlStrideMicroInst : public VectorMemMicroInst
{
  protected:
  uint8_t regIdx;
    VlStrideMicroInst(const char *mnem, ExtMachInst _machInst,
                      OpClass __opClass, uint8_t _regIdx,
                      uint8_t _microIdx, uint8_t _microVl)
        : VectorMemMicroInst(mnem, _machInst, __opClass, _microVl,
                             _microIdx, 0)
        , regIdx(_regIdx)
    {}

    std::string generateDisassembly(
        Addr pc, const loader::SymbolTable *symtab) const override;
};

class VMvWholeMacroInst : public VectorArithMacroInst
{
  protected:
    VMvWholeMacroInst(const char* mnem, ExtMachInst _machInst,
                         OpClass __opClass)
        : VectorArithMacroInst(mnem, _machInst, __opClass)
    {}

    std::string generateDisassembly(
            Addr pc, const loader::SymbolTable *symtab) const override;
};

class VMvWholeMicroInst : public VectorArithMicroInst
{
  protected:
    VMvWholeMicroInst(const char *mnem, ExtMachInst _machInst,
                         OpClass __opClass, uint8_t _microVl,
                         uint8_t _microIdx)
        : VectorArithMicroInst(mnem, _machInst, __opClass, _microVl, _microIdx)
    {}

    std::string generateDisassembly(
            Addr pc, const loader::SymbolTable *symtab) const override;
};

template<typename ElemType>
class VMaskMergeMicroInst : public VectorArithMicroInst
{
  private:
    RegId srcRegIdxArr[NumVecInternalRegs];
    RegId destRegIdxArr[1];

  public:
    VMaskMergeMicroInst(ExtMachInst extMachInst, uint8_t _dstReg,
        uint8_t _numSrcs)
        : VectorArithMicroInst("vmask_mv_micro", extMachInst, VectorDummyOp, 0,
            0)
    {
        setRegIdxArrays(
            reinterpret_cast<RegIdArrayPtr>(
                &std::remove_pointer_t<decltype(this)>::srcRegIdxArr),
            reinterpret_cast<RegIdArrayPtr>(
                &std::remove_pointer_t<decltype(this)>::destRegIdxArr));

        _numSrcRegs = 0;
        _numDestRegs = 0;

        setDestRegIdx(_numDestRegs++, vecRegClass[_dstReg]);
        _numTypedDestRegs[VecRegClass]++;
        for (uint8_t i=0; i<_numSrcs; i++) {
            setSrcRegIdx(_numSrcRegs++, vecRegClass[VecMemInternalReg0 + i]);
        }
    }

    Fault execute(ExecContext* xc, Trace::InstRecord* traceData)
            const override {
        vreg_t tmp_d0 = *(vreg_t *)xc->getWritableRegOperand(this, 0);
        auto Vd = tmp_d0.as<uint8_t>();
        constexpr uint8_t elems_per_vreg = VLENB / sizeof(ElemType);
        size_t bit_cnt = elems_per_vreg;
        vreg_t tmp_s;
        xc->getRegOperand(this, 0, &tmp_s);
        auto s = tmp_s.as<uint8_t>();
        // cp the first result and tail
        memcpy(Vd, s, VLENB);
        for (uint8_t i = 1; i < this->_numSrcRegs; i++) {
            xc->getRegOperand(this, i, &tmp_s);
            s = tmp_s.as<uint8_t>();
            if constexpr (elems_per_vreg < 8) {
                constexpr uint8_t m = (1 << elems_per_vreg) - 1;
                const uint8_t mask = m << (i * elems_per_vreg % 8);
                // clr & ext bits
                Vd[bit_cnt/8] ^= Vd[bit_cnt/8] & mask;
                Vd[bit_cnt/8] |= s[bit_cnt/8] & mask;
                bit_cnt += elems_per_vreg;
            } else {
                constexpr uint8_t byte_offset = elems_per_vreg / 8;
                memcpy(Vd + i * byte_offset, s + i * byte_offset, byte_offset);
            }
        }
        xc->setRegOperand(this, 0, &tmp_d0);
        if (traceData)
            traceData->setData(tmp_d0);
        return NoFault;
    }

    std::string generateDisassembly(
            Addr pc, const loader::SymbolTable *symtab) const override {
        std::stringstream ss;
        ss << mnemonic << ' ' << registerName(destRegIdx(0));
        for (uint8_t i = 0; i < this->_numSrcRegs; i++) {
            ss << ", " << registerName(srcRegIdx(i));
        }
        ss << ", offset:" << VLENB / sizeof(ElemType);
        return ss.str();
    }
};

class VxsatMicroInst : public VectorArithMicroInst
{
  private:
    bool* vxsat;
  public:
    VxsatMicroInst(bool* Vxsat, ExtMachInst extMachInst)
        : VectorArithMicroInst("vxsat_micro", extMachInst, VectorDummyOp, 0,
            0)
    {
        vxsat = Vxsat;
    }
    Fault execute(ExecContext* xc, Trace::InstRecord* traceData)
    const override
    {
        xc->setMiscReg(MISCREG_VXSAT,*vxsat);
        auto vcsr = xc->readMiscReg(MISCREG_VCSR);
        xc->setMiscReg(MISCREG_VCSR, ((vcsr&~1)|*vxsat));
        return NoFault;
    }
    std::string generateDisassembly(Addr pc, const loader::SymbolTable *symtab)
      const override
    {
        std::stringstream ss;
        ss << mnemonic << ' ' << "VXSAT" << ", " << (*vxsat ? "0x1" : "0x0");
        return ss.str();
    }
};

} // namespace RiscvISA
} // namespace gem5


#endif // __ARCH_RISCV_INSTS_VECTOR_HH__
