// Low Health / Heartbeat Alert Shader for OBS Studio
// Designed for horror games, clutch moments, and health warning redeems.
// Bloody pulsating vignette, desaturated tunnel vision, and lub-dub heartbeat rhythm.

uniform float intensity<
    string name = "Intensity";
    string widget_type = "slider";
    float minimum = 0.0;
    float maximum = 1.0;
    float step = 0.01;
> = 1.0;

uniform float heart_rate<
    string name = "Heart Rate (BPM)";
    string widget_type = "slider";
    float minimum = 40.0;
    float maximum = 180.0;
    float step = 1.0;
> = 90.0;

uniform float blood_vignette<
    string name = "Blood Vignette Size";
    string widget_type = "slider";
    float minimum = 0.2;
    float maximum = 1.5;
    float step = 0.05;
> = 0.85;

uniform float desaturation<
    string name = "Tunnel Vision Desaturation";
    string widget_type = "slider";
    float minimum = 0.0;
    float maximum = 1.0;
    float step = 0.05;
> = 0.7;

float hash(float2 p) {
    return frac(sin(dot(p, float2(12.9898, 78.233))) * 43758.5453);
}

float noise(float2 p) {
    float2 i = floor(p);
    float2 f = frac(p);
    float a = hash(i);
    float b = hash(i + float2(1.0, 0.0));
    float c = hash(i + float2(0.0, 1.0));
    float d = hash(i + float2(1.0, 1.0));
    float2 u = f * f * (3.0 - 2.0 * f);
    return lerp(a, b, u.x) + (c - a) * u.y * (1.0 - u.x) + (d - b) * u.x * u.y;
}

float4 mainImage(VertData v_in) : TARGET {
    float act = clamp(intensity, 0.0, 1.0);
    if (act <= 0.001) {
        return image.Sample(textureSampler, v_in.uv);
    }

    float2 uv = v_in.uv;

    // Dual-phase "lub-dub" heartbeat pulse curve
    float bps = heart_rate / 60.0;
    float t = frac(elapsed_time * bps);
    float pulse1 = exp(-t * 12.0) * step(0.0, t) * (1.0 - step(0.35, t));
    float pulse2 = exp(-(t - 0.25) * 16.0) * step(0.25, t);
    float heart_pulse = (pulse1 * 1.0 + pulse2 * 0.7) * act;

    // Vignette distance with irregular vascular noise
    float2 p = uv - float2(0.5, 0.5);
    float dist = length(p * float2(1.1, 1.0));
    float vein_noise = noise(uv * 14.0 + elapsed_time * 0.5) * 0.12;
    float vig = smoothstep(0.3, blood_vignette * (1.0 + heart_pulse * 0.25), dist + vein_noise) * act;

    // Sample video with chromatic pulse on beat
    float2 ca_offset = normalize(p + 0.0001) * (heart_pulse * 0.012);
    float r = image.Sample(textureSampler, uv + ca_offset).r;
    float g = image.Sample(textureSampler, uv).g;
    float b = image.Sample(textureSampler, uv - ca_offset).b;
    float4 col = float4(r, g, b, 1.0);

    // Tunnel vision desaturation in center
    float luma = dot(col.rgb, float3(0.299, 0.587, 0.114));
    float3 desat_col = lerp(col.rgb, float3(luma, luma * 0.85, luma * 0.85), desaturation * act);

    // Blend blood-red edges with veins
    float3 blood_color = float3(0.85, 0.03, 0.02) * (1.0 + heart_pulse * 0.5);
    float3 final_col = lerp(desat_col, blood_color, vig * 0.85);

    // Heartbeat camera zoom shake
    float4 original = image.Sample(textureSampler, v_in.uv);
    return lerp(original, float4(final_col, col.a), act);
}
