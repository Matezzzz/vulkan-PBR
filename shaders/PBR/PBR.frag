#version 450
#extension GL_EXT_scalar_block_layout : require


//The PBR fragment shader, this is where the magic happens
//All equations / components of lighting computations are based on tutorials here:
//  Normal mapping and tangent space - https://learnopengl.com/Advanced-Lighting/Normal-Mapping
//  Parallax mapping - https://learnopengl.com/Advanced-Lighting/Parallax-Mapping
//  PBR - theory -https://learnopengl.com/PBR/Theory
//      - implementation - https://learnopengl.com/PBR/Lighting
// As most of the code is just copied/rewritten from these tutorials, I make no attempt to explain it here. If interested in how the result is pieced together, check the links above, they are much better at describing the process than my comments.



layout(location = 0) in vec2 v_tex;
layout(location = 1) in vec3 v_tpos;
layout(location = 2) in vec3 v_camera_tpos;
layout(location = 3) in vec3 v_light_tpos[4];


layout(location = 0) out vec3 o_color;

//all PBR textures
layout(set = 0, binding = 0) uniform sampler2D u_diffuse;
layout(set = 0, binding = 1) uniform sampler2D u_AO;
layout(set = 0, binding = 2) uniform sampler2D u_height;
layout(set = 0, binding = 3) uniform sampler2D u_normal;
layout(set = 0, binding = 4) uniform sampler2D u_roughness;
layout(set = 0, binding = 5) uniform sampler2D u_metallic;

layout(std430, set = 1, binding = 0) uniform u_light_data{
    vec4 light_positions[4];
    vec4 light_colors[4];
};

const float PI = 3.1415926;


vec3 fresnelSchlick(float cosTheta, vec3 F0)
{
    return F0 + (1.0 - F0) * pow(1.0 - cosTheta, 5.0);
}  
float DistributionGGX(vec3 N, vec3 H, float roughness)
{
    float a      = roughness*roughness;
    float a2     = a*a;
    float NdotH  = max(dot(N, H), 0.0);
    float NdotH2 = NdotH*NdotH;
	
    float num   = a2;
    float denom = (NdotH2 * (a2 - 1.0) + 1.0);
    denom = PI * denom * denom;
	
    return num / denom;
}

float GeometrySchlickGGX(float NdotV, float roughness)
{
    float r = (roughness + 1.0);
    float k = (r*r) / 8.0;

    float num   = NdotV;
    float denom = NdotV * (1.0 - k) + k;
	
    return num / denom;
}
float GeometrySmith(vec3 N, vec3 V, vec3 L, float roughness)
{
    float NdotV = max(dot(N, V), 0.0);
    float NdotL = max(dot(N, L), 0.0);
    float ggx2  = GeometrySchlickGGX(NdotV, roughness);
    float ggx1  = GeometrySchlickGGX(NdotL, roughness);
	
    return ggx1 * ggx2;
}

void main()
{
    vec3 V = normalize(v_camera_tpos - v_tpos);

    //PARRALAX MAPPING
    const float parallax_height_scale = 0.1;
    const float parallax_layers = 20;
    const float layer_depth = 1.0 / parallax_layers;
    float current_depth = 0.0;
    vec2 tex_delta = V.xy * parallax_height_scale / parallax_layers;
    
    vec2 tex_coords = v_tex;
    float texture_depth = texture(u_height, tex_coords).r;
    while (current_depth < texture_depth)
    {
        tex_coords -= tex_delta;
        texture_depth = texture(u_height, tex_coords).r;
        current_depth += layer_depth;
    }
    vec2 previous_tex_coords = tex_coords + tex_delta;
    float now_depth = texture_depth - current_depth;
    float previous_depth = texture(u_height, previous_tex_coords).r - current_depth + layer_depth;
    float weight = now_depth / (now_depth - previous_depth);
    tex_coords = mix(tex_coords, previous_tex_coords, weight);

    vec3 albedo = texture(u_diffuse, tex_coords).xyz;
    vec3 normal = texture(u_normal, tex_coords).xyz; // <- Normal mapping (no transform, all vectors are already in tangent space)
    float AO = texture(u_AO, tex_coords).x / 10;
    float roughness = texture(u_roughness, tex_coords).x;
    float metallic = texture(u_metallic, tex_coords).x;

    vec3 N = normalize(normal * 2.0 - 1.0);
    

    vec3 X = vec3(0);

    vec3 Lo = vec3(0.0);
    //PBR calculations for all light sources
    for (int i = 0; i < 4; i++){
        vec3 L = normalize(v_light_tpos[i] - v_tpos);
        vec3 H = normalize(V + L);

        float dist = length(v_light_tpos[i] - v_tpos);
        float attenuation = 100.0 / dist / dist;
        vec3 radiance = attenuation * light_colors[i].xyz;

        vec3 F0 = vec3(0.04);
        F0 = mix(F0, albedo, metallic);
        vec3 F = fresnelSchlick(max(dot(H, V), 0.0), F0);

        float NDF = DistributionGGX(N, H, roughness);   //OK

        float G = GeometrySmith(N, V, L, roughness);

        vec3 specular = NDF*G*F / max(0.001, 4.0 * max(dot(N, V), 0.0) * max(dot(N, L), 0.0));

        X = specular;

        vec3 kS = F;
        vec3 kD = vec3(1.0) - kS;

        kD *= 1.0 - metallic;

        Lo += (kD * albedo / PI + specular) * radiance * max(dot(N, L), 0.0);
    }
    o_color = albedo * AO + Lo;
}