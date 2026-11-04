from pathlib import Path
import importlib.util,sys,re,subprocess,json,hashlib
sys.dont_write_bytecode=True
HERE=Path(__file__).resolve().parent;ROOT=HERE.parents[1]
spec=importlib.util.spec_from_file_location('expand',ROOT/'work/b4-instance192-shader-20261104/compile.py');m=importlib.util.module_from_spec(spec);spec.loader.exec_module(m)
original=m.read
m.read=lambda path:(HERE/'finding-initial-instance192_inc.glsl').read_text() if path.endswith('/instance192_inc.glsl') else original(path)
base='#define MAX_ROUGHNESS_LOD 7.0\n#define SDFGI_OCT_SIZE 6\n#define MAX_DIRECTIONAL_LIGHT_DATA_STRUCTS 8\n#define MAX_LIGHTMAP_TEXTURES 8\n#define MAX_LIGHTMAPS 8\n#define MATERIAL_UNIFORM_SET 3\n#define USE_PARTICLE_TRAILS\n'
rows=[]
for renderer in ('clustered','mobile'):
 path=f'servers/rendering/renderer_rd/shaders/forward_{renderer}/scene_forward_{renderer}.glsl'
 for profile in ('native','instance192_group'):
  for stage,chunk in zip(('vert','frag'),m.read(path).split('#[fragment]')):
   text=m.expand(chunk.replace('#[vertex]',''),path,set()).replace('#VERSION_DEFINES',base+('#define USE_INSTANCE192_OUTPUT\n' if profile!='native' else '')).replace('#MATERIAL_UNIFORMS','').replace('#GLOBALS','')
   text=re.sub(r'^#CODE.*$','',text,flags=re.M)
   file=HERE/f'trail-red-{renderer}-{profile}.{stage}';file.write_text(text)
   command=['glslangValidator','-V','--target-env','vulkan1.1',str(file),'-o',str(HERE/'trail-temp.spv')]
   p=subprocess.run(command,capture_output=True,text=True)
   expected=p.returncode==0 if profile=='native' else p.returncode!=0 and 'Instance192 does not admit native particle trails' in p.stdout+p.stderr
   rows.append({'renderer':renderer,'profile':profile,'stage':stage,'command':command,'returncode':p.returncode,'stdout':p.stdout,'stderr':p.stderr,'target_observed':expected,'sha256':hashlib.sha256(text.encode()).hexdigest()})
   assert expected,rows[-1]
(HERE/'trail-temp.spv').unlink(missing_ok=True)
(HERE/'TRAIL_GLOBAL_GROUP_RED.json').write_text(json.dumps({'rows':rows,'engine_runtime':False,'explanation':'Native materials are globally compiled in enabled Instance192 groups; this proves the exact template failure only. Lifecycle effect is source-proven.'},indent=2)+'\n')
print('Observed all 4 Native controls compile and all 4 external group stages reject the same legal trails material.')
