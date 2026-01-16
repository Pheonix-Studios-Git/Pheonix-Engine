#version 120

uniform sampler2D u_font_text;
uniform float u_sdf_width;
uniform float u_outline_width;
uniform vec4 u_outline_color;

varying vec2 v_uv;
varying vec4 v_color;

void main() {
    float sdf = texture2D(u_font_text, v_uv).r;

    float text_alpha = smoothstep(
        0.5 - u_sdf_width,
        0.5 + u_sdf_width,
        sdf
    );

    float outline_alpha = smoothstep(
        0.5 - u_outline_width,
        0.5 + u_outline_width,
        sdf
    );

    vec3 rgb = mix(
        u_outline_color.rgb,
        v_color.rgb,
        text_alpha
    );

    float alpha = max(text_alpha, outline_alpha);
    alpha *= v_color.a;

    alpha = pow(alpha, 1.0 / 2.2);

    gl_FragColor = vec4(rgb, alpha);
}

