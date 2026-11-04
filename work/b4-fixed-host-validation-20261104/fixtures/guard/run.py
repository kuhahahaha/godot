#!/usr/bin/env python3
"""Independent main-RD guard mechanism fixture; no production authority claim."""
from pathlib import Path
import argparse, hashlib, json, os, re, struct, subprocess, time
ROOT=Path(__file__).resolve().parents[2]
HERE=Path(__file__).resolve().parent
PROJECT=HERE/'project'
HOST=Path('/Users/macbookair/Documents/Codex/2026-10-20/godot-ange/work/system-scalar-native-correctness-20261104/host/editor.arm64')
SHADER=ROOT/'servers/rendering/renderer_rd/shaders/particles_external_guard.glsl'
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def words_bytes(words):return struct.pack('<'+'I'*len(words),*words)
def f(value):return struct.unpack('<I',struct.pack('<f',value))[0]
def vec(x,y,z,w=0):return [f(x),f(y),f(z),f(w)]
def build_cases():
 cases=[]
 def case(name,actual=1,capacity=2,surfaces=1,fault=0,history=False,prev_escape=False,current_escape=False,source_valid=1,source_count=None,history_permitted=1):
  records=[]
  for i in range(capacity):
   row=[0]*48
   for offset in [0,5,10,12,17,22,24,29,34]:row[offset]=f(1)
   row[44:48]=[i,23,19 if history else 17,0]
   records+=row
  previous=records.copy()
  if prev_escape:previous[3]=f(2)
  if current_escape:records[3]=f(2)
  proof=[]
  for value in [-1,1,-1,1,-1,1]:proof+=vec(value,value,value)
  for _ in range(2):proof+=vec(1,0,0)+vec(0,1,0)+vec(0,0,1)
  proof += [0,1,1,1,history_permitted,0,0,0]
  for surface in range(surfaces):
   proof+=vec(-.25,-.25,0)+vec(.25,.25,0)+vec(0,0,0)+vec(0,0,0)
   proof+=vec(-.25,-.25,0)+vec(.25,.25,0)+vec(0,0,0)+vec(0,0,0)
   proof += [1 if surface==0 else 0,6 if surface==0 else 4,0,0]
  buffers=[records,previous,[actual,0,0,0,capacity,1,55,0],[actual if source_count is None else source_count,source_valid],[0xdeadbeef]*16,[0xdeadbeef]*8,[0xdeadbeef]*(surfaces*5),proof]
  count=0 if fault else actual
  expected_commands=[]
  for surface in range(surfaces):expected_commands+=([6 if surface==0 else 4,count,0,0,0] if count else [0]*5)
  case={'name':name,'capacity':capacity,'surfaces':surfaces,'expected_status':[fault,count,0,0,55,0,1,0],'expected_commands':expected_commands}
  for index,words in enumerate(buffers):
   if not words:continue
   (PROJECT/'inputs'/f'{name}-{index}.bin').write_bytes(words_bytes(words))
  cases.append(case)
 case('healthy')
 case('count_zero',actual=0)
 case('capacity_zero',actual=0,capacity=0)
 case('source_invalid',source_valid=0,fault=1)
 case('source_count_mismatch',source_count=0,fault=2)
 case('count_exceeds_capacity',actual=3,fault=2)
 case('current_geometry_escape',current_escape=True,fault=64)
 case('history_reset_ignores_previous',prev_escape=True)
 case('history_valid_rejects_previous',history=True,prev_escape=True,fault=64)
 case('history_not_permitted',history=True,history_permitted=0,fault=32)
 case('multiple_surfaces',surfaces=2)
 case('multiple_chunks',actual=65,capacity=65,surfaces=2)
 (PROJECT/'cases.json').write_text(json.dumps(cases,indent=2)+'\n')
 return cases

def main():
 global HOST, SHADER
 parser=argparse.ArgumentParser();parser.add_argument('--backend',choices=['metal','vulkan'],required=True);parser.add_argument('--godot',type=Path,required=True);parser.add_argument('--expected-sha256',required=True);parser.add_argument('--source-root',type=Path,required=True);args=parser.parse_args()
 assert args.godot.is_absolute() and args.godot.is_file() and sha(args.godot)==args.expected_sha256
 HOST=args.godot;SHADER=args.source_root/'servers/rendering/renderer_rd/shaders/particles_external_guard.glsl'
 (PROJECT/'inputs').mkdir(exist_ok=True)
 cases=build_cases()
 source=SHADER.read_text();(HERE/'production_shader_snapshot.glsl').write_text(source)
 raw=source.replace('#[compute]\n','')
 artifacts={}
 for mode in ['INIT','VALIDATE','FINALIZE','INIT_EMPTY','FINALIZE_EMPTY','INIT_MUTANT']:
  defs='#define MODE_'+('INIT' if mode=='INIT_MUTANT' else mode.removesuffix('_EMPTY'))+'\n'
  if mode.endswith('_EMPTY'):defs+='#define MODE_EMPTY\n'
  text=raw.replace('#VERSION_DEFINES',defs)
  if mode=='INIT_MUTANT':
   before='uint raw_validity = source.words[proof.source_accessor.y];'
   assert text.count(before)==1
   text=text.replace(before,'uint raw_validity = 1u;')
  comp=HERE/(mode+'.comp');comp.write_text(text);spv=PROJECT/(mode+'.spv')
  compile_result=subprocess.run(['glslangValidator','-V',str(comp),'-o',str(spv)],capture_output=True,text=True)
  (HERE/(mode+'-compile.log')).write_text(compile_result.stdout+compile_result.stderr)
  assert compile_result.returncode==0
  subprocess.run(['spirv-val',str(spv)],check=True)
  artifacts[mode]={'glsl_sha256':sha(comp),'spirv_sha256':sha(spv)}
 results=[]
 for phase,mutant in [('baseline',False),('source-invalid-mutant',True),('restored-production',False)]:
  command=[str(HOST),'--path',str(PROJECT),'--rendering-driver',args.backend,'--rendering-method','mobile','--render-thread','safe','--resolution','64x64','--audio-driver','Dummy','--disable-crash-handler']
  if mutant:command+=['--','--mutant']
  start=time.monotonic()
  try:
   run=subprocess.run(command,capture_output=True,text=True,timeout=90)
   rc=run.returncode;log=run.stdout+run.stderr
  except subprocess.TimeoutExpired as exc:
   rc=124;log=(exc.stdout or b'').decode(errors='replace')+(exc.stderr or b'').decode(errors='replace')
  logfile=HERE/(args.backend+'-'+phase+'.log');logfile.write_text(log)
  match=re.search(r'^GUARD_RESULT (.+)$',log,re.M)
  report=json.loads(match[1]) if match else None
  errors=[line for line in log.splitlines() if 'ERROR:' in line or 'SCRIPT ERROR:' in line or 'GUARD_FATAL' in line]
  expected_red=bool(mutant and rc==1 and report and len(report['rows'])==1 and report['rows'][0]['name']=='source_invalid' and not report['rows'][0]['pass'] and report['rows'][0]['observed']['status']==[0,1,0,0,55,0,1,0] and report['rows'][0]['observed']['commands']==[6,1,0,0,0] and not errors)
  passed=expected_red if mutant else bool(rc==0 and report and report['pass'] and len(report['rows'])==len(cases) and not errors)
  row={'phase':phase,'command':command,'rc':rc,'duration_seconds':time.monotonic()-start,'log':logfile.name,'log_sha256':sha(logfile),'oracle_pass':passed,'targeted_expected_red':expected_red,'errors':errors,'report':report}
  results.append(row)
  manifest={'backend':args.backend,'host':str(HOST),'host_sha256':sha(HOST),'production_shader_sha256':sha(SHADER),'compiled_snapshot_sha256':sha(HERE/'production_shader_snapshot.glsl'),'source_matches_compiled_snapshot':SHADER.read_text()==source,'artifacts':artifacts,'cases':len(cases),'runs':results,'scope':'main-RD shader mechanism with directly constructed fixture buffers; not E09/source authority/wrapper/renderer/adoption/physical-disposal qualification'}
  (HERE/(args.backend+'-results.json')).write_text(json.dumps(manifest,indent=2)+'\n')
  print(json.dumps({'backend':args.backend,'phase':phase,'rc':rc,'oracle_pass':passed,'errors':errors}),flush=True)
  if not passed:raise SystemExit(1)
 print('All production and targeted mutation mechanism checks passed')
if __name__=='__main__':main()
