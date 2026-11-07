"""将本次纯计算ST逐句转译为离线比对用C++；不模拟TwinCAT轴I/O。"""
from pathlib import Path
import re
import sys
import xml.etree.ElementTree as ET

source, output = map(Path, sys.argv[1:3])
wrapper = len(sys.argv) == 4 and sys.argv[3] == "--wrapper"
root = ET.parse(source).getroot()
decl = root.find(".//Declaration").text
body = root.find(".//ST").text
body = re.sub(r"//[^\n]*|\(\*.*?\*\)", "", body, flags=re.S)
tokens = re.findall(r":=|<>|>=|<=|\.\.|[A-Za-z_]\w*|\d+(?:\.\d+)?(?:[eE][+-]?\d+)?|[^\s]", body)
pos = 0

def expression(items):
    s = " ".join("==" if token == "=" else "=" if token == ":=" else token for token in items)
    for old, new in {
        "TRUE": "true", "FALSE": "false", "AND": "&&", "OR": "||", "NOT": "!",
        "MIN": "std::min", "MAX": "std::max", "ABS": "std::abs",
        "SIN": "std::sin", "COS": "std::cos", "ACOS": "std::acos",
        "ATAN": "std::atan", "SQRT": "std::sqrt",
        "UDINT_TO_LREAL": "double", "POSITIONTYPE_ABSOLUTE": "0",
    }.items():
        s = re.sub(r"\b" + old + r"\b", new, s)
    s = s.replace("<>", "!=")
    s = re.sub(r"TwinCAT_SystemInfoVarList\s*\.\s*_TaskInfo\s*\[\s*1\s*\]\s*\.\s*CycleTime", "cycle_time_100ns", s)
    s = re.sub(r"TwinCAT_SystemInfoVarList\s*\.\s*_TaskInfo\s*\[\s*1\s*\]\s*\.\s*FirstCycle", "first_cycle", s)
    return s

def condition(items):
    return expression(items)

assert expression(["valid", ":=", "dir", "=", "1"]) == "valid = dir == 1"

def until(marker):
    global pos
    begin = pos
    while tokens[pos] != marker:
        pos += 1
    result = tokens[begin:pos]
    pos += 1
    return result

def case_label():
    return re.match(r"\d+(?: , \d+)* :", " ".join(tokens[pos:])) is not None

def call(items):
    opening = items.index("(")
    target = expression(items[:opening])
    values, start, depth = [], opening+1, 0
    for n in range(start, len(items)):
        if items[n] in ("(", "["): depth += 1
        elif items[n] in (")", "]"): depth -= 1
        if depth == 0 and items[n] == "," or depth == -1:
            argument = items[start:n]
            assert argument[1] == ":="
            values.append(condition(argument[2:]))
            start = n+1
    if target == "gen": target += ".step"
    elif target == "MC_ExtSetPointGenFeed": target = "feed"
    else: target += ".call"
    return target + "(" + ",".join(values) + ");"

def statements(stops=(), in_case=False):
    global pos
    lines = []
    while pos < len(tokens) and tokens[pos] not in stops:
        if in_case and case_label(): break
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
        elif token == "CASE":
            lines.append("switch (" + expression(until("OF")) + ") {")
            while tokens[pos] != "END_CASE":
                labels = until(":")
                lines += ["case " + value + ":" for value in labels if value != ","]
                lines += ["{"] + statements(("END_CASE",), True) + ["break;", "}"]
            pos += 1
            lines.append("}")
        elif token == "EXIT":
            lines.append("break;")
        elif token == "RETURN":
            lines.append("return;")
        else:
            pos -= 1
            items = until(";")
            lines.append(call(items) if items[0] in ("gen", "fb_enable", "fb_disable", "fb_stop", "MC_ExtSetPointGenFeed")
                         else expression(items) + ";")
    return lines

code = ["#pragma once", "#include <array>", "#include <cmath>", "#include <algorithm>"]
code += ["#include \"arm_cart_jog_stage_signals.h\"", "struct StCartJogStage : JogStageSignals<StCartJog> {"] if wrapper else ["struct StCartJog {"]
variables = re.findall(r"\bVAR_OUTPUT\s+(.*?)END_VAR|\bVAR(?!\s+CONSTANT)\s+(.*?)END_VAR", decl, re.S)
for output_vars, locals_ in variables:
    for names, kind, init in re.findall(
        r"([A-Za-z_][\w, ]*)\s*:\s*(ARRAY\[1\.\.5\] OF (?:MC_\w+|LREAL|DINT)|ArmCartJogGen|LREAL|UDINT|INT|BOOL)(?:\s*:=\s*([^;]+))?\s*;",
        re.sub(r"//[^\n]*", "", output_vars + locals_),
    ):
        if wrapper and (kind == "ArmCartJogGen" or "MC_" in kind): continue
        cpp = {"LREAL": "double", "INT": "int", "UDINT": "std::uint32_t", "BOOL": "bool",
               "ARRAY[1..5] OF LREAL": "std::array<double,6>", "ARRAY[1..5] OF DINT": "std::array<int,6>"}[kind]
        for name in names.split(","):
            code.append(cpp + " " + name.strip() + " = " + (init or "{}") + ";")
constant_block = re.search(r"VAR CONSTANT(.*?)END_VAR", decl, re.S).group(1)
for name, kind, value in re.findall(r"(\w+)\s*:\s*(ARRAY\[1\.\.5\] OF LREAL|LREAL|INT)\s*:=\s*([^;]+);", constant_block):
    if kind in ("LREAL", "INT"):
        cpp = "double" if kind == "LREAL" else "int"
        code.append("static constexpr " + cpp + " " + name + " = " + value + ";")
    else:
        code.append("static constexpr std::array<double,6> " + name + " = {{0," + value.strip()[1:-1] + "}};")
code.append("void step() {" if wrapper else "void step(bool start,bool held,int mode,int dir,double speed,const std::array<double,6>& seed,double cycle_dt) {")
code += statements()
code += ["}", "};"]
assert pos == len(tokens)
output.write_text("\n".join(code) + "\n", encoding="utf-8")
print("PASS ST translation:", len(tokens), "tokens;", len(code), "C++ lines; all statements consumed")
