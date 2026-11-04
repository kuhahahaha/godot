#version 450
layout(local_size_x=1,local_size_y=1,local_size_z=1) in;
layout(set=0,binding=0,std430) buffer ScalarOutput { uint word; } output_data;
void main() {
    output_data.word = 0x40400000u;
}
