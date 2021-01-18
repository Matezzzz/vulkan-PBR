#version 450
//for std430 layout
#extension GL_EXT_scalar_block_layout : require


//xy position of vertex
layout (location = 0) in vec2 v_position;

//texture coordinates
layout(location = 0) out vec2 v_tex;
//position in tangent space
layout(location = 1) out vec3 v_tpos;
//camera position in tangent space
layout(location = 2) out vec3 v_camera_tpos;
//light positions in tangent space
layout(location = 3) out vec3 v_light_tpos[4];

layout(std430, set = 1, binding = 0) uniform u_light_data{
    vec4 light_positions[4];
    vec4 light_colors[4];
};

layout( push_constant ) uniform push_const{
    mat4 MVP;
    vec3 camera_pos;
};



void main(){
    // fill in z coordinate as 0
    v_tpos = vec3(v_position, 0);
    //compute vertex position by multiplying pos by MVP
    gl_Position = MVP * vec4(v_tpos.xyz, 1.0);
    //derive texture coordinates as XY position / 5
    v_tex = v_position.xy / 5;

    //normal points straight up, tangent in the direction of x axis
    vec3 normal = vec3(0.0, 0.0, 1.0);
    vec3 tangent = vec3(1, 0, 0);
    //create TBN matrix - transforms world to tangent space
    mat3 TBN = transpose(mat3(normalize(tangent), normalize(cross(normal, tangent)), normal));

    //convert vertex and camera pos to tangent space
    v_tpos = TBN * v_tpos;
    v_camera_tpos = TBN * camera_pos;
    //convert all light coordinates to tangent space
    for (int i = 0; i < 4; i++){
        v_light_tpos[i] = TBN * light_positions[i].xyz;
    }
}
