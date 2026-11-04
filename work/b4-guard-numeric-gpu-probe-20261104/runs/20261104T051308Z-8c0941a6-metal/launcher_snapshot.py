#!/usr/bin/env python3
from pathlib import Path
import argparse,datetime,hashlib,json,re,shutil,subprocess,time,uuid
HERE=Path(__file__).resolve().parent
ROOT=HERE.parents[1]
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def main():
 p=argparse.ArgumentParser();p.add_argument('--godot',type=Path,required=True);p.add_argument('--sha256',required=True);p.add_argument('--build-identity',type=Path,required=True);p.add_argument('--backend',choices=['metal','vulkan'],default='metal');a=p.parse_args()
 assert a.godot.is_absolute() and a.godot.is_file() and sha(a.godot)==a.sha256
 assert a.build_identity.is_absolute() and a.build_identity.is_file()
 stamp=datetime.datetime.now(datetime.timezone.utc).strftime('%Y%m%dT%H%M%SZ')+'-'+uuid.uuid4().hex[:8]
 out=HERE/'runs'/(stamp+'-'+a.backend);out.mkdir(parents=True)
 fixture=out/'fixture';shutil.copytree(HERE/'template',fixture)
 shutil.copyfile(HERE/'FROZEN_ORACLES.json',out/'FROZEN_ORACLES.json');shutil.copyfile(a.build_identity,out/'BUILD_IDENTITY.json');shutil.copyfile(__file__,out/'launcher_snapshot.py')
 command=['python3',str(fixture/'run.py'),'--backend',a.backend,'--godot',str(a.godot),'--expected-sha256',a.sha256,'--source-root',str(ROOT)]
 started=time.monotonic()
 try:
  r=subprocess.run(command,capture_output=True,text=True,timeout=120);rc=r.returncode;log=r.stdout+r.stderr
 except subprocess.TimeoutExpired as e:
  rc=124;log=(e.stdout or b'').decode(errors='replace')+(e.stderr or b'').decode(errors='replace')
 (out/'launcher.log').write_text(log)
 manifest=fixture/(a.backend+'-results.json')
 result=json.loads(manifest.read_text()) if manifest.exists() else None
 report=result['runs'][-1]['report'] if result and result['runs'] else None
 deviations=[x for x in report['rows'] if not x['pass']] if report else []
 passed=bool(rc==0 and result and len(report['rows'])==9 and all(x['pass'] for x in report['rows']))
 summary={'command':command,'rc':rc,'duration_seconds':time.monotonic()-started,'host_sha256':a.sha256,'host_still_matches':sha(a.godot)==a.sha256,'build_identity_sha256':sha(a.build_identity),'pass':passed,'deviations':deviations,'report':report,'requested_shader_compile_policy':'default; no option changed','scope':'finite numeric mechanism probes, not full numeric profile/material/source-authority qualification'}
 (out/'RESULT.json').write_text(json.dumps(summary,indent=2)+'\n')
 files={str(f.relative_to(out)):{'bytes':f.stat().st_size,'sha256':sha(f)} for f in out.rglob('*') if f.is_file() and '.godot/' not in str(f)}
 (out/'FILES.json').write_text(json.dumps(files,indent=2)+'\n')
 print(json.dumps({'run_dir':str(out),'rc':rc,'pass':passed,'deviations':deviations}),flush=True)
 raise SystemExit(0 if passed else 1)
if __name__=='__main__':main()
