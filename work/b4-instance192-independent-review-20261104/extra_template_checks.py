#!/usr/bin/env python3
"""Independent combinations using actual templates, manually supplied snippets. No Engine compiler/GPU."""
from pathlib import Path
import hashlib, importlib.util, json, re, subprocess, sys
sys.dont_write_bytecode=True
HERE=Path(__file__).resolve().parent
ROOT=HERE.parents[1]
spec=importlib.util.spec_from_file_location('author_expand',ROOT/'work/b4-instance192-shader-20261104/compile.py')
m=importlib.util.module_from_spec(spec);spec.loader.exec_module(m)
base='''#define MAX_ROUGHNESS_LOD 7.0
#define SDFGI_OCT_SIZE 6
#define MAX_DIRECTIONAL_LIGHT_DATA_STRUCTS 8
#define MAX_LIGHTMAP_TEXTURES 8
#define MAX_LIGHTMAPS 8
#define MATERIAL_UNIFORM_SET 3
#define USE_INSTANCE192_OUTPUT
#define INV_MODEL_MATRIX_USED
#define MODEL_MATRIX_USED
#define COLOR_USED
#define CUSTOM0_USED
#define CUSTOM1_USED
#define CUSTOM2_USED
#define CUSTOM3_USED
#define UV_USED
#define UV2_USED
#define DO_SIDE_CHECK
#define TANGENT_USED
'''
rows=[]
for renderer in ('clustered','mobile'):
 path=f'servers/rendering/renderer_rd/shaders/forward_{renderer}/scene_forward_{renderer}.glsl'
 chunks=m.read(path).split('#[fragment]')
 cases={'tbn_color':'','normalmap':'#define NORMAL_MAP_USED\n#define BENT_NORMAL_MAP_USED\n','vertex_lighting':'#define USE_VERTEX_LIGHTING\n','alpha_motion_multiview':'#define ALPHA_HASH_USED\n#define USE_MULTIVIEW\n'+('#define MOTION_VECTORS\n#define MODE_SEPARATE_SPECULAR\n' if renderer=='clustered' else '#define MODE_RENDER_MOTION_VECTORS\n'),'shadow':'#define MODE_RENDER_DEPTH\n#define SHADOW_PASS\n','dual_shadow':'#define MODE_RENDER_DEPTH\n#define MODE_DUAL_PARABOLOID\n#define SHADOW_PASS\n'}
 for case,flags in cases.items():
  for uber in (False,True):
   for fp16 in ((False,True) if renderer=='mobile' else (False,)):
    label=f'{renderer}-{case}-uber{int(uber)}-fp16{int(fp16)}';files=[]
    for stage,chunk in zip(('vert','frag'),chunks):
     text=m.expand(chunk.replace('#[vertex]',''),path,set()).replace('#VERSION_DEFINES',base+flags+('#define UBERSHADER\n' if uber else '')+('#define EXPLICIT_FP16\n' if fp16 else '')).replace('#MATERIAL_UNIFORMS','')
     direction='out' if stage=='vert' else 'in'
     text=text.replace('#GLOBALS',f'layout(location=15) {direction} vec4 witness;')
     color='color_interp' if renderer=='clustered' else 'color_highp'
     vertex=f'witness={color}+instance_custom+custom0_attrib+custom1_attrib+custom2_attrib+custom3_attrib; vertex += (read_inv_model_matrix*read_model_matrix*vec4(vertex,1.0)).xyz*0.001; normal_highp += model_normal_matrix[0]*0.001;'
     frag='if(!MATERIAL_FRONT_FACING && witness.x<0.0){discard;} if((read_inv_model_matrix*read_model_matrix)[0][0]<0.0){discard;} normal_highp+=model_normal_matrix[0]*0.001;'
     text=text.replace('#CODE : VERTEX',vertex).replace('#CODE : FRAGMENT',frag)
     text=re.sub(r'^#CODE.*$','',text,flags=re.M)
     shader=HERE/f'{label}.{stage}';shader.write_text(text);files.append(str(shader))
     cmd=['glslangValidator','-V','--target-env','vulkan1.1',str(shader),'-o',str(HERE/'temp.spv')]
     p=subprocess.run(cmd,capture_output=True,text=True)
     rows.append({'label':label,'stage':stage,'command':cmd,'returncode':p.returncode,'stdout':p.stdout,'stderr':p.stderr,'sha256':hashlib.sha256(text.encode()).hexdigest()})
     if p.returncode: print(label,stage,p.stdout,p.stderr); (HERE/'EXTRA_RESULTS.json').write_text(json.dumps({'rows':rows,'inputs':m.used},indent=2));sys.exit(1)
     v=subprocess.run(['spirv-val','--target-env','vulkan1.1',str(HERE/'temp.spv')],capture_output=True,text=True)
     rows[-1]['spirv_validation_returncode']=v.returncode;assert v.returncode==0,v.stderr
    cmd=['glslangValidator','-V','--target-env','vulkan1.1','-l',*files,'-o',str(HERE/'temp.spv')]
    p=subprocess.run(cmd,capture_output=True,text=True)
    rows.append({'label':label,'stage':'link','command':cmd,'returncode':p.returncode,'stdout':p.stdout,'stderr':p.stderr});assert p.returncode==0,p.stdout
    print(label,'PASS',flush=True)
(HERE/'temp.spv').unlink()
(HERE/'EXTRA_RESULTS.json').write_text(json.dumps({'rows':rows,'inputs':m.used,'gpu':False,'engine_shader_compiler':False},indent=2)+'\n')
print('PASS stages',sum(x['stage']!='link' for x in rows),'links',sum(x['stage']=='link' for x in rows))
