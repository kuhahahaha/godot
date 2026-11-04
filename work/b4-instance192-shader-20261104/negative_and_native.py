#!/usr/bin/env python3
"""Actual GLSL negative compiles and Native preprocessed-token comparison."""
from pathlib import Path
import hashlib,importlib.util,json,re,subprocess,sys,tempfile
sys.dont_write_bytecode=True
HERE=Path(__file__).resolve().parent
spec=importlib.util.spec_from_file_location('instance_compile',HERE/'compile.py');module=importlib.util.module_from_spec(spec);spec.loader.exec_module(module)
ROOT=module.ROOT
DEFINES='''#define MAX_ROUGHNESS_LOD 7.0
#define SDFGI_OCT_SIZE 6
#define MAX_DIRECTIONAL_LIGHT_DATA_STRUCTS 8
#define MAX_LIGHTMAP_TEXTURES 8
#define MAX_LIGHTMAPS 8
#define MATERIAL_UNIFORM_SET 3
'''
original_read=module.read
baseline_inputs={}
BASELINE=subprocess.check_output(['git','rev-parse','HEAD'],cwd=ROOT,text=True).strip()
def baseline_read(path):
 if re.search(r'/forward_(clustered|mobile)/scene_forward_\1(?:_inc)?\.glsl$',path):
  data=subprocess.check_output(['git','show',BASELINE+':'+path],cwd=ROOT)
  baseline_inputs[path]=hashlib.sha256(data).hexdigest();return data.decode()
 return original_read(path)
def source(renderer,stage,flags,baseline=False):
 path='servers/rendering/renderer_rd/shaders/forward_'+renderer+'/scene_forward_'+renderer+'.glsl'
 module.read=baseline_read if baseline else original_read
 chunks=module.read(path).split('#[fragment]');text=chunks[0].replace('#[vertex]','') if stage=='vert' else chunks[1]
 text=module.expand(text,path,set());module.read=original_read
 text=text.replace('#VERSION_DEFINES',DEFINES+flags).replace('#MATERIAL_UNIFORMS','')
 direction='out' if stage=='vert' else 'in'
 text=text.replace('#GLOBALS',f'layout(location=15) {direction} vec4 author_varying15;')
 text=text.replace('#CODE : VERTEX','author_varying15=vec4(vertex,1.0);').replace('#CODE : FRAGMENT','if(author_varying15.w<0.0){discard;}')
 return re.sub(r'^#CODE.*$','',text,flags=re.M)
def invoke(text,stage,preprocess=False):
 with tempfile.TemporaryDirectory(prefix='instance192-negative-') as directory:
  shader=Path(directory)/('scene.'+stage);shader.write_text(text)
  command=['glslangValidator','-E','-S',stage,str(shader)] if preprocess else ['glslangValidator','-V','--target-env','vulkan1.1',str(shader),'-o',str(Path(directory)/'scene.spv')]
  result=subprocess.run(command,text=True,capture_output=True)
  return result
rows=[]
for renderer in ('clustered','mobile'):
 for stage in ('vert','frag'):
  for uber in ('','#define UBERSHADER\n'):
   for case,flags in (('color',''),('motion','#define MOTION_VECTORS\n' if renderer=='clustered' else '#define USE_MULTIVIEW\n#define MODE_RENDER_MOTION_VECTORS\n'),('trails','#define USE_PARTICLE_TRAILS\n')):
    old=invoke(source(renderer,stage,flags+uber,True),stage,True);new=invoke(source(renderer,stage,flags+uber),stage,True)
    assert old.returncode==new.returncode==0,(old.stderr,new.stderr)
    tokens=lambda text:re.findall(r'[A-Za-z_]\w*|\d+(?:\.\d*)?|[^\s]',text)
    assert tokens(old.stdout)==tokens(new.stdout),(renderer,stage,case,uber)
    rows.append({'kind':'native_token_equivalence','renderer':renderer,'stage':stage,'case':case,'uber':bool(uber),'pass':True})
    print(renderer,stage,case,uber.strip() or 'specialized','Native token-equivalent PASS')
 for name,flags,error in (
   ('double','#define USE_INSTANCE192_OUTPUT\n#define USE_DOUBLE_PRECISION\n','Instance192 raster requires'),
   ('profile_alias','#define USE_INSTANCE192_OUTPUT\n#define USE_INDEXED144_VERTEX_INPUT\n','distinct raster profiles')):
  result=invoke(source(renderer,'vert',flags),'vert')
  assert result.returncode!=0 and error in result.stdout+result.stderr,(renderer,name,result.stdout,result.stderr)
  rows.append({'kind':'expected_refusal','renderer':renderer,'case':name,'returncode':result.returncode,'diagnostic':result.stdout+result.stderr})
  print(renderer,name,'expected refusal PASS')
 text=source(renderer,'vert','#define USE_INSTANCE192_OUTPUT\n#define USE_PARTICLE_TRAILS\n')
 text=text.replace('// ShaderRD enables a group for every material version.', '#ifdef USE_PARTICLE_TRAILS\n#error Instance192 does not admit native particle trails\n#endif\n// ShaderRD enables a group for every material version.')
 poisoned=invoke(text,'vert')
 assert poisoned.returncode!=0 and 'native particle trails' in poisoned.stdout+poisoned.stderr
 rows.append({'kind':'global_group_trails_poison_red','renderer':renderer,'returncode':poisoned.returncode,'diagnostic':poisoned.stdout+poisoned.stderr})
 print(renderer,'old material-specific #error globalgroup target RED confirmed')
 text=source(renderer,'vert','#define USE_INSTANCE192_OUTPUT\n')
 # Recreate the actual rejected interface from the first proposal: an internal
 # varying at15 overlaps the compiler's first real author varying at15.
 text,count=re.subn(r'layout\(location = 10\)( out flat uvec[23])',r'layout(location = 15)\1',text,count=1)
 assert count==1
 result=invoke(text,'vert')
 assert result.returncode!=0 and 'overlapping use of location' in result.stdout+result.stderr,(result.stdout,result.stderr)
 rows.append({'kind':'varying_collision_red','renderer':renderer,'returncode':result.returncode,'diagnostic':result.stdout+result.stderr})
 print(renderer,'location15 collision expected RED confirmed')
(HERE/'negative-native-results.json').write_text(json.dumps({'rows':rows,'baseline':BASELINE,'baseline_shader_sha256':baseline_inputs,'inputs':module.used,'gpu':False,'engine_build':False},indent=2)+'\n')
print('checks:',len(rows))
