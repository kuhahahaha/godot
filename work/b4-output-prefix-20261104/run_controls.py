#!/usr/bin/env python3
"""Compile verbatim production prefix bodies with MOCK RD/witness dependencies."""
from pathlib import Path
import hashlib,json,subprocess
ROOT=Path(__file__).resolve().parents[2];HERE=Path(__file__).resolve().parent
header=ROOT/'servers/rendering/renderer_rd/storage_rd/external_output_storage.h'
source=ROOT/'servers/rendering/renderer_rd/storage_rd/external_output_storage.cpp'
config=ROOT/'servers/rendering/renderer_rd/storage_rd/external_output_config.cpp'
sha=lambda p:hashlib.sha256(p.read_bytes()).hexdigest()
def strip(text):return '\n'.join(x for x in text.splitlines() if not x.startswith('#include') and not x.startswith('#pragma once'))
c=config.read_text();begin=c.index('Error ExternalOutputConfig::checked_buffer_bytes(');end=c.index('\nError ExternalOutputConfig::decode_u64',begin)
base=(HERE/'control_support.cpp.in').read_text()+strip(header.read_text())+'\nnamespace RendererRD {\n'+c[begin:end]+'\n}\n'+strip(source.read_text())+'\n'+(HERE/'control_tests.cpp.in').read_text()
cases=[('production',None,None,0,None),('lost-failure-retirement','\t\t\trequest_retirement();','\t\t\t/* omitted */',-1,'r.enqueued_count==2'),('premature-enqueued-return','report.returned_count == report.created_count','report.returned_count + report.enqueued_count == report.created_count',-1,'!r.all_driver_returned'),('unknown-reset','\t\t\t\tunknown = true;','\t\t\t\t/* omitted */',-1,'s->observe(r)==ERR_INVALID_DATA&&!r.all_driver_returned')]
rows=[]
for name,old,new,expected,target in cases:
 text=base
 if old:
  assert text.count(old)==1,(name,text.count(old));text=text.replace(old,new)
 path=HERE/(name+'.cpp.in');path.write_text(text);binary=HERE/(name+'.bin')
 cmd=['clang++','-std=c++17','-Wall','-Wextra','-Werror','-x','c++',str(path),'-o',str(binary)]
 built=subprocess.run(cmd,capture_output=True,text=True);assert built.returncode==0,built.stderr
 run=subprocess.run([str(binary)],capture_output=True,text=True)
 passed=run.returncode==0 if expected==0 else run.returncode!=0 and target in run.stderr
 rows.append({'name':name,'source_sha256':sha(path),'binary_sha256':sha(binary),'compile_command':cmd,'compile_exit':built.returncode,'command':[str(binary)],'returncode':run.returncode,'stdout':run.stdout,'stderr':run.stderr,'expected_assertion':target,'matched':passed})
 binary.unlink();assert passed,(name,run.stdout,run.stderr)
(HERE/'CONTROL_RESULTS.json').write_text(json.dumps({'production_sources':{str(p.relative_to(ROOT)):sha(p) for p in [header,source,config]},'rows':rows,'engine_test':False,'gpu':False,'witness_and_rd_ports':'mock; full engine doctests separately provided, not executed here'},indent=2)+'\n')
print('PASS source-body mock control + 3 targeted assertion mutants; not Engine/GPU')
