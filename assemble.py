# VS-16 ASSEMBLER

import sys

if len(sys.argv) < 2:
    print("usage: %s <input-file> [ <output-file> ]" % sys.argv[0])
    print("       If no output file is specified, will produce a file a.out.")
    sys.exit(0)

if len(sys.argv) < 3:
    outfilename = "a.out"
else:
    outfilename = sys.argv[2]

instr_table = {}

reg_table = { 'a': 0, 'b': 1, 'c': 2, 'd': 3, 'e': 4, 's': 5, 'p': 6, 'f': 7 }

label_table = {}

jump_list = []

current_position = 0

def table(instruction):
    def decorator(func):
        instr_table[instruction] = func
        return func
    return decorator

def get_reg(name):
    try:
        return reg_table[name]
    except KeyError:
        print("Unexpected: %s; expected register name", name)
        return 0

def get_indirect(s):
    if '[' not in s or s[-1] != ']':
        print("Unexpected: %s; expected memory reference ([reg] or offset[reg])", s)
        return 0, 0
    if s[0] == '[':
        offset = 0
    else:
        offset = int(s.split('[')[0].trim())
    return offset, get_reg(s.split('[')[0].trim()[:-1])

def parse_num(num):
    if num[0] == '$':
        return int(num[1:], 16)
    else:
        return int(num)

@table('stop')
def inst_stop():
    return b'\x00\xff'

@table('ret')
def inst_ret():
    return b'\x00\xaa'

@table('nop')
def inst_nop():
    return b'\x00\x01'

@table('push')
def inst_push(a):
    r1 = get_reg(a)
    return bytes([ 0x01, r1 << 5 ])

@table('pop')
def inst_pop(a):
    r1 = get_reg(a)
    return bytes([ 0x02, r1 << 5 ])

@table('jr')
def inst_pop(a):
    r1 = get_reg(a)
    return bytes([ 0x03, r1 << 5 ])

@table('add')
def inst_add(a, b):
    r1 = get_reg(a)
    r2 = get_reg(b)
    return bytes([ 0x08 | r1, r2 << 5 | 0x00 ])

@table('sub')
def inst_sub(a, b):
    r1 = get_reg(a)
    r2 = get_reg(b)
    return bytes([ 0x08 | r1, r2 << 5 | 0x01 ])

@table('mul')
def inst_mul(a, b):
    r1 = get_reg(a)
    r2 = get_reg(b)
    return bytes([ 0x08 | r1, r2 << 5 | 0x02 ])

@table('and')
def inst_and(a, b):
    r1 = get_reg(a)
    r2 = get_reg(b)
    return bytes([ 0x08 | r1, r2 << 5 | 0x03 ])

@table('or')
def inst_or(a, b):
    r1 = get_reg(a)
    r2 = get_reg(b)
    return bytes([ 0x08 | r1, r2 << 5 | 0x04 ])

@table('xor')
def inst_xor(a, b):
    r1 = get_reg(a)
    r2 = get_reg(b)
    return bytes([ 0x08 | r1, r2 << 5 | 0x05 ])

@table('not')
def inst_not(a):
    r1 = get_reg(a)
    return bytes([ 0x08 | r1, 0x06 ])

@table('not')
def inst_not(a):
    r1 = get_reg(a)
    return bytes([ 0x08 | r1, 0x06 ])

@table('neg')
def inst_neg(a):
    r1 = get_reg(a)
    return bytes([ 0x08 | r1, 0x07 ])

@table('inc')
def inst_inc(a):
    r1 = get_reg(a)
    return bytes([ 0x08 | r1, 0x08 ])

@table('dec')
def inst_dec(a):
    r1 = get_reg(a)
    return bytes([ 0x08 | r1, 0x09 ])

@table('sl')
def inst_sl(a, b):
    r1 = get_reg(a)
    r2 = get_reg(b)
    return bytes([ 0x08 | r1, r2 << 5 | 0x0a ])

@table('srl')
def inst_srl(a, b):
    r1 = get_reg(a)
    r2 = get_reg(b)
    return bytes([ 0x08 | r1, r2 << 5 | 0x0b ])

@table('sra')
def inst_sra(a, b):
    r1 = get_reg(a)
    r2 = get_reg(b)
    return bytes([ 0x08 | r1, r2 << 5 | 0x0c ])

@table('cmp')
def inst_cmp(a, b):
    r1 = get_reg(a)
    r2 = get_reg(b)
    return bytes([ 0x08 | r1, r2 << 5 | 0x0d ])

@table('brl')
def inst_brl(a, b):
    r1 = get_reg(a)
    r2 = get_reg(b)
    return bytes([ 0x08 | r1, r2 << 5 | 0x0e ])

@table('brr')
def inst_brr(a, b):
    r1 = get_reg(a)
    r2 = get_reg(b)
    return bytes([ 0x08 | r1, r2 << 5 | 0x0f ])

@table('bit')
def inst_bit(a, b):
    r1 = get_reg(a)
    r2 = get_reg(b)
    return bytes([ 0x08 | r1, r2 << 5 | 0x10 ])

@table('li')
def inst_li(a, b):
    r1 = get_reg(a)
    v = parse_num(b)
    if v > 0xff:
        print("Number %s is too large for immediate instruction" % b)
        v = v & 0xff
    return bytes([ 0x40 | r1, v ])

@table('lui')
def inst_lui(a, b):
    r1 = get_reg(a)
    v = parse_num(b)
    if v > 0xff:
        print("Number %s is too large for immediate instruction" % b)
        v = v & 0xff
    return bytes([ 0x30 | r1, v ])

@table('addi')
def inst_addi(a, b):
    r1 = get_reg(a)
    v = parse_num(b)
    if v > 0xff:
        print("Number %s is too large for immediate instruction" % b)
        v = v & 0xff
    return bytes([ 0x48 | r1, v ])

@table('muli')
def inst_muli(a, b):
    r1 = get_reg(a)
    v = parse_num(b)
    if v > 0xff:
        print("Number %s is too large for immediate instruction" % b)
        v = v & 0xff
    return bytes([ 0x50 | r1, v ])

@table('subi')
def inst_subi(a, b):
    r1 = get_reg(a)
    v = parse_num(b)
    if v > 0xff:
        print("Number %s is too large for immediate instruction" % b)
        v = v & 0xff
    return bytes([ 0x58 | r1, v ])

@table('andi')
def inst_andi(a, b):
    r1 = get_reg(a)
    v = parse_num(b)
    if v > 0xff:
        print("Number %s is too large for immediate instruction" % b)
        v = v & 0xff
    return bytes([ 0x60 | r1, v ])

@table('ori')
def inst_ori(a, b):
    r1 = get_reg(a)
    v = parse_num(b)
    if v > 0xff:
        print("Number %s is too large for immediate instruction" % b)
        v = v & 0xff
    return bytes([ 0x68 | r1, v ])

@table('xori')
def inst_xori(a, b):
    r1 = get_reg(a)
    v = parse_num(b)
    if v > 0xff:
        print("Number %s is too large for immediate instruction" % b)
        v = v & 0xff
    return bytes([ 0x70 | r1, v ])

@table('sli')
def inst_sli(a, b):
    r1 = get_reg(a)
    v = parse_num(b)
    if v > 0xf:
        print("Number %s is too large for immediate bit instruction" % b)
        v = v & 0xf
    return bytes([ 0x78 | r1, 0x80 | v ])

@table('srli')
def inst_srli(a, b):
    r1 = get_reg(a)
    v = parse_num(b)
    if v > 0xf:
        print("Number %s is too large for immediate bit instruction" % b)
        v = v & 0xf
    return bytes([ 0x78 | r1, 0x90 | v ])

@table('srai')
def inst_srai(a, b):
    r1 = get_reg(a)
    v = parse_num(b)
    if v > 0xf:
        print("Number %s is too large for immediate bit instruction" % b)
        v = v & 0xf
    return bytes([ 0x78 | r1, 0xa0 | v ])

@table('brli')
def inst_srai(a, b):
    r1 = get_reg(a)
    v = parse_num(b)
    if v > 0xf:
        print("Number %s is too large for immediate bit instruction" % b)
        v = v & 0xf
    return bytes([ 0x78 | r1, 0xb0 | v ])

@table('brri')
def inst_srai(a, b):
    r1 = get_reg(a)
    v = parse_num(b)
    if v > 0xf:
        print("Number %s is too large for immediate bit instruction" % b)
        v = v & 0xf
    return bytes([ 0x78 | r1, 0xc0 | v ])

@table('biti')
def inst_srai(a, b):
    r1 = get_reg(a)
    v = parse_num(b)
    if v > 0xf:
        print("Number %s is too large for immediate bit instruction" % b)
        v = v & 0xf
    return bytes([ 0x78 | r1, 0xd0 | v ])

@table('lw')
def inst_lw(a, b):
    dest = get_reg(a)
    offset, src = get_indirect(b)
    if offset > 4 or offset < -3:
        print("Offset %d too large" % b)
        offset = 0
    if offset < 0:
        offset += 8
    return bytes([ 0x10 | dest, src << 5 | 0x00 | offset])

@table('llo')
def inst_llow(a, b):
    dest = get_reg(a)
    offset, src = get_indirect(b)
    if offset > 4 or offset < -3:
        print("Offset %d too large" % b)
        offset = 0
    if offset < 0:
        offset += 8
    return bytes([ 0x10 | dest, src << 5 | 0x08 | offset])

@table('lhl')
def inst_lhl(a, b):
    dest = get_reg(a)
    offset, src = get_indirect(b)
    if offset > 4 or offset < -3:
        print("Offset %d too large" % b)
        offset = 0
    if offset < 0:
        offset += 8
    return bytes([ 0x10 | dest, src << 5 | 0x10 | offset])

@table('lhh')
def inst_lhh(a, b):
    dest = get_reg(a)
    offset, src = get_indirect(b)
    if offset > 4 or offset < -3:
        print("Offset %d too large" % b)
        offset = 0
    if offset < 0:
        offset += 8
    return bytes([ 0x10 | dest, src << 5 | 0x18 | offset])

@table('sw')
def inst_sw(a, b):
    offset, dest = get_indirect(a)
    src = get_reg(b)
    if offset > 4 or offset < -3:
        print("Offset %d too large" % b)
        offset = 0
    if offset < 0:
        offset += 8
    return bytes([ 0x18 | dest, src << 5 | 0x00 | offset])

@table('slo')
def inst_slo(a, b):
    offset, dest = get_indirect(a)
    src = get_reg(b)
    if offset > 4 or offset < -3:
        print("Offset %d too large" % b)
        offset = 0
    if offset < 0:
        offset += 8
    return bytes([ 0x18 | dest, src << 5 | 0x08 | offset])

@table('slh')
def inst_slh(a, b):
    offset, dest = get_indirect(a)
    src = get_reg(b)
    if offset > 4 or offset < -3:
        print("Offset %d too large" % b)
        offset = 0
    if offset < 0:
        offset += 8
    return bytes([ 0x18 | dest, src << 5 | 0x10 | offset])

@table('shh')
def inst_shh(a, b):
    offset, dest = get_indirect(a)
    src = get_reg(b)
    if offset > 4 or offset < -3:
        print("Offset %d too large" % b)
        offset = 0
    if offset < 0:
        offset += 8
    return bytes([ 0x18 | dest, src << 5 | 0x18 | offset])

@table('mov')
def inst_mov(a, b):
    dest = get_reg(a)
    src = get_reg(b)
    return bytes([ 0x20 | dest, src << 5 | 0x00 ])

@table('movhl')
def inst_movhl(a, b):
    dest = get_reg(a)
    src = get_reg(b)
    return bytes([ 0x20 | dest, src << 5 | 0x01 ])

@table('movlh')
def inst_movlh(a, b):
    dest = get_reg(a)
    src = get_reg(b)
    return bytes([ 0x20 | dest, src << 5 | 0x02 ])

@table('movhh')
def inst_movhh(a, b):
    dest = get_reg(a)
    src = get_reg(b)
    return bytes([ 0x20 | dest, src << 5 | 0x03 ])

@table('movll')
def inst_movll(a, b):
    dest = get_reg(a)
    src = get_reg(b)
    return bytes([ 0x20 | dest, src << 5 | 0x04 ])

@table('xchg')
def inst_xchg(a, b):
    dest = get_reg(a)
    src = get_reg(b)
    return bytes([ 0x20 | dest, src << 5 | 0x05 ])

@table('j')
def inst_j(a):
    jump_list.append((current_position, 'j', a))
    return bytes([0, 0])

@table('je')
def inst_je(a):
    jump_list.append((current_position, 'je', a))
    return bytes([0, 0])

@table('jne')
def inst_jne(a):
    jump_list.append((current_position, 'jne', a))
    return bytes([0, 0])

@table('jl')
def inst_jl(a):
    jump_list.append((current_position, 'jl', a))
    return bytes([0, 0])

@table('jg')
def inst_jg(a):
    jump_list.append((current_position, 'jg', a))
    return bytes([0, 0])

@table('call')
def inst_call(a):
    jump_list.append((current_position, 'call', a))
    return bytes([0, 0])

with open(sys.argv[1]) as infile, open(outfilename, 'wb') as outfile:
    # Write header
    while 1:
        title = str.encode(input("ROM title: "))
        if len(title) <= 60: break
        print("Title too long; max length 60 bytes.")

    ba = bytearray()

    ba += bytes([0x12, 0x34, 0x56, 0x78])
    ba += title

    ba += bytes([0] * (252 - len(title)))

    current_position = 0x80

    label = ""

    print("Assembling...")

    for line in infile:
        line = line.split(";")[0].lower().strip() # strip comments

        if ":" in line:
            label = line.split(":")[0].strip()
            line = ":".join(line.split(":")[1:])

        instr = line.split(" ")[0].strip()
        if instr == '': continue

        line = line[len(instr)+1:]
        args = [x.strip() for x in line.split(",") if len(x.strip()) > 0]

        opcode = instr_table[instr](*args)

        if label != "":
            label_table[label] = current_position
            label = ""
            if current_position % 2 == 1:
                # for now, just align all labels to 4-byte boundaries
                # TODO we could do better by only doing this for ones targeted
                # with a J or CALL, but we'll do this later
                ba += inst_nop()

        ba += opcode

        current_position += 1

    print("Linking...")

    for jump in jump_list:
        location, jtype, label = jump

    outfile.write(ba)
