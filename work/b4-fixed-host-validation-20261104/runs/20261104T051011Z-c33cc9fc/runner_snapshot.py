#!/usr/bin/env python3
"""Serial validation of one explicitly supplied, hash-locked completed host.
No default executable, engine build, temporary bin invocation, or metadata-schema guess.
"""
from pathlib import Path
import argparse,datetime,hashlib,json,re,shutil,subprocess,time,uuid,xml.etree.ElementTree as ET
HERE=Path(__file__).resolve().parent
SOURCE=HERE.parents[1]
EXPECTED=json.loads((HERE/'EXPECTED_TESTS.json').read_text())
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def exact_filter(names):
 assert all('*' not in x and '?' not in x for x in names)
 return ','.join(x.replace('\\','\\\\').replace(',','\\,') for x in names)
def invoke(command,folder,timeout=120):
 start=time.monotonic()
 try:
  run=subprocess.run(command,capture_output=True,text=True,timeout=timeout)
  rc=run.returncode;stdout=run.stdout;stderr=run.stderr
 except subprocess.TimeoutExpired as e:
  rc=124;stdout=(e.stdout or b'').decode(errors='replace');stderr=(e.stderr or b'').decode(errors='replace')
 (folder/'stdout.log').write_text(stdout);(folder/'stderr.log').write_text(stderr)
 row={'command':command,'rc':rc,'duration_seconds':time.monotonic()-start,'stdout_sha256':sha(folder/'stdout.log'),'stderr_sha256':sha(folder/'stderr.log')}
 (folder/'PROCESS.json').write_text(json.dumps(row,indent=2)+'\n')
 return row,stdout+stderr

def parse_doctest(path,names,discovery):
 root=ET.parse(path).getroot();assert root.tag=='doctest'
 cases=root.findall('.//TestCase');observed=[x.attrib['name'] for x in cases]
 assert sorted(observed)==sorted(names) and len(observed)==len(set(observed)),(observed,names)
 assert not any(c.attrib.get('skipped')=='true' for c in cases)
 summary=root.find('OverallResultsTestCases');assert summary is not None
 if discovery:
  assert int(summary.attrib['unskipped'])==len(names)
  return {'exact_discovered_names':observed,'count':len(observed)}
 assert int(summary.attrib['successes'])==len(names) and int(summary.attrib['failures'])==0
 assertions=root.find('OverallResultsAsserts');assert assertions is not None
 assert int(assertions.attrib['failures'])==0 and int(assertions.attrib['successes'])>0
 for case in cases:
  stats=case.find('OverallResultsAsserts');assert stats is not None
  assert stats.attrib.get('test_case_success')=='true' and int(stats.attrib['failures'])==0
  assert not case.findall('.//Exception')
 return {'exact_executed_names':observed,'count':len(observed),'assertions':int(assertions.attrib['successes'])}

def run_engine(host,folder):
 folder.mkdir();results=[]
 all_names=[name for group in EXPECTED['groups'] for name in group['cases']]
 jobs=[('discovery',all_names,True)]+[(g['name'],g['cases'],False) for g in EXPECTED['groups']]
 for label,names,discovery in jobs:
  current=folder/label;current.mkdir();xml=current/'doctest.xml'
  command=[str(host),'--test','--reporters=xml','--out='+str(xml),'--no-colors=true','--no-skipped-summary=true','--case-sensitive=true','--test-case='+exact_filter(names)]
  if discovery:command+=['--list-test-cases']
  process,log=invoke(command,current)
  row={'name':label,**process,'expected_count':len(names),'discovery_only':discovery}
  try:
   assert process['rc']==0
   row['parsed']=parse_doctest(xml,names,discovery)
   # New tests suppress their expected-error printing; retain any unexpected diagnostics.
   errors=[line for line in log.splitlines() if 'ERROR:' in line or 'SCRIPT ERROR:' in line]
   row['error_lines']=errors;assert not errors,errors
   row['pass']=True
  except Exception as error:
   row['pass']=False;row['validation_error']=str(error)
  results.append(row);(folder/'RESULTS.json').write_text(json.dumps(results,indent=2)+'\n')
  print(json.dumps({'engine_job':label,'rc':process['rc'],'pass':row['pass']}),flush=True)
  if not row['pass']:raise RuntimeError('Engine discovery/exact-case validation failed: '+label)
 return results

def main():
 parser=argparse.ArgumentParser();parser.add_argument('--godot',type=Path,required=True);parser.add_argument('--sha256',required=True);parser.add_argument('--build-identity',type=Path,required=True);parser.add_argument('--source-revision',required=True);parser.add_argument('--stage',choices=['engine','runtime','all'],default='engine');args=parser.parse_args()
 host=args.godot
 assert host.is_absolute() and host.is_file() and re.fullmatch(r'[0-9a-f]{64}',args.sha256)
 assert sha(host)==args.sha256,'host hash mismatch'
 assert re.fullmatch(r'[0-9a-f]{40}',args.source_revision),'root-supplied canonical source revision required'
 assert not host.resolve().is_relative_to((SOURCE/'bin').resolve()),'refuse temporary build-tree bin output; supply the fixed host copy'
 assert args.build_identity.is_absolute() and args.build_identity.is_file()
 identity=json.loads(args.build_identity.read_text()) # Preserve actual schema; no guessed identity fields.
 for group in EXPECTED['groups']:
  assert sha(SOURCE/group['source_file'])==group['source_sha256'],'test source drift'
 stamp=datetime.datetime.now(datetime.timezone.utc).strftime('%Y%m%dT%H%M%SZ')+'-'+uuid.uuid4().hex[:8]
 run_dir=HERE/'runs'/stamp;run_dir.mkdir(parents=True)
 shutil.copyfile(args.build_identity,run_dir/'BUILD_IDENTITY.json');shutil.copyfile(HERE/'EXPECTED_TESTS.json',run_dir/'EXPECTED_TESTS.json');shutil.copyfile(__file__,run_dir/'runner_snapshot.py')
 rows=[];result={'pass':False,'stage':args.stage,'host':str(host),'host_sha256':args.sha256,'canonical_source_revision':args.source_revision,'test_names_baseline_revision':EXPECTED.get('names_baseline_revision',EXPECTED['source_revision']),'build_identity_original':str(args.build_identity),'build_identity_sha256':sha(args.build_identity),'build_identity_schema':'preserved verbatim, identity acceptance supplied by root','rows':rows}
 try:
  if args.stage in ['engine','all']:result['engine']=run_engine(host,run_dir/'engine')
  if args.stage in ['runtime','all']:
   array=run_dir/'array-length';shutil.copytree(HERE/'fixtures/array-length',array,ignore=shutil.ignore_patterns('__pycache__','*.pyc'))
   for label,options in [('metal-logical-unaligned',[]),('metal-aligned',['--aligned'])]:
    process_dir=run_dir/label;process_dir.mkdir()
    command=['python3',str(array/'run.py'),'--backend','metal','--godot',str(host)]+options
    process,_=invoke(command,process_dir,120)
    row={'name':label,**process,'pass':process['rc']==0};rows.append(row)
    assert row['pass'],label+' failed'
    assert sha(host)==args.sha256,'host changed during validation'
   guard=run_dir/'guard';shutil.copytree(HERE/'fixtures/guard',guard,ignore=shutil.ignore_patterns('__pycache__','*.pyc'))
   process_dir=run_dir/'metal-guard';process_dir.mkdir()
   command=['python3',str(guard/'run.py'),'--backend','metal','--godot',str(host),'--expected-sha256',args.sha256,'--source-root',str(SOURCE)]
   process,_=invoke(command,process_dir,300)
   row={'name':'metal-guard-production-mutant-restored',**process,'pass':process['rc']==0};rows.append(row);assert row['pass'],'guard GPU mechanism failed'
  assert sha(host)==args.sha256,'host changed during validation'
  result['pass']=True
 except Exception as error:
  result['validation_error']=str(error)
 finally:
  (run_dir/'RESULT.json').write_text(json.dumps(result,indent=2)+'\n')
  files={str(p.relative_to(run_dir)):{'bytes':p.stat().st_size,'sha256':sha(p)} for p in run_dir.rglob('*') if p.is_file() and '.godot/' not in str(p)}
  (run_dir/'FILES.json').write_text(json.dumps(files,indent=2)+'\n')
 print(json.dumps({'run_dir':str(run_dir),'pass':result['pass']}),flush=True)
 raise SystemExit(0 if result['pass'] else 1)
if __name__=='__main__':main()
