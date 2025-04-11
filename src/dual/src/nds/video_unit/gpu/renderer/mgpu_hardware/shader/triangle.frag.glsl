#version 450

layout(location = 0) in vec3 v_color;
layout(location = 1) in vec2 v_uv;

layout(location = 0) out vec4 frag_color;

layout(set = 0, binding = 0) uniform Uniforms {
    vec3 u_color_multiply;
};

layout(set = 0, binding = 1) uniform sampler2D u_test_texture;

void main() {
    //frag_color = vec4(v_color * u_color_multiply, 1.0);
    //frag_color = vec4(v_uv, 0.0, 1.0);
    frag_color = texture(u_test_texture, v_uv) * vec4(v_color, 1.0);
}