#!/usr/bin/env python3
"""Observe the original storage owner's cumulative limit on real main RD."""
import hashlib,json,os,re,subprocess
from pathlib import Path
p=Path(__file__).resolve().parent;repo=p.parents[1];editor=repo/'bin/godot.macos.editor.dev.arm64'
lib=Path('/Users/macbookair/Documents/Work/ANGE/godot-cpp/bin/libgodot-cpp.macos.template_debug.arm64.a')
inputs=[editor,p/'runtime/probe.dylib',p/'runtime/probe.gd',p/'runtime/probe.gdextension',p/'probe.cpp',lib]
def sha(q):return hashlib.sha256(q.read_bytes()).hexdigest()
before={str(x):sha(x) for x in inputs}
cmd=[str(editor),'--disable-crash-handler','--path',str(p/'runtime'),'--rendering-driver','metal','--rendering-method','mobile','--render-thread','safe','--script','probe.gd']
env=dict(os.environ,ANGE_MACOS_BACKGROUND_RENDER='1')
try:
 run=subprocess.run(cmd,env=env,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,timeout=180)
 rc=run.returncode;log=run.stdout.decode('utf-8',errors='replace');timed_out=False
except subprocess.TimeoutExpired as e:
 rc=None;log=(e.stdout or b'').decode('utf-8',errors='replace');timed_out=True
(p/'metal.log').write_text(log)
rows=re.findall(r'CAPACITY_RETURN index=(\d+) backing=(\d+) event=(\d+) ack=1',log)
checks={
 'exit':rc==0 and not timed_out,
 'real_metal':'Metal ' in log and 'Apple M3 Pro' in log,
 'physical_returns_128':len(rows)==128 and [int(x[0]) for x in rows]==list(range(128)) and len({x[1] for x in rows})==128 and len({x[2] for x in rows})==128,
 '129_refused':'CAPACITY_OBSERVED completed=128 attempted=129 error=5 reason=19 no_backing=1' in log,
 'old_request_rejected':'CAPACITY_OLD_REQUEST_REJECTED completed=128 error=33 reason=17 no_backing=1 new_rt_operation=1' in log,
 'clean_bridge':'CAPACITY_CONTROL_DONE physical_returns=128 acknowledgements=128 bridge_closed=1' in log,
 'manager_close':log.count('ORIGINAL_NUMERIC_LOAD=0\n')==1 and log.count('ORIGINAL_NUMERIC_UNLOAD=0\n')==1,
 'dso_finalizer':0<=log.find('ORIGINAL_NUMERIC_DSO_UNLOAD\n')<log.find('ORIGINAL_NUMERIC_UNLOAD=0\n'),
 'clean':not any(x in log for x in ('ERROR:','FATAL','Leaked instance','SIGSEGV','SIGABRT')),
}
after={str(x):sha(x) for x in inputs};out={'command':cmd,'rc':rc,'timed_out':timed_out,'checks':checks,'return_count':len(rows),'inputs_before':before,'inputs_after':after,'inputs_unchanged':before==after}
(p/'METAL_RECEIPT.json').write_text(json.dumps(out,indent=2)+'\n')
print('rc',rc,'returns',len(rows),'failed',[k for k,v in checks.items() if not v],'inputs_unchanged',before==after,flush=True)
if not all(checks.values()) or before!=after:raise SystemExit(1)
