#include <inttypes.h>
#include <stdio.h>
#include <string.h>
#include <SFML/Graphics.h>

/* Interpreter flag constants */
#define RUN_FLAG 1
#define CRASH_FLAG 2
#define JUMP_FLAG 4
#define CARRY_FLAG 8
#define ZERO_FLAG 16

typedef uint32_t u32;

typedef int32_t i32;

typedef uint16_t u16;

typedef int16_t i16;

typedef uint8_t u8;

sfRenderWindow *window;

char rom_title[15];

// uh, this can be arbitrarily big I guess
// I'll probably heap-allocate this? But I've
// gone so far without heap allocation!
// Maybe when switching banks it should read
// the appropriate block from disk?
unsigned char rom_buffer[65536];

// Here's our machine!
typedef struct interp {
    // 14 general use registers
    u16 a;
    u16 b;
    u16 c;
    u16 d;

    u16 e;
    u16 f;
    u16 g;
    u16 h;

    u16 i;
    u16 j;
    u16 k;
    u16 l;

    u16 m;
    u16 n;

    // stack pointer register
    u16 sp;

    // program counter
    u16 pc;

    // interpreter flags
    u8 flags;

    // bank IDs (for accessing more RAM/ROM)
    // we always get the bottom 8k of RAM at
    // $8000 - $9FFF, and any other 8k of RAM
    // at $a000 - $bfff. we can swap banks by
    // writing to some specific memory address
    // that I haven't figured out yet
    u8 ram_bank;
    // similarly, we always get the bottom 16k
    // of ROM at $0000 - $3fff, and any other
    // 16k of ROM at $4000 - $7fff.
    u8 rom_bank;
    // then, the top 16k of address space is
    // mapped to hardware registers, video ram,
    // sound ram, etc. haven't quite worked
    // this all out yet

    // pointer to ROM data
    u8 *rom;

    // 64k of sweet sweet mem
    u8 mem[65536];
} interp;

void do_instr(interp *I);

void insert_string(u8 *mem, u16 offset, int length, char *str) {
    int i = 0;
    while (i < length) {
        mem[offset * 2 + i] = str[i];
        i++;
    }
}

u16 *get_reg(interp *I, char reg_id) {
    // given register id (0-7), return a pointer to the right register
    switch(reg_id) {
        case 0:  return &I->a;  break;
        case 1:  return &I->b;  break;
        case 2:  return &I->c;  break;
        case 3:  return &I->d;  break;
        case 4:  return &I->e;  break;
        case 5:  return &I->f;  break;
        case 6:  return &I->g;  break;
        case 7:  return &I->h;  break;
        case 8:  return &I->i;  break;
        case 9:  return &I->j;  break;
        case 10: return &I->k;  break;
        case 11: return &I->l;  break;
        case 12: return &I->m;  break;
        case 13: return &I->n;  break;
        case 14: return &I->sp; break;
        case 15: return &I->pc; break;
        default:
            fprintf(stderr, "internal error: invalid reg id %d\n", reg_id);
            return 0;
    }
}

// Uh, it's apparently implementation-defined for C as to
// whether right shift is arithmetic or logical. So we've
// got to work around it both ways, hooray.
// So now we have 'sra' (arithmetic right shift)
// and 'srl' (logical right shift).
u16 sra(u16 val, int amt) {
    int signbit = val & 0x8000;
    val = ((val >> amt) & 0x3fff) | signbit;
    return val;
}

u16 srl(u16 val, int amt) {
    int signbit = val & 0x8000;
    val = ((val >> amt) & 0x3fff) | (signbit ? (1 << (15 - amt)) : 0);
    return val;
}

void store_byte(interp *I, u16 addr, u8 value) {
    // Writing below $8000 does nothing - this is ROM, so
    // it's read only, obvi
    if (addr < 0x8000) {
        fprintf(stderr, "Attempt to write to ROM-mapped location $%04X "
                "(pc: $%04X)\n", addr, I->pc);
        return;
    }

    // 0x8000-0x9fff is always the first 8k of RAM
    if (addr < 0xa000) {
        I->mem[addr - 0x8000] = value;
        return;
    }

    // writing to 0xa000-0xbfff writes to the current banked chunk of RAM
    // (there are 8 of these, although #0 is always also mapped to
    // 0x8000-0x9fff)
    if (addr < 0xc000) {
        I->mem[addr - 0xa000 + I->ram_bank * 0x2000] = value;
        return;
    }

    printf("Unimplemented writing to thing!\n");
}

u8 load_byte(interp *I, u16 addr) {
    // Reading below $4000 returns stuff in first 16k of ROM, always
    if (addr < 0x4000) {
        return I->rom[addr];
    }

    // Reading $4000 - $7fff returns stuff in a different 16k chunk of ROM
    if (addr < 0x8000) {
        return I->rom[addr + I->rom_bank * 0x4000];
    }

    // Reading $8000 - $9fff returns values in first 8k of RAM
    if (addr < 0xa000) {
        return I->mem[addr - 0x8000];
    }

    // Reading $a000 - $bfff returns values in other 8k of RAM
    if (addr < 0xc000) {
        return I->mem[addr - 0xa000 + I->ram_bank * 0x2000];
    }

    printf("Unimplemented reading from thing!\n");
    return 0;
}

void store_word(interp *I, u16 addr, u16 value) {
    if (addr % 2 == 1) {
        fprintf(stderr, "Unaligned word write to $%04X (pc: $%04X)\n", addr, I->pc);
        return;
    }

    u8 hival = srl(value, 8) & 0xff;
    u8 loval = value & 0xff;

    store_byte(I, addr, hival);
    store_byte(I, addr + 1, loval);
}

u16 load_word(interp *I, u16 addr) {
    if (addr % 2 == 1) {
        fprintf(stderr, "Unaligned word read at $%04X (pc: $%04X)\n", addr, I->pc);
        return 0;
    }

    u8 hival = load_byte(I, addr);
    u8 loval = load_byte(I, addr+1);

    return ((u16)hival << 8) | loval;
}

int main (int argc, char **argv) {
    sfVideoMode mode = {192, 144, 32};
    window = sfRenderWindow_create(mode, "vs-16", sfClose, NULL);
    if (!window) {
        printf("Failed to initialize window.\n");
        return 1;
    }

    interp I;
    I.flags = RUN_FLAG;

    // program starts at 0x0100, after a 256-byte header
    I.pc = 0x0100;

    // stack starts at 0x9ffe so it's always available
    // and not in a bank that might get swapped out
    I.sp = 0x9ffe;

    if (argc != 2) {
        printf("Please supply a file name.\n");
        return 0;
    }

    FILE *rom = fopen(argv[1], "rb");

    size_t size = fread(rom_buffer, 1, sizeof(I.mem), rom);

    printf("Read %lu bytes from ROM.\n", size);

    I.rom = rom_buffer;

    strncpy(rom_title, (char*)&rom_buffer[2], 14);
    rom_title[14] = '\0';
    printf("Loaded: %s\n", rom_title);

    while (I.flags & RUN_FLAG) {
        do_instr(&I);
    }

    printf("==== FINAL STATE ====\n");
    printf("Reg: a: %04X b: %04X c: %04X d: %04X\n", I.a, I.b, I.c, I.d);
    printf("     e: %04X f: %04X g: %04X h: %04X\n", I.e, I.f, I.g, I.h);
    printf("     i: %04X j: %04X k: %04X l: %04X\n", I.i, I.j, I.k, I.l);
    printf("     m: %04X n: %04X S: %04X P: %04X\n", I.m, I.n, I.sp, I.pc);
}

void do_instr(interp *I) {
    u16 instr = load_word(I, I->pc);

    // first 4 bits are the opcode
    u16 instrtype = srl(instr, 12) & 0xf;

    printf("Instruction @ 0x%04X: 0x%04X\n", I->pc, instr);

    int ok = 0;

    // how much to increment pc by after performing instruction
    u8 pc_increment = 2;

    if (instrtype == 0x0) {
        // 0000: miscellaneous
        // get the 12 remaining bits
        u16 subcode = srl(instr, 8) & 0xf;
        u16 rest = instr & 0xff;
        if (subcode == 0) {
            // code 0 = 'special' instructions
            if (rest == 0xff) {
                // 0x00ff = STOP
                I->flags &= ~RUN_FLAG;
                printf("Stop.\n");
                ok = 1;
            } else if (rest == 0x01) {
                // 0x0001 = NOP
                ok = 1;
            } else if (rest == 0xaa) {
                // 0x00aa = RETURN
                // pops return address off stack and jumps to it
                u16 retaddr = load_word(I, I->sp);
                I->sp += 2;
                I->pc = retaddr;
                // don't increase pc
                pc_increment = 0;
                ok = 1;
            }
        } else if (subcode == 1) {
            // PUSH
            //      0000 0001 xxxx ----
            // xxxx = register to push
            I->sp -= 2;
            u8 reg_idx = srl(rest, 4);
            u16 *push_reg = get_reg(I, reg_idx);
            store_word(I, I->sp, *push_reg);
            ok = 1;
        } else if (subcode == 2) {
            // POP
            //      0000 0010 xxxx ----
            // xxxx = register to pop into
            u8 reg_idx = srl(rest, 4);
            u16 *pop_reg = get_reg(I, reg_idx);
            *pop_reg = load_word(I, I->sp);
            I->sp += 2;
            ok = 1;
        } else if (subcode == 3) {
            // Jump to register
            //      0000 0011 xxxx ----
            // xxx = register containing address to jump to
            u8 reg_idx = srl(rest, 4);
            u16 *jump_reg = get_reg(I, reg_idx);
            I->pc = *jump_reg;
            // don't increment pc
            pc_increment = 0;
            ok = 1;
        }
    } else if ((instrtype & 0x8) == 0x8) {
        // prefix 1 = arithmetic instructions
        //
        // format: 1oooooxx xxyyyyyy
        //
        //  ooooo = arithmetic operation
        //   xxxx = dest register (like x86, also a source for eg add)
        // yyyyyy = other src register, or special value

        ok = 1;

        u8 op       = srl(instr, 10) & 0x1f;
        u8 dest_idx = srl(instr,  6) &  0xf;
        u8 src_idx  = srl(instr,  0) & 0x3f;

        // again, reset MATH FLAGS
        I->flags &= ~CARRY_FLAG;
        I->flags &= ~ZERO_FLAG;

        u16 *dest = get_reg(I, dest_idx);

        u16 srcval;

        if (src_idx < 0x10) {
            // yy yyyy = 00 rrrr, where rrrr is register ID
            u16 *src = get_reg(I, src_idx & 0xf);
            srcval = *src;
        } else if (src_idx < 0x20) {
            // yy yyyy = 01 vvvv, where vvvv is a small immediate
            // value from 0-15 (can be used for immediate bitshifts,
            // bit testing, etc. where we only need these values)
            srcval = src_idx & 0xf;
        } else if (src_idx == 0x20) {
            // yy yyyy = 10 0000
            // this means there's a 16-bit immediate value following
            // the instruction; we use this as the second operand

            // Get the immediate value
            srcval = load_word(I, I->pc + 2);
            pc_increment += 2;
        } else {
            // uh I don't know what 0x2? or 0x3? should be yet
            // but we've got some room here to expand!
            fprintf(stderr, "Unknown source operand $%X for "
                    "arithmetic instruction\n", src_idx);
            srcval = 0;
            ok = 0;
        }

        if (op == 0x00) {
            // Move register/load immediate
            *dest = srcval;
        } else if (op == 0x01) {
            // Addition!
            if ((int)*dest + (int)srcval > 0xFFFF) I->flags |= CARRY_FLAG;
            *dest += srcval;
        } else if (op == 0x02) {
            // Subtraction
            if ((int)*dest - (int)srcval < 0) I->flags |= CARRY_FLAG;
            *dest -= srcval;
        } else if (op == 0x03) {
            // Unsigned multiplication
            if ((u32)*dest * (u32)srcval > 0xFFFF) I->flags |= CARRY_FLAG;
            *dest = (u16)((u32)*dest * (u32)srcval);
        } else if (op == 0x04) {
            // Signed multiplication

            // I think this is right? (I hope this is right...)
            // First convert to signed, then sign-extend. :/
            i32 sdest = (i32)(i16)*dest;
            i32 ssrc = (i32)(i16)srcval;
            if (sdest * ssrc >= 0x8000) I->flags |= CARRY_FLAG;
            *dest = (u16)(i16)(sdest * ssrc);

        } else if (op == 0x05) {
            // Unsigned division
            *dest /= srcval;
        } else if (op == 0x06) {
            // Signed division
            *dest = ((i16)*dest / (i16)srcval);
        } else if (op == 0x07) {
            // Unsigned modulo
            *dest = ((*dest % srcval) + srcval) % srcval;
        } else if (op == 0x08) {
            // Signed modulo
            // Non-stupid signed modulo, though
            *dest = (u16)(((i16)*dest % srcval) + srcval) % srcval;
        } else if (op == 0x09) {
            // Bitwise AND
            *dest &= srcval;
        } else if (op == 0x0a) {
            // Bitwise OR
            *dest |= srcval;
        } else if (op == 0x0b) {
            // Bitwise XOR
            *dest ^= srcval;
        } else if (op == 0x0c) {
            // Bitwise negation (doesn't use src)
            *dest = ~*dest;
        } else if (op == 0x0d) {
            // Arithmetic negation (doesn't use src)
            *dest = 0xFFFF - *dest + 1;
        } else if (op == 0x0e) {
            // Increment dest (doesn't use src)
            // (Sets carry flag if the thing wrapped around)
            if (*dest == 0xFFFF) I->flags |= CARRY_FLAG;
            (*dest)++;
        } else if (op == 0x0f) {
            // Decrement dest (doesn't use src)
            // (Also sets carry flag if the thing wrapped around)
            if (*dest == 0x0000) I->flags |= CARRY_FLAG;
            (*dest)--;
        } else if (op == 0x10) {
            // Logical left shift
            // (Also sets carry flag if the thing wrapped around)
            if (*dest >= 0x8000) I->flags |= CARRY_FLAG;
            *dest <<= srcval;
        } else if (op == 0x11) {
            // Logical right shift
            *dest = srl(*dest, srcval);
        } else if (op == 0x12) {
            // Arithmetic right shift
            *dest = sra(*dest, srcval);
        } else if (op == 0x13) {
            // Bit rotate left
            u8 amount = srcval & 0xf;
            *dest = srl(*dest, 16 - amount) | (u16)(*dest << amount);
        } else if (op == 0x14) {
            // Bit rotate right
            u8 amount = srcval & 0xf;
            *dest = (*dest << (16 - amount)) | srl(*dest, amount);
        } else if (op == 0x15) {
            // Bit test
            u8 bit = srcval & 0xf;
            if (!(*dest & (1 << bit))) {
                I->flags |= ZERO_FLAG;
            }
        /* unused operation space here */
        } else if (op == 0x1e) {
            // Unsigned comparison
            if (*dest < srcval) I->flags |= CARRY_FLAG;
            if (*dest == srcval) I->flags |= ZERO_FLAG;
        } else if (op == 0x1f) {
            // Signed comparison
            if ((i16)*dest < (i16)srcval) I->flags |= CARRY_FLAG;
            if (*dest == srcval) I->flags |= ZERO_FLAG;
        } else {
            ok = 0;
        }

        if (*dest == 0 && op < 0x1e) {
            I->flags |= ZERO_FLAG;
        }
    } else if ((instrtype & 0xc) == 0x4) {
        // 01??: Load/store instructions
        //      01ooxxxx00yyyyyy
        //     oo = operation type (load/store word/byte)
        //   xxxx = register to load/store into/from
        // yyyyyy = register w/ memory location (possibly imm. offset follows)

        ok = 1;

        u8 op     = srl(instr, 12) &  0x3;
        u8 reg_id = srl(instr,  8) &  0xf;
        u8 mem_id = srl(instr,  0) & 0x3f;

        u16 *reg = get_reg(I, reg_id);

        u16 addr;
        if (mem_id < 0x10) {
            // yy yyyy = 00 rrrr; address in register rrrr
            addr = *get_reg(I, mem_id & 0xf);
        } else if (mem_id < 0x20) {
            // yy yyyy = 01 rrrr; address in rrrr + imm. offset following
            addr = *get_reg(I, mem_id & 0xf);
            addr += load_word(I, I->pc + 2);
            pc_increment += 2;
        } else if (mem_id == 0x20) {
            // yy yyyy = 01 0000; no register, immediate address following
            addr = load_word(I, I->pc + 2);
            pc_increment += 2;
        } else {
            fprintf(stderr, "Unknown address mode $%X for load/store "
                    "(pc: $%04X)\n", mem_id, I->pc);
            addr = 0;
            ok = 0;
        }

        if (op == 0) {
            // Load word
            *reg = load_word(I, addr);
        } else if (op == 1) {
            // Load byte
            *reg = load_byte(I, addr);
        } else if (op == 2) {
            // Store word
            store_word(I, addr, *reg);
        } else if (op == 3) {
            // Store byte
            store_byte(I, addr, (*reg) & 0xff);
        }
    } else if ((instrtype & 0xe) == 0x2) {
        // 001?: jump
        //
        //      001oooaa aaaaaaaa
        //
        // ooo = jump type
        //          0: unconditional
        //          1: equal (ZF)
        //          2: not equal (~ZF)
        //          3: less than (CF)
        //          4: greater than or equal to (~CF)
        //          5: less than or equal to (ZF|CF)
        //          6: greater than (~(ZF|CF))
        //          7: subroutine (save return address)
        // a...= jump offset if relative jump
        //          (measured in words, so we can jump
        //           +/- 512 words, where instructions
        //           are either 1 or 2 words)
        //          (NOTE: if a = 0, uses an immediate
        //           value following the instruction as
        //           the address to jump to, rather than
        //           a relative jump direction)
        u8  op       = srl(instr, 10) &    0x7;
        u16 offset   = srl(instr,  0) & 0x03ff;

        u8 should_jump = 0;

        switch (op) {
            case 0: should_jump = 1; break;
            case 1: should_jump = (I->flags & ZERO_FLAG); break;
            case 2: should_jump = !(I->flags & ZERO_FLAG); break;
            case 3: should_jump = (I->flags & CARRY_FLAG); break;
            case 4: should_jump = !(I->flags & CARRY_FLAG); break;
            case 5: should_jump = (I->flags & (ZERO_FLAG | CARRY_FLAG)); break;
            case 6: should_jump = !(I->flags & (ZERO_FLAG | CARRY_FLAG)); break;
            case 7: should_jump = 1; break;
        }

        if (should_jump) {
            if (op == 7) {
                // push return address for subroutine call
                I->sp -= 2;
                store_word(I, I->sp, I->pc + 2);
            }

            if (offset != 0) {
                // relative jump
                // sign-extend from 10 to 16 bits
                u16 signbit = offset & 0x0200;
                i16 soffset = (signed) offset;

                if (signbit) {
                    soffset -= 0x0400;
                }

                I->pc += soffset * 2;
            } else {
                // absolute jump
                u16 new_addr = load_word(I, I->pc + 2);
                I->pc = new_addr;
            }

            // don't advance pc
            pc_increment = 0;
        } else if (offset == 0) {
            // if not jumping, need to jump over immediate address
            pc_increment += 2;
        }


        ok = 1;
    }
    /* unused instruction space: prefix 0001 isn't anything */

    if (!ok) {
        // TODO put up a dialogue box or something on error! jeez, rude
        printf("Unknown opcode: 0x%X\n", instr);
        // crash :(
        I->flags &= ~RUN_FLAG;
        I->flags |= CRASH_FLAG;
    } else {
        I->pc += pc_increment;
    }
}
