#version 120

uniform vec4 u_color;
uniform sampler2D u_texture;

varying vec2 v_uv;
varying vec3 v_normal;

void main() {
    gl_FragColor = u_color * texture2D(u_texture, v_uv);
}