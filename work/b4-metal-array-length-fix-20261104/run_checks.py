from pathlib import Path
import subprocess,json,hashlib
ROOT=Path.cwd();HERE=ROOT/'work/b4-metal-array-length-fix-20261104';rows=[]
def run(command,expect=0,name=''):
 p=subprocess.run(command,text=True,capture_output=True)
 rows.append({'name':name,'command':list(map(str,command)),'returncode':p.returncode,'expected_returncode':expect,'stdout':p.stdout,'stderr':p.stderr})
 (HERE/'CHECK_RESULTS.json').write_text(json.dumps({'rows':rows,'engine_build':False,'gpu':False},indent=2)+'\n')
 assert p.returncode==expect,(name,p.stdout,p.stderr)
 print(name,'PASS',flush=True)
run([str(HERE/'layout_test')],name='production layout helper')
for mode in ('INIT','VALIDATE','FINALIZE','INIT_EMPTY','FINALIZE_EMPTY'):
 spv=ROOT/'work/b4-guard-main-rd-gpu-20261104/project'/f'{mode}.spv'
 run([str(HERE/'translate_test'),str(spv),'baseline',str(HERE/f'{mode}-baseline.metal')],2,mode+' exact mapping RED')
 run([str(HERE/'translate_test'),str(spv),'fixed',str(HERE/f'{mode}-fixed.metal')],0,mode+' auxiliary mapping GREEN')
# Nonzero binding numbers and independent descriptor sets.
source='''#version 450
layout(local_size_x=1)in;
layout(set=0,binding=3,std430)readonly buffer A{uvec4 prefix;uint x[];}a;
layout(set=1,binding=9,std430)readonly buffer B{vec4 x[];}b;
layout(set=1,binding=17,std430)buffer O{uint x[];}o;
void main(){o.x[0]=a.x.length();o.x[1]=b.x.length();o.x[2]=o.x.length();o.x[3]=a.x[0]+uint(b.x[0].x);}
'''
file=HERE/'multi_set.comp';file.write_text(source);spv=file.with_suffix('.spv')
run(['glslangValidator','-V',str(file),'-o',str(spv)],name='multi-set GLSL')
run(['spirv-val',str(spv)],name='multi-set SPIRV')
run([str(HERE/'translate_test'),str(spv),'baseline',str(HERE/'multi_set-baseline.metal')],2,'multi-set baseline RED')
run([str(HERE/'translate_test'),str(spv),'fixed',str(HERE/'multi_set-fixed.metal')],0,'multi-set disjoint auxiliary GREEN')
# Real descriptor-array shader exercises the verbatim production refusal.
array='''#version 450
layout(local_size_x=1)in;
layout(set=0,binding=0,std430)readonly buffer A{uint x[];}a[2];
layout(set=0,binding=1,std430)buffer O{uint x[];}o;
void main(){o.x[0]=a[1].x.length();}
'''
file=HERE/'descriptor_array.comp';file.write_text(array);spv=file.with_suffix('.spv')
run(['glslangValidator','-V',str(file),'-o',str(spv)],name='array GLSL legal upstream')
run([str(HERE/'translate_test'),str(spv),'fixed',str(HERE/'descriptor_array.metal')],3,'actual container array-refusal slice')
# Texture+sampler array consumes four IDs before one storage ID. The size table
# must reserve5, not original GLSL binding4 or the number of declarations2.
mixed='''#version 450
layout(local_size_x=1)in;
layout(set=0,binding=1)uniform sampler2D tex[2];
layout(set=0,binding=4,std430)buffer O{uint x[];}o;
void main(){o.x[0]=o.x.length()+uint(textureLod(tex[1],vec2(0.25),0).x);}
'''
file=HERE/'mixed_resources.comp';file.write_text(mixed);spv=file.with_suffix('.spv')
run(['glslangValidator','-V',str(file),'-o',str(spv)],name='mixed resources GLSL')
run([str(HERE/'translate_test'),str(spv),'fixed',str(HERE/'mixed_resources.metal')],0,'mixed resources auxiliary ID5')
assert 'spvBufferSizeConstants [[id(5)]]' in (HERE/'mixed_resources.metal').read_text()
# Actual production direct gate: length refused, same shader without length allowed.
run([str(HERE/'translate_test'),str(spv),'direct',str(HERE/'direct_length.metal')],4,'actual container direct-length refusal slice')
file=HERE/'direct_no_length.comp';file.write_text(mixed.replace('o.x.length()','3u'));spv=file.with_suffix('.spv')
run(['glslangValidator','-V',str(file),'-o',str(spv)],name='direct no-length GLSL')
run([str(HERE/'translate_test'),str(spv),'direct',str(HERE/'direct_no_length.metal')],0,'direct no-length retained')
# Different resources in each stage use one merged set layout. Translation
# receives the whole binding union, just as the actual container does.
vertex='''#version 450
layout(set=0,binding=4,std430)readonly buffer A{uint x[];}a;
void main(){gl_Position=vec4(float(a.x.length()),0,0,1);}
'''
fragment='''#version 450
layout(set=0,binding=7,std430)readonly buffer B{uint x[];}b;
layout(location=0)out vec4 color;
void main(){color=vec4(float(b.x.length()));}
'''
stages=[]
for ext,source in [('vert',vertex),('frag',fragment)]:
 file=HERE/f'merged.{ext}';file.write_text(source);spv=file.with_suffix(f'.{ext}.spv');stages.append(spv)
 run(['glslangValidator','-V',str(file),'-o',str(spv)],name='merged '+ext+' GLSL')
for i,ext in enumerate(('vert','frag')):
 run([str(HERE/'translate_test'),str(stages[i]),'fixed',str(HERE/f'merged-{ext}.metal'),str(stages[1-i])],0,'merged reflection '+ext+' auxiliary ID2')
 assert 'spvBufferSizeConstants [[id(2)]]' in (HERE/f'merged-{ext}.metal').read_text()
print('ALL CHECKS PASS',flush=True)
