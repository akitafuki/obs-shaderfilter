// Handheld Camera Shake & Sway Shader
// Simulates natural handheld camera drift, sway, and organic motion with auto-zoom compensation.

uniform float shake_speed<
    string label = "Shake Speed";
    string widget_type = "slider";
    float minimum = 0.1;
    float maximum = 5.0;
    float step = 0.1;
> = 1.0;

uniform float shake_amount<
    string label = "Shake Intensity";
    string widget_type = "slider";
    float minimum = 0.0;
    float maximum = 50.0;
    float step = 1.0;
> = 12.0;

uniform float rotation_amount<
    string label = "Rotation Sway";
    string widget_type = "slider";
    float minimum = 0.0;
    float maximum = 5.0;
    float step = 0.1;
> = 0.8;

uniform float zoom_compensation<
    string label = "Border Zoom Compensation (%)";
    string widget_type = "slider";
    float minimum = 0.0;
    float maximum = 20.0;
    float step = 1.0;
> = 5.0;

float2 rotate_uv(float2 uv, float angle, float2 center)
{
    float s = sin(angle);
    float c = cos(angle);
    float2 p = uv - center;
    return float2(p.x * c - p.y * s, p.x * s + p.y * c) + center;
}

float4 mainImage(VertData v_in) : TARGET
{
    float t = elapsed_time * shake_speed;

    // Multi-frequency harmonic motion
    float offset_x = sin(t * 1.7) * 0.5 + sin(t * 3.3) * 0.3 + sin(t * 7.1) * 0.2;
    float offset_y = cos(t * 1.3) * 0.5 + cos(t * 2.9) * 0.3 + cos(t * 6.7) * 0.2;
    float rot = (sin(t * 1.1) * 0.6 + cos(t * 2.3) * 0.4) * (rotation_amount * 0.01745); // convert to radians

    float2 displacement = float2(offset_x, offset_y) * (shake_amount * uv_pixel_interval);

    // Apply auto zoom scaling so borders never expose out-of-frame gaps
    float zoom_scale = 1.0 + (zoom_compensation * 0.01);
    float2 centered_uv = (v_in.uv - 0.5) / zoom_scale + 0.5;

    // Apply translation displacement
    float2 shifted_uv = centered_uv + displacement;

    // Apply rotational sway around center
    float2 final_uv = rotate_uv(shifted_uv, rot, float2(0.5, 0.5));

    return image.Sample(textureSampler, final_uv);
}
