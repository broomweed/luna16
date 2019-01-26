#include <inttypes.h>
#include <stdio.h>
#include <string.h>
#include <SFML/Graphics.h>

/* Interpreter flag constants */
#define RUN_FLAG 1
#define CRASH_FLAG 2
#define JUMP_FLAG 4

/* Flag register flag constants */
#define CARRY_FLAG 1
#define ZERO_FLAG 2

typedef uint16_t u16;

typedef uint8_t u8;

sfRenderWindow *window;

char rom_title[61];

// Here's our machine!
typedef struct interp {
    // general use registers
    u16 a;
    u16 b;
    u16 c;
    u16 d;
    u16 e;

    // stack pointer register
    u16 s;

    // program counter
    u16 p;

    // flag register
    // 16 is definitely way too many flags :/
    // Maybe we'll split this into like an 8-bit
    // flag register and an 8-bit normal register
    u16 f;

    // interpreter flags
    u8 flags;

    // 128k of sweet sweet mem
    u8 mem[131072];
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
    // (yeah, 'return a pointer to the register' makes perfect
    //  sense, shut up :P)
    switch(reg_id) {
        case 0:
            return &I->a; break;
        case 1:
            return &I->b; break;
        case 2:
            return &I->c; break;
        case 3:
            return &I->d; break;
        case 4:
            return &I->e; break;
        case 5:
            return &I->s; break;
        case 6:
            return &I->p; break;
        case 7:
            return &I->f; break;
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

int main (int argc, char **argv) {
    sfVideoMode mode = {192, 144, 32};
    window = sfRenderWindow_create(mode, "vs-16", sfClose, NULL);
    if (!window) {
        printf("Failed to initialize window.\n");
        return 1;
    }

    interp I;
    I.flags = RUN_FLAG;

    // program starts at 0x0080, after a 256-byte header
    I.p = 0x0080;

    // stack starts at 0xcfff (will probably change this once
    // we decide what the memory layout looks like)
    I.s = 0xcfff;

    if (argc != 2) {
        printf("Please supply a file name.\n");
        return 0;
    }

    FILE *rom = fopen(argv[1], "rb");

    size_t size = fread(I.mem, 1, sizeof(I.mem), rom);

    printf("Read %lu bytes from ROM.\n", size);

    strncpy(rom_title, (char*)&I.mem[4], 60);
    rom_title[60] = '\0';
    printf("Loaded: %s\n", rom_title);

    /*printf("==== INITIAL RAM STATE ====\n");
    for (int i = 0; i < 512; i++) {
        if (i % 32 == 0) printf("%04X  ", i);
        printf("%02x ", I.mem[i]);
        if (i % 32 == 31) printf("\n");
    }*/

    while (I.flags & RUN_FLAG) {
        do_instr(&I);
    }

    printf("==== FINAL STATE ====\n");
    printf("Reg: a: %04X b: %04X c: %04X d: %04X\n", I.a, I.b, I.c, I.d);
    printf("     e: %04X s: %04X p: %04X f: %04X\n", I.e, I.s, I.p, I.f);
}

void do_instr(interp *I) {
    // it's big-endian, I guess? why not
    u16 instr = (I->mem[I->p * 2] << 8) | I->mem[I->p * 2 + 1];

    printf("Instruction @ 0x%04X: 0x%04X\n", I->p * 2, instr);

    // first 5 bits are the opcode
    u16 instrtype = (instr >> 11) & 0x1f;

    int ok = 0;

    // reset jump flag
    I->flags &= ~JUMP_FLAG;

    if (instrtype == 0) {
        // type 0: miscellaneous
        // get the 11 remaining bits
        u16 subcode = (instr >> 8) & 7;
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
                u16 retaddr = (I->mem[I->s * 2 + 2] << 8) | (I->mem[I->s * 2 + 3]);
                I->s ++;
                I->p = retaddr;
                I->flags |= JUMP_FLAG;
                ok = 1;
            }
        } else if (subcode == 1) {
            // PUSH
            //      00000 001 xxx-----
            // xxx = register to push
            u8 reg_idx = srl(rest, 5);
            u16 *push_reg = get_reg(I, reg_idx);
            I->mem[I->s * 2] = srl(*push_reg, 8);
            I->mem[I->s * 2 + 1] = *push_reg & 0x00ff;
            I->s --;
            ok = 1;
        } else if (subcode == 2) {
            // POP
            //      00000 010 xxx-----
            // xxx = register to push
            u8 reg_idx = srl(rest, 5);
            u16 *push_reg = get_reg(I, reg_idx);
            *push_reg = (I->mem[I->s * 2 + 2] << 8) | (I->mem[I->s * 2 + 3]);
            I->s ++;
            ok = 1;
        } else if (subcode == 3) {
            // Jump to register
            //      00000 011 xxx-----
            // xxx = register containing address to jump to
            u8 reg_idx = srl(rest, 5);
            u16 *jump_reg = get_reg(I, reg_idx);
            I->p = *jump_reg;
            I->flags |= JUMP_FLAG;
            ok = 1;
        }
    } else if ((instrtype & ~7) == 8) {
        // code 01xxx = immediate instrs
        // (~7 masks out the three bottom bits)
        // next 3 bits are the register to operate on
        // and the following 8 bytes are the immediate value

        // reset flags for MATH!!
        I->f &= ~CARRY_FLAG;
        I->f &= ~ZERO_FLAG;

        // break up the bits
        u8 op = instrtype & 7;
        u8 reg = (instr >> 8) & 7;
        u8 val = (instr & 0xff);

        // find which register we're looking at
        u16 *load_reg = get_reg(I, reg);
        if (op == 0) {
            // 01000 = load immediate
            *load_reg = val;
            ok = 1;
        } else if (op == 1) {
            // 01001 = add immediate
            // set carry flag if overflow
            if ((int)*load_reg + val > 0xFFFF) {
                I->f |= CARRY_FLAG;
            }
            *load_reg += val;
            ok = 1;
        } else if (op == 2) {
            // 01010 = multiply immediate
            // set carry flag if overflow
            if ((int)*load_reg * val > 0xFFFF) {
                I->f |= CARRY_FLAG;
            }
            *load_reg *= val;
            ok = 1;
        } else if (op == 3) {
            // 01011 = subtract immediate
            // set carry flag if UNDERflow
            if ((int)*load_reg - val < 0) {
                I->f |= CARRY_FLAG;
            }
            *load_reg -= val;
            ok = 1;
        } else if (op == 4) {
            // 01100 = bitwise and immediate
            *load_reg &= val;
            ok = 1;
        } else if (op == 5) {
            // 01101 = bitwise or immediate
            *load_reg |= val;
            ok = 1;
        } else if (op == 6) {
            // 01110 = bitwise xor immediate
            *load_reg ^= val;
            ok = 1;
        } else if (op == 7) {
            // 01111 = bit operations immediate
            u8 optype = val & 0xf0;
            u8 amount = val & 0x0f;
            if (optype == 0x80) {
                // 01111xxx1000yyyy = bitshift left (by yyyy bits)
                *load_reg <<= amount;
                ok = 1;
            } else if (optype == 0x90) {
                // 01111xxx1001yyyy = logical bitshift right
                *load_reg = srl(*load_reg, amount);
                ok = 1;
            } else if (optype == 0xa0) {
                // 01111xxx1010yyyy = arithmetic bitshift right
                *load_reg = sra(*load_reg, amount);
                ok = 1;
            } else if (optype == 0xb0) {
                // 01111xxx1011yyyy = bit rotate left
                *load_reg = srl(*load_reg, 16 - amount) | (u16)(*load_reg << amount);
                ok = 1;
            } else if (optype == 0xc0) {
                // 01111xxx1100yyyy = bit rotate right
                *load_reg = (*load_reg << (16 - amount)) | srl(*load_reg, amount);
                ok = 1;
            } else if (optype == 0xd0) {
                // 01111xxx1101yyyy = bit test
                if (!(*load_reg & (1 << amount))) {
                    I->f |= ZERO_FLAG;
                }
                ok = 1;
            }
        }

        if (*load_reg == 0) {
            I->f |= ZERO_FLAG;
        }
    } else if (instrtype == 1) {
        // 00001 = arithmetic register instructions
        //
        // format: 00001xxxyyyzzzzz
        //
        // xxx = dest register (like x86, also a source for eg add)
        // yyy = other src register
        // zzzzz = arithmetic operation
        //
        // dang, I really wish C had binary literals -- they'd make
        // these bitmasks a lot clearer.
        // (although it should be p. clear from the format given above
        // what we're doing here.)
        u8 op       =  instr & 0x1f;
        u8 src_idx  = (instr >> 5) & 0x7;
        u8 dest_idx = (instr >> 8) & 0x7;

        // again, reset MATH FLAGS
        I->f &= ~CARRY_FLAG;
        I->f &= ~ZERO_FLAG;

        u16 *dest = get_reg(I, dest_idx);
        u16 *src = get_reg(I, src_idx);

        if (op == 0x00) {
            // Addition!
            if ((int)*dest + (int)*src > 0xFFFF) I->f |= CARRY_FLAG;
            *dest += *src;
            ok = 1;
        } else if (op == 0x01) {
            // Subtraction
            if ((int)*dest - (int)*src < 0) I->f |= CARRY_FLAG;
            *dest -= *src;
            ok = 1;
        } else if (op == 0x02) {
            // Multiplication
            if ((uint32_t)*dest * (uint32_t)*src > 0xFFFF) I->f |= CARRY_FLAG;
            *dest -= *src;
            ok = 1;
        } else if (op == 0x03) {
            // Bitwise AND
            *dest &= *src;
            ok = 1;
        } else if (op == 0x04) {
            // Bitwise OR
            *dest |= *src;
            ok = 1;
        } else if (op == 0x05) {
            // Bitwise XOR
            *dest ^= *src;
            ok = 1;
        } else if (op == 0x06) {
            // Bitwise negation (doesn't use src)
            *dest = ~*dest;
            ok = 1;
        } else if (op == 0x07) {
            // Arithmetic negation (doesn't use src)
            *dest = 0xFFFF - *dest + 1;
            ok = 1;
        } else if (op == 0x08) {
            // Increment dest (doesn't use src)
            // (Sets carry flag if the thing wrapped around)
            if (*dest == 0xFFFF) I->f |= CARRY_FLAG;
            (*dest)++;
            ok = 1;
        } else if (op == 0x09) {
            // Decrement dest (doesn't use src)
            // (Also sets carry flag if the thing wrapped around)
            if (*dest == 0x0000) I->f |= CARRY_FLAG;
            (*dest)--;
            ok = 1;
        } else if (op == 0x0a) {
            // Logical left shift
            // (Also sets carry flag if the thing wrapped around)
            if (*dest >= 0x8000) I->f |= CARRY_FLAG;
            *dest <<= *src;
            ok = 1;
        } else if (op == 0x0b) {
            // Logical right shift
            *dest = srl(*dest, *src);
            ok = 1;
        } else if (op == 0x0c) {
            // Arithmetic right shift
            *dest = sra(*dest, *src);
            ok = 1;
        } else if (op == 0x0d) {
            // Comparison
            // Basically the same as SUB dest, src - except it
            // doesn't actually place the result in dest, it
            // just sets the flags.
            if (*dest < *src) I->f |= CARRY_FLAG;
            if (*dest == *src) I->f |= ZERO_FLAG;
            ok = 1;
        } else if (op == 0x0e) {
            // Bit rotate left
            u8 amount = *src & 0xf;
            *dest = srl(*dest, 16 - amount) | (u16)(*dest << amount);
            ok = 1;
        } else if (op == 0x0f) {
            // Bit rotate right
            u8 amount = *src & 0xf;
            *dest = (*dest << (16 - amount)) | srl(*dest, amount);
            ok = 1;
        } else if (op == 0x10) {
            // Bit test
            u8 bit = *src & 0xf;
            if (!(*dest & (1 << bit))) {
                I->f |= ZERO_FLAG;
            }
            ok = 1;
        }

        if (*dest == 0) {
            I->f |= ZERO_FLAG;
        }
    } else if (instrtype == 2) {
        // 00010: Load instructions
        // The format is similar-ish to the arithmetic instructions above:
        //      00010xxxyyyzzwww
        // xxx = value register (loaded into)
        // yyy = address register (points into memory)
        //  zz = operation type (load word, load high/low byte)
        // www = offset, measured in 16-bit words
        //       (signed, except 100 = +4, not -3)
        signed offset =  instr &  0x07;
        u8 op         = (instr &  0x18) >> 3;
        u8 addr_idx   = (instr &  0xe0) >> 5;
        u8 dest_idx   = (instr & 0x700) >> 8;

        if (offset > 4) offset -= 8;

        u16 *addr = get_reg(I, addr_idx);
        u16 *dest = get_reg(I, dest_idx);

        if (op == 0) {
            // Load word
            *dest = (I->mem[(*addr + offset) * 2] << 8) | I->mem[(*addr + offset) * 2 + 1];
        } else if (op == 1) {
            // Load low byte into low byte of register
            // (leaves high byte untouched)
            *dest &= 0xff00;
            *dest |= I->mem[(*addr + offset) * 2 + 1];
        } else if (op == 2) {
            // Load high byte into low byte of register
            // (leaves high byte untouched)
            *dest &= 0xff00;
            *dest |= I->mem[(*addr + offset) * 2 + 1];
        } else if (op == 3) {
            // Load high byte into high byte of register
            // (leaves low byte untouched)
            *dest &= 0x00ff;
            *dest |= I->mem[(*addr + offset) * 2 + 1] << 8;
        }
        ok = 1;
    } else if (instrtype == 3) {
        // 00011: Store instructions
        // Same as the above format.
        //      00011xxxyyyzzwww
        // xxx = value register (stored from)
        // yyy = address register (points into memory)
        //  zz = operation type (store word, store high/low byte)
        // www = offset, measured in 16-bit words
        //       (signed, except 100 = +4, not -3)
        signed offset =  instr &  0x07;
        u8 op         = (instr &  0x18) >> 3;
        u8 addr_idx   = (instr &  0xe0) >> 5;
        u8 src_idx    = (instr & 0x700) >> 8;

        if (offset > 4) offset -= 8;

        u16 *addr = get_reg(I, addr_idx);
        u16 *src  = get_reg(I, src_idx);

        if (op == 0) {
            // Store word
            I->mem[(*addr + offset) * 2] = srl(*src, 8);
            I->mem[(*addr + offset) * 2 + 1] = (*src & 0x00ff);
        } else if (op == 1) {
            // Store low byte of register into low byte of memory word
            // (leaves high byte of word untouched)
            I->mem[(*addr + offset) * 2 + 1] = *src & 0x00ff;
        } else if (op == 2) {
            // Store low byte of register into high byte of memory word
            // (leaves low byte of word untouched)
            I->mem[(*addr + offset) * 2] = *src & 0x00ff;
        } else if (op == 3) {
            // Store high byte of register into low byte of memory word
            // (leaves high byte of word untouched)
            I->mem[(*addr + offset) * 2 + 1] = srl(*src, 8);
            *src &= 0x00ff;
            *src |= I->mem[(*addr + offset) * 2 + 1] << 8;
        }
        ok = 1;
    } else if (instrtype == 4) {
        // 00100: Move instruction(s?)
        //      00100xxxyyy00zzz
        // xxx = destination register
        // yyy = source register
        // zzz = move operation
        u8 op = instr & 3;
        u8 src_idx  = (instr >> 5) & 7;
        u8 dest_idx = (instr >> 8) & 7;

        u16 *src  = get_reg(I, src_idx);
        u16 *dest = get_reg(I, dest_idx);

        if (op == 0) {
            // 000: Regular move
            *dest = *src;
            ok = 1;
        } else if (op == 1) {
            // 001: Move high byte to low byte
            // (leave high byte of dest unchanged)
            *dest = (*dest & 0xff00) | (*src & 0xff00);
            ok = 1;
        } else if (op == 2) {
            // 010: Move low byte to high byte
            // (leave low byte of dest unchanged)
            *dest = (*src & 0x00ff) | (*dest & 0x00ff);
            ok = 1;
        } else if (op == 3) {
            // 011: Move high byte to high byte
            // (leave low byte of dest unchanged)
            *dest = (*src & 0xff00) | (*dest & 0x00ff);
            ok = 1;
        } else if (op == 4) {
            // 100: Move low byte to low byte
            // (leave high byte of dest unchanged)
            *dest = (*dest & 0xff00) | (*src & 0x00ff);
            ok = 1;
        } else if (op == 5) {
            // 101: Exchange two registers
            *dest ^= *src;
            *src ^= *dest;
            *dest ^= *src;
            ok = 1;
        }
    } else if (instrtype == 6) {
        // 00110: Load upper immediate
        //
        // This one stores an 8-bit value in the high
        // byte of a register, and zeroes out the low
        // byte. You can load an immediate word by
        // doing a load-upper-immediate, followed by
        // an or-immediate of the lower byte value.
        //
        //      00110xxxIIIIIIII
        //       xxx = destination register
        // IIIIIIIII = 8-bit immediate value
        u8 dest_idx = (instr >> 8) & 7;
        u8 immediate = (instr & 0xff);

        u16 *dest = get_reg(I, dest_idx);

        *dest = immediate << 8;
        ok = 1;
    } else if ((instrtype & 0x1c) == 0x18) {
        // 110??: unconditional absolute jump
        //
        //      110JJJJJ JJJJJJJJ
        //
        // JJJ... is a 14-bit address representing
        // the location to jump to. It's expanded
        // to a full address because the low bit
        // is always 0 (long-jump targets have to
        // be aligned to 4-byte boundaries), and
        // it uses the high 2 bits of the current PC
        // for the high bits of the target as well -
        // to jump out of the current 32K page, use
        // the register jump instruction.
        u16 old_p = I->p;
        u16 new_p = instr & 0x1fff;
        I->p = (old_p & 0xc000) | (new_p << 1);
        I->flags |= JUMP_FLAG;
        ok = 1;
    } else if ((instrtype & 0x1c) == 0x1c) {
        // 111??: function call
        //
        //      111JJJJJ JJJJJJJJ
        //
        // This is like the unconditional jump, but
        // pushes the address of the next instruction
        // onto the stack before jumping, so that we
        // can jump back with the 'ret' instruction.
        u16 old_p = I->p;
        u16 new_p = instr & 0x1fff;
        I->p = (old_p & 0xc000) | (new_p << 1);
        I->mem[I->s * 2] = srl(old_p + 1, 8);
        I->mem[I->s * 2 + 1] = (old_p + 1) & 0x00ff;
        I->s --;
        I->flags |= JUMP_FLAG;
        ok = 1;
    } else if ((instrtype & 0x18) == 0x10) {
        // 10xx?: conditional jumps
        //
        //      10xxJJJJ JJJJJJJJ
        //
        // Here, JJJ... is a relative jump amount
        // forwards or backwards. So we can jump
        // +/- 2^11 words in memory (= 2048
        // instructions.) If you need to jump
        // further, you can branch somewhere close
        // by, then use an unconditional or register
        // jump from there.
        u8 op = (instr >> 12) & 3;
        int16_t jump = instr & 0x0fff;
        if (jump > 0x07ff) jump -= 0x1000;

        u8 cond;

        if (op == 0) {
            // Branch if equal (i.e. is zero flag set)
            cond = I->f & ZERO_FLAG;
        } else if (op == 1) {
            // Branch if not equal
            cond = ~(I->f & ZERO_FLAG);
        } else if (op == 2) {
            // Branch if less than (ie. carry flag set)
            cond = I->f & CARRY_FLAG;
        } else if (op == 3) {
            // Branch if greater than (neither carry flag nor zero flag set)
            cond = ~(I->f & (CARRY_FLAG | ZERO_FLAG));
        }

        if (cond) {
            I->p += jump;
            I->flags |= JUMP_FLAG;
        }
    }

    if (!ok) {
        // TODO put up a dialogue box or something on error! jeez, rude
        printf("Unknown opcode: 0x%X\n", instr);
        // crash :(
        I->flags &= ~RUN_FLAG;
        I->flags |= CRASH_FLAG;
    } else if (!(I->flags & JUMP_FLAG)) {
        // only advance PC if we didn't just do a jump
        I->p ++;
    }
}
