# obs-shaderfilter

[![OBS Studio](https://img.shields.io/badge/OBS%20Studio-32.0%2B-blue.svg)](https://obsproject.com/)
[![Platform](https://img.shields.io/badge/platform-Windows%20%7C%20macOS%20%7C%20Linux-lightgrey.svg)](https://github.com/akitafuki/obs-shaderfilter)
[![License](https://img.shields.io/badge/license-GPL--2.0-green.svg)](LICENSE)
[![Shaders](https://img.shields.io/badge/bundled%20shaders-170%2B-orange.svg)](data/examples)

**obs-shaderfilter** is a high-performance plugin for [OBS Studio](https://obsproject.com/) that lets you apply custom HLSL/GLSL pixel shaders, vertex shaders, and transitions to any OBS source, scene, or audio capture in real time.

---

## ✨ Key Features

* **🎨 1-Click Preset Library**: Browse and preview over 170+ bundled shaders directly from a dropdown list inside OBS properties.
* **⚡ Live Hot-Reloading**: Automatically recompiles your shader whenever the `.shader` or `.effect` file is saved in your favorite code editor.
* **🎯 Accurate Error Diagnostics**: Compiler errors and warnings are mapped back to exact line numbers in your shader files for seamless debugging.
* **🌈 HDR & Wide Color Gamut Support**: Full compatibility with Rec. 709, Rec. 2100 PQ / HLG, and 16-bit float buffers, complete with built-in ACES filmic tonemapping and color transforms.
* **🎵 Audio Reactivity**: Synchronize visual effects to your microphone, music, or desktop audio with real-time `audio_peak` and `audio_magnitude` uniforms.
* **🔄 Multi-Pass Rendering**: Create advanced effects like bloom, multi-tap blurs, and feedback loops using the intermediate `pass_texture` ping-pong buffer.
* **🕹️ Shadertoy Converter**: 1-click conversion from GLSL Shadertoy code to OBS HLSL.
* **🎛️ Dynamic UI Controls**: Expose custom sliders, color pickers, drop-downs, and file selectors in OBS with simple `<annotation>` blocks.

---

## 📦 Installation

### Windows (64-bit)
1. Download the latest release from the [Releases](https://github.com/akitafuki/obs-shaderfilter/releases) page.
2. Extract the archive into your OBS Studio installation folder (typically `C:\Program Files\obs-studio\`).
3. Ensure `obs-shaderfilter.dll` is located at `obs-studio\obs-plugins\64bit\` and the `data` folder is at `obs-studio\data\obs-plugins\obs-shaderfilter\`.

### macOS (Apple Silicon & Intel)
1. Download the macOS installer package (`.pkg`) or zip file from the Releases page.
2. Extract and copy `obs-shaderfilter.plugin` to `~/Library/Application Support/obs-studio/plugins/`.

### Linux (Flatpak)
```bash
# Create target plugin directories
mkdir -p ~/.var/app/com.obsproject.Studio/config/obs-studio/plugins/obs-shaderfilter/bin/64bit
mkdir -p ~/.var/app/com.obsproject.Studio/config/obs-studio/plugins/obs-shaderfilter/data

# Copy binary and assets
cp build/obs-shaderfilter.so ~/.var/app/com.obsproject.Studio/config/obs-studio/plugins/obs-shaderfilter/bin/64bit/
cp -r data/* ~/.var/app/com.obsproject.Studio/config/obs-studio/plugins/obs-shaderfilter/data/
```

### Linux (System Install / Native)
```bash
# For user-specific install:
mkdir -p ~/.config/obs-studio/plugins/obs-shaderfilter/bin/64bit
mkdir -p ~/.config/obs-studio/plugins/obs-shaderfilter/data
cp build/obs-shaderfilter.so ~/.config/obs-studio/plugins/obs-shaderfilter/bin/64bit/
cp -r data/* ~/.config/obs-studio/plugins/obs-shaderfilter/data/
```

---

## 🚀 Quick Start & Usage

1. In OBS Studio, right-click on any source or scene and choose **Filters**.
2. Click **+** and select **User-defined shader**.
3. In the filter properties:
   - **Preset / Example Shaders**: Select from 170+ pre-made shaders in the dropdown menu.
   - **Load from file**: Enable this to load external `.shader` or `.effect` files.
   - **Auto reload on file change**: Check this box to enable instant live reloading when editing in VS Code or any text editor.
   - **Extra Pixels**: Add margin pixels on the left, right, top, or bottom for effects that expand beyond the original source boundaries (e.g. drop shadows, glows, borders).

---

## 🛠️ Shader Authoring Guide

OBS shaders are written in OBS's dialect of HLSL (Direct3D 11 style) with automatic cross-compilation to OpenGL GLSL on Linux and Metal on macOS.

### Exposing Custom UI Parameters in OBS

Declare variables with annotations to automatically generate customized sliders, dropdowns, and color pickers in OBS:

```hlsl
// Custom Slider
uniform float Speed<
    string label = "Animation Speed";
    string widget_type = "slider";
    float minimum = 0.0;
    float maximum = 5.0;
    float step = 0.1;
> = 1.0;

// Dropdown List (Select)
uniform int RenderMode<
    string label = "Render Mode";
    string widget_type = "select";
    int option_0_value = 0;
    string option_0_label = "Default";
    int option_1_value = 1;
    string option_1_label = "High Contrast";
    int option_2_value = 2;
    string option_2_label = "Monochrome";
> = 0;

// Color Picker (RGBA)
uniform float4 GlowColor<
    string label = "Glow Color";
> = {0.0, 0.8, 1.0, 1.0};

// Static Notes / Information
uniform string instructions<
    string widget_type = "info";
> = "Adjust the speed and glow color to match your scene.";
```

---

### Built-in Standard Uniforms

These uniforms are automatically provided by the template and updated each frame:

| Uniform | Type | Description |
| :--- | :--- | :--- |
| **`ViewProj`** | `float4x4` | Standard view/projection matrix. |
| **`image`** | `texture2d` | The input texture of the source or previous filter in the chain. |
| **`previous_image`** | `texture2d` | The source input texture from the prior rendered frame. |
| **`previous_output`** | `texture2d` | The output texture from the previous frame (useful for feedback/trails). |
| **`pass_texture`** | `texture2d` | The intermediate pre-pass texture for multi-pass effects. |
| **`elapsed_time`** | `float` | Continuous time in seconds since filter creation. |
| **`elapsed_time_start`** | `float` | Time in seconds elapsed since shader was loaded. |
| **`elapsed_time_show`** | `float` | Time in seconds elapsed since filter was made visible. |
| **`elapsed_time_active`**| `float` | Time in seconds elapsed since filter became active. |
| **`elapsed_time_enable`**| `float` | Time in seconds elapsed since filter was enabled. |
| **`delta_time`** | `float` | Exact delta time (seconds) since the previous frame. |
| **`frame_count`** | `int` | Monotonically increasing frame counter. |
| **`canvas_size`** | `float2` | Base OBS canvas dimensions `(width, height)`. |
| **`uv_size`** | `float2` | Source dimensions `(width, height)`. |
| **`uv_pixel_interval`** | `float2` | Texel size in UV coordinates `(1.0/width, 1.0/height)`. |
| **`uv_offset`** | `float2` | UV offset applied for extra border pixels. |
| **`uv_scale`** | `float2` | UV scale applied for extra border pixels. |
| **`audio_peak`** | `float` | Real-time instantaneous peak audio level (`0.0`–`1.0`). |
| **`audio_magnitude`** | `float` | Real-time RMS audio level (`0.0`–`1.0`). |
| **`color_space`** | `int` | OBS color space (`0: sRGB`, `1: sRGB 16F`, `2: 709 Extended`, `3: 2100 PQ`, `4: 2100 HLG`). |
| **`rand_f`** | `float` | Pseudo-random float (`0.0`–`1.0`) changing every frame. |
| **`rand_instance_f`** | `float` | Random float seeded once per filter instance. |
| **`rand_activation_f`**| `float` | Random float updated on filter activation/settings change. |

---

### Built-in Color & HDR Functions

Shaders automatically have access to photometric and color conversion helpers:

```hlsl
// Converts standard sRGB non-linear color to linear color
float3 srgb_nonlinear_to_linear(float3 c);

// Converts linear color back to standard sRGB gamma
float3 srgb_linear_to_nonlinear(float3 c);

// Applies standard ACES filmic tonemapping curve to high dynamic range color
float3 tonemap_aces(float3 hdr_color);

// Gamut coordinate conversions between Rec. 709 (sRGB) and Rec. 2020 (HDR)
float3 rec709_to_rec2020(float3 c);
float3 rec2020_to_rec709(float3 c);
```

---

### Preprocessing Directives

* **`#include "<file>"`**: Inserts shared shader functions or headers before compilation. Cyclic includes are automatically detected and prevented.
* **`#define NAME value`**: Standard text replacement macros.
* **`#define USE_PM_ALPHA 1`**: Disables internal pre-multiplied alpha normalization if your shader already performs its own alpha corrections.

---

## 🎨 Bundled Shader Library

Over 170+ shaders are included out of the box. Below is a featured selection:

| Shader | Category | Description | Preview / Demo |
| :--- | :--- | :--- | :--- |
| `filmic_aces.shader` | Color / Cinema | Hollywood-standard ACES filmic tone curve with exposure, contrast, temperature, and tint controls. | Built-in |
| `audio_neon_pulse.shader` | Audio Reactive | Neon edge glow contours synchronized in real-time to microphone or desktop audio. | Built-in |
| `anamorphic_streak.shader` | Lighting | Sci-fi anamorphic lens flare horizontal light streaks from bright highlights. | Built-in |
| `camera_shake.shader` | Camera / Motion | Realistic handheld camera sway and organic drift with automatic border zoom. | Built-in |
| `cyberpunk_hologram.shader` | Stylized | Futuristic hologram projection with scanline beams, chromatic drift, and glitches. | Built-in |
| `tilt_shift.shader` | Optics / Depth | Miniature toy diorama depth-of-field effect with adjustable focal band. | Built-in |
| `bloom.shader` | Lighting | Adds cinematic bloom and light diffusion to bright elements. | ![bloom](https://github.com/exeldro/obs-shaderfilter/assets/5457024/567e5dc4-ec20-42fa-a344-2be1e6516b01) |
| `bent-camera.shader` | 3D / Perspective | Curved perspective warp for webcam overlay angles. | ![bent-camera](https://github.com/exeldro/obs-shaderfilter/assets/5457024/5fb6fec8-fc1b-46eb-96aa-17ce37a7ca20) |
| `chromatic-aberration.shader` | Lens FX | RGB channel color fringing and lens distortion. | ![chromatic](https://github.com/exeldro/obs-shaderfilter/assets/5457024/ab99dc36-b9c2-405d-b9ca-3216866003fa) |
| `edge_detection.shader` | Stylized | Sobel edge detector with color invert and alpha support. | Built-in |
| `fire.shader` | Animated | Animated procedural fire with customizable speed and colors. | [YouTube Demo](https://youtu.be/jcTsC0zSNAs) |
| `gaussian-blur-advanced.shader` | Blur | Smooth multi-directional blur with strength and mask controls. | Built-in |
| `matrix.effect` | Glitch / Retro | Falling green digital rain and matrix code overlay. | ![matrix](https://github.com/exeldro/obs-shaderfilter/assets/5457024/79d2b028-4ea8-405d-a560-846f3ea78357) |
| `pixelation.shader` | Retro | Pixelates input video with customizable pixel block size. | ![pixel](https://github.com/exeldro/obs-shaderfilter/assets/5457024/88b9db62-9fc7-4a1a-b7a2-cf22355be390) |
| `rounded_stroke.shader` | Overlay | Rounds source corners with optional animated outer border strokes. | [YouTube Demo](https://youtu.be/J8mQIEKvWt0) |
| `scan_line.shader` | Retro / CRT | Old-school cathode ray tube scanlines with scrolling. | ![scanline](https://github.com/exeldro/obs-shaderfilter/assets/5457024/d5913e00-ff88-4276-8b12-e13305e5c2bd) |
| `spotlight.shader` | Lighting | Stationary or animated tracking spotlight. | ![spotlight](https://github.com/exeldro/obs-shaderfilter/assets/5457024/f9aebc02-4da5-4d30-b2f3-a9d9d7511f9f) |
| `vignetting.shader` | Lens FX | Smooth radial border darkening with inner and outer radius controls. | ![vignette](https://github.com/exeldro/obs-shaderfilter/assets/5457024/b7d32bb9-014d-4152-9be1-8bdb498121f0) |

---

## 🔨 Building from Source

### Prerequisites
* [CMake](https://cmake.org/) (version 3.20 or newer)
* [Ninja](https://ninja-build.org/) or platform build tool
* C/C++ compiler (GCC 10+, Clang 12+, or MSVC 2022)
* [libobs](https://github.com/obsproject/obs-studio) development headers

### Build Instructions

```bash
# 1. Configure CMake
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo

# 2. Compile the plugin
cmake --build build

# 3. Run automated shader test suite
python3 scripts/validate_shaders.py
```

---

## 📜 Credits & Acknowledgments

* **Original Plugin Creator**: [Charles Fettinger](https://github.com/Oncorporation)
* **Legacy Upgrades & Shaders**: [Exeldro](https://github.com/exeldro)
* **Modernized Fork & Enhancements**: Maintained by [akitafuki](https://github.com/akitafuki/obs-shaderfilter) with modular multi-file architecture, OBS 30 – 32+ support, categorized shader browser, resolution downscaling, frame rate throttling, live hot-reloading, multi-pass ping-pong rendering, and automated shader test suites.

## 📄 License

This project is licensed under the **GNU General Public License v2.0** - see the [LICENSE](LICENSE) file for details.
