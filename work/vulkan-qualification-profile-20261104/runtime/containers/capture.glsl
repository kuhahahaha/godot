#version 450
layout(local_size_x=1,local_size_y=1,local_size_z=1) in;
layout(set=0,binding=0,std430) buffer RawResults { uint words[]; } result_data;
layout(set=0,binding=1,std430) buffer FaultPlane { uint words[]; } fault_data;
layout(set=0,binding=2,std430) buffer ResultEpilogue { uint words[]; } epilogue_data;
layout(set=1,binding=0,std430) readonly buffer RawParameters { uint words[]; } source_data;
layout(push_constant,std430) uniform Push { uint base; uint envelope; uint context; uint iteration; } p;
void main() {
    result_data.words[0] = source_data.words[1] + p.base + p.envelope + p.context + p.iteration;
    fault_data.words[0] = source_data.words[1];
    epilogue_data.words[0] = source_data.words[3] + 1u;
}
