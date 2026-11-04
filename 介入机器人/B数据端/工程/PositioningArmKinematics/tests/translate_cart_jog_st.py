"""将本次纯计算ST逐句转译为离线比对用C++；不模拟TwinCAT轴I/O。"""
from pathlib import Path
import re
import sys
import xml.etree.ElementTree as ET

source, output = map(Path, sys.argv[1:])
root = ET.parse(source).getroot()
decl = root.find(".//Declaration").text
body = root.find(".//ST").text
body = re.sub(r"//[^\n]*|\(\*.*?\*\)", "", body, flags=re.S)
tokens = re.findall(r":=|<>|>=|<=|\.\.|[A-Za-z_]\w*|\d+(?:\.\d+)?(?:[eE][+-]?\d+)?|[^\s]", body)
pos = 0

def expression(items):
    s = " ".join(items)
    for old, new in {
        "TRUE": "true", "FALSE": "false", "AND": "&&", "OR": "||", "NOT": "!",
        "MIN": "std::min", "MAX": "std::max", "ABS": "std::abs",
        "SIN": "std::sin", "COS": "std::cos", "ACOS": "std::acos",
        "ATAN": "std::atan", "SQRT": "std::sqrt",
    }.items():
        s = re.sub(r"\b" + old + r"\b", new, s)
    s = s.replace("<>", "!=").replace(":=", "=")
    return s

def condition(items):
    return re.sub(r"(?<![<>=!])=(?!=)", "==", expression(items))

def until(marker):
    global pos
    begin = pos
    while tokens[pos] != marker:
        pos += 1
    result = tokens[begin:pos]
    pos += 1
    return result

def statements(stops=()):
    global pos
    lines = []
    while pos < len(tokens) and tokens[pos] not in stops:
        token = tokens[pos]
        pos += 1
        if token == ";":
            continue
        if token == "IF":
            lines.append("if (" + condition(until("THEN")) + ") {")
            lines += statements(("ELSIF", "ELSE", "END_IF"))
            while tokens[pos] == "ELSIF":
                pos += 1
                lines.append("} else if (" + condition(until("THEN")) + ") {")
                lines += statements(("ELSIF", "ELSE", "END_IF"))
            if tokens[pos] == "ELSE":
                pos += 1
                lines.append("} else {")
                lines += statements(("END_IF",))
            assert tokens[pos] == "END_IF"
            pos += 1
            lines.append("}")
        elif token == "FOR":
            head = until("DO")
            var = head[0]
            to = head.index("TO")
            lines.append("for (" + expression(head[:to]) + "; " + var + " <= "
                         + expression(head[to+1:]) + "; ++" + var + ") {")
            lines += statements(("END_FOR",))
            pos += 1
            lines.append("}")
        elif token == "WHILE":
            lines.append("while (" + condition(until("DO")) + ") {")
            lines += statements(("END_WHILE",))
            pos += 1
            lines.append("}")
        elif token == "EXIT":
            lines.append("break;")
        elif token == "RETURN":
            lines.append("return;")
        else:
            pos -= 1
            lines.append(expression(until(";")) + ";")
    return lines

code = ["#pragma once", "#include <array>", "#include <cmath>", "#include <algorithm>", "struct StCartJog {"]
variables = re.findall(r"\bVAR_OUTPUT\s+(.*?)END_VAR|\bVAR(?!\s+CONSTANT)\s+(.*?)END_VAR", decl, re.S)
for output_vars, locals_ in variables:
    for names, kind, init in re.findall(
        r"([A-Za-z_][\w, ]*)\s*:\s*(ARRAY\[1\.\.5\] OF LREAL|LREAL|INT|BOOL)(?:\s*:=\s*([^;]+))?\s*;",
        re.sub(r"//[^\n]*", "", output_vars + locals_),
    ):
        cpp = {"LREAL": "double", "INT": "int", "BOOL": "bool", "ARRAY[1..5] OF LREAL": "std::array<double,6>"}[kind]
        for name in names.split(","):
            code.append(cpp + " " + name.strip() + " = " + (init or "{}") + ";")
constant_block = re.search(r"VAR CONSTANT(.*?)END_VAR", decl, re.S).group(1)
for name, kind, value in re.findall(r"(\w+)\s*:\s*(ARRAY\[1\.\.5\] OF LREAL|LREAL|INT)\s*:=\s*([^;]+);", constant_block):
    if kind in ("LREAL", "INT"):
        cpp = "double" if kind == "LREAL" else "int"
        code.append("static constexpr " + cpp + " " + name + " = " + value + ";")
    else:
        code.append("static constexpr std::array<double,6> " + name + " = {{0," + value.strip()[1:-1] + "}};")
code.append("void step(bool start,bool held,int mode,int dir,double speed,const std::array<double,6>& seed,double dt) {")
code += statements()
code += ["}", "};"]
assert pos == len(tokens)
output.write_text("\n".join(code) + "\n", encoding="utf-8")
print("PASS ST translation:", len(tokens), "tokens;", len(code), "C++ lines; all statements consumed")
