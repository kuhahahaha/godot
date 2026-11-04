#!/usr/bin/env python3
"""Compile real complete renderer templates; no engine/GPU execution."""
from pathlib import Path
import argparse, hashlib, json, posixpath, re, subprocess, tempfile
ROOT=Path(__file__).resolve().parents[2]
FALLBACK=Path('/Users/macbookair/Documents/Codex/2026-10-20/godot-ange/worktrees/vulkan-qualification-profile-20261104')
used={}
def read(path):
 p=ROOT/path
 if not p.is_file():p=FALLBACK/path
 data=p.read_bytes();used[path]={'path':str(p),'sha256':hashlib.sha256(data).hexdigest()}
 return data.decode()
def expand(text,parent,seen):
 def include(m):
  path=posixpath.normpath(posixpath.join(posixpath.dirname(parent),m[1]))
  if path in seen:return ''
  seen.add(path);return expand(read(path),path,seen)
 return re.sub(r'^\s*#include "([^"]+)".*$',include,text,flags=re.M)
def main():
 parser=argparse.ArgumentParser();parser.add_argument('--renderer',choices=['clustered','mobile']);parser.add_argument('--quick',action='store_true');args=parser.parse_args()
 defines='''#define MAX_ROUGHNESS_LOD 7.0
#define SDFGI_OCT_SIZE 6
#define MAX_DIRECTIONAL_LIGHT_DATA_STRUCTS 8
#define MAX_LIGHTMAP_TEXTURES 8
#define MAX_LIGHTMAPS 8
#define MATERIAL_UNIFORM_SET 3
'''
 rows=[]
 cases=[('color',''),('inverse','#define INV_MODEL_MATRIX_USED\n#define MODEL_MATRIX_USED\n'),('depth','#define MODE_RENDER_DEPTH\n'),('dual','#define MODE_RENDER_DEPTH\n#define MODE_DUAL_PARABOLOID\n'),('material','#define MODE_RENDER_DEPTH\n#define MODE_RENDER_MATERIAL\n'),('alpha_hash','#define ALPHA_HASH_USED\n'),('custom','#define COLOR_USED\n#define CUSTOM0_USED\n#define CUSTOM1_USED\n#define CUSTOM2_USED\n#define CUSTOM3_USED\n')]
 for r in ([args.renderer] if args.renderer else ['clustered','mobile']):
  cases_r=cases+[('motion', '#define MOTION_VECTORS\n' if r=='clustered' else '#define USE_MULTIVIEW\n#define MODE_RENDER_MOTION_VECTORS\n')]
  if r=='clustered':
   cases_r += [('normal_roughness','#define MODE_RENDER_DEPTH\n#define MODE_RENDER_NORMAL_ROUGHNESS\n'),('voxel','#define MODE_RENDER_DEPTH\n#define MODE_RENDER_NORMAL_ROUGHNESS\n#define MODE_RENDER_VOXEL_GI\n'),('sdf','#define MODE_RENDER_DEPTH\n#define MODE_RENDER_SDF\n')]
  cases_r += [('native_trails','#define USE_PARTICLE_TRAILS\n'),('native_trails_depth','#define USE_PARTICLE_TRAILS\n#define MODE_RENDER_DEPTH\n'),('native_trails_motion','#define USE_PARTICLE_TRAILS\n'+('#define MOTION_VECTORS\n' if r=='clustered' else '#define USE_MULTIVIEW\n#define MODE_RENDER_MOTION_VECTORS\n'))]
  cases_r += [('lightmap','#define USE_LIGHTMAP\n'),('multiview_color','#define USE_MULTIVIEW\n'),('multiview_shadow','#define USE_MULTIVIEW\n#define MODE_RENDER_DEPTH\n')]
  if r=='mobile':cases_r += [('fog_mask','#define DEFER_OPAQUE_FOG\n'),('multiview_fog_lightmap','#define DEFER_OPAQUE_FOG\n#define USE_MULTIVIEW\n#define USE_LIGHTMAP\n'),('fp16_color','#define EXPLICIT_FP16\n')]
  if args.quick:cases_r=cases_r[:1]
  path='servers/rendering/renderer_rd/shaders/forward_'+r+'/scene_forward_'+r+'.glsl'
  stages=read(path).split('#[fragment]');vertex=stages[0].replace('#[vertex]','');fragment=stages[1]
  for name,case in cases_r:
   for profile in ['native','instance192']:
    for uber in ['','UBERSHADER']:
     sources=[]
     for stage,text in [('vert',vertex),('frag',fragment)]:
      text=expand(text,path,set())
      flags=defines+case+('#define USE_INSTANCE192_OUTPUT\n' if profile=='instance192' else '')+('#define UBERSHADER\n' if uber else '')
      text=text.replace('#VERSION_DEFINES',flags).replace('#MATERIAL_UNIFORMS','')
      direction='out' if stage=='vert' else 'in'
      # The real ShaderCompiler starts author varyings at15. Both are used.
      text=text.replace('#GLOBALS',f'layout(location=15) {direction} vec4 author_varying15;\nlayout(location=16) {direction} vec4 author_varying16;')
      vertex_code='author_varying15 = vec4(vertex, 1.0); author_varying16 = vec4(vertex, 2.0);'
      if name=='inverse':vertex_code += 'vertex=(read_inv_model_matrix*read_model_matrix*vec4(vertex,1.0)).xyz;'
      if name=='custom':vertex_code += 'vertex += custom0_attrib.xyz + custom1_attrib.xyz + custom2_attrib.xyz + custom3_attrib.xyz + instance_custom.xyz;'
      fragment_code='if (author_varying15.w + author_varying16.w < 0.0) { discard; }'
      fragment_code += 'if(!MATERIAL_FRONT_FACING && author_varying16.w<0.0) { discard; }'
      if name=='inverse':fragment_code += 'if ((read_inv_model_matrix*read_model_matrix)[0][0]<0.0) { discard; }'
      text=text.replace('#CODE : VERTEX',vertex_code).replace('#CODE : FRAGMENT',fragment_code)
      text=re.sub(r'^#CODE.*$','',text,flags=re.M)
      sources.append((stage,text))
      with tempfile.TemporaryDirectory(prefix='instance192-full-') as directory:
       shader=Path(directory)/('scene.'+stage);shader.write_text(text)
       spv=Path(directory)/'scene.spv'
       cmd=['glslangValidator','-V','--target-env','vulkan1.1',str(shader),'-o',str(spv)]
       proc=subprocess.run(cmd,text=True,capture_output=True)
       row={'renderer':r,'case':name,'profile':profile,'ubershader':bool(uber),'stage':stage,'returncode':proc.returncode,'stdout':proc.stdout,'stderr':proc.stderr,'source_sha256':hashlib.sha256(text.encode()).hexdigest()};rows.append(row)
       if proc.returncode:
        print(json.dumps(row,indent=2));(ROOT/'work/b4-instance192-shader-20261104/failing.glsl').write_text(text);return 1
       validation=subprocess.run(['spirv-val','--target-env','vulkan1.1',str(spv)],text=True,capture_output=True)
       if validation.returncode:print(validation.stdout+validation.stderr);return 1
       row['spirv_validation_returncode']=validation.returncode
       row['spirv_sha256']=hashlib.sha256(spv.read_bytes()).hexdigest()
       row['spirv_bytes']=spv.stat().st_size
       assembly=subprocess.check_output(['spirv-dis',str(spv)],text=True)
       if profile=='instance192':
        symbol='instance192_packet_interp' if r=='clustered' else 'instance192_interp'
        assert f'OpDecorate %{symbol} Location 10' in assembly
        if stage=='frag':assert 'OpDecorate %instance192_cull_mode SpecId 4' in assembly
        if name=='motion' and stage=='vert':assert 'OpName %previous_vertex_attrib' not in assembly
       assert 'OpDecorate %author_varying15 Location 15' in assembly
       assert 'OpDecorate %author_varying16 Location 16' in assembly
     with tempfile.TemporaryDirectory(prefix='instance192-link-') as directory:
      shaders=[]
      for stage,text in sources:
       shader=Path(directory)/('scene.'+stage);shader.write_text(text);shaders.append(str(shader))
      linked=subprocess.run(['glslangValidator','-V','--target-env','vulkan1.1','-l',*shaders,'-o',str(Path(directory)/'linked.spv')],text=True,capture_output=True)
      if linked.returncode:print(linked.stdout+linked.stderr);return 1
      for row in rows[-2:]:row['stage_link_returncode']=linked.returncode
     print(r,name,profile,uber or 'specialized','PASS',flush=True)
 (ROOT/'work/b4-instance192-shader-20261104/compile-results.json').write_text(json.dumps({'rows':rows,'inputs':used,'gpu':False,'engine_build':False},indent=2)+'\n')
 print('stages:',len(rows));return 0
if __name__=='__main__':raise SystemExit(main())
