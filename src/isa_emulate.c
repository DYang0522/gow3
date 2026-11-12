#include "isa_emulate.h"

static unsigned char *isa_image;

void isa_emulate_set_image(unsigned char *image) { isa_image = image; }

static uint64_t width_mask(const IsaSite *site) {
    return site->width == 64 ? ~0ull : (1ull << site->width) - 1;
}

static unsigned char *effective_address(const IsaSite *site, const IsaRegs *regs) {
    if (site->mem_flags & ISA_MEM_RIP_RELATIVE) return isa_image + site->disp;
    uint64_t address = (uint64_t)site->disp;
    if (site->base != ISA_NO_REG) address += regs->gpr[site->base];
    if (site->index != ISA_NO_REG) address += regs->gpr[site->index] * site->scale;
    return (unsigned char *)(uintptr_t)address;
}

/* Unaligned accesses are valid on x86; the constant-size copies inline to plain moves. */
static uint64_t load(const IsaSite *site, const IsaRegs *regs) {
    unsigned char *at = effective_address(site, regs);
    if (site->width == 16) { uint16_t v; __builtin_memcpy(&v, at, 2); return v; }
    if (site->width == 32) { uint32_t v; __builtin_memcpy(&v, at, 4); return v; }
    uint64_t v;
    __builtin_memcpy(&v, at, 8);
    return v;
}

static void store(const IsaSite *site, const IsaRegs *regs, uint64_t value) {
    unsigned char *at = effective_address(site, regs);
    if (site->width == 16) { uint16_t v = (uint16_t)value; __builtin_memcpy(at, &v, 2); }
    else if (site->width == 32) { uint32_t v = (uint32_t)value; __builtin_memcpy(at, &v, 4); }
    else __builtin_memcpy(at, &value, 8);
}

/* The operand in a register slot; a slot left NO_REG holds the memory operand. */
static uint64_t read_operand(const IsaSite *site, const IsaRegs *regs, uint8_t reg) {
    if (reg == ISA_NO_REG) return load(site, regs);
    return regs->gpr[reg] & width_mask(site);
}

/* 64-bit writes replace the register, 32-bit ones zero-extend, 16-bit ones keep bits 16-63. */
static void write_dst(const IsaSite *site, IsaRegs *regs, uint64_t value) {
    if (site->dst == ISA_NO_REG) { store(site, regs, value); return; }
    uint64_t *reg = &regs->gpr[site->dst];
    if (site->width == 64) *reg = value;
    else if (site->width == 32) *reg = (uint32_t)value;
    else *reg = (*reg & ~0xffffull) | (value & 0xffff);
}

static void set_flags(IsaRegs *regs, uint64_t mask, uint64_t value) {
    regs->rflags = (regs->rflags & ~mask) | (value & mask);
}

static uint64_t byte_swap(const IsaSite *site, uint64_t value) {
    if (site->width == 16) return __builtin_bswap16((uint16_t)value);
    if (site->width == 32) return __builtin_bswap32((uint32_t)value);
    return __builtin_bswap64(value);
}

static uint64_t zf_sf(const IsaSite *site, uint64_t result) {
    return (result == 0 ? ISA_ZF : 0) | (result >> (site->width - 1) & 1 ? ISA_SF : 0);
}


/* IEEE-754 binary32 <-> binary16 conversion. No host F16C or floating-point instructions
 * are used, so this also runs on Sandy Bridge. */
static uint32_t half_to_float(uint16_t value) {
    uint32_t sign=(uint32_t)(value&0x8000u)<<16;
    unsigned exponent=(value>>10)&31u;
    uint32_t fraction=value&1023u;
    if (!exponent) {
        if (!fraction) return sign;
        int e=-14;
        while (!(fraction&0x400u)) { fraction<<=1; --e; }
        fraction&=0x3ffu;
        return sign|(uint32_t)(e+127)<<23|fraction<<13;
    }
    if (exponent==31) return sign|0x7f800000u|fraction<<13;
    return sign|(uint32_t)((int)exponent-15+127)<<23|fraction<<13;
}

static int round_up(uint32_t remainder, unsigned shift, uint32_t kept,
                    unsigned rounding, unsigned sign) {
    if (!remainder) return 0;
    switch (rounding) {
    case 1: return sign != 0;             /* toward -infinity */
    case 2: return sign == 0;             /* toward +infinity */
    case 3: return 0;                     /* toward zero */
    default: {
        uint32_t halfway=1u<<(shift-1);
        return remainder>halfway || (remainder==halfway && (kept&1u));
    }
    }
}

static uint16_t overflow_half(unsigned sign, unsigned rounding) {
    int infinity=rounding==0 || (rounding==1 && sign) || (rounding==2 && !sign);
    return (uint16_t)((sign<<15)|(infinity ? 0x7c00u : 0x7bffu));
}

static uint16_t float_to_half(uint32_t value, unsigned rounding) {
    unsigned sign=value>>31;
    unsigned exponent=(value>>23)&255u;
    uint32_t fraction=value&0x7fffffu;
    if (exponent==255) {
        if (!fraction) return (uint16_t)((sign<<15)|0x7c00u);
        uint16_t payload=(uint16_t)(fraction>>13)|0x0200u;
        return (uint16_t)((sign<<15)|0x7c00u|payload);
    }
    if (!exponent) {
        int away=(rounding==1 && sign) || (rounding==2 && !sign);
        return (uint16_t)((sign<<15)|(fraction && away ? 1u : 0u));
    }
    int e=(int)exponent-112;
    if (e>=31) return overflow_half(sign,rounding);
    if (e<=0) {
        if (e < -10) {
            int away=(rounding==1 && sign) || (rounding==2 && !sign);
            return (uint16_t)((sign<<15)|(away ? 1u : 0u));
        }
        uint32_t significand=fraction|0x800000u;
        unsigned shift=(unsigned)(14-e);
        uint32_t kept=significand>>shift;
        uint32_t remainder=significand&((1u<<shift)-1u);
        if (round_up(remainder,shift,kept,rounding,sign)) ++kept;
        return (uint16_t)((sign<<15)|kept);
    }
    uint32_t kept=((uint32_t)e<<10)|(fraction>>13);
    uint32_t remainder=fraction&0x1fffu;
    if (round_up(remainder,13,kept,rounding,sign)) ++kept;
    if (kept>=0x7c00u) return overflow_half(sign,rounding);
    return (uint16_t)((sign<<15)|kept);
}

static void emulate_f16c(const IsaSite *site, IsaRegs *regs) {
    const unsigned lanes=site->width==256 ? 8u : 4u;
    const unsigned rounding_code=site->reserved[0]&7u;
    const unsigned rounding=rounding_code==4 ? ((regs->mxcsr>>13)&3u) : rounding_code;
    const uint8_t *source=site->src1==ISA_NO_REG ? effective_address(site,regs) : regs->ymm[site->src1];
    if (site->kind==ISA_VCVTPH2PS) {
        if (site->dst==ISA_NO_REG) return;
        uint8_t *destination=regs->ymm[site->dst];
        memset(destination,0,32);
        for (unsigned i=0;i<lanes;++i) {
            uint16_t half;
            uint32_t single;
            __builtin_memcpy(&half,source+i*2,2);
            single=half_to_float(half);
            __builtin_memcpy(destination+i*4,&single,4);
        }
        return;
    }
    if (site->kind==ISA_VCVTPS2PH) {
        uint8_t converted[16]={0};
        for (unsigned i=0;i<lanes;++i) {
            uint32_t single;
            uint16_t half;
            __builtin_memcpy(&single,source+i*4,4);
            half=float_to_half(single,rounding);
            __builtin_memcpy(converted+i*2,&half,2);
        }
        if (site->dst==ISA_NO_REG) {
            __builtin_memcpy(effective_address(site,regs),converted,lanes*2);
        } else {
            memset(regs->ymm[site->dst],0,32);
            __builtin_memcpy(regs->ymm[site->dst],converted,lanes*2);
        }
    }
}

uint64_t isa_defined_flags(uint8_t kind) {
    switch (kind) {
    case ISA_ANDN: case ISA_BLSI: case ISA_BLSMSK: case ISA_BLSR: return ISA_SF | ISA_ZF | ISA_CF | ISA_OF;
    case ISA_BEXTR: return ISA_ZF | ISA_CF | ISA_OF;
    case ISA_LZCNT: case ISA_TZCNT: return ISA_CF | ISA_ZF;
    default: return 0;
    }
}

void ABI isa_emulate(const IsaSite *site, IsaRegs *regs) {
    if (site->kind==ISA_VCVTPH2PS || site->kind==ISA_VCVTPS2PH) {
        emulate_f16c(site,regs);
        return;
    }
    uint64_t mask = width_mask(site), defined = isa_defined_flags(site->kind);
    uint64_t width = site->width, source, result, flags;
    switch (site->kind) {
    case ISA_MOVBE_LOAD:
    case ISA_MOVBE_STORE:
        write_dst(site, regs, byte_swap(site, read_operand(site, regs, site->src1)));
        return;
    case ISA_LZCNT:
    case ISA_TZCNT:
        source = read_operand(site, regs, site->src1);
        if (source == 0) result = width;
        else if (site->kind == ISA_LZCNT) result = (uint64_t)__builtin_clzll(source) - (64 - width);
        else result = (uint64_t)__builtin_ctzll(source);
        flags = (source == 0 ? ISA_CF : 0) | (result == 0 ? ISA_ZF : 0);
        break;
    case ISA_ANDN:
        result = ~read_operand(site, regs, site->src1) & read_operand(site, regs, site->src2) & mask;
        flags = zf_sf(site, result);
        break;
    case ISA_BEXTR: {
        source = read_operand(site, regs, site->src1);
        uint64_t control = read_operand(site, regs, site->src2);
        uint64_t start = control & 0xff, length = control >> 8 & 0xff;
        result = 0;
        if (start < width && length != 0) {
            if (length > width - start) length = width - start;
            result = source >> start & (length == 64 ? ~0ull : (1ull << length) - 1);
        }
        flags = result == 0 ? ISA_ZF : 0;
        break;
    }
    case ISA_BLSI:
        source = read_operand(site, regs, site->src1);
        result = source & (0 - source) & mask;
        flags = zf_sf(site, result) | (source != 0 ? ISA_CF : 0);
        break;
    case ISA_BLSMSK:
        source = read_operand(site, regs, site->src1);
        result = (source ^ (source - 1)) & mask;
        flags = (zf_sf(site, result) & ISA_SF) | (source == 0 ? ISA_CF : 0);
        break;
    case ISA_BLSR:
        source = read_operand(site, regs, site->src1);
        result = source & (source - 1) & mask;
        flags = zf_sf(site, result) | (source == 0 ? ISA_CF : 0);
        break;
    default:
        return;
    }
    write_dst(site, regs, result);
    set_flags(regs, defined, flags);
}
