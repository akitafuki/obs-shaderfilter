// Heavenly Ascension / God Rays Shader for OBS Studio
// Designed for Twitch holy clutches, godly luck, and angelic ascension redeems.
// Volumetric crepuscular light shafts, golden radiance, and soft heavenly bloom.

uniform float intensity<
    string name = "Intensity";
    string widget_type = "slider";
    float minimum = 0.0;
    float maximum = 1.0;
    float step = 0.01;
> = 1.0;

uniform float num_rays<
    string name = "Number of Rays";
    string widget_type = "slider";
    float minimum = 4.0;
    float maximum = 32.0;
    float step = 1.0;
> = 16.0;

uniform float ray_speed<
    string name = "Ray Rotation Speed";
    string widget_type = "slider";
    float minimum = -5.0;
    float maximum = 5.0;
    float step = 0.1;
> = 0.8;

uniform float beam_intensity<
    string name = "Beam Brightness";
    string widget_type = "slider";
    float minimum = 0.0;
    float maximum = 3.0;
    float step = 0.1;
> = 1.5;

uniform float2 light_origin<
    string name = "Light Source Position";
> = {0.5, 0.4};

float4 mainImage(VertData v_in) : TARGET {
    float act = clamp(intensity, 0.0, 1.0);
    if (act <= 0.001) {
        return image.Sample(textureSampler, v_in.uv);
    }

    float2 uv = v_in.uv;
    float4 src = image.Sample(textureSampler, uv);

    // Vector from light origin
    float2 delta = uv - light_origin;
    float dist = length(delta);
    float angle = atan2(delta.y, delta.x);

    // Rotating volumetric radial light shafts
    float ray_pattern1 = sin(angle * num_rays + elapsed_time * ray_speed);
    float ray_pattern2 = cos(angle * (num_rays * 0.75) - elapsed_time * (ray_speed * 1.3));
    float rays = saturate((ray_pattern1 * 0.6 + ray_pattern2 * 0.4) + 0.3);

    // Attenuation over distance
    float attenuation = 1.0 / (1.0 + dist * 2.5);
    float god_ray = rays * attenuation * beam_intensity * act;

    // Heavenly warm golden-white beam color
    float3 ray_color = lerp(float3(1.0, 0.88, 0.45), float3(1.0, 1.0, 0.95), rays);

    // Screen-space radial blur bloom
    float3 bloom = float3(0.0, 0.0, 0.0);
    float2 step_vec = delta * (0.02 * act);
    float2 cur_uv = uv;
    for (int i = 0; i < 8; i++) {
        cur_uv -= step_vec;
        float3 sample_col = image.Sample(textureSampler, cur_uv).rgb;
        float luma = dot(sample_col, float3(0.299, 0.587, 0.114));
        bloom += sample_col * smoothstep(0.5, 1.0, luma) * 0.125;
    }

    // Blend heavenly rays & bloom over source
    float3 final_col = src.rgb + ray_color * god_ray + bloom * 0.6 * act;

    float4 original = image.Sample(textureSampler, v_in.uv);
    return lerp(original, float4(final_col, src.a), act);
}
