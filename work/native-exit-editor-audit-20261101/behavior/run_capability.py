#!/usr/bin/env python3
"""Copied capability input: capability success and resource-clean exit are separate checks."""
import argparse,hashlib,json,re,shutil,subprocess,time
from pathlib import Path
HERE=Path(__file__).resolve().parent
AUDIT=HERE.parent
def sha(p):return hashlib.sha256(Path(p).read_bytes()).hexdigest()
parser=argparse.ArgumentParser(description=__doc__)
parser.add_argument('--pin',type=Path,required=True)
parser.add_argument('--expect-clean',action='store_true')
args=parser.parse_args();pin=json.loads(args.pin.read_text());binary=Path(pin['binary']);controlled=Path(pin['controlled_build'])
assert sha(binary)==pin['sha256'] and sha(controlled)==pin['controlled_build_sha256']
output=HERE/'capability-results'/time.strftime('%Y%m%d-%H%M%S');output.mkdir(parents=True,exist_ok=False)
project=output/'probe';shutil.copytree(AUDIT/'baseline',project)
assert sha(project/'check.gd')==sha(AUDIT/'baseline/check.gd')
command=[str(binary),'--headless','--editor','--verbose','--path',str(project),'--script',str(project/'check.gd')]
log=output/'capability.log'
with log.open('w') as stream:
 try:result=subprocess.run(command,stdout=stream,stderr=subprocess.STDOUT,timeout=30);code=result.returncode
 except subprocess.TimeoutExpired:code='timeout'
text=log.read_text(errors='replace');matches=re.findall(r'ANGE_EDITOR_CAPABILITY (\{[^\n]+\})',text)
capability=json.loads(matches[0]) if len(matches)==1 else None
canvas={k:int(n) for n,k in re.findall(r'(\d+) RIDs of type "([^"]+)" were leaked',text)}
objects=re.search(r'(\d+) ObjectDB instances were leaked at exit',text)
rids={k:int(n) for n,k in re.findall(r"(\d+) RID allocations of type '([^']+)' were leaked at exit",text)}
checks={'not_timeout':code!='timeout','capability_exit_zero':code==0,'all_capability_fields_true':capability is not None and all(capability.get(k) is True for k in ['editor_hint','EditorExportPlugin','GDScript','plugin_instance']),'host_unchanged':sha(binary)==pin['sha256'] and sha(controlled)==pin['controlled_build_sha256'],'source_unchanged':sha(project/'check.gd')==sha(AUDIT/'baseline/check.gd')}
clean=not canvas and not rids and not objects and not re.search(r'ERROR:|were leaked|was leaked|CrashHandler|SIGSEGV',text)
receipt={'scope':'ordinary headless editor capability and resource exit; no GPU or product acceptance','pin':pin,'command':command,'returncode':code,'capability':capability,'script_sha256':sha(project/'check.gd'),'checks':checks,'resource_clean':clean,'canvas_leaks':canvas,'objectdb_leaks':int(objects[1]) if objects else 0,'other_rid_leaks':rids,'log':str(log),'log_sha256':sha(log)}
(output/'RESULTS.json').write_text(json.dumps(receipt,indent=2)+'\n')
print(json.dumps({'checks':checks,'resource_clean':clean,'canvas':canvas,'ObjectDB':receipt['objectdb_leaks'],'receipt':str(output/'RESULTS.json')},indent=2),flush=True)
if not all(checks.values()) or (args.expect_clean and not clean):raise SystemExit(1)
