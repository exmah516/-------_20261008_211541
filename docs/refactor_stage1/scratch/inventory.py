"""同一口径比较 main 外层长驻声明，并导出当前词法引用索引；不修改生产文件。"""
from pathlib import Path
import csv
import io
import json
import re
import subprocess

ROOT = Path(__file__).resolve().parents[3]
OUT = ROOT / 'docs/refactor_stage1'
MAIN = '介入机器人/B数据端/工程/ADS项目/main.cpp'
COMMANDS = '介入机器人/B数据端/工程/ADS项目/control_messages.h'


def clean(text):
    pattern = r'''//[^\n]*|/\*[\s\S]*?\*/|L?"(?:\\.|[^"\\])*"|'(?:\\.|[^'\\])*' '''
    return re.sub(pattern, lambda match: re.sub(r'[^\n]', ' ', match[0]), text, flags=re.X)


def declarations(text):
    code = clean(text).splitlines()
    loop = next(n for n, line in enumerate(code, 1) if re.search(r'while\s*\(true\)', line))
    depth, rows, lambda_header = 0, {}, False
    for n, line in enumerate(code, 1):
        outer = depth == 1 and n < loop
        static = n > loop and re.search(r'\bstatic\s+(?!const)', line)
        lambda_start = outer and re.search(r'\bauto\s+\w+\s*=\s*\[', line)
        if (outer or static) and not lambda_start and not lambda_header:
            match = re.match(r'\s*(?:static\s+)?((?:(?:const|constexpr)\s+)?(?:std::[\w:<>]+|homing::Record|(?:unsigned\s+)?(?:short|long|int|char)|[A-Z]\w*|bool|double|wchar_t|char|auto)(?:\s*\*\s*(?:const\s*)?)?)\s+(\w.*)', line)
            if match:
                typ, rest = match.groups()
                pieces, start, nesting = [], 0, 0
                for i, c in enumerate(rest):
                    if c in '([{': nesting += 1
                    elif c in ')]}': nesting -= 1
                    elif c == ',' and nesting == 0:
                        pieces.append(rest[start:i]); start = i + 1
                pieces.append(rest[start:])
                for piece in pieces:
                    name = re.match(r'\s*(\w+)(\[[^]]*\])?', piece)
                    if name:
                        rows.setdefault(name[1], {'name': name[1], 'type': typ.strip() + (name[2] or ''), 'line': n})
        # 原版本的唯一匿名类型实例；其字段另在结构体表统计。
        if depth == 2 and re.match(r'\s*}\s*pending_startup\s*;', line):
            rows['pending_startup'] = {'name': 'pending_startup', 'type': 'PendingStartupParams', 'line': n}
        if lambda_start:
            lambda_header = True
        if lambda_header and '{' in line:
            lambda_header = False
        depth += line.count('{') - line.count('}')
    return list(rows.values()), code, loop


def source(revision, path):
    if revision == 'worktree':
        return (ROOT / path).read_text(encoding='utf-8-sig')
    return subprocess.check_output(['git', 'show', revision + ':' + path], cwd=ROOT).decode('utf-8-sig').replace('\r\n', '\n')


def summarize(lines):
    return ','.join(map(str, sorted(set(lines))))


results = {}
for revision in ('d2bb5fc', 'worktree'):
    text = source(revision, MAIN)
    rows, code, loop = declarations(text)
    command_source = source(revision, COMMANDS)
    block = re.search(r'enum class VisCommandType[^{}]*\{(.*?)\};', clean(command_source), re.S)[1]
    commands = re.findall(r'\b(\w+)\s*=', block)
    results[revision] = {'main_lines': len(text.splitlines()), 'loop_line': loop,
                         'long_lived_declarations': len(rows),
                         'mutable_declarations': sum(not re.search(r'\bconst(?:expr)?\b', row['type']) for row in rows),
                         'internal_command_members_including_none': len(commands), 'declarations': rows}
    if revision == 'worktree':
        for row in rows:
            write, other = [], []
            pattern = re.compile(r'(?<![\w.])' + row['name'] + r'\b')
            for n, line in enumerate(code, 1):
                for match in pattern.finditer(line):
                    tail = line[match.end():]
                    if re.match(r'(?:\s*\[[^]]*\]|\s*\.\s*\w+|\s*->\s*\w+)*\s*(?:=(?!=)|[+*/%|&^-]=|\+\+|--)', tail):
                        write.append(n)
                    else: other.append(n)
            row['explicit_write_lines'] = summarize([row['line'], *write])
            row['other_reference_lines'] = summarize(other)
        with (OUT / 'main_state_inventory.csv').open('w', encoding='utf-8', newline='') as file:
            writer = csv.DictWriter(file, ['name', 'type', 'line', 'explicit_write_lines', 'other_reference_lines'])
            writer.writeheader(); writer.writerows(rows)
        table = ['| 名称 | 类型 | 声明 | 显式写入/初始化 | 其它引用 |', '|---|---|---|---|---|']
        for row in rows:
            cite = lambda value: '`main.cpp:' + str(value) + '`' if value else '—'
            table.append('| `{name}` | `{type}` | {line} | {write} | {read} |'.format(**(row | {'line': cite(row['line'])}), write=cite(row['explicit_write_lines']), read=cite(row['other_reference_lines'])))
        (OUT / 'scratch/main_state_table.md').write_text('\n'.join(table) + '\n', encoding='utf-8')
(OUT / 'size_comparison.json').write_text(json.dumps(results, ensure_ascii=False, indent=2), encoding='utf-8')
print(json.dumps({key: {k: v for k, v in value.items() if k != 'declarations'} for key, value in results.items()}, indent=2))
