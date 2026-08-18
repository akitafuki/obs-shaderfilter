// Tilt-Shift Miniature Depth of Field Shader
// Creates a miniature diorama effect using graduated directional blur outside a customizable focal band.

uniform float focus_position<
    string label = "Focus Position (Y)";
    string widget_type = "slider";
    float minimum = 0.0;
    float maximum = 1.0;
    float step = 0.02;
> = 0.5;

uniform float focus_width<
    string label = "Focus Band Width";
    string widget_type = "slider";
    float minimum = 0.05;
    float maximum = 0.8;
    float step = 0.02;
> = 0.2;

uniform float blur_amount<
    string label = "Blur Radius";
    string widget_type = "slider";
    float minimum = 1.0;
    float maximum = 25.0;
    float step = 1.0;
> = 10.0;

uniform float saturation_boost<
    string label = "Miniature Saturation Boost";
    string widget_type = "slider";
    float minimum = 1.0;
    float maximum = 2.0;
    float step = 0.05;
> = 1.25;

float4 mainImage(VertData v_in) : TARGET
{
    float dist = abs(v_in.uv.y - focus_position);
    float blur_factor = smoothstep(focus_width * 0.5, focus_width * 1.5, dist);

    float blur_step = blur_factor * blur_amount;
    float2 px = uv_pixel_interval * blur_step;

    float4 center = image.Sample(textureSampler, v_in.uv);
    if (blur_factor <= 0.001) {
        return center;
    }

    // 9-tap bilateral Poisson-style distribution
    float4 sum = center * 0.24;
    sum += image.Sample(textureSampler, v_in.uv + float2( 0.0,  1.0) * px) * 0.14;
    sum += image.Sample(textureSampler, v_in.uv + float2( 0.0, -1.0) * px) * 0.14;
    sum += image.Sample(textureSampler, v_in.uv + float2( 0.7,  0.7) * px) * 0.10;
    sum += image.Sample(textureSampler, v_in.uv + float2(-0.7,  0.7) * px) * 0.10;
    sum += image.Sample(textureSampler, v_in.uv + float2( 0.7, -0.7) * px) * 0.10;
    sum += image.Sample(textureSampler, v_in.uv + float2(-0.7, -0.7) * px) * 0.10;
    sum += image.Sample(textureSampler, v_in.uv + float2( 0.0,  2.0) * px) * 0.04;
    sum += image.Sample(textureSampler, v_in.uv + float2( 0.0, -2.0) * px) * 0.04;

    // Apply miniature saturation / toy vibrance boost
    float luma = dot(sum.rgb, float3(0.2126, 0.7152, 0.0722));
    sum.rgb = lerp(float3(luma, luma, luma), sum.rgb, saturation_boost);

    return sum;
}
