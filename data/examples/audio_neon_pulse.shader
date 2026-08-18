// Audio-Reactive Neon Edge Glow Shader
// Detects subject contours and generates glowing neon outlines pulsing to audio volume / beat.

uniform float4 neon_color<
    string label = "Neon Color";
> = {0.0, 0.9, 1.0, 1.0};

uniform float sensitivity<
    string label = "Audio Sensitivity";
    string widget_type = "slider";
    float minimum = 0.5;
    float maximum = 5.0;
    float step = 0.1;
> = 2.0;

uniform float edge_thickness<
    string label = "Edge Thickness";
    string widget_type = "slider";
    float minimum = 0.5;
    float maximum = 5.0;
    float step = 0.25;
> = 1.5;

uniform float edge_threshold<
    string label = "Edge Threshold";
    string widget_type = "slider";
    float minimum = 0.05;
    float maximum = 1.0;
    float step = 0.05;
> = 0.25;

uniform float glow_intensity<
    string label = "Glow Intensity";
    string widget_type = "slider";
    float minimum = 0.5;
    float maximum = 4.0;
    float step = 0.1;
> = 1.8;

uniform float base_mix<
    string label = "Source Video Visibility";
    string widget_type = "slider";
    float minimum = 0.0;
    float maximum = 1.0;
    float step = 0.05;
> = 0.85;

uniform bool cycle_colors<
    string label = "Cycle Rainbow Colors";
> = false;

float get_luma(float2 uv)
{
    float4 col = image.Sample(textureSampler, uv);
    return dot(col.rgb, float3(0.299, 0.587, 0.114));
}

float4 mainImage(VertData v_in) : TARGET
{
    float4 src = image.Sample(textureSampler, v_in.uv);
    float2 offset = uv_pixel_interval * edge_thickness;

    // Sobel edge filter kernels
    float l00 = get_luma(v_in.uv + float2(-offset.x, -offset.y));
    float l01 = get_luma(v_in.uv + float2(0.0,       -offset.y));
    float l02 = get_luma(v_in.uv + float2( offset.x, -offset.y));
    float l10 = get_luma(v_in.uv + float2(-offset.x,  0.0));
    float l12 = get_luma(v_in.uv + float2( offset.x,  0.0));
    float l20 = get_luma(v_in.uv + float2(-offset.x,  offset.y));
    float l21 = get_luma(v_in.uv + float2(0.0,        offset.y));
    float l22 = get_luma(v_in.uv + float2( offset.x,  offset.y));

    float gx = (l02 + 2.0 * l12 + l22) - (l00 + 2.0 * l10 + l20);
    float gy = (l20 + 2.0 * l21 + l22) - (l00 + 2.0 * l01 + l02);
    float edge = sqrt(gx * gx + gy * gy);

    // Threshold edge response
    edge = smoothstep(edge_threshold, edge_threshold + 0.3, edge);

    // Audio reactivity: peak for spikes + magnitude for baseline presence
    float audio_level = max(audio_peak, audio_magnitude * 1.5) * sensitivity;
    audio_level = clamp(audio_level, 0.05, 3.0);

    // Color generation
    float3 glow_col = neon_color.rgb;
    if (cycle_colors) {
        float hue = frac(elapsed_time * 0.2 + v_in.uv.x * 0.3);
        float3 rainbow = 0.5 + 0.5 * cos(6.28318 * (hue + float3(0.0, 0.33, 0.67)));
        glow_col = rainbow;
    }

    // Compose final glow and additive blend
    float3 edge_glow = glow_col * edge * glow_intensity * audio_level;
    float3 result = src.rgb * base_mix + edge_glow;

    return float4(result, src.a);
}
