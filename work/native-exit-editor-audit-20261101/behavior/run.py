#!/usr/bin/env python3
"""Three bounded ordinary headless subprocesses; behavioral RED is not a timeout or leak count."""
import argparse,hashlib,json,re,subprocess,time
from pathlib import Path
HERE=Path(__file__).resolve().parent
REPO=HERE.parents[2]
def sha(p):return hashlib.sha256(Path(p).read_bytes()).hexdigest()
parser=argparse.ArgumentParser(description=__doc__)
parser.add_argument('--pin',type=Path,default=REPO/'work/physics-wrapper-finish-recovery-20261101/PINNED_HOST.json')
parser.add_argument('--expect-fixed',action='store_true',help='After the source repair, all three cases must pass their actual-free assertions')
args=parser.parse_args()
pin=json.loads(args.pin.read_text());binary=Path(pin['binary']);controlled=Path(pin['controlled_build'])
assert sha(binary)==pin['sha256'] and sha(controlled)==pin['controlled_build_sha256']
output=HERE/'results'/time.strftime('%Y%m%d-%H%M%S');output.mkdir(parents=True,exist_ok=False)
receipt={'scope':'ordinary Godot selected SceneTree delete-queue ownership; no GPU/native extension/product acceptance','pin':pin,'script_sha256':sha(HERE/'probe/queue_free.gd'),'project_sha256':sha(HERE/'probe/project.godot'),'expect_fixed':args.expect_fixed,'runs':[]}
for case,editor,mode in [('editor_detached',True,'detached'),('runtime_detached',False,'detached'),('editor_attached',True,'attached')]:
 command=[str(binary),'--headless']+(['--editor'] if editor else [])+['--path',str(HERE/'probe'),'--script',str(HERE/'probe/queue_free.gd'),'--',mode]
 log=output/(case+'.log')
 print('running',case,flush=True)
 with log.open('w') as stream:
  try:
   done=subprocess.run(command,cwd=REPO,stdout=stream,stderr=subprocess.STDOUT,timeout=30);code=done.returncode
  except subprocess.TimeoutExpired:
   code='timeout'
 text=log.read_text(errors='replace');matches=re.findall(r'QUEUE_FREE_RESULT (\{[^\n]+\})',text)
 result=json.loads(matches[0]) if len(matches)==1 else None
 expect_red=editor and mode=='detached' and not args.expect_fixed
 exact_red=code==42 and result=={'editor':True,'mode':'detached','actual_self_frames':2,'node_invalid':False,'tree_exited':1,'predelete':0,'passed':False} and 'QUEUE_FREE_RED cause=queue_did_not_drain_after_two_actual_tree_frames' in text and text.index('QUEUE_FREE_RESULT ')<text.index('QUEUE_FREE_MANUAL_CLEANUP ')
 exact_green=code==0 and result=={'editor':editor,'mode':mode,'actual_self_frames':2,'node_invalid':True,'tree_exited':1,'predelete':1,'passed':True} and 'QUEUE_FREE_GREEN actual_free=1 tree_exited=1 predelete=1' in text
 checks={'not_timeout':code!='timeout','one_behavior_result':len(matches)==1,'actual_expected_behavior':exact_red if expect_red else exact_green,'no_parse_or_script_error':not re.search(r'Parse Error|SCRIPT ERROR|QUEUE_FREE_UNEXPECTED',text)}
 counts=re.search(r'(\d+) ObjectDB instances were leaked at exit',text)
 receipt['runs'].append({'case':case,'command':command,'returncode':code,'expected_behavior':'precise detached-owner RED' if expect_red else 'actual-free GREEN','behavior':result,'checks':checks,'objectdb_leak_summary':int(counts[1]) if counts else 0,'log':str(log),'log_sha256':sha(log)})
 (output/'RESULTS.json').write_text(json.dumps(receipt,indent=2)+'\n')
 print(case,'rc',code,'behavior',result,'checks',checks,flush=True)
 if not all(checks.values()):raise SystemExit(1)
receipt['host_unchanged']=sha(binary)==pin['sha256'] and sha(controlled)==pin['controlled_build_sha256']
receipt['source_unchanged']=sha(HERE/'probe/queue_free.gd')==receipt['script_sha256'] and sha(HERE/'probe/project.godot')==receipt['project_sha256']
(output/'RESULTS.json').write_text(json.dumps(receipt,indent=2)+'\n')
assert receipt['host_unchanged'] and receipt['source_unchanged']
print('receipt',output/'RESULTS.json',flush=True)
