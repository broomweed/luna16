import ply.yacc as yacc
import assemlex

tokens = assemlex.tokens

def p_lines_empty(p):
    '''lines : '''
    p[0] = []

def p_lines_nonempty(p):
    '''lines : lines line'''
    if p[2] is not None:
        p[0] = p[1] + [ p[2] ]
    else:
        p[0] = p[1]

def p_line_empty(p):
    '''line : EOL'''
    p[0] = None

def p_label(p):
    '''label : NAME'''
    p[0] = p[1]

def p_line_label(p):
    '''line : label COLON'''
    p[0] = (p.lineno(2), '(label)', p[1])

def p_line_simple(p):
    '''line : NAME EOL'''
    p[0] = (p.lineno(2), p[1], [])

def p_line_params(p):
    '''line : NAME params EOL'''
    p[0] = (p.lineno(3), p[1], p[2])

def p_line_directive(p):
    '''line : directive EOL'''
    p[0] = (p.lineno(2), *p[1])

def p_line_hexdata(p):
    '''line : hexdatadecl EOL'''
    p[0] = p[1]

def p_params_single(p):
    '''params : param'''
    p[0] = [ p[1] ]

def p_params_multiple(p):
    '''params : params COMMA param'''
    p[0] = p[1] + [ p[3] ]

def p_param_simple(p):
    '''param : simpleparam'''
    p[0] = p[1]

def p_param_mem(p):
    '''param : OPENBRACKET simpleparam CLOSEBRACKET'''
    p[0] = ('ref', p[2], 0)

def p_param_memoffset(p):
    '''param : NUMBER OPENBRACKET REG CLOSEBRACKET'''
    p[0] = ('ref', ('reg', p[3]), p[1])

def p_param_memoffsetname(p):
    '''param : NAME OPENBRACKET REG CLOSEBRACKET'''
    p[0] = ('ref', ('reg', p[3]), p[1])

def p_simpleparam_number(p):
    '''simpleparam : NUMBER'''
    p[0] = p[1]

def p_simpleparam_name(p):
    '''simpleparam : NAME'''
    p[0] = p[1]

def p_simpleparam_reg(p):
    '''simpleparam : REG'''
    p[0] = ('reg', p[1])

def p_directive_params(p):
    '''directive : HASHTAG dirparams'''
    p[0] = (p[1], p[2])

def p_directive_noparams(p):
    '''directive : HASHTAG'''
    p[0] = (p[1], [])

def p_dirparams_single(p):
    '''dirparams : dirparam'''
    p[0] = [ p[1] ]

def p_dirparams_multiple(p):
    '''dirparams : dirparams dirparam'''
    p[0] = p[1] + [ p[2] ]

def p_dirparam_simple(p):
    '''dirparam : simpleparam'''
    p[0] = p[1]

def p_dirparam_str(p):
    '''dirparam : STRING'''
    p[0] = ('str', p[1])

def p_hexdatadecl(p):
    '''hexdatadecl : HEX dirparams'''
    p[0] = (p.lineno(1), 'data', p[2])

def p_error(p):
    if not p:
        print("unexpected end of file while parsing!")
        return
    else:
        print("Syntax error at token", p.value, "on line", p.lineno)

    # read to next EOL token - fortunately our syntax is simple!
    while True:
        tok = parser.token()
        if not tok or tok.type == 'EOL':
            break
    parser.errok()

parser = yacc.yacc()
