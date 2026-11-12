from pathlib import Path
import re

ROOT = Path(__file__).resolve().parents[3]
ADS = ROOT / '介入机器人/B数据端/工程/ADS项目'
MAIN = ADS / 'main.cpp'

def clean_source(s):
    # Preserve every newline and character position; remove string/comment tokens.
    pattern = re.compile(r'''//[^\n]*|/\*[\s\S]*?\*/|L?"(?:\\.|[^"\\])*"|'(?:\\.|[^'\\])*' ''', re.X)
    return pattern.sub(lambda m: ''.join('\n' if c == '\n' else ' ' for c in m[0]), s)

def read(path):
    raw = path.read_text(encoding='utf-8-sig')
    return raw.splitlines(), clean_source(raw).splitlines()

raw, code = read(MAIN)
depth = 0
outer = []
closures = []
for n, (src, line) in enumerate(zip(raw, code), 1):
    before = depth
    if before == 1 and n < 966:
        if re.match(r'\s*auto\s+\w+\s*=\s*\[', line):
            closures.append((n, src.strip()))
        elif re.match(r'\s*(?:const(?:expr)?\s+)?(?:std::[\w:<>]+|homing::Record|[A-Z]\w*|bool|int|double|wchar_t|char|auto|unsigned short|unsigned int|unsigned long)\b', line):
            outer.append((n, src.strip()))
    depth += line.count('{') - line.count('}')

for n, line in enumerate(code,1):
    if n >= 966 and re.search(r'\bstatic\s+(?!const)', line):
        outer.append((n, raw[n-1].strip()))

Path(__file__).with_name('main_outer_declarations.txt').write_text('\n'.join(f'{n}: {s}' for n,s in outer)+'\n',encoding='utf-8')
Path(__file__).with_name('main_outer_closures.txt').write_text('\n'.join(f'{n}: {s}' for n,s in closures)+'\n',encoding='utf-8')

def summarize(nums):
    nums = sorted(set(nums))
    if not nums: return '—'
    groups = []
    for n in nums:
        if groups and n == groups[-1][-1]+1: groups[-1].append(n)
        else: groups.append([n])
    return ','.join(str(g[0]) if len(g)==1 else f'{g[0]}–{g[-1]}' for g in groups)

def refs(name, lines=code):
    pat = re.compile(r'(?<![\w.])'+re.escape(name)+r'\b')
    direct, address, other = [], [], []
    for n,line in enumerate(lines,1):
        hits = list(pat.finditer(line))
        if not hits: continue
        for m in hits:
            tail = line[m.end():]
            # Root variable followed by indexing/member chains then a real assignment.
            lhs = re.match(r'(?:\s*\[[^\]]*\]|\s*\.\s*\w+|\s*->\s*\w+)*\s*(?:=(?!=)|[+*/%|&^-]=|\+\+|--)', tail)
            prefix = line[:m.start()].rstrip()
            if lhs or prefix.endswith(('++','--')): direct.append(n)
            elif prefix.endswith('&'): address.append(n)
            else: other.append(n)
    return summarize(direct), summarize(address), summarize(other)

def parse_decl(n, text):
    src = text.split('//',1)[0].strip().rstrip(';')
    src = re.sub(r'^static\s+', '', src)
    m = re.match(r'(.+?)\s+([A-Za-z_]\w*)(.*)$', src)
    if not m: return []
    # Split at the first identifier whose prefix is a recognized type.
    known = r'(?:(?:const(?:expr)?\s+)?(?:std::[\w:<>]+|homing::Record|(?:unsigned\s+)?(?:short|long|int|char)|[A-Z]\w*|bool|double|wchar_t|char|auto))(?:\s*\*)?'
    m = re.match(r'('+known+r')\s+(.*)$',src)
    if not m: return []
    typ, rest = m.groups()
    # Main declarations have no comma inside their type; split top-level initializer commas.
    parts, start, stack = [],0,0
    for i,c in enumerate(rest):
        if c in '([{<': stack+=1
        elif c in ')]}>': stack-=1
        elif c==',' and stack==0: parts.append(rest[start:i]);start=i+1
    parts.append(rest[start:])
    results=[]
    for p in parts:
        x=re.match(r'\s*([A-Za-z_]\w*)(\[[^\]]+\])?',p)
        if x and x[1] not in ('return','if','else'): results.append((n,typ+(x[2] or ''),x[1],text))
    return results

decls=[]
for n,s in outer: decls.extend(parse_decl(n,s))
decls.append((926,'PendingStartupParams','pending_startup','} pending_startup;'))
not_outer = {'rebuild_window','log_window_rebuild','purpose','replace_pending','reason','source'}
decls = [d for d in decls if d[2] not in not_outer]
unique={}
for d in decls: unique.setdefault(d[2],d)
decls=sorted(unique.values())

with Path(__file__).with_name('main_outer_inventory.tsv').open('w',encoding='utf-8') as f:
    f.write('name\ttype\tdefinition\texplicit_assignment_lines\taddress_escape_lines\tother_reference_lines\n')
    for n,t,name,s in decls:
        w,a,r=refs(name)
        f.write(f'{name}\t{t}\t{n}\t{w}\t{a}\t{r}\n')

print(f'{len(decls)} long-lived declarations, {len(closures)} outer closures')
