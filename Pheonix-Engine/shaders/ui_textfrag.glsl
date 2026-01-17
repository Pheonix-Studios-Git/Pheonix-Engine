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

    float edge = u_sdf_width;
    if (u_pixel_height < 24.0)
        edge *= 0.6;

    float text_alpha = smoothstep(0.5 - edge, 0.5 + edge, sdf);
    float outline_alpha = smoothstep(0.5 - u_outline_width, 0.5 + u_outline_width, sdf);

    vec3 rgb = mix(u_outline_color.rgb, v_color.rgb, text_alpha);
    float alpha = max(text_alpha, outline_alpha) * v_color.a;

    gl_FragColor = vec4(rgb, alpha);
}

