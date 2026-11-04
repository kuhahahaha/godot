#version 450
layout(set=0,binding=7,std430)readonly buffer B{uint x[];}b;
layout(location=0)out vec4 color;
void main(){color=vec4(float(b.x.length()));}
