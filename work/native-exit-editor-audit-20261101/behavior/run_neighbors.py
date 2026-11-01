#!/usr/bin/env python3
"""Narrow repaired-host startup neighbors: check-only, default editor, custom MainLoop."""
import argparse,hashlib,json,re,subprocess,time
from pathlib import Path
HERE=Path(__file__).resolve().parent
def sha(p):return hashlib.sha256(Path(p).read_bytes()).hexdigest()
parser=argparse.ArgumentParser(description=__doc__)
parser.add_argument('--pin',type=Path,required=True)
parser.add_argument('--capability',action='store_true',help='Also invoke the copied original EditorExportPlugin capability input with explicit clean-exit acceptance')
args=parser.parse_args();pin=json.loads(args.pin.read_text());binary=Path(pin['binary']);controlled=Path(pin['controlled_build'])
assert sha(binary)==pin['sha256'] and sha(controlled)==pin['controlled_build_sha256']
project=HERE/'probe';output=HERE/'neighbor-results'/time.strftime('%Y%m%d-%H%M%S');output.mkdir(parents=True,exist_ok=False)
base=[str(binary),'--headless','--editor','--verbose','--path',str(project)]
cases=[('check_only',base+['--check-only','--script',str(project/'queue_free.gd')]),('editor_no_script',base+['--quit-after','8']),('custom_main_loop',base+['--script',str(project/'custom_main_loop.gd')])]
receipt={'scope':'ordinary startup selection and resource exit neighbors; no GPU/product acceptance','pin':pin,'source_sha256':{name:sha(project/name) for name in ['queue_free.gd','custom_main_loop.gd','project.godot']},'runs':[]}
for case,command in cases:
 log=output/(case+'.log');print('running',case,flush=True)
 with log.open('w') as stream:
  try:done=subprocess.run(command,stdout=stream,stderr=subprocess.STDOUT,timeout=30);code=done.returncode
  except subprocess.TimeoutExpired:code='timeout'
 text=log.read_text(errors='replace')
 checks={'not_timeout':code!='timeout','exit_zero':code==0,'no_exit_leaks':not re.search(r'were leaked|was leaked|Leaked instance:',text),'no_script_or_engine_errors':not re.search(r'SCRIPT ERROR|Parse Error|ERROR:|CrashHandler|SIGSEGV',text)}
 observation=None
 if case=='check_only':checks['script_not_executed']='QUEUE_FREE_ENROLLED ' not in text and 'QUEUE_FREE_RESULT ' not in text
 if case=='editor_no_script':checks['ordinary_editor_created']='first_scan_filesystem' in text
 if case=='custom_main_loop':
  matches=re.findall(r'CUSTOM_MAIN_LOOP_OBSERVATION (\{[^\n]+\})',text)
  observation=json.loads(matches[0]) if len(matches)==1 else None
  checks['only_actual_custom_loop']=observation=={'selected_self':True,'is_scene_tree':False,'debug_build':True,'orphan_windows':0,'orphan_editor_nodes':0}
  checks['no_editor_node_started']='first_scan_filesystem' not in text
 receipt['runs'].append({'case':case,'command':command,'returncode':code,'checks':checks,'observation':observation,'log':str(log),'log_sha256':sha(log)})
 (output/'RESULTS.json').write_text(json.dumps(receipt,indent=2)+'\n');print(case,'checks',checks,flush=True)
 if not all(checks.values()):raise SystemExit(1)
receipt['host_unchanged']=sha(binary)==pin['sha256'] and sha(controlled)==pin['controlled_build_sha256']
receipt['source_unchanged']=all(sha(project/name)==digest for name,digest in receipt['source_sha256'].items())
(output/'RESULTS.json').write_text(json.dumps(receipt,indent=2)+'\n')
assert receipt['host_unchanged'] and receipt['source_unchanged']
if args.capability:
 import sys
 command=[sys.executable,str(HERE/'run_capability.py'),'--pin',str(args.pin.resolve()),'--expect-clean']
 result=subprocess.run(command);receipt['capability_runner']={'command':command,'returncode':result.returncode}
 (output/'RESULTS.json').write_text(json.dumps(receipt,indent=2)+'\n')
 if result.returncode:raise SystemExit(result.returncode)
print('receipt',output/'RESULTS.json',flush=True)
