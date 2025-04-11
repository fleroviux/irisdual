#version 450

layout(location = 0) in vec4 a_position;
layout(location = 1) in vec4 a_color;
layout(location = 2) in vec2 a_uv;

layout(location = 0) out vec3 v_color;
layout(location = 1) out vec2 v_uv;

void main() {
  v_color = a_color.rgb;
  v_uv = a_uv;
  gl_Position = a_position;
}