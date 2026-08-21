// Underwater Aquarium & Caustics Shader for OBS Studio
// Designed for chill streams, submerged redeems, and aquatic ambience.
// Refractive water wobble, animated sunlight caustics, and photorealistic refractive rising bubbles.

uniform float intensity<
    string name = "Intensity";
    string widget_type = "slider";
    float minimum = 0.0;
    float maximum = 1.0;
    float step = 0.01;
> = 1.0;

uniform float wave_strength<
    string name = "Water Refraction Wobble";
    string widget_type = "slider";
    float minimum = 0.0;
    float maximum = 0.04;
    float step = 0.002;
> = 0.012;

uniform float caustics_brightness<
    string name = "Sunlight Caustics";
    string widget_type = "slider";
    float minimum = 0.0;
    float maximum = 2.5;
    float step = 0.05;
> = 1.1;

uniform float ocean_tint<
    string name = "Ocean Blue Depth Tint";
    string widget_type = "slider";
    float minimum = 0.0;
    float maximum = 1.0;
    float step = 0.05;
> = 0.65;

uniform float bubble_density<
    string name = "Bubble Amount (0 = Disabled)";
    string widget_type = "slider";
    float minimum = 0.0;
    float maximum = 2.0;
    float step = 0.1;
> = 0.8;

float hash21(float2 p) {
    p = frac(p * float2(234.34, 435.345));
    p += dot(p, p + 34.23);
    return frac(p.x * p.y);
}

// Multi-layered animated sunlight caustics
float water_caustic(float2 uv, float speed) {
    float t = elapsed_time * speed;
    float2 p1 = uv * 8.0 + float2(sin(t * 0.7), cos(t * 0.9));
    float2 p2 = uv * 12.0 + float2(cos(t * 1.1), sin(t * 0.8));
    float c1 = sin(p1.x + sin(p1.y + t)) * sin(p1.y + sin(p1.x + t));
    float c2 = sin(p2.x + sin(p2.y - t)) * sin(p2.y + sin(p2.x - t));
    return pow(saturate((c1 + c2) * 0.5 + 0.5), 3.5);
}

float4 mainImage(VertData v_in) : TARGET {
    float act = clamp(intensity, 0.0, 1.0);
    if (act <= 0.001) {
        return image.Sample(textureSampler, v_in.uv);
    }

    float2 uv = v_in.uv;

    // Gentle aquatic buoyancy wave wobble
    float t = elapsed_time * 1.8;
    float2 wave = float2(
        sin(uv.y * 14.0 + t) * cos(uv.x * 10.0 + t * 0.7),
        cos(uv.x * 12.0 - t * 0.9) * sin(uv.y * 11.0 + t * 0.6)
    ) * (wave_strength * act);

    float2 distorted_uv = uv + wave;
    float4 col = image.Sample(textureSampler, distorted_uv);

    // Deep oceanic turquoise / blue color grading
    float luma = dot(col.rgb, float3(0.299, 0.587, 0.114));
    float3 ocean_blue = float3(luma * 0.08, luma * 0.65 + 0.12, luma * 0.95 + 0.28);
    float3 submerged_col = lerp(col.rgb, ocean_blue, ocean_tint * act);

    // Moving sunlight caustics network with chromatic turquoise fringe
    float caustic = water_caustic(distorted_uv, 1.1) * (caustics_brightness * act);
    float3 caustic_color = float3(0.35, 0.92, 1.0) * caustic;
    submerged_col += caustic_color;

    // Photorealistic Refractive Rising Bubbles
    if (bubble_density > 0.01) {
        // Multi-stream staggered bubble columns
        for (int col_i = 0; col_i < 6; col_i++) {
            float col_seed = float(col_i) * 1.73;
            float col_x = frac(sin(col_seed * 12.9898) * 43758.5453);
            
            // Speed and size variation per bubble stream
            float b_speed = 0.18 + hash21(float2(col_seed, 1.2)) * 0.18;
            float b_size = (0.012 + hash21(float2(col_seed, 4.5)) * 0.018) * bubble_density;
            
            // Vertical movement with horizontal buoyancy wiggle
            float b_y = frac(1.0 - (elapsed_time * b_speed + col_seed * 0.3));
            float wiggle = sin(b_y * 22.0 + elapsed_time * 3.5) * (0.02 + b_size * 0.5);
            float2 b_pos = float2(col_x + wiggle, b_y);

            // Vector from current pixel to bubble center
            float2 b_delta = (distorted_uv - b_pos) * float2(1.0, 1.33); // slight aspect correction
            float dist = length(b_delta);

            if (dist < b_size) {
                float norm_dist = dist / b_size;
                float2 normal_2d = b_delta / (b_size + 0.0001);
                
                // Spherical refraction inside bubble
                float2 refract_uv = distorted_uv - normal_2d * (b_size * 0.85);
                float3 bubble_interior = image.Sample(textureSampler, refract_uv).rgb;
                
                // Fresnel membrane rim lighting
                float fresnel = pow(norm_dist, 3.0);
                float3 rim_color = float3(0.6, 0.95, 1.0) * fresnel * 1.8;
                
                // Top-left sun specular crescent glint
                float2 sun_dir = normalize(float2(-0.4, -0.6));
                float spec_dot = saturate(dot(-normal_2d, sun_dir));
                float specular = pow(spec_dot, 8.0) * 2.2;
                float3 spec_color = float3(1.0, 1.0, 1.0) * specular;
                
                float3 shaded_bubble = lerp(bubble_interior, rim_color, fresnel * 0.75) + spec_color;
                
                // Smooth anti-aliased edge blend
                float edge_alpha = smoothstep(b_size, b_size - 0.002, dist);
                submerged_col = lerp(submerged_col, shaded_bubble, edge_alpha * act);
            }
        }
    }

    float4 original = image.Sample(textureSampler, v_in.uv);
    return lerp(original, float4(submerged_col, col.a), act);
}
