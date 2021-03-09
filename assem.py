from assemlex import lexer
from assemparse import parser

import sys
from math import log

verbose = False

instr_table = {}

lineno = 0

reg_table = {
    'a':  0,  'b':  1,  'c':  2,  'd':  3,
    'e':  4,  'f':  5,  'g':  6,  'h':  7,
    'i':  8,  'j':  9,  'k': 10,  'l': 11,
    'm': 12,  'n': 13, 'sp': 14, 'pc': 15
}

label_table = {}

jump_list = []

current_position = 0

fragments = []

class AssembleError(Exception):
    pass

def is_reg(value):
    return typeof(value) == tuple and value[0] == 'reg'

def is_mem(value):
    return typeof(value) == tuple and value[0] == 'ref'

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

@table('halt')
def i_nop():
    return bytes([0x00, 0x02])

@table('ret')
def i_ret():
    return bytes([0x00, 0xaa])

@table('reti')
def i_reti():
    return bytes([0x00, 0xab])

@table('di')
def i_di():
    return bytes([0x00, 0xdd])

@table('ei')
def i_ei():
    return bytes([0x00, 0xee])

@table('data')
def i_data(*data):
    result = bytes()
    return bytes(data)

@table('push')
def i_push(reg):
    if not is_reg(reg):
        raise AssembleError("PUSH can accept only a register as an argument")
    return bytes([0x01, get_reg(reg) << 4])

@table('pop')
def i_pop(reg):
    if not is_reg(reg):
        raise AssembleError("POP can accept only a register as an argument")
    return bytes([0x02, get_reg(reg) << 4])

@table('jr')
def i_jr(reg):
    if not is_reg(reg):
        raise AssembleError("JR can accept only a register as an argument")
    return bytes([0x03, get_reg(reg) << 4])

@table('swap')
def i_swap(reg1, reg2):
    r1 = get_reg(reg1)
    r2 = get_reg(reg2)
    return bytes([0x04, (r1 << 4) | r2])

def gen_arith(code):
    ''' Generate an arithmetic instruction with
        operation id `code`, taking two arguments. '''

    def inner(dest, src):
        if not is_reg(dest):
            raise AssembleError(
                'destination operand for arithmetic operation must be a register'
            )

        xxxx = get_reg(dest)
        xx1 = (xxxx >> 2) & 3
        xx2 = xxxx & 3

        if is_reg(src):
            # Register
            yyyyyy = get_reg(src)
        elif type(src) == int:
            while src < 0:
                src += 65536
            # Immediate operand
            if 0 <= src <= 15:
                # Small immediate operand
                yyyyyy = 0x10 | src
            elif src in {16, 32, 64, 128, 256, 512, 1024, 2048, 4096, 8192, 16384, 32768}:
                # Power of two above 8
                yyyyyy = 0x20 | int(log(src, 2))
            elif src == 0xFFFF:
                # Negative one
                yyyyyy = 0x21
            else:
                # Big immediate operand
                yyyyyy = 0x20
            imm = src
        elif type(src) == str:
            # Label (or macro but we don't have them yet!)
            yyyyyy = 0x20
            imm = 0
            jump_list.append((lineno, fragment_id, current_position + 2, src))

        result = bytes([ 0x80 | (code << 2) | xx1, (xx2 << 6) | yyyyyy ])

        if yyyyyy == 0x20:
            result += bytes([ (imm & 0xff00) >> 8, imm & 0x00ff ])

        return result

    return inner

def gen_arith_unary(code):
    ''' Generate an arithmetic instruction with
        operation id `code`, taking only one argument. '''

    def inner(dest):
        if not is_reg(dest):
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

def gen_loadstore(name, code, is_load):
    def inner(reg, mem):
        if is_load:
            mem_param = 'source'
            reg_param = 'destination'
        else:
            mem_param = 'destination'
            reg_param = 'source'
            reg, mem = mem, reg

        if not is_mem(mem):
            raise AssembleError(
                mem_param + ' operand of ' + name.upper() + ' must be a memory location'
            )

        _, origin, offset = mem

        if not is_reg(reg):
            raise AssembleError(
                reg_param + ' operand of ' + name.upper() + ' must be a register'
            )

        xxxx = get_reg(reg)

        xxx = xxxx >> 1
        x = xxxx & 1

        if is_reg(origin):
            if type(offset) == int:
                imm = offset
                yyyyyy = get_reg(origin)
                if offset != 0:
                    yyyyyy |= 0x10
            elif type(offset) == str:
                imm = -1
                yyyyyy = get_reg(origin) | 0x10
                jump_list.append((lineno, fragment_id, current_position + 2, offset))

        elif type(origin) == int:
            imm = origin
            yyyyyy = 0x20

        elif type(origin) == str:
            imm = -1
            yyyyyy = 0x20
            jump_list.append((lineno, fragment_id, current_position + 2, origin))

        else:
            raise AssembleError("Unknown parameter to " + name.upper() + ":", repr(origin))

        result = bytes([ code | xxx, (x << 7) | yyyyyy ])

        if imm != 0:
            result += bytes([ (imm & 0xff00) >> 8, imm & 0x00ff ])

        return result

    return inner

instr_table['lw'] = gen_loadstore('lw', 0x20, True)
instr_table['lb'] = gen_loadstore('lb', 0x28, True)
instr_table['sw'] = gen_loadstore('sw', 0x30, False)
instr_table['sb'] = gen_loadstore('sb', 0x38, False)

def gen_jump(code):
    def inner(where):
        jump_list.append((lineno, fragment_id, current_position + 2, where))
        return bytes([ 0x40 | (code << 2), 0x00, 0x00, 0x00 ])
    return inner

instr_table['jmp'] = gen_jump(0)
instr_table['je']  = gen_jump(1)
instr_table['jz']  = gen_jump(1)
instr_table['jne'] = gen_jump(2)
instr_table['jnz'] = gen_jump(2)
instr_table['jlt'] = gen_jump(3)
instr_table['jc']  = gen_jump(3)
instr_table['jge'] = gen_jump(4)
instr_table['jnc'] = gen_jump(4)
instr_table['jle'] = gen_jump(5)
instr_table['jgt'] = gen_jump(6)
# TODO need signed comparison jumps
# (and I guess rename the unsigned ones to like ja jb jbe etc)
instr_table['jsr'] = gen_jump(15)

current_position = 0x0
section_offset = -1
section_id = "?"
fragment_id = 0

ba = bytearray()

def end_section():
    global fragment_id
    # Save current fragment and start a new one.
    # Offset of -1 means 'wherever idk'
    if len(ba) > 0:
        fragments.append({
            'name': section_id,
            'section': 'rom',
            'offset': section_offset,
            'data': ba
        })
        fragment_id += 1

def new_section(sid, offset):
    global section_id, section_offset, ba, current_position
    end_section()
    section_offset = offset
    section_id = sid
    ba = bytearray()
    current_position = 0x0

def handle_directive(dct, args):
    if dct == '#at':
        # Directive #at tells the assembler to insert
        # following code at a particular byte address
        # in the ROM. Useful for interrupt handlers.
        if len(args) != 2:
            raise AssembleError("#at needs two arguments: offset and section name")
        if type(args[0]) != int:
            raise AssembleError("first argument to #at must be integer offset")
        if type(args[1]) != tuple or args[1][0] != 'str':
            raise AssembleError("second argument to #at must be string")
        new_section(args[1][1], args[0])
    elif dct == '#section':
        # Tells the assembler the next part doesn't have
        # to be contiguous with the last. This is like
        # #at, but doesn't specify a particular place to
        # put the code.
        if len(args) != 1:
            raise AssembleError("#section needs one argument: section name")
        if type(args[0]) != tuple or args[0][0] != 'str':
            raise AssembleError("argument to #section must be string")
        new_section(args[0][1], -1)
    elif dct == '#include_bin':
        global ba, current_position
        # Just dump a bunch of bytes from an external file
        # into this file.
        if len(args) != 1:
            raise AssembleError("#include_bin needs one argument: file name to include")
        if type(args[0]) != tuple or args[0][0] != 'str':
            raise AssembleError("argument to #include_bin must be string")
        # Read file into byte array
        with open(args[0][1], 'rb') as binfile:
            binary_data = bytes(binfile.read())
        ba += binary_data
        current_position += len(binary_data)
    else:
        raise AssembleError("unknown directive '%s'" % dct)

def overlap_error(frag1, frag2):
    name1 = frag1['name']
    offset1 = frag1['offset']
    length1 = len(frag1['data'])

    name2 = frag2['name']
    offset2 = frag2['offset']
    length2 = len(frag2['data'])

    print("Error: section '%s' overlaps with section '%s'" % (name1, name2))
    print("%24s offset: %6d ($%04X)" % (name1, offset1, offset1))
    print("%24s length: %6d ($%04X)" % (name1, length1, length1))
    print("%24s offset: %6d ($%04X)" % (name2, offset2, offset2))
    print("%24s length: %6d ($%04X)" % (name2, length2, length2))

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

    if verbose: print("\n".join([str(x) for x in ast]))

    if len(sys.argv) < 3:
        outfilename = "a.out"
    else:
        outfilename = sys.argv[2]

    if len(sys.argv) < 4:
        romtitle = "????"
    elif len(sys.argv[3]) > 30:
        print("Title too long. Max length 30 characters.")
        sys.exit(0)
    else:
        romtitle = sys.argv[3]

    with open(sys.argv[1]) as infile, open(outfilename, 'wb') as outfile:
        new_section("header", 0)

        offset = 0

        # Write header
        ba += bytes([0xCA, 0x55])
        ba += romtitle.encode('utf-8')

        ba += bytes([0] * (126 - len(romtitle)))

        new_section("?", -1)

        print("Assembling...")

        errors = 0

        for line in ast:
            lineno, instr, args = line

            try:
                if instr.startswith('#'):
                    # Handle directive.
                    handle_directive(instr, args)
                elif instr != '(label)':
                    try:
                        opcode = instr_table[instr.lower()](*args)
                    except KeyError:
                        raise AssembleError("unknown instruction '%s'" % instr)
                    ba += opcode
                    if verbose: print(" ".join("%X" % x for x in opcode))
                    current_position += len(opcode)
                else:
                    label_table[args] = (fragment_id, current_position)
            except AssembleError as e:
                print("Error at " + sys.argv[1] + " line %d:" % lineno, str(e))
                print("    > ", source_lines[lineno - 1].strip())
                errors += 1

        end_section()

        if errors > 0:
            sys.exit(1)

        print("Placing fragments...")

        # Figure out the memory layout of the ROM. We structure this list like
        # [ frag1, 64, frag2, frag3, 128 ] -- numbers represent free bytes,
        # and non-numbers represent fragments to put there.
        layout = [ 1 ]

        overlaps = 0

        # First, make sure we can fit all the fixed-position fragments.
        for frag in (f for f in fragments if f['offset'] != -1):
            offset = frag['offset']
            length = len(frag['data'])
            name = frag['name']

            if offset % 2 == 1:
                print("Error: section '%s' is misaligned. Offsets must "
                      "be multiples of 2. (current offset: %d/$%04X)"
                      % (name, offset, offset))
                break

            # Find the appropriate chunk of free space to place it
            byte_index = 0
            for i in range(len(layout)):
                if type(layout[i]) == int:
                    byte_index += layout[i]
                    if byte_index > offset:
                        # Ooh, put it here!
                        # Back up one chunk so we have the byte index
                        # of the beginning of the free chunk
                        byte_index -= layout[i]
                        # How much free space before?
                        free_before = offset - byte_index
                        # How much free space after?
                        free_after = byte_index + layout[i] - (offset + length)
                        if free_after < 0 and i < len(layout)-1:
                            next_offset = layout[i+1]['offset']
                            next_length = len(layout[i+1]['data'])
                            next_name = layout[i+1]['name']

                            overlap_error(frag, layout[i+1])
                            overlaps += 1
                            break
                        else:
                            # Put this fragment in.
                            new_list = []
                            if free_before > 0:
                                new_list.append(free_before)
                            new_list.append(frag)
                            if free_after > 0:
                                new_list.append(free_after)
                            layout[i:i+1] = new_list
                            break
                else:
                    # Move over this fragment and make sure we didn't go too far
                    byte_index += len(layout[i]['data'])
                    if byte_index > offset:
                        overlap_error(frag, layout[i])
                        overlaps += 1
                        break
            else:
                # We didn't break, so we must have run off the end.
                # This means we need to insert more free space at the
                # end of the rom
                gap = offset - byte_index
                if gap > 0:
                    layout.append(gap)
                layout.append(frag)


            if verbose:
                print("Inserted fragment %s, layout now:" % frag['name'],
                    [f if type(f) == int else f['name'] for f in layout]
                )

        if overlaps > 0:
            sys.exit(1)

        # Now place all the movable fragments
        for frag in (f for f in fragments if f['offset'] == -1):
            size = len(frag['data'])

            byte_offset = 0
            for i, space in enumerate(layout):
                if type(space) != int:
                    byte_offset += len(space['data'])
                    continue

                byte_offset += space
                if size <= space:
                    # It fits! This is a p. greedy algorithm
                    # but I think that's ok for now
                    byte_offset -= space
                    newlist = [ frag ]
                    if size != space:
                        newlist.append(space - size)

                    layout[i:i+1] = newlist
                    frag['offset'] = byte_offset
                    break
            else:
                # We can just tack it directly onto the end
                frag['offset'] = byte_offset
                layout.append(frag)

            if verbose:
                print("Inserted fragment %s, layout now:" % frag['name'],
                    [f if type(f) == int else f['name'] for f in layout]
                )

        print("Resolving labels...")

        jump_errors = 0

        for (lineno, frag_id, jump_from, label) in jump_list:
            # NOTE: The current jump implementation is inefficient;
            # it never uses relative jumps. I'm not smart enough to
            # figure out when to use a short jump, since that moves
            # all the labels around after the jump when it gets shorter.
            # (and we might have already computed some jumps to those
            # labels! oh no)
            # There's definitely a good way to do this, where we make
            # multiple passes and find out which jumps can be relative
            # first (if we never add any bytes, this will never change,
            # although we might miss a few that could be shortened.)
            # I'll figure this out later. For now, we get the naive
            # version.
            # (Future update: actually, might be easier to just only
            #  use the relative jumps within a section? I dunno,
            #  figure it out later)

            # Add address for absolute jump
            try:
                fragment, offset = label_table[label]
            except KeyError:
                print("Error at %s line %d: no such label %s"
                                      % (sys.argv[1], lineno, label))
                print("    > ", source_lines[lineno - 1].strip())
                jump_errors += 1
                continue

            jump_to = fragments[fragment]['offset'] + offset
            fragments[frag_id]['data'][jump_from:jump_from+2] \
                                = bytes([jump_to >> 8, jump_to & 0xff])

        if jump_errors > 0:
            sys.exit(1)

        # Now put together the whole thing
        arr = bytearray()
        for fragment in layout:
            if type(fragment) == int:
                arr += bytes([0] * fragment)
            else:
                arr += fragment['data']

        outfile.write(arr)

        print("Success! Output to", outfilename)
