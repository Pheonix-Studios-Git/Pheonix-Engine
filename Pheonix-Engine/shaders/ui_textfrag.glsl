#version 120

uniform sampler2D u_font_text;
uniform vec4 u_color; // RGBA

varying vec2 v_uv;

void main(){
    float sdf = texture2D(u_font_text, v_uv).r; // SDF Stored in Red channel
    float alpha = smoothstep(0.5 - 0.02, 0.5 + 0.02, sdf); // 0.02 Softness
    gl_FragColor = vec4(u_color.rgb, u_color.a * alpha);
}
