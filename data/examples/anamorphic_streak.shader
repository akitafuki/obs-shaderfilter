// Cinematic Anamorphic Lens Flare & Streak Shader
// Extracts bright highlights and generates horizontal cinema-style light flares.

uniform float4 streak_color<
    string label = "Streak Tint";
> = {0.2, 0.6, 1.0, 1.0};

uniform float streak_length<
    string label = "Streak Length";
    string widget_type = "slider";
    float minimum = 10.0;
    float maximum = 200.0;
    float step = 5.0;
> = 60.0;

uniform float threshold<
    string label = "Highlight Threshold";
    string widget_type = "slider";
    float minimum = 0.5;
    float maximum = 1.0;
    float step = 0.02;
> = 0.82;

uniform float intensity<
    string label = "Streak Intensity";
    string widget_type = "slider";
    float minimum = 0.2;
    float maximum = 4.0;
    float step = 0.1;
> = 1.5;

float3 get_highlight(float2 uv)
{
    float4 col = image.Sample(textureSampler, uv);
    float luma = dot(col.rgb, float3(0.2126, 0.7152, 0.0722));
    float weight = max(0.0, luma - threshold) / max(0.001, 1.0 - threshold);
    return col.rgb * weight;
}

float4 mainImage(VertData v_in) : TARGET
{
    float4 src = image.Sample(textureSampler, v_in.uv);
    float2 px = uv_pixel_interval;

    float3 streak = float3(0.0, 0.0, 0.0);
    float total_weight = 0.0;

    // 8-tap bilateral exponential decay horizontal sampling
    [loop] for (int i = 1; i <= 8; i++) {
        float fi = float(i);
        float dist = fi * (streak_length * 0.125);
        float weight = exp(-0.35 * fi);

        float2 uv_r = v_in.uv + float2(dist * px.x, 0.0);
        float2 uv_l = v_in.uv - float2(dist * px.x, 0.0);

        streak += get_highlight(uv_r) * weight;
        streak += get_highlight(uv_l) * weight;
        total_weight += weight * 2.0;
    }

    streak += get_highlight(v_in.uv) * 1.5;
    total_weight += 1.5;

    streak = (streak / total_weight) * intensity * streak_color.rgb;

    // Soft additive composition with filmic shoulder compression
    float3 final_col = src.rgb + streak;

    return float4(final_col, src.a);
}
