#version 450
layout (location = 0) in vec4 vPos;
layout (location = 1) in vec2 coord;
out vec2 outCoord;
void main(){
    gl_Position=vPos;
    outCoord=coord;
}