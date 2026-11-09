"""用候选重跑E11指定的原T1/T2/T10/T12，保持原输入和阈值。"""
from pathlib import Path

repo = Path(__file__).resolve().parents[2]
setup = (repo / "tools/revision_round7/run_dls.py").read_text(encoding="utf-8").split('run("translate"')[0]
setup = setup.replace("docs/revision_round7_20261109", "docs/revision_round7_smooth_20261109")
setup = setup.replace('build = out / "dls_build"', 'build = out / "e11_build"')
exec(compile(setup, "smooth_e11_setup", "exec"))
candidate = out / "dls_candidate"
original = (tests / "arm_cart_jog_tests.cpp").read_text(encoding="utf-8")
helpers = original[original.index("Axes seed("):original.index("double psi_deg(")]
helpers += original[original.index("void result(const char*"):original.index("double angle_error(")]
helpers += original[original.index("double angle_error("):original.index("int main()")]
body = original[original.index('    std::cout << std::setprecision(12);'):original.index("    auto g = begin(1,1,5,seed(1,90));")]
body += original[original.index("    bool t10=true;"):original.index("    bool y1=true;")]
# 已删除的发生器内部几何缓存改由原测试腕点公式计算，不引入旧算法字段。
body = body.replace("Gen g = begin(1, 1, 5, p);", 'Run r("T1",p,1,1,5); Gen& g=r.g;')
body = body.replace("auto g = begin(mode,1,speed,p);", 'Run r("T2",p,mode,1,speed); Gen& g=r.g;')
body = body.replace("g=begin(mode,1,speed,p);", 'Run r("T10",p,mode,1,speed); Gen& g=r.g;')
body = body.replace("g.wx", "wrist_xy(g.p)[0]").replace("g.wy", "wrist_xy(g.p)[1]")
body = body.replace("g.branch", "(std::sin(p[2]*Gen::RAD+Gen::PI)>=0?1.0:-1.0)")
body = body.replace('tick(g,true,1,1,5,"T1")', 'r.tick(true,1,1,5)')
body = body.replace('tick(g,n<=3000,mode,1,speed,"T2")', 'r.tick(n<=3000,mode,1,speed)')
body = body.replace('tick(g,true,mode,1,speed,"T10")', 'r.tick(true,mode,1,speed)')
body = body.replace('        const auto initial = truth.forward(joints(p));',
    '        const auto initial = truth.forward(joints(p));\n        double case_line=0,case_yaw=0,minimum_s=1;')
body = body.replace('            const auto fk = truth.forward(joints(g.p));',
    '            const auto fk = truth.forward(joints(g.p));\n'
    '            case_line=std::max(case_line,std::abs(fk.position[mode==1?1:0]-initial.position[mode==1?1:0])*1000);\n'
    '            case_yaw=std::max(case_yaw,angle_error(yaw(fk),yaw(initial))/Gen::RAD);\n'
    '            minimum_s=std::min(minimum_s,g.s);')
body = body.replace('        }\n    }\n    std::cout << "T2 line_mm=',
    '        }\n        std::cout << "T2_CASE mode="<<mode<<" speed="<<speed<<" effective_elbow="<<elbow'
    '<<" q2="<<p[1]<<" q3="<<p[2]<<" q4="<<p[3]<<" q5="<<p[4]'
    '<<" line_mm="<<case_line<<" yaw_deg="<<case_yaw<<" damped="<<r.damped<<" minimum_s="<<minimum_s<<"\\n";\n'
    '    }\n    std::cout << "T2 line_mm=', 1)
source = '#define main dls_acceptance_main\n#include "dls_acceptance.cpp"\n#undef main\n#include <cstring>\n'
source += helpers + '\nint main()\n{\n' + body
source += '\n    std::cout << "E11 cycles=" << cycles << " ST_max_difference=" << st_difference << " max_acc=" << max_acceleration << " failures=" << failures << "\\n";\n'
source += '    result("E11_ST_EQ",st_difference==0&&discrete);\n    return failures?1:0;\n}\n'
generated = build / "original_e11_cases.cpp"
generated.write_text(source, encoding="utf-8")
run("e11_translate", [sys.executable, tests / "translate_cart_jog_st.py", candidate / "ArmCartJogGen.TcPOU",
    build / "arm_cart_jog_st_generated.h"])
flags = ["/nologo", "/O2", "/std:c++17", "/EHsc", "/utf-8", "/DNOMINMAX", "/MD", "/W3", "/bigobj",
    "/I" + str(kin / "include"), "/I" + str(kin / ".deps/Library/include/eigen3"),
    "/I" + str(candidate), "/I" + str(build)]
exe = build / "original_e11_cases.exe"
if run("e11_build", [cl, *flags, generated, kin / "src/kinematics.cpp", kin / "src/program_zero.cpp", "/Fe:" + str(exe)]) == 0:
    run("e11_candidate", [exe])
(out / "e11_results.json").write_text(json.dumps(results, indent=2), encoding="utf-8")
raise SystemExit(1 if any(results.values()) else 0)
