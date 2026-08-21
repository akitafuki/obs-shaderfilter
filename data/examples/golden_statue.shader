// 24K Golden Statue & Glitter Sparkle Shader for OBS Studio
// Designed for Twitch wins, sub goals, hype moments, and gold trophy redeems.
// Metallic gold gradient mapping, continuous anisotropic specular gleam, and seamless star glitter.

uniform float intensity<
    string name = "Intensity";
    string widget_type = "slider";
    float minimum = 0.0;
    float maximum = 1.0;
    float step = 0.01;
> = 1.0;

uniform float sheen_speed<
    string name = "Metallic Sheen Speed";
    string widget_type = "slider";
    float minimum = 0.0;
    float maximum = 4.0;
    float step = 0.1;
> = 1.2;

uniform float sparkle_amount<
    string name = "Glitter Sparkles";
    string widget_type = "slider";
    float minimum = 0.0;
    float maximum = 2.0;
    float step = 0.1;
> = 0.8;

uniform float metallic_contrast<
    string name = "Metallic Contrast";
    string widget_type = "slider";
    float minimum = 0.5;
    float maximum = 2.5;
    float step = 0.05;
> = 1.3;

float hash22(float2 p) {
    float3 p3 = frac(float3(p.xyx) * 0.1031);
    p3 += dot(p3, p3.yzx + 33.33);
    return frac((p3.x + p3.y) * p3.z);
}

// Smooth exponential 4-point diamond star sparkle (no hard clipping or division singularities)
float calc_star_glint(float2 delta, float size) {
    float2 d = abs(delta);
    float core = exp(-length(d) * 45.0);
    float beam_h = exp(-d.x * 55.0 - d.y * 12.0);
    float beam_v = exp(-d.y * 55.0 - d.x * 12.0);
    return (core * 0.6 + (beam_h + beam_v) * 0.4) * size;
}

float4 mainImage(VertData v_in) : TARGET {
    float act = clamp(intensity, 0.0, 1.0);
    if (act <= 0.001) {
        return image.Sample(textureSampler, v_in.uv);
    }

    float2 uv = v_in.uv;
    float4 src = image.Sample(textureSampler, uv);
    float luma = dot(src.rgb, float3(0.299, 0.587, 0.114));

    // Curvature & metallic tone curve
    float m_luma = pow(saturate(luma), 1.0 / max(0.1, metallic_contrast));

    // Rich 24K Gold Palette
    float3 shadow_gold = float3(0.28, 0.14, 0.02); // Deep rich amber
    float3 mid_gold    = float3(0.96, 0.74, 0.12); // Pure 24K gold
    float3 high_gold   = float3(1.00, 0.94, 0.58); // Radiant light gold
    float3 spec_gold   = float3(1.00, 1.00, 0.94); // Specular white-gold

    float3 gold_col;
    if (m_luma < 0.45) {
        gold_col = lerp(shadow_gold, mid_gold, m_luma / 0.45);
    } else if (m_luma < 0.82) {
        gold_col = lerp(mid_gold, high_gold, (m_luma - 0.45) / 0.37);
    } else {
        gold_col = lerp(high_gold, spec_gold, (m_luma - 0.82) / 0.18);
    }

    // Moving Anisotropic Diagonal Light Sweep
    float sheen_pos = frac(elapsed_time * sheen_speed * 0.25);
    float sheen_dist = abs((uv.x + uv.y * 0.5) - sheen_pos * 2.0);
    float sheen_highlight = smoothstep(0.22, 0.0, sheen_dist) * m_luma * 0.75;
    gold_col += spec_gold * sheen_highlight;

    // Seamless 3x3 Cellular Glitter Sparkles (no square artifacts or clipping seams)
    if (sparkle_amount > 0.01) {
        float2 grid_scale = float2(22.0, 22.0);
        float2 cell = floor(uv * grid_scale);
        float total_sparkle = 0.0;

        // Check 3x3 adjacent neighborhood so sparkles extend smoothly across boundaries
        for (int dy = -1; dy <= 1; dy++) {
            for (int dx = -1; dx <= 1; dx++) {
                float2 cur_cell = cell + float2(float(dx), float(dy));
                float rand_val = hash22(cur_cell);
                
                // Only a subset of cells contain active glitter points
                if (rand_val > 0.70) {
                    float2 spark_pos = (cur_cell + float2(hash22(cur_cell + 1.3), hash22(cur_cell + 4.7))) / grid_scale;
                    
                    // Sample luma at sparkle location so sparkles only appear on bright highlights
                    float spark_luma = dot(image.Sample(textureSampler, spark_pos).rgb, float3(0.299, 0.587, 0.114));
                    float highlight_mask = smoothstep(0.45, 0.80, spark_luma);
                    
                    if (highlight_mask > 0.01) {
                        float spark_time = elapsed_time * 5.5 + rand_val * 6.28;
                        float spark_phase = pow(max(0.0, sin(spark_time)), 6.0);
                        float2 delta = uv - spark_pos;
                        
                        total_sparkle += calc_star_glint(delta, spark_phase * highlight_mask);
                    }
                }
            }
        }

        gold_col += float3(1.0, 0.96, 0.75) * (total_sparkle * sparkle_amount * 1.6);
    }

    float4 original = image.Sample(textureSampler, v_in.uv);
    return lerp(original, float4(saturate(gold_col), src.a), act);
}
