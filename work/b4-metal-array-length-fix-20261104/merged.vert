#version 450
layout(set=0,binding=4,std430)readonly buffer A{uint x[];}a;
void main(){gl_Position=vec4(float(a.x.length()),0,0,1);}
