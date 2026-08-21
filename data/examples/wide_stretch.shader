// Wide Walk Meme Shader for OBS Studio
// Designed for Twitch "Wide Streamer" meme redeems.
// Authentic anamorphic wide stretch with walking bob and sway - 100% solid, zero ghosting.

uniform float intensity<
    string name = "Intensity";
    string widget_type = "slider";
    float minimum = 0.0;
    float maximum = 1.0;
    float step = 0.01;
> = 1.0;

uniform float wide_factor<
    string name = "Wide Multiplier (Chonk)";
    string widget_type = "slider";
    float minimum = 1.0;
    float maximum = 4.0;
    float step = 0.1;
> = 2.4;

uniform float walk_speed<
    string name = "Walking Tempo Speed";
    string widget_type = "slider";
    float minimum = 0.0;
    float maximum = 15.0;
    float step = 0.2;
> = 7.0;

uniform float walk_bob<
    string name = "Bobbing Height";
    string widget_type = "slider";
    float minimum = 0.0;
    float maximum = 0.15;
    float step = 0.005;
> = 0.04;

uniform float walk_sway<
    string name = "Strut Sway Amount";
    string widget_type = "slider";
    float minimum = 0.0;
    float maximum = 0.10;
    float step = 0.005;
> = 0.03;

float4 mainImage(VertData v_in) : TARGET {
    float act = clamp(intensity, 0.0, 1.0);
    if (act <= 0.001) {
        return image.Sample(textureSampler, v_in.uv);
    }

    float2 uv = v_in.uv;

    // Walking rhythm bob & sway
    float step_phase = elapsed_time * walk_speed;
    float bob = abs(sin(step_phase)) * (walk_bob * act);
    float sway = sin(step_phase * 0.5) * (walk_sway * act);

    // Continuous direct coordinate scaling (no image crossfade ghosting)
    float cur_wide = lerp(1.0, wide_factor, act);
    float cur_squash = lerp(1.0, 1.0 + (wide_factor - 1.0) * 0.35, act);

    float2 p = uv - float2(0.5, 0.5);
    p.x = (p.x - sway) / cur_wide;
    p.y = (p.y + bob) * cur_squash;

    float2 sample_uv = float2(0.5, 0.5) + p;

    // Sample directly (100% solid, zero double-image ghosting)
    return image.Sample(textureSampler, saturate(sample_uv));
}
