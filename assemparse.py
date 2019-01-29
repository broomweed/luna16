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

def p_label_dot(p):
    '''label : DOT NAME'''
    p[0] = '.' + p[2]

def p_label_nodot(p):
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
    p[0] = (p.lineno(2), p[1])

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
    '''param : NUMBER OPENBRACKET NAME CLOSEBRACKET'''
    p[0] = ('ref', p[3], p[1])

def p_simpleparam_number(p):
    '''simpleparam : NUMBER'''
    p[0] = p[1]

def p_simpleparam_name(p):
    '''simpleparam : NAME'''
    p[0] = p[1]

def p_simpleparam_reg(p):
    '''simpleparam : REG'''
    p[0] = ('reg', p[1])

def p_directive(p):
    '''directive : HASH NAME params'''
    p[0] = ('#' + p[2], p[3])

def p_error(p):
    print("Error at token", p.type)
    if not p:
        print("unexpected end of file while parsing!")

    # read to next EOL token - fortunately our syntax is simple!
    while True:
        tok = parser.token()
        if not tok or tok.type == 'EOL':
            break
    parser.errok()

parser = yacc.yacc()
