#version 120

uniform sampler2D u_texture;
uniform float u_corner_radius;
uniform vec2 u_texel_size;
uniform float u_noise;
uniform vec2 u_size;

varying vec2 v_uv;
varying vec4 v_color;

float rounded_rect(vec2 p, vec2 half_size, float radius) {
    vec2 q = abs(p) - half_size + vec2(radius);
    return length(max(q, 0.0)) - radius;
}

float hash(vec2 p) {
    return fract(sin(dot(p, vec2(127.1,311.7))) * 43758.5453);
}

void main() {
    vec2 p = v_uv * u_size - u_size * 0.5;
    float d = rounded_rect(p, u_size * 0.5, u_corner_radius);

    float aa = fwidth(d);
    float alpha = 1.0 - smoothstep(0.0, aa, d);
    if (alpha <= 0.0)
        discard;

    float glow = smoothstep(0.0, 1.0, 1.0 - d / u_corner_radius);
    float noise = (hash(gl_FragCoord.xy) - 0.5) * u_noise;

    vec3 base = v_color.rgb;
    base += glow * 0.06;
    base += noise;

    vec3 tex = texture2D(u_texture, v_uv).rgb;
    base = base * 0.98 + tex * 0.02;

    gl_FragColor = vec4(base, v_color.a * alpha);
}
