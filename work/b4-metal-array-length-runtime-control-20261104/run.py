#!/usr/bin/env python3
"""Actual main-RD length/address tests. Every invocation has an immutable run folder."""
from pathlib import Path
import argparse, datetime, hashlib, json, re, shutil, struct, subprocess, time, uuid
HERE=Path(__file__).resolve().parent
OLD_HOST=Path('/Users/macbookair/Documents/Codex/2026-10-20/godot-ange/work/system-scalar-native-correctness-20261104/host/editor.arm64')
OLD_SHA='4e99d8df4482f596ee892944a38d9bff28bf8f622c8a4b2cd4e7081a33e6c528'
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def floats(xs):return [struct.unpack('<I',struct.pack('<f',x))[0] for x in xs]
def make_inputs(project,aligned):
 rows=[]
 # Oracles are explicit finite examples, independent of shader source/introspection.
 specs=[
  ('baseline_reuse_A_to_B','A','B', [11,12,13,14,15],[101,102,103,104,21,22,23,24,25,26], list(range(1,13)),
   [[11,12,13,14,114],[101,102,103,104,21,22,23,24,25,122],floats([1,2,3,4,5,6,7,8,101,202,303,404]),[5,6,3,4]],[20,40,48,16]),
  ('recreated_sizes_B_to_B','B','B',[31,32,33,34,35,36,37],[201,202,203,204,41,42],[5,6,7,8],
   [[31,32,33,34,35,36,234],[201,202,203,204,41,242],floats([105,206,307,408]),[7,2,1,4]],[28,24,16,16]),
  ('reverse_reuse_B_to_A','B','A',[11,12,13,14,15],[101,102,103,104,21,22,23,24,25,26], list(range(1,13)),
   [[11,12,13,14,114],[101,102,103,104,21,22,23,24,25,122],floats([1,2,3,4,5,6,7,8,101,202,303,404]),[5,6,3,4]],[20,40,48,16])]
 if aligned:
  specs=[
   ('baseline_reuse_A_to_B','A','B',list(range(11,19)),[101,102,103,104,21,22,23,24],list(range(1,13)),
    [list(range(11,18))+[114],[101,102,103,104,21,22,23,122],floats([1,2,3,4,5,6,7,8,101,202,303,404]),[8,4,3,4]],[32,32,48,16]),
   ('recreated_sizes_B_to_B','B','B',list(range(31,43)),[201,202,203,204]+list(range(41,49)),list(range(5,13)),
    [list(range(31,42))+[234],[201,202,203,204]+list(range(41,48))+[242],floats([5,6,7,8,105,206,307,408]),[12,8,2,4]],[48,48,32,16]),
   ('reverse_reuse_B_to_A','B','A',list(range(11,19)),[101,102,103,104,21,22,23,24],list(range(1,13)),
    [list(range(11,18))+[114],[101,102,103,104,21,22,23,122],floats([1,2,3,4,5,6,7,8,101,202,303,404]),[8,4,3,4]],[32,32,48,16])]
  a_source=project/'control-no-length.comp'
  a_source.write_text(a_source.read_text().replace('uint a=5u;','uint a=8u;').replace('uint b=6u;','uint b=4u;'))
 for name,set_shader,pipeline,a,b,c,expected,sizes in specs:
  arrays=[a,b,floats(c),[0xdeadbeef]*4]
  for i,words in enumerate(arrays):
   raw=struct.pack('<'+'I'*len(words),*words);assert len(raw)==sizes[i]
   (project/f'{name}-{i}.bin').write_bytes(raw)
  rows.append({'name':name,'input_name':name,'set_shader':set_shader,'pipeline':pipeline,'byte_sizes':sizes,'expected_words':expected})
 (project/'cases.json').write_text(json.dumps(rows,indent=2)+'\n')
 return rows

def main():
 p=argparse.ArgumentParser();p.add_argument('--backend',choices=['metal','vulkan'],required=True);p.add_argument('--godot',type=Path,default=OLD_HOST);p.add_argument('--control-only',action='store_true');p.add_argument('--aligned',action='store_true');p.add_argument('--expect-metal-aux-red',action='store_true');args=p.parse_args()
 assert args.godot.is_absolute() and args.godot.is_file()
 if args.godot==OLD_HOST:assert sha(args.godot)==OLD_SHA,'frozen host hash mismatch'
 if args.expect_metal_aux_red:assert args.backend=='metal' and not args.control_only
 stamp=datetime.datetime.now(datetime.timezone.utc).strftime('%Y%m%dT%H%M%SZ')+'-'+uuid.uuid4().hex[:8]
 run_dir=HERE/'runs'/(stamp+'-'+args.backend+('-control' if args.control_only else ''));run_dir.mkdir(parents=True)
 project=run_dir/'project';shutil.copytree(HERE/'template',project)
 shutil.copyfile(Path(__file__),run_dir/'launcher_snapshot.py')
 cases=make_inputs(project,args.aligned)
 for mode,filename in [('A','control-no-length.comp'),('B','control.comp')]:
  cmd=['glslangValidator','-V',str(project/filename),'-o',str(project/f'control-{mode}.spv')]
  result=subprocess.run(cmd,capture_output=True,text=True);(run_dir/f'compile-{mode}.log').write_text(result.stdout+result.stderr)
  assert result.returncode==0,result.stderr
  subprocess.run(['spirv-val',str(project/f'control-{mode}.spv')],check=True)
  assembly=subprocess.run(['spirv-dis',str(project/f'control-{mode}.spv')],check=True,capture_output=True,text=True).stdout
  (run_dir/f'control-{mode}.spvasm').write_text(assembly)
  assert ('OpArrayLength' in assembly)==(mode=='B')
 command=[str(args.godot),'--path',str(project),'--rendering-driver',args.backend,'--rendering-method','mobile','--render-thread','safe','--resolution','64x64','--audio-driver','Dummy','--disable-crash-handler']
 if args.control_only:command+=['--','--control-only']
 start=time.monotonic()
 try:
  result=subprocess.run(command,capture_output=True,text=True,timeout=90);rc=result.returncode;log=result.stdout+result.stderr
 except subprocess.TimeoutExpired as error:
  rc=124;log=(error.stdout or b'').decode(errors='replace')+(error.stderr or b'').decode(errors='replace')
 (run_dir/'host.log').write_text(log)
 match=re.search(r'^LENGTH_RESULT (.+)$',log,re.M);report=json.loads(match[1]) if match else None
 fatal=[line for line in log.splitlines() if 'ERROR:' in line or 'LENGTH_FATAL' in line]
 red=bool(args.expect_metal_aux_red and rc==2 and 'LENGTH_PIPELINE_READY A' in log and 'LENGTH_FATAL pipeline creation B' in log and 'spvBufferSizeConstants' in log and ('casts away qualifiers' in log or 'duplicate member' in log or 'redefinition' in log) and report and not report['pass'] and not report['rows'])
 green=bool(not args.expect_metal_aux_red and rc==0 and report and report['pass'] and len(report['rows'])==(1 if args.control_only else len(cases)) and not fatal)
 summary={'backend':args.backend,'control_only':args.control_only,'aligned_sizes':args.aligned,'command':command,'host_sha256':sha(args.godot),'frozen_old_baseline':sha(args.godot)==OLD_SHA,'rc':rc,'duration_seconds':time.monotonic()-start,'expected_aux_collision_red':red,'literal_oracle_green':green,'accepted':red or green,'report':report,'error_lines':fatal,'scope':'Main-RD finite array length/address mechanism; free requests only; no production authority or physical-disposal qualification','dynamic_api_control':'deferred: public uniform API exposes no dynamic buffer offset binding'}
 (run_dir/'RESULT.json').write_text(json.dumps(summary,indent=2)+'\n')
 files={str(f.relative_to(run_dir)):{'bytes':f.stat().st_size,'sha256':sha(f)} for f in run_dir.rglob('*') if f.is_file() and '.godot/' not in str(f)}
 (run_dir/'FILES.json').write_text(json.dumps(files,indent=2)+'\n')
 print(json.dumps({'run_dir':str(run_dir),'rc':rc,'accepted':summary['accepted'],'green':green,'targeted_red':red}),flush=True)
 raise SystemExit(0 if summary['accepted'] else 1)
if __name__=='__main__':main()
