#include "obs-shaderfilter.h"

float (*move_get_transition_filter)(obs_source_t *filter_from, obs_source_t **filter_to) = NULL;

const char *effect_template_begin = "\
uniform float4x4 ViewProj;\n\
uniform texture2d image;\n\
\n\
uniform float2 uv_offset;\n\
uniform float2 uv_scale;\n\
uniform float2 uv_pixel_interval;\n\
uniform float2 uv_size;\n\
uniform float rand_f;\n\
uniform float rand_instance_f;\n\
uniform float rand_activation_f;\n\
uniform float elapsed_time;\n\
uniform float elapsed_time_start;\n\
uniform float elapsed_time_show;\n\
uniform float elapsed_time_active;\n\
uniform float elapsed_time_enable;\n\
uniform int loops;\n\
uniform float loop_second;\n\
uniform float local_time;\n\
uniform float2 canvas_size;\n\
uniform float delta_time;\n\
uniform int frame_count;\n\
uniform int color_space;\n\
uniform float audio_peak;\n\
uniform float audio_magnitude;\n\
\n\
sampler_state textureSampler{\n\
	Filter = Linear;\n\
	AddressU = Border;\n\
	AddressV = Border;\n\
	BorderColor = 00000000;\n\
};\n\
\n\
struct VertData {\n\
	float4 pos : POSITION;\n\
	float2 uv : TEXCOORD0;\n\
};\n\
\n\
VertData mainTransform(VertData v_in)\n\
{\n\
	VertData vert_out;\n\
	vert_out.pos = mul(float4(v_in.pos.xyz, 1.0), ViewProj);\n\
	vert_out.uv = v_in.uv * uv_scale + uv_offset;\n\
	return vert_out;\n\
}\n\
\n\
float srgb_nonlinear_to_linear_channel(float u)\n\
{\n\
	return (u <= 0.04045) ? (u / 12.92) : pow((u + 0.055) / 1.055, 2.4);\n\
}\n\
\n\
float3 srgb_nonlinear_to_linear(float3 v)\n\
{\n\
	return float3(srgb_nonlinear_to_linear_channel(v.r),\n\
		      srgb_nonlinear_to_linear_channel(v.g),\n\
		      srgb_nonlinear_to_linear_channel(v.b));\n\
}\n\
\n\
float srgb_linear_to_nonlinear_channel(float u)\n\
{\n\
	return (u <= 0.0031308) ? (u * 12.92) : (1.055 * pow(u, 1.0 / 2.4) - 0.055);\n\
}\n\
\n\
float3 srgb_linear_to_nonlinear(float3 v)\n\
{\n\
	return float3(srgb_linear_to_nonlinear_channel(v.r),\n\
		      srgb_linear_to_nonlinear_channel(v.g),\n\
		      srgb_linear_to_nonlinear_channel(v.b));\n\
}\n\
\n\
float3 tonemap_aces(float3 x)\n\
{\n\
	float a = 2.51;\n\
	float b = 0.03;\n\
	float c = 2.43;\n\
	float d = 0.59;\n\
	float e = 0.14;\n\
	return clamp((x * (a * x + b)) / (x * (c * x + d) + e), 0.0, 1.0);\n\
}\n\
\n\
float3 rec709_to_rec2020(float3 c)\n\
{\n\
	return float3(\n\
		dot(c, float3(0.6274040, 0.3292820, 0.0433136)),\n\
		dot(c, float3(0.0690970, 0.9195400, 0.0113612)),\n\
		dot(c, float3(0.0163916, 0.0880132, 0.8955950))\n\
	);\n\
}\n\
\n\
float3 rec2020_to_rec709(float3 c)\n\
{\n\
	return float3(\n\
		dot(c, float3(1.6604910, -0.5876411, -0.0728499)),\n\
		dot(c, float3(-0.1245505, 1.1328999, -0.0083494)),\n\
		dot(c, float3(-0.0181508, -0.1005789, 1.1187297))\n\
	);\n\
}\n\
\n";

const char *effect_template_default_image_shader = "\n\
float4 mainImage(VertData v_in) : TARGET\n\
{\n\
	return image.Sample(textureSampler, v_in.uv);\n\
}\n\
";

const char *effect_template_default_transition_image_shader = "\n\
uniform texture2d image_a;\n\
uniform texture2d image_b;\n\
uniform float transition_time = 0.5;\n\
uniform bool convert_linear = true;\n\
\n\
float4 mainImage(VertData v_in) : TARGET\n\
{\n\
	float4 a_val = image_a.Sample(textureSampler, v_in.uv);\n\
	float4 b_val = image_b.Sample(textureSampler, v_in.uv);\n\
	float4 rgba = lerp(a_val, b_val, transition_time);\n\
	if (convert_linear)\n\
		rgba.rgb = srgb_nonlinear_to_linear(rgba.rgb);\n\
	return rgba;\n\
}\n\
";

const char *effect_template_end = "\n\
technique Draw\n\
{\n\
	pass\n\
	{\n\
		vertex_shader = mainTransform(v_in);\n\
		pixel_shader = mainImage(v_in);\n\
	}\n\
}\n";

const char *shader_filter_texture_file_filter = "Textures (*.bmp *.tga *.png *.jpeg *.jpg *.gif);;";

OBS_DECLARE_MODULE()
OBS_MODULE_USE_DEFAULT_LOCALE("obs-shaderfilter", "en-US")

bool obs_module_load(void) {
  blog(LOG_INFO, "[obs-shaderfilter] loaded version %s", PROJECT_VERSION);
  obs_register_source(&shader_filter);
  obs_register_source(&shader_transition);

  return true;
}

void obs_module_unload(void) {}

void obs_module_post_load(void) {
  if (obs_get_module("move-transition") == NULL)
    return;
  proc_handler_t *ph = obs_get_proc_handler();
  struct calldata cd;
  calldata_init(&cd);
  calldata_set_string(&cd, "filter_id", shader_filter.id);
  if (proc_handler_call(ph, "move_get_transition_filter_function", &cd)) {
    move_get_transition_filter = calldata_ptr(&cd, "callback");
  }
  calldata_free(&cd);
}
