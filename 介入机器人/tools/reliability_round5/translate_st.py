"""将完整 ST 控制语句转为 C++，FB/时钟由显式离线替身驱动；拒绝未识别声明。"""
from pathlib import Path
import re
import xml.etree.ElementTree as ET


def clean(text):
    return re.sub(r"//[^\n]*|\(\*.*?\*\)", "", text, flags=re.S)


def lex(text):
    text = re.sub(r"16#([\da-fA-F]+)", lambda m: str(int(m[1], 16)), text)
    text = re.sub(r"T#(\d+)(MS|S)", lambda m: str(int(m[1]) * (1 if m[2].upper() == 'MS' else 1000)), text, flags=re.I)
    return re.findall(r":=|<>|>=|<=|\.\.|[A-Za-z_]\w*|\d+(?:\.\d+)?(?:[eE][+-]?\d+)?|[^\s]", text)


def expr(items):
    text = ' '.join(items)
    text = re.sub(r"MC_Direction\s*\.\s*MC_Positive_Direction", '1', text)
    text = re.sub(r"MC_Direction\s*\.\s*MC_Negative_Direction", '-1', text)
    text = re.sub(r"MC_Direction\s*\.\s*MC_Current_Direction", '0', text)
    text = re.sub(r"\.\s*([A-Za-z_]\w*)", r'["\1"]', text)
    text = re.sub(r"(?<![<>:])=(?!=)", '==', text).replace(':=', '=').replace('<>', '!=')
    for old, new in {'TRUE':'true','FALSE':'false','AND':'&&','OR':'||','NOT':'!',
                     'ABS':'std::abs','MIN':'st_min','MAX':'st_max','TO_LREAL':'double',
                     'MC_Aborting':'0','POSITIONTYPE_ABSOLUTE':'0'}.items():
        text = re.sub(r'\b'+old+r'\b', new, text)
    return text


class Parser:
    def __init__(self, text): self.tokens, self.pos = lex(clean(text)), 0
    def until(self, marker):
        start = self.pos
        while self.tokens[self.pos] != marker: self.pos += 1
        result = self.tokens[start:self.pos]; self.pos += 1
        return result
    def label(self): return re.match(r'(?:\d+|_[a-z_]+)(?: , (?:\d+|_[a-z_]+))* :', ' '.join(self.tokens[self.pos:])) is not None
    def call(self, items):
        opening = items.index('(')
        target = expr(items[:opening])
        args, start, depth = [], opening+1, 0
        for n in range(start, len(items)):
            if items[n] in ('(', '['): depth += 1
            if items[n] in (')', ']'): depth -= 1
            if (depth == 0 and items[n] == ',') or depth == -1:
                value = items[start:n]
                if value:
                    assert value[1] == ':=', value
                    if value[0] not in ('Axis', 'Master', 'Slave'):
                        args.append('{"'+value[0]+'", '+expr(value[2:])+'}')
                    elif target == 'MC_ExtSetPointGenFeed': axis = expr(value[2:])
                start = n+1
        if target == 'MC_ExtSetPointGenFeed': return 'feed('+axis+', {'+', '.join(args)+'});'
        return 'call('+target+', {'+', '.join(args)+'});'
    def statements(self, stops=(), in_case=False):
        lines=[]
        while self.pos < len(self.tokens) and self.tokens[self.pos] not in stops:
            if in_case and self.label(): break
            token=self.tokens[self.pos]; self.pos+=1
            if token==';': continue
            if token=='IF':
                lines+=['if ('+expr(self.until('THEN'))+') {']+self.statements(('ELSIF','ELSE','END_IF'))
                while self.tokens[self.pos]=='ELSIF':
                    self.pos+=1; lines+=['} else if ('+expr(self.until('THEN'))+') {']+self.statements(('ELSIF','ELSE','END_IF'))
                if self.tokens[self.pos]=='ELSE':
                    self.pos+=1; lines+=['} else {']+self.statements(('END_IF',))
                assert self.tokens[self.pos]=='END_IF'; self.pos+=1; lines+=['}']
            elif token=='FOR':
                head=self.until('DO'); to=head.index('TO'); var=head[0]
                lines+=['for ('+expr(head[:to])+'; double('+var+') <= '+expr(head[to+1:])+'; ++'+var+') {']+self.statements(('END_FOR',))
                self.pos+=1; lines+=['}']
            elif token=='CASE':
                lines+=['switch (int('+expr(self.until('OF'))+')) {']
                while self.tokens[self.pos]!='END_CASE':
                    if self.tokens[self.pos]=='ELSE': self.pos+=1; lines+=['default: {']
                    else: lines+=['case '+x+':' for x in self.until(':') if x!=',']+['{']
                    lines+=self.statements(('END_CASE','ELSE'),True)+['break;','}']
                self.pos+=1; lines+=['}']
            elif token in ('EXIT','RETURN','CONTINUE'): lines+=[{'EXIT':'break;','RETURN':'return;','CONTINUE':'continue;'}[token]]
            else:
                self.pos-=1; items=self.until(';')
                lines += [expr(items)+';' if ':=' in items and ('(' not in items or items.index(':=') < items.index('(')) else self.call(items)]
        return lines


def declaration(text, prefix=''):
    declarations=[]; initial=[]
    blocks=re.findall(r'\bVAR(?:_INPUT|_OUTPUT|_GLOBAL)?(?:\s+CONSTANT)?\s+(.*?)END_VAR',clean(text),re.S)
    for block in blocks:
        for item in block.split(';'):
            if not item.strip(): continue
            item=re.sub(r'\bAT\s+%[IQM]\*', '', item)
            match=re.fullmatch(r'\s*([\w,\s]+)\s*:\s*(.*?)(?:\s*:=\s*(.*))?\s*',item,re.S)
            assert match,item
            names,kind,value=match.groups()
            for name in names.split(','):
                name=name.strip(); target=prefix+'["'+name+'"]' if prefix else name
                if not prefix: declarations.append('Node '+name+';')
                array_bounds=re.findall(r'ARRAY\s*\[\s*(\d+)\s*\.\.\s*(\d+)\s*\]',kind,re.I)
                if value:
                    if value.strip().startswith('['):
                        values=lex(value.strip()[1:-1]); parts=' '.join(values).split(',')
                        for i,v in enumerate(parts,int(array_bounds[0][0]) if array_bounds else 1):
                            initial.append(target+'['+str(i)+'] = '+expr(lex(v))+';')
                    else: initial.append(target+' = '+expr(lex(value))+';')
                fb=kind.strip().split()[-1]
                if fb=='TON' or fb.startswith('MC_'):
                    if array_bounds:
                        lo,hi=map(int,array_bounds[0])
                        for i in range(lo,hi+1): initial.append(target+'['+str(i)+'].kind = "'+fb+'";')
                    else: initial.append(target+'.kind = "'+fb+'";')
    return declarations,initial


def translate(source, name):
    root=ET.fromstring(source)
    declarations,initial=declaration(root.find('.//Declaration').text)
    declarations += ['Node '+action.get('Name')+';' for action in root.findall('.//Action')]
    parser=Parser(root.find('.//ST').text); body=parser.statements()
    assert parser.pos==len(parser.tokens)
    return '\n'.join(['struct '+name+' {','Node& G;']+declarations+['explicit '+name+'(Node& global) : G(global) {']+initial+['}','void cycle() {']+body+['}','};'])
