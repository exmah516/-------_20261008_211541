const fs = require('fs');
const path = require('path');
const crypto = require('crypto');
const cp = require('child_process');

const root = path.resolve(__dirname, '../..');
const runner = path.join(root, '介入机器人/B数据端/工程/ADS项目/tools/run_offline_tests.cjs');
const out = path.resolve(process.argv[2] || path.join(root, 'docs/refactor_stage0/logs/baseline'));
fs.mkdirSync(out, { recursive: true });

const suites = ['dual', 'recovery', 'cylinder', 'ads', 'handle', 'recording'];
const artifacts = [];
for (const suite of suites) {
  const report = path.join(out, suite);
  fs.mkdirSync(report, { recursive: true });
  const r = cp.spawnSync(process.execPath, [runner, suite, report], {
    cwd: path.dirname(runner), encoding: 'utf8'
  });
  const log = (r.stdout || '') + (r.stderr || '');
  fs.writeFileSync(path.join(out, suite + '.log'), log, 'utf8');
  if (r.status !== 0) {
    console.error(log);
    process.exit(r.status || 1);
  }
  artifacts.push({
    suite,
    log: path.relative(root, path.join(out, suite + '.log')).replaceAll('\\', '/'),
    sha256: crypto.createHash('sha256').update(log).digest('hex'),
    status: 'pass'
  });
}

const sourceFiles = [
  'main.cpp', 'control_types.h', 'control_types.cpp', 'motion_sync.h', 'motion_sync.cpp',
  'return_recovery.h', 'force_feedback.h', 'force_feedback.cpp', 'control_messages.h',
  'plc_io.h', 'plc_io.cpp', 'ads_communication.h', 'ads_communication.cpp'
];
const sourceHashes = Object.fromEntries(sourceFiles.map(f => {
  const p = path.join(root, '介入机器人/B数据端/工程/ADS项目', f);
  return [f, crypto.createHash('sha256').update(fs.readFileSync(p)).digest('hex')];
}));

const scenarios = [
  ['普通双手柄跟随', 'dual'],
  ['轴1先行', 'dual'],
  ['轴4对轴6联动', 'dual'],
  ['双手柄换手四种触发', 'dual'],
  ['计划回退 Dual/重试/可能已启动/故障续做', 'recovery'],
  ['屈曲恢复与保持后重新按键', 'dual'],
  ['连接保持、急停、回退故障、寻参复用', 'recovery'],
  ['开始控制、电缸手动、Y阀、注射器、轴4租约', 'cylinder'],
  ['力反馈滤波、换手保持、取零', 'recording']
].map(([name, suite]) => ({
  name, suite, trace: 'existing-offline-suite',
  per_tick_fields: ['refer[7]', 'cylinder[4]', 'axis1_fast_return', 'axis6_fast_retract', 'planned_return_req', 'state'],
  exact_before_after: 'pending-after-deletion'
}));

const manifest = {
  schema_version: 1,
  generated_utc: new Date().toISOString(),
  production_source_hashes: sourceHashes,
  suites: artifacts,
  scenarios,
  r2_8_assertions: 51,
  note: '现有离线 runner 没有统一逐拍 trace 接口；本文件先固定完整套件输出与生产源哈希，不能把日志摘要冒充逐拍黄金值。'
};
fs.writeFileSync(path.join(out, 'manifest.json'), JSON.stringify(manifest, null, 2), 'utf8');
console.log('PASS ' + artifacts.length + ' suites; baseline manifest: ' + path.join(out, 'manifest.json'));
