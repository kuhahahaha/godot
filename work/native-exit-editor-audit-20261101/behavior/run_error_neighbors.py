#!/usr/bin/env python3
"""Safe script-failure checks; do not execute the macOS modal non-MainLoop failure route."""
import argparse,hashlib,json,re,subprocess,time
from pathlib import Path
HERE=Path(__file__).resolve().parent
def sha(p):return hashlib.sha256(Path(p).read_bytes()).hexdigest()
parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('--pin',type=Path,required=True);args=parser.parse_args()
pin=json.loads(args.pin.read_text());binary=Path(pin['binary']);controlled=Path(pin['controlled_build'])
assert sha(binary)==pin['sha256'] and sha(controlled)==pin['controlled_build_sha256']
project=HERE/'failure-probe';output=HERE/'error-neighbor-results'/time.strftime('%Y%m%d-%H%M%S');output.mkdir(parents=True,exist_ok=False)
base=[str(binary),'--headless','--editor','--verbose','--path',str(project)]
cases=[('invalid_check_only',base+['--check-only','--script',str(project/'invalid_check_only.gd')],1),('missing_script',base+['--script',str(project/'does_not_exist.gd')],1),('nonmainloop_check_only',base+['--check-only','--script',str(project/'not_main_loop.gd')],0)]
receipt={'scope':'ordinary editor explicit-script validation/load failure; expected errors are retained and classified, not swallowed','pin':pin,'source_sha256':{n:sha(project/n) for n in ['invalid_check_only.gd','not_main_loop.gd','project.godot']},'runs':[],'not_run':'Runtime non-MainLoop cast failure invokes OS_MacOS::alert/NSAlert.runModal even headless; do not create a blocking alert in this bounded ordinary-child run.'}
for case,command,expected in cases:
 log=output/(case+'.log');print('running',case,flush=True)
 with log.open('w') as stream:
  try:done=subprocess.run(command,stdout=stream,stderr=subprocess.STDOUT,timeout=30);code=done.returncode
  except subprocess.TimeoutExpired:code='timeout'
 text=log.read_text(errors='replace')
 checks={'not_timeout':code!='timeout','exact_exit':code==expected,'no_exit_leaks':not re.search(r'were leaked|was leaked|Leaked instance:',text),'no_crash':not re.search(r'CrashHandler|SIGSEGV',text)}
 if case=='invalid_check_only':checks['exact_expected_error']='SCRIPT ERROR: Parse Error:' in text and 'invalid_check_only.gd' in text
 elif case=='missing_script':checks['exact_expected_error']="Can't load script:" in text and 'does_not_exist.gd' in text
 else:checks['no_instantiation_or_script_error']='NOT_MAIN_LOOP_INSTANTIATED' not in text and not re.search(r'SCRIPT ERROR|Parse Error|ERROR:',text)
 receipt['runs'].append({'case':case,'command':command,'returncode':code,'expected_returncode':expected,'checks':checks,'error_lines':[s for s in text.splitlines() if 'ERROR:' in s or 'Parse Error:' in s],'log':str(log),'log_sha256':sha(log)})
 (output/'RESULTS.json').write_text(json.dumps(receipt,indent=2)+'\n');print(case,'rc',code,'checks',checks,flush=True)
 if not all(checks.values()):raise SystemExit(1)
receipt['host_unchanged']=sha(binary)==pin['sha256'] and sha(controlled)==pin['controlled_build_sha256']
receipt['source_unchanged']=all(sha(project/n)==h for n,h in receipt['source_sha256'].items())
(output/'RESULTS.json').write_text(json.dumps(receipt,indent=2)+'\n');assert receipt['host_unchanged'] and receipt['source_unchanged']
print('receipt',output/'RESULTS.json',flush=True)
