#version 120

uniform sampler2D u_font_text;
uniform float u_sdf_width;
uniform float u_outline_width;
uniform float u_pixel_height;
uniform vec4 u_outline_color;

varying vec2 v_uv;
varying vec4 v_color;

void main() {
    float sdf = texture2D(u_font_text, v_uv).r;

    float edge_adjustment = 0.0;
    float aa_min = 0.01;

    if (u_pixel_height < 16.0) {
        edge_adjustment = 0.05;
        aa_min = 0.03;
    }
    
    float edge = u_sdf_width * edge_adjustment;
    float aa = max(fwidth(sdf) * 0.5, aa_min);

    float text_alpha = smoothstep(0.5 - edge - aa, 0.5 + edge + aa, sdf);
    float outline_alpha = smoothstep(0.5 - u_outline_width - aa, 0.5 + u_outline_width + aa, sdf);

    vec3 rgb = mix(u_outline_color.rgb, v_color.rgb, text_alpha);
    float alpha = max(text_alpha, outline_alpha) * v_color.a;

    alpha = pow(alpha, 1.0/1.2);

    gl_FragColor = vec4(rgb, alpha);
}

