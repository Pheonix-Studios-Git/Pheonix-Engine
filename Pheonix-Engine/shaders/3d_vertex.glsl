#version 120

attribute vec3 a_pos;
attribute vec2 a_uv;
attribute vec3 a_normal;

uniform mat4 u_model;
uniform mat4 u_view;
uniform mat4 u_projection;

varying vec2 v_uv;
varying vec3 v_normal;

void main() {
    v_uv = a_uv;
    v_normal = a_normal;
    gl_Position = u_projection * u_view * u_model * vec4(a_pos, 1.0);
}