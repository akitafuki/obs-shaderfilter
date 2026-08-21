// Nuclear Deep-Fry Shader for OBS Studio
// Designed for Twitch chat redeems, fails, and bass-boosted moments.
// Features unsharp-mask crunch, extreme saturation/contrast boost, chromatic aberration, and audio-reactive vibration.

uniform float intensity<
    string name = "Intensity";
    string widget_type = "slider";
    float minimum = 0.0;
    float maximum = 1.0;
    float step = 0.01;
> = 1.0;

uniform float saturation<
    string name = "Saturation Boost";
    string widget_type = "slider";
    float minimum = 1.0;
    float maximum = 5.0;
    float step = 0.05;
> = 2.8;

uniform float contrast<
    string name = "Contrast Crunch";
    string widget_type = "slider";
    float minimum = 1.0;
    float maximum = 4.0;
    float step = 0.05;
> = 2.2;

uniform float sharpness<
    string name = "Edge Sharpness Crunch";
    string widget_type = "slider";
    float minimum = 0.0;
    float maximum = 4.0;
    float step = 0.05;
> = 2.5;

uniform float chromatic_aberration<
    string name = "Chromatic Aberration";
    string widget_type = "slider";
    float minimum = 0.0;
    float maximum = 20.0;
    float step = 0.1;
> = 6.0;

uniform float audio_shake<
    string name = "Audio-Reactive Shake";
    string widget_type = "slider";
    float minimum = 0.0;
    float maximum = 2.0;
    float step = 0.05;
> = 1.0;

uniform float audio_magnitude;
uniform float audio_peak;

float3 rgb_to_hsv(float3 c) {
    float4 K = float4(0.0, -1.0 / 3.0, 2.0 / 3.0, -1.0);
    float4 p = lerp(float4(c.bg, K.wz), float4(c.gb, K.xy), step(c.b, c.g));
    float4 q = lerp(float4(p.xyw, c.r), float4(c.r, p.yzx), step(p.x, c.r));
    float d = q.x - min(q.w, q.y);
    float e = 1.0e-10;
    return float3(abs(q.z + (q.w - q.y) / (6.0 * d + e)), d / (q.x + e), q.x);
}

float3 hsv_to_rgb(float3 c) {
    float4 K = float4(1.0, 2.0 / 3.0, 1.0 / 3.0, 3.0);
    float3 p = abs(frac(c.xxx + K.xyz) * 6.0 - K.www);
    return c.z * lerp(K.xxx, clamp(p - K.xxx, 0.0, 1.0), c.y);
}

float4 mainImage(VertData v_in) : TARGET {
    float act = clamp(intensity, 0.0, 1.0);
    if (act <= 0.001) {
        return image.Sample(textureSampler, v_in.uv);
    }

    float2 uv_pixel = uv_pixel_interval;
    float2 uv = v_in.uv;

    // Audio-reactive violent screen vibration
    float shake_amt = (audio_magnitude * 0.015 + audio_peak * 0.01) * audio_shake * act;
    float shake_time = elapsed_time * 65.0;
    float2 shake_offset = float2(
        sin(shake_time * 1.3) * cos(shake_time * 0.7),
        cos(shake_time * 1.1) * sin(shake_time * 0.9)
    ) * shake_amt;
    uv += shake_offset;

    // Chromatic Aberration Offset
    float2 center_offset = uv - float2(0.5, 0.5);
    float dist = length(center_offset);
    float2 ca_dir = (dist > 0.001) ? normalize(center_offset) : float2(1.0, 0.0);
    float2 ca_offset = ca_dir * (chromatic_aberration * uv_pixel * (dist + 0.2) * act);

    // Sample RGB with chromatic split
    float r = image.Sample(textureSampler, uv + ca_offset).r;
    float g = image.Sample(textureSampler, uv).g;
    float b = image.Sample(textureSampler, uv - ca_offset).b;
    float a = image.Sample(textureSampler, uv).a;
    float3 col = float3(r, g, b);

    // Unsharp mask edge sharpening kernel
    float3 col_up    = image.Sample(textureSampler, uv + float2(0.0, -uv_pixel.y * 2.0)).rgb;
    float3 col_down  = image.Sample(textureSampler, uv + float2(0.0,  uv_pixel.y * 2.0)).rgb;
    float3 col_left  = image.Sample(textureSampler, uv + float2(-uv_pixel.x * 2.0, 0.0)).rgb;
    float3 col_right = image.Sample(textureSampler, uv + float2( uv_pixel.x * 2.0, 0.0)).rgb;
    float3 blur_col = (col_up + col_down + col_left + col_right) * 0.25;

    float3 sharp_col = col + (col - blur_col) * (sharpness * 1.8 * act);
    col = lerp(col, sharp_col, act);

    // Extreme Saturation Boost in HSV space
    float3 hsv = rgb_to_hsv(saturate(col));
    hsv.y = saturate(hsv.y * lerp(1.0, saturation, act));
    // Warm hue shift toward yellow/red
    hsv.x = frac(hsv.x - 0.03 * act * (1.0 - hsv.z));
    float3 sat_col = hsv_to_rgb(hsv);

    // High Contrast S-Curve Crunch
    float c_factor = lerp(1.0, contrast, act);
    float3 cont_col = (sat_col - 0.5) * c_factor + 0.5;

    // Hard clipping flare
    float3 final_col = saturate(cont_col);

    // Slight noise grit
    float noise = frac(sin(dot(uv * elapsed_time, float2(12.9898, 78.233))) * 43758.5453);
    final_col += (noise - 0.5) * 0.08 * act;

    float4 original = image.Sample(textureSampler, v_in.uv);
    return lerp(original, float4(saturate(final_col), a), act);
}
