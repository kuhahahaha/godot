#!/usr/bin/env python3
"""Extract the actual two renderer decoder functions AND their original call sites."""
from pathlib import Path
import hashlib,json,re
P=Path(__file__).resolve().parent
ROOT=P.parent.parent
BASE=ROOT/'servers/rendering/renderer_rd/shaders'
def extract(source,signature):
 start=source.index(signature);brace=source.index('{',start);depth=1;i=brace+1
 while depth:
  if source[i]=='{':depth+=1
  elif source[i]=='}':depth-=1
  i+=1
 return source[start:i]
metadata={}
for kind,folder,name in [('mobile','forward_mobile','scene_forward_mobile.glsl'),('clustered','forward_clustered','scene_forward_clustered.glsl')]:
 path=BASE/folder/name;s=path.read_text()
 funcs=[extract((BASE/'oct_inc.glsl').read_text(),'vec3 oct_to_vec3('),extract(s,'void axis_angle_to_tbn('),extract(s,'void _unpack_vertex_attributes(')]
 calls=[]
 for match in re.finditer(r'\n\t_unpack_vertex_attributes\(',s):
  end=s.index(');',match.start())+2;calls.append(s[match.start():end])
 assert len(calls)==2
 code='#version 450\n#define NORMAL_USED\n#define TANGENT_USED\n#define USE_INDEXED144_VERTEX_INPUT\n#define M_PI 3.14159265358979323846\nlayout(local_size_x=1,local_size_y=1,local_size_z=1) in;\nlayout(set=0,binding=0,std430) buffer Result { vec4 values[]; } output_data;\nlayout(set=0,binding=1,std430) readonly buffer Input { vec4 values[]; } input_data;\nstruct Instance {vec4 compressed_aabb_position_pad;vec4 compressed_aabb_size_pad;};\nstruct Instances {Instance data[1];};\nInstances instances;\nstruct DrawCall {uint instance_index;};\nDrawCall draw_call;\nvec4 vertex_angle_attrib;\nvec4 axis_tangent_attrib;\nvec4 tangent_attrib;\nvec4 previous_vertex_attrib;\nvec4 previous_normal_attrib;\nvec4 previous_tangent_attrib;\n'
 code+='\n'.join(funcs)+'\nvoid main(){\n draw_call.instance_index=0u;uint instance_index=0u;vertex_angle_attrib=input_data.values[0];axis_tangent_attrib=input_data.values[1];tangent_attrib=input_data.values[2];previous_vertex_attrib=input_data.values[3];previous_normal_attrib=input_data.values[4];previous_tangent_attrib=input_data.values[5];\ninstances.data[0].compressed_aabb_position_pad=input_data.values[6];\ninstances.data[0].compressed_aabb_size_pad=input_data.values[7];\nvec3 vertex,normal,tangent,binormal,prev_vertex,prev_normal,prev_tangent,prev_binormal;\n'
 code+='\n'.join(calls)
 code+='\noutput_data.values[0]=vec4(vertex,0);output_data.values[1]=vec4(normal,0);output_data.values[2]=vec4(tangent,0);output_data.values[3]=vec4(binormal,0);\noutput_data.values[4]=vec4(prev_vertex,0);output_data.values[5]=vec4(prev_normal,0);output_data.values[6]=vec4(prev_tangent,0);output_data.values[7]=vec4(prev_binormal,0);\n}\n'
 (P/'runtime'/f'{kind}.comp').write_text(code)
 metadata[kind]={'source_path':str(path),'source_sha256':hashlib.sha256(path.read_bytes()).hexdigest(),'fixture_sha256':hashlib.sha256(code.encode()).hexdigest(),'actual_calls':calls,'scope':'actual original GLSL decoder+calls in compute scaffolding, not actual vertex-input/draw/material proof'}
(P/'DECODER_SOURCE.json').write_text(json.dumps(metadata,indent=2)+'\n')
