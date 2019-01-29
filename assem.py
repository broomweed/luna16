from assemlex import lexer
from assemparse import parser
import sys

instr_table = {}

reg_table = {
    'a':  0,  'b':  1,  'c':  2,  'd':  3,
    'e':  4,  'f':  5,  'g':  6,  'h':  7,
    'i':  8,  'j':  9,  'k': 10,  'l': 11,
    'm': 12,  'n': 13, 'sp': 14, 'pc': 15
}

label_table = {}

jump_list = []

current_position = 0

class AssembleError(Exception):
    pass

def table(instruction):
    def decorator(func):
        instr_table[instruction] = func
        return func
    return decorator

def get_reg(r):
    name = r[1].lower()
    try:
        return reg_table[name]
    except KeyError:
        raise AssembleError("%s: no such register" % name)

@table('stop')
def i_stop():
    return bytes([0x00, 0xFF])

@table('nop')
def i_nop():
    return bytes([0x00, 0x01])

@table('ret')
def i_nop():
    return bytes([0x00, 0xaa])

@table('push')
def i_push(reg):
    if type(reg) != tuple or reg[0] != 'reg':
        raise AssembleError("PUSH can accept only a register as an argument")
    return bytes([0x01, get_reg(reg) << 4])

@table('pop')
def i_pop(reg):
    if type(reg) != tuple or reg[0] != 'reg':
        raise AssembleError("POP can accept only a register as an argument")
    return bytes([0x02, get_reg(reg) << 4])

@table('jr')
def i_jr(reg):
    if type(reg) != tuple or reg[0] != 'reg':
        raise AssembleError("JR can accept only a register as an argument")
    return bytes([0x03, get_reg(reg) << 4])

def gen_arith(code):
    ''' Generate an arithmetic instruction with
        operation id `code`, taking two arguments. '''

    def inner(dest, src):
        if type(dest) != tuple or dest[0] != 'reg':
            raise AssembleError(
                'destination operand for arithmetic operation must be a register'
            )

        xxxx = get_reg(dest)
        xx1 = (xxxx >> 2) & 3
        xx2 = xxxx & 3

        if type(src) == tuple and src[0] == 'reg':
            # Register
            yyyyyy = get_reg(src)
        if type(src) == int:
            # Immediate operand
            if 0 <= src <= 15:
                # Small immediate operand
                yyyyyy = 0x10 & src
            elif src == -1:
                # Negative one
                yyyyyy = 0x21
            else:
                # Big immediate operand
                yyyyyy = 0x20

        result = bytes([ 0x80 | (code << 2) | xx1, (xx2 << 6) | yyyyyy ])

        if yyyyyy == 0x20:
            result += bytes([ (src & 0xff00) >> 8, src & 0x00ff ])

        return result

    return inner

def gen_arith_unary(code):
    ''' Generate an arithmetic instruction with
        operation id `code`, taking only one argument. '''

    def inner(dest):
        if type(dest) != tuple or dest[0] != 'reg':
            raise AssembleError(
                'destination operand for arithmetic operation must be a register'
            )

        xxxx = get_reg(dest)
        xx1 = (xxxx >> 2) & 3
        xx2 = xxxx & 3

        result = bytes([ 0x80 | (code << 2) | xx1, (xx2 << 6) ])

        return result

    return inner

instr_table['mov']  = gen_arith(0x00)
instr_table['add']  = gen_arith(0x01)
instr_table['sub']  = gen_arith(0x02)
instr_table['mul']  = gen_arith(0x03)
instr_table['muls'] = gen_arith(0x04)
instr_table['div']  = gen_arith(0x05)
instr_table['divs'] = gen_arith(0x06)
instr_table['mod']  = gen_arith(0x07)
instr_table['mods'] = gen_arith(0x08)
instr_table['and']  = gen_arith(0x09)
instr_table['or']   = gen_arith(0x0a)
instr_table['xor']  = gen_arith(0x0b)
instr_table['cpl']  = gen_arith_unary(0x0c)
instr_table['neg']  = gen_arith_unary(0x0d)
instr_table['inc']  = gen_arith_unary(0x0e)
instr_table['dec']  = gen_arith_unary(0x0f)
instr_table['sll']  = gen_arith(0x10)
instr_table['srl']  = gen_arith(0x11)
instr_table['sra']  = gen_arith(0x12)
instr_table['rbl']  = gen_arith(0x13)
instr_table['rbr']  = gen_arith(0x14)
instr_table['bit']  = gen_arith(0x15)
instr_table['addc'] = gen_arith(0x16)
instr_table['subc'] = gen_arith(0x17)
instr_table['mulc'] = gen_arith(0x18)

instr_table['cmp']  = gen_arith(0x1e)
instr_table['cmps'] = gen_arith(0x1f)

@table('lw')
def i_lb(dest, src):
    if type(src) != tuple or src[0] != 'ref':
        raise AssembleError(
            'source operand of LW must be a memory location'
        )

    _, origin, offset = src

    if type(dest) != tuple or dest[0] != 'reg':
        raise AssembleError(
            'destination operand of LW must be a register'
        )

    xxxx = get_reg(dest)

    if type(origin) == tuple and origin[0] == 'reg':
        imm = offset
        yyyyyy = get_reg(origin)
        if offset != 0:
            yyyyyy &= 0x10

    elif type(origin) == int:
        imm = src
        yyyyyy = 0x20

    result = bytes([ 0x40 | xxxx, yyyyyy ])

    if imm != 0:
        result += bytes([ (imm & 0xff00) >> 8, imm & 0x00ff ])

    return result

@table('lb')
def i_lb(dest, src):
    if type(src) != tuple or src[0] != 'ref':
        raise AssembleError(
            'source operand of LB must be a memory location'
        )

    _, origin, offset = src

    if type(dest) != tuple or dest[0] != 'reg':
        raise AssembleError(
            'destination operand of LB must be a register'
        )

    xxxx = get_reg(dest)

    if type(origin) == tuple and origin[0] == 'reg':
        imm = offset
        yyyyyy = get_reg(origin)
        if offset != 0:
            yyyyyy &= 0x10

    elif type(origin) == int:
        imm = src
        yyyyyy = 0x20

    result = bytes([ 0x50 | xxxx, yyyyyy ])

    if imm != 0:
        result += bytes([ (imm & 0xff00) >> 8, imm & 0x00ff ])

    return result

@table('sw')
def i_sw(dest, src):
    if type(src) != tuple or src[0] != 'reg':
        raise AssembleError(
            'source operand of SW must be a register'
        )

    xxxx = get_reg(src)

    if type(dest) != tuple or dest[0] != 'ref':
        raise AssembleError(
            'destination operand of SW must be a memory location'
        )

    _, origin, offset = dest

    if type(origin) == tuple and origin[0] == 'reg':
        imm = offset
        yyyyyy = get_reg(origin)
        if offset != 0:
            yyyyyy &= 0x10
    elif type(origin) == int:
        imm = src
        yyyyyy = 0x20

    result = bytes([ 0x60 | xxxx, yyyyyy ])

    if imm != 0:
        result += bytes([ (imm & 0xff00) >> 8, imm & 0x00ff ])

    return result

@table('sb')
def i_sb(dest, src):
    if type(src) != tuple or src[0] != 'reg':
        raise AssembleError(
            'source operand of SB must be a register'
        )

    xxxx = get_reg(src)

    if type(dest) != tuple or dest[0] != 'ref':
        raise AssembleError(
            'destination operand of SB must be a memory location'
        )

    _, origin, offset = dest

    if type(origin) == tuple and origin[0] == 'reg':
        if offset == 0:
            imm = 0
            yyyyyy = get_reg(origin)
        else:
            imm = offset
            yyyyyy = get_reg(origin) & 0x10
    elif type(origin) == int:
        imm = src
        yyyyyy = 0x20

    result = bytes([ 0x70 | xxxx, yyyyyy ])

    if imm != 0:
        result += bytes([ (imm & 0xff00) >> 8, imm & 0x00ff ])

    return result

if __name__ == "__main__":
    if len(sys.argv) < 2:
        print("usage: %s <input-file> [ <output-file> [ <program-title> ] ]" % sys.argv[0])
        print("       If no output file is specified, will produce a file a.out.")
        sys.exit(0)

    with open(sys.argv[1]) as infile:
        # Keep lines around so we can print out specific lines
        # when there's an error.
        source_lines = infile.readlines()

    ast = parser.parse("".join(source_lines), lexer=lexer)

    print("\n".join([str(x) for x in ast]))

    if len(sys.argv) < 3:
        outfilename = "a.out"
    else:
        outfilename = sys.argv[2]

    if len(sys.argv) < 4:
        romtitle = "????"
    elif len(sys.argv[3]) > 14:
        print("Title too long. Max length 14 characters.")
        sys.exit(0)
    else:
        romtitle = sys.argv[3]

    with open(sys.argv[1]) as infile, open(outfilename, 'wb') as outfile:
        ba = bytearray()

        # Write header
        ba += bytes([0xCA, 0x55])
        ba += romtitle.encode('utf-8')

        ba += bytes([0] * (254 - len(romtitle)))

        current_position = 0x100

        label = ""

        print("Assembling...")

        for line in ast:
            lineno, instr, args = line

            try:
                opcode = instr_table[instr.lower()](*args)

                if label != "":
                    label_table[label] = current_position
                    label = ""
                    if current_position % 2 == 1:
                        # for now, just align all labels to 4-byte boundaries
                        # TODO we could do better by only doing this for ones targeted
                        # with a J or CALL, but we'll do this later
                        ba += inst_nop()

                ba += opcode

                current_position += 2
            except KeyError:
                print("Error: unknown instruction %s" % instr)
            except AssembleError as e:
                print("Error at " + sys.argv[1] + " line %d:" % lineno, str(e))
                print("    > ", source_lines[lineno - 1].strip())

        outfile.write(ba)
