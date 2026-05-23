#version 120

attribute vec2 a_pos;
attribute vec2 a_uv;
attribute vec4 a_normal;

uniform mat4 u_model;
uniform mat4 u_view;
uniform mat4 u_projection;

varying vec2 v_uv;

void main() {
    v_uv = a_uv;
    gl_Position = u_projection * u_view * u_model * vec4(a_pos, 1.0);
}