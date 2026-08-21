// Thanos Snap / Ash Disintegration Shader for OBS Studio
// Designed for Twitch timeout redeems, stream endings, and dramatic farewells.
// Organic noise erosion, burning ember boundary glow, and realistic flaking ash particle dispersion.

uniform float progress<
    string name = "Disintegration Progress";
    string widget_type = "slider";
    float minimum = 0.0;
    float maximum = 1.0;
    float step = 0.01;
> = 0.5;

uniform float particle_drift<
    string name = "Wind Drift & Scatter";
    string widget_type = "slider";
    float minimum = 0.0;
    float maximum = 2.0;
    float step = 0.05;
> = 0.8;

uniform float ember_heat<
    string name = "Burning Ember Glow";
    string widget_type = "slider";
    float minimum = 0.0;
    float maximum = 3.0;
    float step = 0.1;
> = 2.0;

uniform float dissolve_angle<
    string name = "Dissolve Direction (Angle)";
    string widget_type = "slider";
    float minimum = 0.0;
    float maximum = 360.0;
    float step = 5.0;
> = 45.0;

float hash12(float2 p) {
    float3 p3 = frac(float3(p.xyx) * 0.1031);
    p3 += dot(p3, p3.yzx + 33.33);
    return frac((p3.x + p3.y) * p3.z);
}

float noise2D(float2 p) {
    float2 i = floor(p);
    float2 f = frac(p);
    float a = hash12(i);
    float b = hash12(i + float2(1.0, 0.0));
    float c = hash12(i + float2(0.0, 1.0));
    float d = hash12(i + float2(1.0, 1.0));
    float2 u = f * f * (3.0 - 2.0 * f);
    return lerp(a, b, u.x) + (c - a) * u.y * (1.0 - u.x) + (d - b) * u.x * u.y;
}

float fbm_ash(float2 p) {
    float v = 0.0;
    float a = 0.5;
    for (int i = 0; i < 4; i++) {
        v += a * noise2D(p);
        p = p * 2.2 + float2(1.7, 4.3);
        a *= 0.5;
    }
    return v;
}

float4 mainImage(VertData v_in) : TARGET {
    float p_val = clamp(progress, 0.0, 1.0);
    if (p_val <= 0.001) {
        return image.Sample(textureSampler, v_in.uv);
    }
    if (p_val >= 0.999) {
        return float4(0.0, 0.0, 0.0, 0.0);
    }

    float2 uv = v_in.uv;

    // Directional sweep coordinate
    float rad = dissolve_angle * (3.14159265 / 180.0);
    float2 dir = float2(cos(rad), sin(rad));
    float sweep_pos = dot(uv - float2(0.5, 0.5), dir) + 0.5;

    // Multi-scale organic fractal erosion noise
    float n1 = fbm_ash(uv * 10.0);
    float n2 = noise2D(uv * 25.0 + float2(elapsed_time * 0.4, 0.0));
    float erosion = sweep_pos + (n1 * 0.45 + n2 * 0.15 - 0.3);

    // Erosion distance threshold: 0 at boundary, positive in solid, negative in dissolved
    float sweep_target = p_val * 1.5 - 0.25;
    float dissolve_delta = erosion - sweep_target;

    // 1. SOLID REGION: Video with burning ember edge
    if (dissolve_delta >= 0.0) {
        float4 col = image.Sample(textureSampler, uv);
        // Fiery glowing ember edge
        if (dissolve_delta < 0.10) {
            float heat = 1.0 - (dissolve_delta / 0.10);
            float3 ember_col = lerp(float3(1.0, 0.15, 0.01), float3(1.0, 0.85, 0.15), heat * heat);
            col.rgb += ember_col * (heat * ember_heat * 1.8);
        }
        return col;
    }

    // 2. DISSOLVED / ASH REGION: Organic flaking particles drifting away
    float dist_behind = -dissolve_delta;
    if (dist_behind > 0.45) {
        return float4(0.0, 0.0, 0.0, 0.0); // Ash has fully dissipated
    }

    // Turbulent wind drift vector
    float wind_time = elapsed_time * 1.8;
    float2 curl = float2(
        sin(uv.y * 14.0 + wind_time) * 0.5 + cos(uv.x * 10.0 + wind_time * 0.8),
        -1.0 - sin(uv.x * 12.0 + wind_time) * 0.4
    );
    float2 drift_offset = (dir * 0.6 + curl * 0.4) * (particle_drift * dist_behind * 0.8);

    // Sample the source video at the origin of the drifting flake
    float2 particle_origin_uv = uv - drift_offset;
    float flake_noise = fbm_ash(particle_origin_uv * 38.0);

    // Form discrete organic flaking ash clusters
    float flake_threshold = 0.52 + dist_behind * 0.9;
    if (flake_noise > flake_threshold && particle_origin_uv.x >= 0.0 && particle_origin_uv.x <= 1.0 &&
        particle_origin_uv.y >= 0.0 && particle_origin_uv.y <= 1.0) {
        
        float4 flake_src = image.Sample(textureSampler, particle_origin_uv);
        float fade = saturate(1.0 - (dist_behind / 0.45));
        
        // Embers flickering on drifting ash
        float flicker = noise2D(particle_origin_uv * 50.0 + float2(0.0, wind_time * 3.0));
        float3 glowing_ember = float3(1.0, 0.4, 0.05) * (flicker * ember_heat);
        
        float3 ash_col = lerp(flake_src.rgb * 0.6, glowing_ember, (1.0 - dist_behind * 2.0) * 0.7);
        return float4(ash_col * fade, flake_src.a * fade);
    }

    return float4(0.0, 0.0, 0.0, 0.0);
}
