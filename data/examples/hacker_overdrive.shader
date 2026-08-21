// Cyberpunk Hacker Matrix Overdrive Shader for OBS Studio
// Designed for Twitch "I'm in", hacking sequences, and cyber alert redeems.
// Procedural digital matrix rain, horizontal scanline tearing, and phosphor glow.

uniform float intensity<
    string name = "Intensity";
    string widget_type = "slider";
    float minimum = 0.0;
    float maximum = 1.0;
    float step = 0.01;
> = 1.0;

uniform float rain_speed<
    string name = "Matrix Rain Speed";
    string widget_type = "slider";
    float minimum = 0.5;
    float maximum = 10.0;
    float step = 0.5;
> = 3.5;

uniform float glitch_frequency<
    string name = "Glitch Tear Rate";
    string widget_type = "slider";
    float minimum = 0.0;
    float maximum = 2.0;
    float step = 0.05;
> = 0.8;

uniform float green_tint<
    string name = "Cyber Green Phosphor Tint";
    string widget_type = "slider";
    float minimum = 0.0;
    float maximum = 1.0;
    float step = 0.05;
> = 0.75;

float hash11(float p) {
    p = frac(p * 0.1031);
    p *= p + 33.33;
    p *= p + p;
    return frac(p);
}

float hash21(float2 p) {
    float3 p3 = frac(float3(p.xyx) * 0.1031);
    p3 += dot(p3, p3.yzx + 33.33);
    return frac((p3.x + p3.y) * p3.z);
}

float4 mainImage(VertData v_in) : TARGET {
    float act = clamp(intensity, 0.0, 1.0);
    if (act <= 0.001) {
        return image.Sample(textureSampler, v_in.uv);
    }

    float2 uv = v_in.uv;

    // Horizontal glitch displacement tearing
    float glitch_time = floor(elapsed_time * 12.0);
    float glitch_line = step(0.96 - glitch_frequency * 0.08, hash21(float2(floor(uv.y * 35.0), glitch_time)));
    float glitch_shift = (hash11(glitch_time + uv.y) - 0.5) * 0.08 * glitch_line * act;
    uv.x += glitch_shift;

    // Sample video with RGB chromatic glitch
    float r = image.Sample(textureSampler, uv + float2(glitch_shift * 1.5, 0.0)).r;
    float g = image.Sample(textureSampler, uv).g;
    float b = image.Sample(textureSampler, uv - float2(glitch_shift * 1.5, 0.0)).b;
    float3 base_col = float3(r, g, b);

    // Matrix Digital Rain Columns
    float num_cols = 75.0;
    float col_id = floor(uv.x * num_cols);
    float col_speed = 1.0 + hash11(col_id) * 1.5;
    float drop_t = elapsed_time * rain_speed * col_speed;
    float drop_y = frac(uv.y * 1.2 - drop_t + hash11(col_id * 3.14));

    // Glyph flicker and trail
    float trail = (1.0 - drop_y) * step(0.1, drop_y);
    float glyph_flicker = step(0.4, hash21(float2(col_id, floor(uv.y * 45.0 + drop_t * 5.0))));
    float matrix_stream = trail * glyph_flicker * 1.4;

    // CRT Scanlines
    float scanline = 0.85 + 0.15 * sin(v_in.uv.y * 800.0);

    // Cyber green color grading
    float luma = dot(base_col, float3(0.299, 0.587, 0.114));
    float3 cyber_green = float3(luma * 0.2, luma * 1.35, luma * 0.4);
    float3 blended_video = lerp(base_col, cyber_green, green_tint * act) * scanline;

    // Add bright matrix digital rain glyphs
    float3 matrix_color = float3(0.1, 1.0, 0.3) * matrix_stream * act;
    float3 final_col = blended_video + matrix_color;

    float4 original = image.Sample(textureSampler, v_in.uv);
    return lerp(original, float4(final_col, 1.0), act);
}
