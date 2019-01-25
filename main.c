#include <inttypes.h>
#include <stdio.h>
#include <SFML/Graphics.h>

#define RUN_FLAG 1
#define CARRY_FLAG 1

typedef uint16_t u16;

typedef uint8_t u8;

sfRenderWindow *window;

typedef struct interp {
    u16 a;
    u16 b;
    u16 c;
    u16 d;
    u16 e;

    u16 f;

    u16 s;
    u16 p;

    u8 flags;

    // 128k of sweet sweet RAM
    u8 ram[131072];
} interp;

void do_instr(interp *I);

void insert_string(u8 *ram, u16 offset, char *str) {
    int i = 0;
    while (str[i]) {
        ram[offset * 2 + i] = str[i];
        i++;
    }
}

u16 *get_reg(interp *I, char reg_id) {
    // given register id (0-7), return a pointer to the right one
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

int main (int argc, char **argv) {
    sfVideoMode mode = {192, 144, 32};
    window = sfRenderWindow_create(mode, "acs-16", sfClose, NULL);
    if (!window) {
        printf("Failed to initialize window.\n");
        return 1;
    }

    interp I;
    I.flags = RUN_FLAG;
    I.p = 0x80;
    insert_string(I.ram, I.p, "\x41\x02\x49\xff\x51\x10\x59\x01\x51\x10\x58\x01\x00\x00");

    while (I.flags & RUN_FLAG) {
        do_instr(&I);
    }

    printf("==== FINAL STATE ====\n");
    printf("Reg: a: %04X b: %04X c: %04X d: %04X\n", I.a, I.b, I.c, I.d);
    printf("     e: %04X s: %04X p: %04X f: %04X\n", I.e, I.s, I.p, I.f);
}

void do_instr(interp *I) {
    // it's big-endian, I guess? why not
    u16 instr = (I->ram[I->p * 2] << 8) | I->ram[I->p * 2 + 1];

    printf("Instruction @ 0x%X: 0x%X\n", I->p * 2, instr);

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

        // reset carry flag
        I->f &= ~CARRY_FLAG;

        // break up the bits
        u8 op = instrtype & 3;
        u8 reg = (instr >> 8) & 7;
        u8 val = (instr & 0xff);

        // find which register we're looking at
        u16 *load_reg = get_reg(I, reg);
        if (op == 0) {
            // 01000 = load immediate
            // TODO: do we want to sign extend this? i think no but ??
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
        }
    }

    if (!ok) {
        printf("Unknown opcode: 0x%X\n", instr);
        // crash :(
        I->flags &= ~RUN_FLAG;
    } else {
        I->p ++;
    }
}
