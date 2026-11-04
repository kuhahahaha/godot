from pathlib import Path
import subprocess,re,json,hashlib
ROOT=Path.cwd();HERE=ROOT/'work/b4-guard-metal-diagnosis-20261104';GPU=ROOT/'work/b4-guard-main-rd-gpu-20261104'
rows=[]
for mode in ('INIT','VALIDATE','FINALIZE','INIT_EMPTY','FINALIZE_EMPTY'):
 for change in ('original','validate_only_records'):
  if change=='original':spv=GPU/'project'/f'{mode}.spv';command=None
  else:
   text=(GPU/f'{mode}.comp').read_text().replace('#ifndef MODE_EMPTY','#if defined(MODE_VALIDATE) && !defined(MODE_EMPTY)')
   file=HERE/f'{mode}-{change}.comp';file.write_text(text);spv=file.with_suffix('.spv')
   command=['glslangValidator','-V',str(file),'-o',str(spv)]
   p=subprocess.run(command,text=True,capture_output=True);assert p.returncode==0,(p.stdout,p.stderr)
  validation=subprocess.run(['spirv-val',str(spv)],capture_output=True,text=True);assert validation.returncode==0,validation.stderr
  assembly=subprocess.check_output(['spirv-dis',str(spv)],text=True);(HERE/f'{mode}-{change}.spvasm').write_text(assembly)
  names=dict(re.findall(r'OpName\s+(%\S+)\s+"([^"]+)"',assembly))
  binding=dict(re.findall(r'OpDecorate\s+(%\S+)\s+Binding\s+(\d+)',assembly))
  desc=[{'id':i,'name':names.get(i,i),'binding':int(b)} for i,b in binding.items()];desc.sort(key=lambda x:x['binding'])
  lengths=re.findall(r'OpArrayLength\s+%\S+\s+(%\S+)\s+\d+',assembly)
  # Exact driver mapping: sorted reflected resources get sequential ARG IDs.
  mapping=[dict(x,metal_arg_id=n) for n,x in enumerate(desc)]
  rows.append({'mode':mode,'change':change,'command':command,'spirv_sha256':hashlib.sha256(spv.read_bytes()).hexdigest(),'spirv_val_returncode':validation.returncode,'descriptors':mapping,'OpArrayLength_buffers':[names.get(i,i) for i in lengths],'aux_collision_prediction':mapping[0] if lengths and mapping else None})
(HERE/'SPIRV_INSPECTION.json').write_text(json.dumps({'rows':rows,'basis':'spirv-dis exact bytecode; Metal ARG and collision are source-derived, not a new MSL conversion or GPU run'},indent=2)+'\n')
for r in rows:print(r['mode'],r['change'],'bindings',[x['binding'] for x in r['descriptors']],'arraylength',r['OpArrayLength_buffers'],'predicted collision',r['aux_collision_prediction']['name'])
