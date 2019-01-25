#include <inttypes.h>
#include <stdio.h>
#include <SFML/Graphics.h>

/* Interpreter flag constants */
#define RUN_FLAG 1
#define CRASH_FLAG 2

/* Flag register flag constants */
#define CARRY_FLAG 1
#define ZERO_FLAG 2

typedef uint16_t u16;

typedef uint8_t u8;

sfRenderWindow *window;

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

    // progRAM counter
    u16 p;

    // flag register
    // 16 is definitely way too many flags :/
    // Maybe we'll split this into like an 8-bit
    // flag register and an 8-bit normal register
    u16 f;

    // interpreter flags
    u8 flags;

    // 128k of sweet sweet RAM
    u8 RAM[131072];
} interp;

void do_instr(interp *I);

void insert_string(u8 *RAM, u16 offset, int length, char *str) {
    int i = 0;
    while (i < length) {
        RAM[offset * 2 + i] = str[i];
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
    val = ((val >> amt) & 0x3fff) | (signbit ? 0x4000 : 0);
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
    I.p = 0x80;
    insert_string(I.RAM, I.p, 12, "\x0a\x06\x0b\x06\x44\x01\x0a\x8b\x0b\x8c\x00\x00");

    while (I.flags & RUN_FLAG) {
        do_instr(&I);
    }

    printf("==== FINAL STATE ====\n");
    printf("Reg: a: %04X b: %04X c: %04X d: %04X\n", I.a, I.b, I.c, I.d);
    printf("     e: %04X s: %04X p: %04X f: %04X\n", I.e, I.s, I.p, I.f);
}

void do_instr(interp *I) {
    // it's big-endian, I guess? why not
    u16 instr = (I->RAM[I->p * 2] << 8) | I->RAM[I->p * 2 + 1];

    printf("Instruction @ 0x%04X: 0x%04X\n", I->p * 2, instr);

    // first 5 bits are the opcode
    u16 instrtype = (instr >> 11) & 0x1f;

    int ok = 0;

    if (instrtype == 0) {
        // code 0 = 'special' instructions
        // get the 11 remaining bits
        u16 rest = instr & 0x7f;
        if (rest == 0) {
            // 0x0000 = STOP
            I->flags &= ~RUN_FLAG;
            printf("Stop.\n");
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
        u8 op = instrtype & 3;
        u8 reg = (instr >> 8) & 7;
        u8 val = (instr & 0xff);

        // find which register we're looking at
        u16 *load_reg = get_reg(I, reg);
        if (op == 0) {
            // 01000 = load immediate
            // TODO: do we want to sign extend this? i think no but ??
            // TODO this should actually probably be in the LOAD category
            // and it should be like, load immediate low (we can then have
            // load immediate high...)
            *load_reg = val;
        } else if (op == 1) {
            // 01001 = add immediate
            // set carry flag if overflow
            if ((int)*load_reg + val > 0xFFFF) {
                I->f |= CARRY_FLAG;
            }
            *load_reg += val;
        } else if (op == 2) {
            // 01010 = multiply immediate
            // set carry flag if overflow
            if ((int)*load_reg * val > 0xFFFF) {
                I->f |= CARRY_FLAG;
            }
            *load_reg *= val;
        } else if (op == 3) {
            // 01011 = subtract immediate
            // set carry flag if UNDERflow
            if ((int)*load_reg - val < 0) {
                I->f |= CARRY_FLAG;
            }
            *load_reg -= val;
        } else if (op == 4) {
            // 01100 = bitwise and immediate
            *load_reg &= val;
        } else if (op == 5) {
            // 01101 = bitwise or immediate
            *load_reg |= val;
        } else if (op == 6) {
            // 01110 = bitwise xor immediate
            *load_reg ^= val;
        } else if (op == 7) {
            // 01111 = bitshift immediate
            // (01111xxx1 = bitshift left; 01111xxx0 = bitshift right)
            if (val & 0x80) {
                // we mask off the high bits since they're flags
                // (not all of them are used, but yknow...)
                *load_reg <<= (val & 0xf);
            } else {
                // logical shift right
                *load_reg = srl(*load_reg, val & 0xf);
                // TODO: we never need more than 4 bits for
                // the immediate value on this one (since uh,
                // shifting by more than 15 is useless),
                // so there's definitely room to add an
                // arithmetic right shift as well... :O
                // and bit rotation would also be cool.
                // and bit testing. we can stuff a lot in
                // here, basically we have 16 instructions'
                // worth of space in here as long as their
                // immediate thing isn't more than 4 bits
                *load_reg &= ~0x8000;
            }
            ok = 1;
        }
        ok = 1;

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
        u8 op = instr & 0x1f;
        u8 src_idx  = (instr &  0xe0) >> 5;
        u8 dest_idx = (instr & 0x700) >> 8;

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
            *dest = -*dest;
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
        signed offset = instr & 0x07;
        u8 op       = (instr &  0x18) >> 3;
        u8 addr_idx = (instr &  0xe0) >> 5;
        u8 dest_idx = (instr & 0x700) >> 8;

        if (offset > 4) offset -= 8;

        u16 *addr = get_reg(I, addr_idx);
        u16 *dest = get_reg(I, dest_idx);

        if (op == 0) {
            // Load word
            *dest = (I->RAM[(*addr + offset) * 2] << 8) | I->RAM[(*addr + offset) * 2 + 1];
        } else if (op == 1) {
            // Load low byte into low byte of register
            // (leaves high byte untouched)
            *dest &= 0xff00;
            *dest |= I->RAM[(*addr + offset) * 2 + 1];
        } else if (op == 2) {
            // Load high byte into low byte of register
            // (leaves high byte untouched)
            *dest &= 0xff00;
            *dest |= I->RAM[(*addr + offset) * 2 + 1];
        } else if (op == 3) {
            // Load high byte into high byte of register
            // (leaves low byte untouched)
            *dest &= 0x00ff;
            *dest |= I->RAM[(*addr + offset) * 2 + 1] << 8;
        }
        ok = 1;
    } else if (instrtype == 3) {
        // 00011: Store instructions
        // Same as the above format.
        //      00010xxxyyyzzwww
        // xxx = value register (stored from)
        // yyy = address register (points into memory)
        //  zz = operation type (store word, store high/low byte)
        // www = offset, measured in 16-bit words
        //       (signed, except 100 = +4, not -3)
        signed offset = instr & 0x07;
        u8 op       = (instr &  0x18) >> 3;
        u8 addr_idx = (instr &  0xe0) >> 5;
        u8 src_idx  = (instr & 0x700) >> 8;

        if (offset > 4) offset -= 8;

        u16 *addr = get_reg(I, addr_idx);
        u16 *src  = get_reg(I, src_idx);

        if (op == 0) {
            // Store word
            I->RAM[(*addr + offset) * 2]     = (*src & 0xff00) >> 8;
            I->RAM[(*addr + offset) * 2 + 1] = (*src & 0x00ff);
        } else if (op == 1) {
            // Store low byte of register into low byte of memory word
            // (leaves high byte of word untouched)
            I->RAM[(*addr + offset) * 2 + 1] = (*src & 0x00ff);
        } else if (op == 2) {
            // Store low byte of register into high byte of memory word
            // (leaves low byte of word untouched)
            I->RAM[(*addr + offset) * 2] = (*src & 0xff00) >> 8;
        } else if (op == 3) {
            // Load high byte into high byte of register
            // (leaves low byte untouched)
            *src &= 0x00ff;
            *src |= I->RAM[(*addr + offset) * 2 + 1] << 8;
        }
        ok = 1;
    }

    if (!ok) {
        // TODO put up a dialogue box or something on error! jeez, rude
        printf("Unknown opcode: 0x%X\n", instr);
        // crash :(
        I->flags &= ~RUN_FLAG;
        I->flags |= CRASH_FLAG;
    } else {
        I->p ++;
    }
}
