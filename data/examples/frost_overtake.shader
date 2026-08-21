// Freeze / Frost Crystal Creep Shader for OBS Studio
// Designed for Twitch "Freeze the Streamer", cold takes, and blizzard redeems.
// Intricate dendritic ice crystals creeping inward from screen edges, smooth frosted glass blur, and icy blue grading.

uniform float intensity<
    string name = "Intensity";
    string widget_type = "slider";
    float minimum = 0.0;
    float maximum = 1.0;
    float step = 0.01;
> = 1.0;

uniform float frost_progress<
    string name = "Frost Progress (Edge to Center)";
    string widget_type = "slider";
    float minimum = 0.0;
    float maximum = 1.0;
    float step = 0.01;
> = 0.65;

uniform float creep_animation_speed<
    string name = "Auto-Creep Speed (0 = Manual)";
    string widget_type = "slider";
    float minimum = 0.0;
    float maximum = 2.0;
    float step = 0.05;
> = 0.0;

uniform float blur_strength<
    string name = "Frosted Glass Blur";
    string widget_type = "slider";
    float minimum = 0.0;
    float maximum = 10.0;
    float step = 0.5;
> = 5.0;

uniform float crystal_scale<
    string name = "Crystal Dendrite Scale";
    string widget_type = "slider";
    float minimum = 2.0;
    float maximum = 12.0;
    float step = 0.5;
> = 6.0;

float hash21(float2 p) {
    p = frac(p * float2(123.34, 456.21));
    p += dot(p, p + 45.32);
    return frac(p.x * p.y);
}

float noise2D(float2 p) {
    float2 i = floor(p);
    float2 f = frac(p);
    float a = hash21(i);
    float b = hash21(i + float2(1.0, 0.0));
    float c = hash21(i + float2(0.0, 1.0));
    float d = hash21(i + float2(1.0, 1.0));
    float2 u = f * f * (3.0 - 2.0 * f);
    return lerp(a, b, u.x) + (c - a) * u.y * (1.0 - u.x) + (d - b) * u.x * u.y;
}

float voronoi_crystal(float2 p) {
    float2 n = floor(p);
    float2 f = frac(p);
    float md = 8.0;
    for (int j = -1; j <= 1; j++) {
        for (int i = -1; i <= 1; i++) {
            float2 g = float2(float(i), float(j));
            float2 o = float2(hash21(n + g), hash21(n + g + 17.4));
            float2 r = g + o - f;
            float d = dot(r, r);
            md = min(md, d);
        }
    }
    return sqrt(md);
}

float fbm_frost(float2 p) {
    float v = 0.0;
    float a = 0.5;
    for (int i = 0; i < 4; i++) {
        v += a * noise2D(p);
        p = p * 2.2 + float2(3.1, 7.4);
        a *= 0.5;
    }
    return v;
}

float4 mainImage(VertData v_in) : TARGET {
    float act = clamp(intensity, 0.0, 1.0);
    if (act <= 0.001) {
        return image.Sample(textureSampler, v_in.uv);
    }

    float2 uv = v_in.uv;
    float2 uv_pixel = uv_pixel_interval;

    // Calculate distance from screen perimeter (0 at borders, 0.5 at center)
    float edge_d = min(min(uv.x, 1.0 - uv.x), min(uv.y, 1.0 - uv.y));
    float border_proximity = 1.0 - edge_d * 2.0; // 1.0 at outer border, 0.0 at center

    // Animate creep if speed > 0
    float eff_progress = frost_progress;
    if (creep_animation_speed > 0.01) {
        eff_progress = saturate(frost_progress + (sin(elapsed_time * creep_animation_speed) * 0.5 + 0.5) * 0.4);
    }

    // High detail fractal ice crystal dendrite pattern
    float v_cryst = voronoi_crystal(uv * crystal_scale * 2.0);
    float fbm_dendrite = fbm_frost(uv * crystal_scale * 1.5);
    float crystal_field = v_cryst * 0.55 + fbm_dendrite * 0.45;

    // Sharp inward creep threshold: borders freeze first, then dendrites grow into center
    float creep_threshold = border_proximity + (crystal_field - 0.5) * 0.65;
    float frost_target = eff_progress * 1.35 * act;
    float frost_mask = smoothstep(1.0 - frost_target, 1.15 - frost_target, creep_threshold);

    // If completely unfrozen at this pixel, return crisp original image immediately
    if (frost_mask <= 0.001) {
        return image.Sample(textureSampler, v_in.uv);
    }

    // Smooth 9-tap Frosted Glass Blur (clean multi-sample, no noisy black dots)
    float blur_rad = blur_strength * frost_mask * uv_pixel.y * 2.5;
    float4 col = image.Sample(textureSampler, uv) * 0.28;
    col += image.Sample(textureSampler, uv + float2( blur_rad,  0.0)) * 0.18;
    col += image.Sample(textureSampler, uv + float2(-blur_rad,  0.0)) * 0.18;
    col += image.Sample(textureSampler, uv + float2( 0.0,  blur_rad)) * 0.18;
    col += image.Sample(textureSampler, uv + float2( 0.0, -blur_rad)) * 0.18;

    // Icy blue & winter color grade under frost
    float luma = dot(col.rgb, float3(0.299, 0.587, 0.114));
    float3 ice_tint = float3(luma * 0.65 + 0.10, luma * 0.85 + 0.22, luma * 1.05 + 0.38);

    // Bright crystalline dendrite outlines and facet specular sparkles
    float edge_branch = smoothstep(0.45, 0.65, crystal_field) * frost_mask;
    float facet_glint = pow(crystal_field, 5.0) * frost_mask * 1.8;
    float3 frost_white = float3(0.92, 0.97, 1.0) * (edge_branch * 0.5 + facet_glint * 0.5);

    // Composite frosted layer smoothly over clear video
    float3 final_col = lerp(col.rgb, ice_tint, frost_mask * 0.7) + frost_white * 0.6;

    float4 original = image.Sample(textureSampler, v_in.uv);
    return lerp(original, float4(saturate(final_col), col.a), frost_mask * act);
}
