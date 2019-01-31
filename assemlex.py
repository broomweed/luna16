import ply.lex as lex

tokens = [ 'REG', 'NAME', 'NUMBER', 'OPENPAREN', 'CLOSEPAREN',
           'EOL', 'COMMA', 'OPENBRACKET', 'CLOSEBRACKET',
           'COLON', 'HASHTAG', 'STRING' ]

t_ignore = ' \t\r'

t_OPENPAREN = r'\('
t_CLOSEPAREN = r'\)'
t_OPENBRACKET = r'\['
t_CLOSEBRACKET = r'\]'
t_COMMA = r','
t_COLON = r':'
t_HASHTAG = r'\#[a-zA-Z0-9_]+'

def t_EOL(t):
    r'\n'
    t.lexer.lineno += 1
    return t

def t_STRING(t):
    r'"([^"]|\\.)*"'
    t.value = t.value[1:len(t.value)-1]
    return t

registers = { 'a', 'b', 'c', 'd', 'e', 'f', 'g', 'h', 'i', 'j', 'k', 'l', 'm', 'sp' 'pc' }

def t_error(t):
    print("Unrecognized character...\n", t.value)

def t_NAME(t):
    r'\.?[a-zA-Z_][a-zA-Z0-9_]*'
    if t.value.lower() in registers:
        t.type = 'REG'
    else:
        t.type = 'NAME'
    return t

def t_NUMBER(t):
    r'-?((\$|0x)-?[0-9a-fA-F]+|-?[0-9]+|-?0b[01]+|-?0q[0-3]+)'

    nega = False
    if t.value.startswith('-'):
        nega = True
        t.value = t.value[1:]

    if t.value.startswith('$'):
        t.value = int(t.value[1:], 16)
    elif t.value.startswith('0x'):
        t.value = int(t.value[2:], 16)
    elif t.value.startswith('0b'):
        t.value = int(t.value[2:], 2)
    elif t.value.startswith('0q'):
        # quaternary
        t.value = int(t.value[2:], 4)
    else:
        t.value = int(t.value)

    if nega: t.value *= -1;

    return t

def t_COMMENT(t):
    r';.*\n'
    pass

lexer = lex.lex()
