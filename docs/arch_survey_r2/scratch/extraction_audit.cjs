// 只执行既有 runner 的抽取逻辑；所有构建/执行/临时写入均由内存替身截断。
const fs = require('fs'), path = require('path'), vm = require('vm'), crypto = require('crypto');
const root = path.resolve(__dirname, '../../..');
const ads = path.join(root, '介入机器人/B数据端/工程/ADS项目');
const runner = path.join(ads, 'tools/run_offline_tests.cjs');
const reads = new Map(), ranges = [], outputs = [], builds = [];
const read = (p, ...args) => { const v = fs.readFileSync(p,...args); if (typeof v === 'string') reads.set(v.replace(/^\uFEFF/,'').replace(/\r\n/g,'\n'),p); return v; };
const fakeFs = {readFileSync: read, mkdtempSync: p => p+'audit', mkdirSync:()=>{}, existsSync:()=>false, copyFileSync:()=>{}, writeFileSync:(p,text)=>outputs.push({name:path.basename(p),text})};
const fakeCp = {execFileSync:()=> 'C:/not-executed',spawnSync:(c,a,o)=>{builds.push({c,a,o});return {stdout:'',stderr:'',status:0};}};
function capture(text,start,end,line){ const lo=text.indexOf(start),hi=text.indexOf(end,lo); if(lo<0||hi<0)throw Error('Missing '+start); const code=text.slice(lo,hi); const p=reads.get(text); if(!p)throw Error('Unmapped input'); ranges.push({source:path.relative(root,p).replace(/\\/g,'/'),runner_line:line,start_line:text.slice(0,lo).split('\n').length,end_line:text.slice(0,hi-1).split('\n').length,start_marker:start,end_marker:end,code,sha256:crypto.createHash('sha256').update(code).digest('hex')}); return code; }
let source=fs.readFileSync(runner,'utf8');
source=source.replace('const lo = text.indexOf(start), hi = text.indexOf(end, lo);','const calls = [...new Error().stack.matchAll(/run_offline_tests.cjs:([0-9]+):/g)]; return __capture(text,start,end,Number(calls[1][1])); const lo = text.indexOf(start), hi = text.indexOf(end, lo);');
vm.runInNewContext(source,{require:n=>n==='fs'?fakeFs:n==='child_process'?fakeCp:n==='os'?{tmpdir:()=>__dirname}:require(n),__dirname:path.dirname(runner),__capture:capture,process:{argv:['node',runner,'all'],env:process.env,stdout:{write:()=>{}},exit:c=>{throw Error('Unexpected exit '+c);}},console:{log:()=>{}}},{filename:'run_offline_tests.cjs'});
for(const r of ranges){ const matches=outputs.filter(o=>o.name.endsWith('.inc')&&(o.text===r.code||o.text===r.code.replace(/else if/g,'if')||o.text===r.code.split('\n').slice(1).join('\n'))); r.outputs=matches.map(o=>o.name); delete r.code; }
const main=fs.readFileSync(path.join(ads,'main.cpp'),'utf8').replace(/^\uFEFF/,'').replace(/\r\n/g,'\n');
function intervals(rs){const sorted=rs.map(r=>[r.start_line,r.end_line]).sort((a,b)=>a[0]-b[0]),union=[];for(const pair of sorted){const prev=union[union.length-1];if(prev&&pair[0]<=prev[1]+1)prev[1]=Math.max(prev[1],pair[1]);else union.push(pair.slice());}return union;}
const mainLineCount=main.split('\n').length-(main.endsWith('\n')?1:0);
const union=intervals(ranges.filter(r=>r.source.endsWith('/main.cpp'))),uncovered=[];let cursor=1;for(const [a,b]of union){if(a>cursor)uncovered.push([cursor,a-1]);cursor=b+1;}if(cursor<=mainLineCount)uncovered.push([cursor,mainLineCount]);
const result={runner:path.relative(root,runner).replace(/\\/g,'/'),ranges,main_union:union,main_not_extracted_by_this_runner:uncovered,build_calls:builds.length};
if(!ranges.length||builds.length!==11)throw Error('Audit self-check failed');
fs.writeFileSync(path.join(__dirname,'extraction_manifest.json'),JSON.stringify(result,null,2)+'\n','utf8');
console.log(JSON.stringify({ranges:ranges.length,main_ranges:ranges.filter(r=>r.source.endsWith('/main.cpp')).length,main_union:union,uncovered,builds:builds.length},null,2));
