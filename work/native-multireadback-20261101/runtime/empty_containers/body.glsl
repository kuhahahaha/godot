#version 450
layout(local_size_x=64,local_size_y=1,local_size_z=1) in;
layout(set=0,binding=0,std430) buffer RawResults { uint words[]; } result_data;
layout(set=0,binding=1,std430) buffer FaultPlane { uint words[]; } fault_data;
layout(set=0,binding=2,std430) buffer ResultEpilogue { uint words[]; } epilogue_data;
layout(set=1,binding=0,std430) readonly buffer RawParameters { uint words[]; } source_data;
layout(push_constant,std430) uniform Push { uint base; uint envelope; uint context; uint iteration; } p;
void main() {
    if (p.envelope == 0u) { return; }
    uint i = gl_GlobalInvocationID.x;
    if (i < p.envelope) {
        result_data.words[i] = source_data.words[0] + i + p.base + p.context + p.iteration;
    }
    if (i == 0u) {
        fault_data.words[0] = source_data.words[2];
        epilogue_data.words[0] = source_data.words[3];
    }
}
