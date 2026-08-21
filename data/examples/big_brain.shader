// Big Brain / Megamind Distortion Shader for OBS Studio
// Designed for Twitch 200-IQ plays, galaxy brain moments, and trolling facecam.
// Smooth monotonic forehead expansion and chin taper with zero ghosting.

uniform float intensity<
    string name = "Intensity";
    string widget_type = "slider";
    float minimum = 0.0;
    float maximum = 1.0;
    float step = 0.01;
> = 1.0;

uniform float brain_expand<
    string name = "Forehead Bulge";
    string widget_type = "slider";
    float minimum = 0.0;
    float maximum = 2.5;
    float step = 0.05;
> = 1.2;

uniform float chin_pinch<
    string name = "Chin / Jaw Taper";
    string widget_type = "slider";
    float minimum = 0.0;
    float maximum = 2.0;
    float step = 0.05;
> = 0.9;

uniform float2 face_center<
    string name = "Face Center";
> = {0.5, 0.5};

uniform float psychic_glow<
    string name = "Psychic Aura Glow";
    string widget_type = "slider";
    float minimum = 0.0;
    float maximum = 1.5;
    float step = 0.05;
> = 0.4;

float4 mainImage(VertData v_in) : TARGET {
    float act = clamp(intensity, 0.0, 1.0);
    if (act <= 0.001) {
        return image.Sample(textureSampler, v_in.uv);
    }

    float2 uv = v_in.uv;
    float2 total_displacement = float2(0.0, 0.0);

    // 1. Forehead Expansion (Bulge Anchor)
    float2 fh_pos = face_center + float2(0.0, -0.16);
    float2 d_fh = uv - fh_pos;
    float r_fh = length(d_fh * float2(1.0, 1.3));
    float fh_rad = 0.44;
    if (r_fh < fh_rad) {
        float w = 1.0 - (r_fh / fh_rad);
        float k = w * w * (3.0 - 2.0 * w);
        // Pull sample coordinate inward to magnify/bulge the forehead
        total_displacement -= d_fh * (k * brain_expand * 0.38);
    }

    // 2. Chin & Jaw Narrowing (Pinch Anchor)
    float2 chin_pos = face_center + float2(0.0, 0.22);
    float2 d_chin = uv - chin_pos;
    float r_chin = length(d_chin * float2(1.3, 1.0));
    float chin_rad = 0.40;
    if (r_chin < chin_rad) {
        float w = 1.0 - (r_chin / chin_rad);
        float k = w * w * (3.0 - 2.0 * w);
        // Push X sample coordinate outward to compress/taper the jaw
        total_displacement.x += d_chin.x * (k * chin_pinch * 0.45);
    }

    // Apply continuous displacement directly to UV (zero double-exposure ghosting)
    float2 sample_uv = uv + total_displacement * act;
    float4 col = image.Sample(textureSampler, saturate(sample_uv));

    // Psychic Galaxy Brain Crown Aura
    if (psychic_glow > 0.01 && uv.y < face_center.y) {
        float2 glow_delta = (uv - (face_center + float2(0.0, -0.28))) * float2(1.2, 2.2);
        float glow_dist = length(glow_delta);
        if (glow_dist < 0.36) {
            float w = 1.0 - (glow_dist / 0.36);
            float glow_mask = w * w * psychic_glow * act;
            float pulse = 0.85 + 0.15 * sin(elapsed_time * 6.0);
            float3 aura_col = lerp(float3(0.1, 0.85, 1.0), float3(1.0, 0.2, 0.95), 0.5 + 0.5 * sin(elapsed_time * 2.5));
            col.rgb += aura_col * (glow_mask * pulse);
        }
    }

    return col;
}
