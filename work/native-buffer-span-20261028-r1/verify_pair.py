#!/usr/bin/env python3
"""Reproducible original-source RED / current-source GREEN, same original archives."""
import hashlib,json,subprocess
from pathlib import Path
P=Path(__file__).resolve().parent
ROOT=P.parent.parent
ORIGINAL='cdea26ccd0'
def sha(q):
 h=hashlib.sha256()
 with q.open('rb') as f:
  for b in iter(lambda:f.read(4*1024*1024),b''):h.update(b)
 return {'bytes':q.stat().st_size,'sha256':h.hexdigest()}
def run(name,cmd):
 (P/(name+'.json')).write_text(json.dumps(cmd,indent=2)+'\n')
 with (P/(name+'.log')).open('w') as f:r=subprocess.run(cmd,cwd=ROOT,stdout=f,stderr=subprocess.STDOUT,timeout=90)
 (P/(name+'.rc')).write_text(str(r.returncode)+'\n')
 print(name,r.returncode,flush=True)
 return r.returncode
original=P/'original_rendering_device.cpp'
original.write_bytes(subprocess.check_output(['git','show',ORIGINAL+':servers/rendering/rendering_device.cpp'],cwd=ROOT))
cc=json.loads((P/'RD_COMPILE.json').read_text());cc[cc.index('-o')+1]=str(P/'original_rendering_device.o');cc[-1]=str(original);cc.insert(-1,'-Iservers/rendering')
red=json.loads((P/'GREEN_LINK.json').read_text());red=[str(P/'original_rendering_device.o') if x.endswith('native-buffer-span-20261028-r1/rendering_device.o') else x for x in red]
green=json.loads((P/'GREEN_LINK.json').read_text())
# Compile each claimed source now; a reusable object is never source proof.
assert run('PAIRED_TEST_COMPILE',json.loads((P/'TEST_COMPILE.json').read_text()))==0
assert run('ORIGINAL_RD_COMPILE',cc)==0
assert run('PAIRED_CURRENT_RD_COMPILE',json.loads((P/'RD_COMPILE.json').read_text()))==0
inputs=[ROOT/x for x in green if x.endswith(('.o','.a'))]
inputs += [P/'original_rendering_device.o']
inputs += [ROOT/'servers/rendering/rendering_device.cpp',ROOT/'tests/servers/rendering/test_external_output_contract.cpp',original,Path(__file__)]
inputs=list(dict.fromkeys(inputs))
before={str(q):sha(q) for q in inputs};(P/'PAIR_INPUTS_BEFORE.json').write_text(json.dumps(before,indent=2)+'\n')
assert run('PAIRED_RED_LINK',red)==0
cmd=json.loads((P/'RED_RUN_COMMAND.json').read_text()); assert run('PAIRED_RED_RUN',cmd)==1
redlog=(P/'PAIRED_RED_RUN.log').read_text();assert '111 | 105 passed | 6 failed' in redlog
redbin=ROOT/green[green.index('-o')+1];(P/'PAIRED_RED_BINARY.json').write_text(json.dumps(sha(redbin),indent=2)+'\n')
assert run('PAIRED_GREEN_LINK',green)==0
cmd=json.loads((P/'GREEN_RUN_COMMAND.json').read_text());assert run('PAIRED_GREEN_RUN',cmd)==0
greenlog=(P/'PAIRED_GREEN_RUN.log').read_text();assert '280 | 280 passed | 0 failed' in greenlog
(P/'PAIRED_GREEN_BINARY.json').write_text(json.dumps(sha(redbin),indent=2)+'\n')
after={str(q):sha(q) for q in inputs};(P/'PAIR_INPUTS_AFTER.json').write_text(json.dumps(after,indent=2)+'\n');assert before==after
(P/'PAIR_RESULT.json').write_text(json.dumps({'original_source_commit':ORIGINAL,'original_source_identity':sha(original),'unchanged_link_inputs':True,'red':{'cases':3,'assertions':111,'failures':6},'green':{'cases':5,'assertions':280,'failures':0},'scope':'original RD source under current same archive inputs; CPU receiver only, no real GPU or owner grant'},indent=2)+'\n')
print('Pair confirmed; exact link inputs unchanged.',flush=True)
