// Cyberpunk Hologram Projection Shader
// Transforms video into a glowing sci-fi holographic projection with scanlines, chromatic drift, and glitch flickers.

uniform float4 hologram_color<
    string label = "Hologram Tint";
> = {0.1, 0.9, 0.95, 1.0};

uniform float scanline_density<
    string label = "Scanline Density";
    string widget_type = "slider";
    float minimum = 100.0;
    float maximum = 800.0;
    float step = 20.0;
> = 350.0;

uniform float chromatic_aberration<
    string label = "Chromatic Drift";
    string widget_type = "slider";
    float minimum = 0.0;
    float maximum = 20.0;
    float step = 1.0;
> = 6.0;

uniform float flicker_amount<
    string label = "Beam Flicker";
    string widget_type = "slider";
    float minimum = 0.0;
    float maximum = 1.0;
    float step = 0.05;
> = 0.25;

uniform float glitch_intensity<
    string label = "Glitch Distortion";
    string widget_type = "slider";
    float minimum = 0.0;
    float maximum = 1.0;
    float step = 0.05;
> = 0.35;

float hash11(float p)
{
    p = frac(p * 0.1031);
    p *= p + 33.33;
    p *= p + p;
    return frac(p);
}

float4 mainImage(VertData v_in) : TARGET
{
    float2 uv = v_in.uv;

    // Periodic horizontal slice glitch displacement
    float slice_block = floor(uv.y * 30.0);
    float glitch_seed = hash11(slice_block + floor(elapsed_time * 8.0));
    if (glitch_seed < (0.15 * glitch_intensity)) {
        float displace = (hash11(slice_block * 12.34) - 0.5) * 0.04 * glitch_intensity;
        uv.x += displace;
    }

    // Chromatic aberration / RGB shift
    float2 chr_offset = float2(chromatic_aberration * uv_pixel_interval.x, 0.0);
    float r = image.Sample(textureSampler, uv + chr_offset).r;
    float g = image.Sample(textureSampler, uv).g;
    float b = image.Sample(textureSampler, uv - chr_offset).b;
    float a = image.Sample(textureSampler, uv).a;

    // Luminance and holographic recolor
    float luma = dot(float3(r, g, b), float3(0.299, 0.587, 0.114));
    float3 holo = luma * hologram_color.rgb * 1.5;

    // Scanlines
    float scanline = sin(uv.y * scanline_density + elapsed_time * 5.0) * 0.5 + 0.5;
    holo *= (0.7 + 0.3 * scanline);

    // Large holographic beam sweep
    float beam = sin(uv.y * 4.0 - elapsed_time * 3.0) * 0.5 + 0.5;
    holo += hologram_color.rgb * beam * 0.15;

    // Random micro-flicker
    float flicker = 1.0 - (hash11(elapsed_time * 40.0) * flicker_amount);
    holo *= flicker;

    return float4(holo, a);
}
