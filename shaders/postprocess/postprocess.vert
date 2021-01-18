#version 450

//vec coords
layout (location = 0) in vec2 a_pos;
//texture coords
layout (location = 1) in vec2 a_tex;

//out tex coords(intepolated)
layout(location = 0) out vec2 v_tex;

void main(){
    //compute vec position by setting z to 0 and w to 1
    gl_Position = vec4(a_pos, 0.0, 1.0);
    //pass tex coords to fragment shader
    v_tex = a_tex;
}