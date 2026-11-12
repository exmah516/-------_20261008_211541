import run_survey_checks as c
c.run('legacy_dual_runner', [__import__('sys').executable, c.ads / 'tools/run_dual_control_test.py', c.out / 'legacy_dual'])
c.run('legacy_axis4_window', [__import__('sys').executable, c.ads / 'tools/check_axis4_window.py'])
