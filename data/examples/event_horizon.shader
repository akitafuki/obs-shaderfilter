// Event Horizon / Black Hole Vortex Shader for OBS Studio
// Designed for Twitch "Shadow Realm", defeat, or dramatic vortex redeems.
// Relativistic gravitational lensing, intense spiral spacetime distortion, and swirling accretion disk.

uniform float intensity<
    string name = "Intensity";
    string widget_type = "slider";
    float minimum = 0.0;
    float maximum = 1.0;
    float step = 0.01;
> = 1.0;

uniform float black_hole_radius<
    string name = "Event Horizon Size";
    string widget_type = "slider";
    float minimum = 0.02;
    float maximum = 0.35;
    float step = 0.01;
> = 0.14;

uniform float swirl_strength<
    string name = "Vortex Swirl Speed";
    string widget_type = "slider";
    float minimum = 0.0;
    float maximum = 12.0;
    float step = 0.2;
> = 5.0;

uniform float lensing_pull<
    string name = "Gravitational Lensing Pull";
    string widget_type = "slider";
    float minimum = 0.0;
    float maximum = 3.0;
    float step = 0.1;
> = 1.5;

uniform float2 vortex_center<
    string name = "Vortex Center";
> = {0.5, 0.5};

uniform float audio_reactivity<
    string name = "Audio-Reactive Beat Pump";
    string widget_type = "slider";
    float minimum = 0.0;
    float maximum = 2.0;
    float step = 0.05;
> = 1.0;

uniform float audio_magnitude;
uniform float audio_peak;

float4 mainImage(VertData v_in) : TARGET {
    float act = clamp(intensity, 0.0, 1.0);
    if (act <= 0.001) {
        return image.Sample(textureSampler, v_in.uv);
    }

    float2 uv = v_in.uv;
    float2 p = uv - vortex_center;
    float r = length(p);

    // Audio beat pulse
    float a_beat = (audio_magnitude * 1.2 + audio_peak * 0.8) * audio_reactivity;
    float bh_r = (black_hole_radius + a_beat * 0.04) * act;

    // Relativistic Spacetime Swirl Distortion
    float theta = atan2(p.y, p.x);
    float swirl_falloff = exp(-r * 3.5);
    float swirl_rot = (swirl_strength * swirl_falloff * 2.5 + elapsed_time * 1.8) * act;
    float distorted_theta = theta + swirl_rot;

    // Gravitational Lensing Pull (Einstein deflection)
    float pull_amount = (lensing_pull * bh_r * 0.6) / (r * 1.8 + 0.08) * act;
    float distorted_r = r + pull_amount * r;

    // Distorted UV Coordinates
    float2 distorted_p = float2(cos(distorted_theta), sin(distorted_theta)) * distorted_r;
    float2 distorted_uv = vortex_center + distorted_p;

    // Sample warped scene
    float4 col = image.Sample(textureSampler, saturate(distorted_uv));

    // Relativistic Swirling Accretion Disk (Fiery plasma glow)
    float disk_radius = bh_r * 1.45;
    float disk_dist = abs(r - disk_radius);
    float disk_glow = smoothstep(bh_r * 0.6, 0.0, disk_dist) * act;
    
    // Relativistic Doppler beaming (brighter on one side of rotation)
    float doppler = 0.7 + 0.3 * cos(distorted_theta);
    float3 fire_plasma = lerp(float3(1.0, 0.25, 0.02), float3(0.3, 0.7, 1.0), sin(distorted_theta * 2.0 + elapsed_time * 3.0) * 0.5 + 0.5);
    col.rgb += fire_plasma * (disk_glow * doppler * 2.8);

    // Event Horizon (Total black void inside Schwarzschild radius with sharp photon ring rim)
    float horizon_mask = smoothstep(bh_r * 0.92, bh_r * 1.04, r);
    float photon_ring = smoothstep(bh_r * 0.08, 0.0, abs(r - bh_r * 1.02)) * act;
    col.rgb = col.rgb * horizon_mask + float3(1.0, 0.95, 0.8) * (photon_ring * 1.5);

    return col;
}
