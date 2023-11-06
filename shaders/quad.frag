#version 450 core
layout(binding = 0) uniform sampler2D screen_tex;
in vec2 outCoord;
layout (location=0) out vec4 color; 
void main(){
    color=texture(screen_tex,outCoord);
    //color=vec4(outCoord.x,outCoord.y,0,1);
}