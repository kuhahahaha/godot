#!/usr/bin/env python3
"""Only the original issued-prefix cancellation control; no canonical build."""
import argparse, hashlib, json, os, re, struct, subprocess
from pathlib import Path
P=Path(__file__).resolve().parent
B=P.parent/'native-numeric-same-request-20261028-r1'
def sha(q):
    h=hashlib.sha256()
    with q.open('rb') as f:
        for b in iter(lambda:f.read(4*1024*1024),b''): h.update(b)
    return {'bytes':q.stat().st_size,'sha256':h.hexdigest()}
a=argparse.ArgumentParser(); a.add_argument('--build',action='store_true'); args=a.parse_args()
build=json.loads((P/'BUILD_COMMAND.json').read_text())
if args.build:
    with (P/'BUILD.log').open('w') as f: r=subprocess.run(build,stdout=f,stderr=subprocess.STDOUT)
    (P/'BUILD.rc').write_text(str(r.returncode)+'\n')
    if r.returncode: raise SystemExit(r.returncode)
cmd=json.loads((P/'RUN_COMMAND.json').read_text())
gen=json.loads((B/'NATIVE_GENERATION_RECEIPT.json').read_text())
assert sha(Path(cmd[0]))==gen['core'], 'Core changed since native generation'
for i,k in enumerate(('capture','body')):
    for part,ext in [('source','comp'),('spirv','spv'),('container','bin')]:
        assert sha(B/'input'/f'{k}.{ext}')==gen['physical'][i][part]
files=[Path(cmd[0]),P/'runtime/probe.dylib',Path(__file__)]
files += [P/x for x in ('probe.cpp','late_cut.inc','late_release.inc','original_unit.h','expected_unit.h','expected_artifacts.h','runtime/probe.gd','runtime/probe.gdextension','runtime/project.godot','BUILD_COMMAND.json','RUN_COMMAND.json')]
files += [B/'NATIVE_GENERATION_RECEIPT.json',B/'input/unit.json']
files += [B/'input'/f'role{i}.bin' for i in range(8)]
files += [B/'input'/f'{k}.{e}' for k in ('capture','body') for e in ('comp','spv','bin')]
files += [Path(x) for x in build if x.endswith(('.o','.a'))]
def pins(): return {str(q):sha(q) for q in dict.fromkeys(files)}
before=pins(); (P/'RUN_INPUTS_BEFORE.json').write_text(json.dumps(before,indent=2)+'\n')
out=P/'result'; out.mkdir(exist_ok=True)
for n in ('facts.bin','report.bin','before_cancel_facts.bin','before_cancel_report.bin','owned_readback.bin'):
    (out/n).unlink(missing_ok=True)
env={**os.environ,'ANGE_MACOS_BACKGROUND_RENDER':'1','NUMERIC_CONTROL_INPUT':str(B/'input'),'NUMERIC_CONTROL_OUTPUT':str(out)}
env.pop('NUMERIC_CONTROL_CANCEL_PREFIX',None)
with (P/'RUN.log').open('w') as f:
    try: r=subprocess.run(cmd,env=env,stdout=f,stderr=subprocess.STDOUT,timeout=90); rc=r.returncode
    except subprocess.TimeoutExpired: rc='timeout'
(P/'RUN.rc').write_text(str(rc)+'\n')
t=(P/'RUN.log').read_text()
checks={'rc':rc==0,'metal':'Metal ' in t and 'Apple M3 Pro' in t,
 'original_unit':gen['unit_digest'] in t,
 'two_interfaces':len(re.findall(r'ORIGINAL_INTERFACE_MATCH physical=[01] ',t))==2,
 'wrong_identity_refused':'LATE_IDENTITY_FAILURE error=30 no_record_reply=1 original_ticket_preserved=1' in t,
 'retained_reply':bool(re.search(r'LATE_RECORD_REPLY_HELD .* observed=1 ack=0',t)),
 'actual_pending_cut':bool(re.search(r'LATE_CUT_NATIVE semantic=[45] issue=[12] completion=1 native=[1-9]\d*',t)),
 'recorded_readback':'capture=1 body=1 exact_readback_issued=1 reply_still_held=1' in t,
 'cancel_and_busy_close':'LATE_STOPPED cancel=1 new_record_refused=44 reason=16 early_backing_pending=1 early_unit_pending=1 ticket_release_busy=1 bridge_close_busy=1' in t,
 'retained_original_return':'exact_prefix_preserved=1 no_new_graph=1 revoked_copy_denied=1 retained_reply_blocked_release=1 reply_ack_once=1' in t,
 'unit_returns':len(re.findall(r'ORIGINAL_UNIT_PHYSICAL_RETURN index=[01] ',t))==2,
 'storage_returns':set(re.findall(r'ORIGINAL_STORAGE_PHYSICAL_RETURN role=(\d+) ',t))==set(map(str,range(8))),
 'bridge_closed':'rt_slots_released=1 physical_returns=1 bridge_closed=1 mode=post_submit_cancel' in t,
 'binding_terminal':t.count('ORIGINAL_CODE_HOLD_BINDING_RETURN stage=2\n')==1,
 'user_terminators':all(t.count(f'ORIGINAL_SDK_USER_TERMINATOR level={i}\n')==1 for i in (2,3)),
 'manager':t.count('ORIGINAL_NUMERIC_LOAD=0\n')==1 and t.count('ORIGINAL_NUMERIC_UNLOAD=0\n')==1,
 'image_finalizer':t.count('ORIGINAL_NUMERIC_DSO_UNLOAD\n')==1 and 0<=t.find('ORIGINAL_NUMERIC_DSO_UNLOAD\n')<t.find('ORIGINAL_NUMERIC_UNLOAD=0\n'),
 'revoked_bytes':not (out/'owned_readback.bin').exists() and 'ORIGINAL_EXACT_READBACK' not in t,
 'clean':not any(x in t for x in ('ERROR:','FATAL','leaked at exit','SIGSEGV','SIGABRT'))}
# Independent raw ABI decode (112B original fact), separate from C++ checks/log flags.
def facts(q):
    data=q.read_bytes(); assert len(data)%112==0
    names='graph_instance graph_frame graph_kind graph_index graph_offset semantic command_slot queue phase issue completion error native completion_source native_code parent reserved backing offset size'.split()
    return [dict(zip(names,v)) for v in struct.iter_unpack('<QQ10IQQqIIQQQ',data)]
if rc==0:
    pre=facts(out/'before_cancel_facts.bin'); post=facts(out/'facts.bin')
    identity=('graph_instance','graph_frame','graph_kind','graph_index','graph_offset','semantic','backing','offset','size')
    checks['raw_prefix']=len(post)>=len(pre) and all(all(x[k]==y[k] for k in identity) for x,y in zip(pre,post))
    checks['raw_native_identity']=all(all(x[k]==y[k] for k in ('native','completion_source','queue','command_slot')) for x,y in zip(pre,post) if x['semantic'] in (4,5))
    checks['raw_no_new_graph']=all(x['semantic'] in (4,5) and x['parent']<len(pre) and pre[x['parent']]['graph_instance'] for x in post[len(pre):])
    checks['raw_pending']=any(x['semantic'] in (4,5) and x['issue'] in (1,2) and x['completion']==1 and x['native'] for x in pre)
    checks['raw_final']=all(x['issue']==1 and x['completion']==2 and x['error']==0 and x['native'] and x['completion_source'] for x in post)
    # Original report ABI: state/phase at 16/20, request at 80, owned size at112.
    r0=(out/'before_cancel_report.bin').read_bytes(); r1=(out/'report.bin').read_bytes()
    checks['raw_report']=len(r0)==len(r1)==184 and struct.unpack_from('<II',r1,16)==(7,6) and struct.unpack_from('<Q',r0,80)[0]==struct.unpack_from('<Q',r1,80)[0]!=0 and struct.unpack_from('<I',r1,112)[0]==0
    decoded={'before':pre,'after':post}
else: decoded={}
after=pins(); (P/'RUN_INPUTS_AFTER.json').write_text(json.dumps(after,indent=2)+'\n'); checks['unchanged_inputs']=before==after
result={'scope':'actual original issued-prefix cancellation; no Host/Verified/U08/Ready/G1 claim','command':cmd,'rc':rc,'checks':checks,'decoded_facts':decoded,'outputs':{q.name:sha(q) for q in out.iterdir() if q.is_file()}}
(P/'RESULTS.json').write_text(json.dumps(result,indent=2)+'\n')
print('late-return rc=',rc,'failed=',[k for k,v in checks.items() if not v],flush=True)
raise SystemExit(0 if all(checks.values()) else 1)
