#version 450
layout (location = 0) in vec4 vPos;
out vec2 coord;
void main(){
    gl_Position=vPos;
    coord=vPos.xy;
}