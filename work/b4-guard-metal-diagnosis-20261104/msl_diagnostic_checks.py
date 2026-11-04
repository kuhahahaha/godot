from pathlib import Path
import subprocess,json,re,hashlib
HERE=Path(__file__).resolve().parent
rows=[]
for name in ('INIT','VALIDATE','INIT-validate_only_records'):
 original=(HERE/f'{name}.metal').read_text()
 for kind in ('actual','manual_nonoverlap_control'):
  text=original
  if kind!='actual':
   ids=[int(x) for x in re.findall(r'\[\[id\((\d+)\)\]\]',text)]
   text=text.replace('spvBufferSizeConstants [[id(0)]]',f'spvBufferSizeConstants [[id({max(ids)+1})]]')
   text=text.replace('// Overlapping binding: ','')
   text=re.sub(r'device auto &(\w+) = \*reinterpret_cast<[^;]+;',lambda m:f'device auto &{m[1]} = *spvDescriptorSet0.{m[1]};',text)
  file=HERE/f'{name}-{kind}.metal';file.write_text(text)
  cmd=['xcrun','-sdk','macosx','metal','-std=metal4.0','-fsyntax-only',str(file)]
  p=subprocess.run(cmd,capture_output=True,text=True)
  expectation=p.returncode!=0 and 'casts away qualifiers' in p.stderr if kind=='actual' else p.returncode==0
  rows.append({'name':name,'kind':kind,'command':cmd,'returncode':p.returncode,'stdout':p.stdout,'stderr':p.stderr,'source_sha256':hashlib.sha256(text.encode()).hexdigest(),'expected':expectation})
  print(name,kind,p.returncode,expectation)
  if 'missing Metal Toolchain' in p.stderr:
   (HERE/'MSL_DIAGNOSTIC_CHECKS.json').write_text(json.dumps({'rows':rows,'status':'UNAVAILABLE','reason':'xcrun metal wrapper exists but Metal Toolchain component is absent; no syntax or manual control validated','gpu_dispatch':False},indent=2)+'\n')
   raise SystemExit(0)
  assert expectation,p.stderr
(HERE/'MSL_DIAGNOSTIC_CHECKS.json').write_text(json.dumps({'rows':rows,'manual_control_is_not_backend_fix':True,'runtime_lengths_unbound':True,'gpu_dispatch':False},indent=2)+'\n')
