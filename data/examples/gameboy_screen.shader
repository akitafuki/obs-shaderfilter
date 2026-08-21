// Game Boy DMG Retro LCD Shader for OBS Studio
// Designed for retro gaming streams, 8-bit aesthetic, and nostalgia chat redeems.
// Authentic 4-shade olive green palette, LCD pixel grid gaps, and Bayer matrix dithering.

uniform float intensity<
    string name = "Intensity";
    string widget_type = "slider";
    float minimum = 0.0;
    float maximum = 1.0;
    float step = 0.01;
> = 1.0;

uniform float pixel_scale<
    string name = "Pixel Resolution";
    string widget_type = "slider";
    float minimum = 64.0;
    float maximum = 320.0;
    float step = 8.0;
> = 160.0;

uniform float dither_amount<
    string name = "Bayer Dithering";
    string widget_type = "slider";
    float minimum = 0.0;
    float maximum = 1.0;
    float step = 0.05;
> = 0.6;

uniform float grid_lines<
    string name = "LCD Grid Lines";
    string widget_type = "slider";
    float minimum = 0.0;
    float maximum = 1.0;
    float step = 0.05;
> = 0.4;

// 4-shade DMG Game Boy Palette Lookup
float3 get_dmg_color(float val) {
    if (val < 0.25) return float3(0.059, 0.220, 0.059); // #0f380f Darkest
    if (val < 0.50) return float3(0.188, 0.384, 0.188); // #306230 Dark Green
    if (val < 0.75) return float3(0.545, 0.675, 0.059); // #8bac0f Light Green
    return float3(0.608, 0.737, 0.059);                 // #9bbc0f Brightest
}

// 4x4 Bayer Matrix lookup function
float get_bayer4x4(int x, int y) {
    int m = (x % 4) + (y % 4) * 4;
    if (m == 0) return 0.0;
    if (m == 1) return 0.5;
    if (m == 2) return 0.125;
    if (m == 3) return 0.625;
    if (m == 4) return 0.75;
    if (m == 5) return 0.25;
    if (m == 6) return 0.875;
    if (m == 7) return 0.375;
    if (m == 8) return 0.1875;
    if (m == 9) return 0.6875;
    if (m == 10) return 0.0625;
    if (m == 11) return 0.5625;
    if (m == 12) return 0.9375;
    if (m == 13) return 0.4375;
    if (m == 14) return 0.8125;
    return 0.3125;
}

float4 mainImage(VertData v_in) : TARGET {
    float act = clamp(intensity, 0.0, 1.0);
    if (act <= 0.001) {
        return image.Sample(textureSampler, v_in.uv);
    }

    // Pixelate UV coordinates to 160x144 native Game Boy aspect
    float aspect = uv_pixel_interval.y / (uv_pixel_interval.x + 0.00001);
    float2 grid_res = float2(pixel_scale * aspect, pixel_scale);
    float2 pixel_coord = floor(v_in.uv * grid_res);
    float2 quantized_uv = (pixel_coord + 0.5) / grid_res;

    float4 src = image.Sample(textureSampler, quantized_uv);
    float luma = dot(src.rgb, float3(0.299, 0.587, 0.114));

    // Apply Bayer ordered dithering
    int bx = int(fmod(pixel_coord.x, 4.0));
    int by = int(fmod(pixel_coord.y, 4.0));
    float dither = (get_bayer4x4(bx, by) - 0.5) * (0.28 * dither_amount);
    float dithered_luma = saturate(luma + dither);

    // Quantize into 4 discrete DMG shades
    float3 dmg_col = get_dmg_color(dithered_luma);

    // LCD Subpixel Grid Gap Lines
    float2 subpixel = frac(v_in.uv * grid_res);
    float grid_mask = smoothstep(0.08, 0.15, subpixel.x) * smoothstep(0.08, 0.15, subpixel.y);
    float3 final_col = dmg_col * lerp(1.0, grid_mask, grid_lines * act);

    float4 original = image.Sample(textureSampler, v_in.uv);
    return lerp(original, float4(final_col, src.a), act);
}
