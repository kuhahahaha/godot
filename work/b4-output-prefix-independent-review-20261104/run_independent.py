from pathlib import Path
import json,hashlib,subprocess,tempfile,shutil
ROOT=Path(__file__).resolve().parents[2];HERE=Path(__file__).resolve().parent;AUTHOR=ROOT/'work/b4-output-prefix-20261104'
header=ROOT/'servers/rendering/renderer_rd/storage_rd/external_output_storage.h';source=header.with_suffix('.cpp');config=header.with_name('external_output_config.cpp')
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def strip(s):return '\n'.join(x for x in s.splitlines() if not x.startswith('#include') and not x.startswith('#pragma once'))
support=(AUTHOR/'control_support.cpp.in').read_text()
support=support.replace('bool busy=false;','bool busy=false;uint32_t busy_after_create=0;int observe_fail_index=-1;')
support=support.replace('  ++creates;bytes.push_back(n);usages.push_back(u.value);','  if(_native_disposal_preflight()!=OK)return _native_disposal_preflight();\n  ++creates;bytes.push_back(n);usages.push_back(u.value);')
support=support.replace('*w.cell={NativeDisposalWitness::State::LIVE,rid.value,1000+creates};cells.push_back(w.cell);return OK;','*w.cell={NativeDisposalWitness::State::LIVE,rid.value,1000+creates};cells.push_back(w.cell);if(creates==busy_after_create)busy=true;return OK;')
support=support.replace('  if(busy)return ERR_BUSY;','  if(_native_disposal_preflight()!=OK)return _native_disposal_preflight();')
support=support.replace('  r=*w.cell;return', '  if(observe_fail_index>=0&&cells[observe_fail_index]==w.cell)return ERR_UNAUTHORIZED;\n  r=*w.cell;return')
c=config.read_text();begin=c.index('Error ExternalOutputConfig::checked_buffer_bytes(');end=c.index('\nError ExternalOutputConfig::decode_u64',begin)
text=support+strip(header.read_text())+'\nnamespace RendererRD {\n'+c[begin:end]+'\n}\n'+strip(source.read_text())+'\n'+(HERE/'independent_cases.cpp.in').read_text()
(HERE/'production-with-mock-dependencies.cpp.in').write_text(text)
rows=[]
with tempfile.TemporaryDirectory(prefix='b4-prefix-independent-') as temp:
 for name,flags in [('normal',[]),('asan-ubsan',['-fsanitize=address,undefined','-fno-omit-frame-pointer'])]:
  binary=str(Path(temp)/name);command=['clang++','-std=c++17','-O1','-Wall','-Wextra','-Werror','-x','c++',str(HERE/'production-with-mock-dependencies.cpp.in'),'-o',binary]+flags
  compile_run=subprocess.run(command,capture_output=True,text=True);assert compile_run.returncode==0,compile_run.stderr
  run=subprocess.run([binary],capture_output=True,text=True,timeout=60)
  log=compile_run.stdout+compile_run.stderr+run.stdout+run.stderr;(HERE/(name+'.log')).write_text(log)
  rows.append({'name':name,'compile_command':command,'rc':run.returncode,'stdout':run.stdout,'stderr':run.stderr,'log_sha256':sha(HERE/(name+'.log'))});assert run.returncode==0,log
source_hashes={str(p.relative_to(ROOT)):sha(p) for p in [header,source,config,ROOT/'tests/servers/rendering/test_external_output_prefix.cpp']}
(HERE/'INDEPENDENT_RESULTS.json').write_text(json.dumps({'production_sources':source_hashes,'rows':rows,'scope':'Verbatim prefix/config production bodies with MOCK RD/witness; neither Engine nor GPU nor actual Core queue'},indent=2)+'\n');print(json.dumps(rows))
