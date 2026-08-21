// Rave Mode / Disco Strobe Shader for OBS Studio
// Designed for Twitch Hype Trains, celebration raids, and dance party redeems.
// Rainbow hue cycles, neon edge glow, strobe flashes, and audio-reactive beat pulses.

uniform float intensity<
    string name = "Intensity";
    string widget_type = "slider";
    float minimum = 0.0;
    float maximum = 1.0;
    float step = 0.01;
> = 1.0;

uniform float cycle_speed<
    string name = "Rainbow Speed";
    string widget_type = "slider";
    float minimum = 0.5;
    float maximum = 10.0;
    float step = 0.5;
> = 4.0;

uniform float neon_glow<
    string name = "Neon Edge Outline";
    string widget_type = "slider";
    float minimum = 0.0;
    float maximum = 2.0;
    float step = 0.1;
> = 1.2;

uniform float strobe_speed<
    string name = "Strobe Flash Speed";
    string widget_type = "slider";
    float minimum = 0.0;
    float maximum = 25.0;
    float step = 1.0;
> = 12.0;

uniform float audio_reactivity<
    string name = "Audio-Reactive Pulse";
    string widget_type = "slider";
    float minimum = 0.0;
    float maximum = 2.0;
    float step = 0.05;
> = 1.0;

uniform float audio_magnitude;
uniform float audio_peak;

float3 rainbow(float h) {
    float3 c = frac(h + float3(0.0, 2.0 / 3.0, 1.0 / 3.0));
    return saturate(abs(c * 6.0 - 3.0) - 1.0);
}

float4 mainImage(VertData v_in) : TARGET {
    float act = clamp(intensity, 0.0, 1.0);
    if (act <= 0.001) {
        return image.Sample(textureSampler, v_in.uv);
    }

    float2 uv = v_in.uv;
    float2 uv_pixel = uv_pixel_interval;

    // Audio beat pump
    float a_beat = (audio_magnitude * 1.5 + audio_peak * 1.0) * audio_reactivity;
    float speed = (cycle_speed + a_beat * 4.0) * elapsed_time;

    // Chromatic laser split on audio peaks
    float2 ca_offset = float2(sin(speed * 2.0), cos(speed * 2.0)) * (0.008 * (1.0 + a_beat) * act);
    float r = image.Sample(textureSampler, uv + ca_offset).r;
    float g = image.Sample(textureSampler, uv).g;
    float b = image.Sample(textureSampler, uv - ca_offset).b;
    float4 col = float4(r, g, b, 1.0);

    // Sobel edge detection for glowing neon wireframe
    float3 col_up    = image.Sample(textureSampler, uv + float2(0.0, -uv_pixel.y * 2.0)).rgb;
    float3 col_down  = image.Sample(textureSampler, uv + float2(0.0,  uv_pixel.y * 2.0)).rgb;
    float3 col_left  = image.Sample(textureSampler, uv + float2(-uv_pixel.x * 2.0, 0.0)).rgb;
    float3 col_right = image.Sample(textureSampler, uv + float2( uv_pixel.x * 2.0, 0.0)).rgb;
    float edge = length(col_up - col_down) + length(col_left - col_right);

    // Rainbow color cycle based on position and time
    float hue = frac(uv.y * 0.5 + uv.x * 0.3 + speed * 0.25);
    float3 neon_color = rainbow(hue);

    // Composite glowing neon outlines over source video
    float3 outline = neon_color * edge * neon_glow * (1.5 + a_beat * 1.2);
    float3 disco_col = col.rgb + outline;

    // Strobe flash pulse
    float strobe = 1.0;
    if (strobe_speed > 0.1) {
        float st_phase = sin(elapsed_time * strobe_speed);
        strobe = 0.8 + 0.2 * step(0.3, st_phase) + a_beat * 0.3;
    }
    disco_col *= strobe;

    float4 original = image.Sample(textureSampler, v_in.uv);
    return lerp(original, float4(disco_col, col.a), act);
}
