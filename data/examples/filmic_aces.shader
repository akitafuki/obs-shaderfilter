// Cinematic ACES Filmic Tonemapper & Color Grading Shader
// Applies the Academy Color Encoding System (ACES) filmic tone curve with exposure, contrast, and color temperature controls.

uniform float exposure<
    string label = "Exposure";
    string widget_type = "slider";
    float minimum = 0.1;
    float maximum = 4.0;
    float step = 0.05;
> = 1.0;

uniform float contrast<
    string label = "Contrast";
    string widget_type = "slider";
    float minimum = 0.5;
    float maximum = 2.0;
    float step = 0.05;
> = 1.05;

uniform float saturation<
    string label = "Saturation";
    string widget_type = "slider";
    float minimum = 0.0;
    float maximum = 2.0;
    float step = 0.05;
> = 1.1;

uniform float temperature<
    string label = "Color Temperature (Cool / Warm)";
    string widget_type = "slider";
    float minimum = -1.0;
    float maximum = 1.0;
    float step = 0.02;
> = 0.0;

uniform float tint<
    string label = "Tint (Green / Magenta)";
    string widget_type = "slider";
    float minimum = -1.0;
    float maximum = 1.0;
    float step = 0.02;
> = 0.0;

float3 adjust_white_balance(float3 col, float temp, float tnt)
{
    float3 balanced = col;
    balanced.r += temp * 0.12;
    balanced.b -= temp * 0.12;
    balanced.g -= tnt * 0.08;
    balanced.r += tnt * 0.04;
    balanced.b += tnt * 0.04;
    return max(float3(0.0, 0.0, 0.0), balanced);
}

float4 mainImage(VertData v_in) : TARGET
{
    float4 src = image.Sample(textureSampler, v_in.uv);
    if (src.a <= 0.0001)
        return src;

    // Convert sRGB non-linear to linear photometric color space
    float3 linear_col = srgb_nonlinear_to_linear(src.rgb);

    // Apply exposure
    linear_col *= exposure;

    // Apply white balance and tint
    linear_col = adjust_white_balance(linear_col, temperature, tint);

    // Apply ACES filmic tone curve
    float3 graded = tonemap_aces(linear_col);

    // Apply contrast in midtone-centered space
    graded = clamp((graded - 0.5) * contrast + 0.5, 0.0, 1.0);

    // Apply saturation adjustment
    float luma = dot(graded, float3(0.2126, 0.7152, 0.0722));
    graded = lerp(float3(luma, luma, luma), graded, saturation);

    // Convert back to gamma-corrected display space
    float3 final_rgb = srgb_linear_to_nonlinear(clamp(graded, 0.0, 1.0));

    return float4(final_rgb, src.a);
}
