#version 450

//tex coords
layout(location = 0) in vec2 v_tex;

//result color
layout(location = 0) out vec4 o_color;

//rendered image texture
layout(set = 0, binding = 0) uniform sampler2D u_render;

void main(){
    //Here an elaborate postprocessing algorithm could be written. I tried Reinhard tone mapping, but it seemed worse to me than just raw result, so...
    //just sample render texture at given coordinates
    o_color = texture(u_render, v_tex);
}