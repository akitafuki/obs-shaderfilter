#include <dirent.h>
#include <graphics/graphics.h>
#include <obs.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <util/dstr.h>
#include <util/platform.h>

const char *template_begin =
    "#define OPENGL 1\n"
    "uniform float4x4 ViewProj;\n"
    "uniform texture2d image;\n"
    "uniform float2 uv_offset;\n"
    "uniform float2 uv_scale;\n"
    "uniform float2 uv_pixel_interval;\n"
    "uniform float2 uv_size;\n"
    "uniform float rand_f;\n"
    "uniform float rand_instance_f;\n"
    "uniform float rand_activation_f;\n"
    "uniform float elapsed_time;\n"
    "uniform float elapsed_time_start;\n"
    "uniform float elapsed_time_show;\n"
    "uniform float elapsed_time_active;\n"
    "uniform float elapsed_time_enable;\n"
    "uniform int loops;\n"
    "uniform float loop_second;\n"
    "uniform float local_time;\n"
    "uniform float2 canvas_size;\n"
    "uniform float delta_time;\n"
    "uniform int frame_count;\n"
    "uniform int color_space;\n"
    "uniform float audio_peak;\n"
    "uniform float audio_magnitude;\n"
    "sampler_state textureSampler{\n"
    "	Filter = Linear;\n"
    "	AddressU = Border;\n"
    "	AddressV = Border;\n"
    "	BorderColor = 00000000;\n"
    "};\n"
    "struct VertData {\n"
    "	float4 pos : POSITION;\n"
    "	float2 uv : TEXCOORD0;\n"
    "};\n"
    "VertData mainTransform(VertData v_in)\n"
    "{\n"
    "	VertData vert_out;\n"
    "	vert_out.pos = mul(float4(v_in.pos.xyz, 1.0), ViewProj);\n"
    "	vert_out.uv = v_in.uv * uv_scale + uv_offset;\n"
    "	return vert_out;\n"
    "}\n"
    "float srgb_nonlinear_to_linear_channel(float u) { return (u <= 0.04045) ? (u / 12.92) : pow((u + 0.055) / 1.055, 2.4); }\n"
    "float3 srgb_nonlinear_to_linear(float3 v) { return float3(srgb_nonlinear_to_linear_channel(v.r), "
    "srgb_nonlinear_to_linear_channel(v.g), srgb_nonlinear_to_linear_channel(v.b)); }\n"
    "float srgb_linear_to_nonlinear_channel(float u) { return (u <= 0.0031308) ? (u * 12.92) : (1.055 * pow(u, 1.0 / 2.4) - "
    "0.055); }\n"
    "float3 srgb_linear_to_nonlinear(float3 v) { return float3(srgb_linear_to_nonlinear_channel(v.r), "
    "srgb_linear_to_nonlinear_channel(v.g), srgb_linear_to_nonlinear_channel(v.b)); }\n"
    "float3 tonemap_aces(float3 x) { float a = 2.51; float b = 0.03; float c = 2.43; float d = 0.59; float e = 0.14; return "
    "clamp((x * (a * x + b)) / (x * (c * x + d) + e), 0.0, 1.0); }\n"
    "float3 rec709_to_rec2020(float3 c) { return float3(dot(c, float3(0.6274040, 0.3292820, 0.0433136)), dot(c, float3(0.0690970, "
    "0.9195400, 0.0113612)), dot(c, float3(0.0163916, 0.0880132, 0.8955950))); }\n"
    "float3 rec2020_to_rec709(float3 c) { return float3(dot(c, float3(1.6604910, -0.5876411, -0.0728499)), dot(c, "
    "float3(-0.1245505, 1.1328999, -0.0083494)), dot(c, float3(-0.0181508, -0.1005789, 1.1187297))); }\n";

const char *template_end = "\ntechnique Draw\n"
                           "{\n"
                           "	pass\n"
                           "	{\n"
                           "		vertex_shader = mainTransform(v_in);\n"
                           "		pixel_shader = mainImage(v_in);\n"
                           "	}\n"
                           "}\n";

static void test_directory(const char *dir_name, int *total, int *passed, int *failed) {
  DIR *d = opendir(dir_name);
  if (!d)
    return;
  struct dirent *dir;
  while ((dir = readdir(d)) != NULL) {
    if (dir->d_name[0] == '.')
      continue;
    char *ext = strrchr(dir->d_name, '.');
    if (!ext || (strcmp(ext, ".shader") != 0 && strcmp(ext, ".effect") != 0))
      continue;

    char path[512];
    snprintf(path, sizeof(path), "%s/%s", dir_name, dir->d_name);

    char *file_text = os_quick_read_utf8_file(path);
    if (!file_text)
      continue;
    (*total)++;

    struct dstr effect_text = {0};
    bool is_effect = (strcmp(ext, ".effect") == 0);
    if (!is_effect) {
      dstr_copy(&effect_text, template_begin);
      dstr_cat(&effect_text, file_text);
      dstr_cat(&effect_text, template_end);
    } else {
      dstr_copy(&effect_text, "#define OPENGL 1\n");
      dstr_cat(&effect_text, file_text);
    }
    dstr_replace(&effect_text, "[loop]", "");

    char *errors = NULL;
    gs_effect_t *eff = gs_effect_create(effect_text.array, dir->d_name, &errors);
    if (!eff) {
      printf("\n❌ FAILED [%s/%s]:\n%s\n", dir_name, dir->d_name, errors ? errors : "Unknown error");
      if (errors)
        bfree(errors);
      (*failed)++;
    } else {
      printf("  ✅ %s/%s\n", dir_name, dir->d_name);
      gs_effect_destroy(eff);
      (*passed)++;
    }
    dstr_free(&effect_text);
    bfree(file_text);
  }
  closedir(d);
}

int main() {
  if (!obs_startup("en-US", NULL, NULL))
    return 1;
  struct obs_video_info ovi = {0};
  ovi.graphics_module = "libobs-opengl";
  ovi.base_width = 1920;
  ovi.base_height = 1080;
  ovi.output_width = 1920;
  ovi.output_height = 1080;
  ovi.fps_num = 60;
  ovi.fps_den = 1;
  ovi.output_format = VIDEO_FORMAT_RGBA;
  ovi.adapter = 0;
  ovi.colorspace = VIDEO_CS_SRGB;
  ovi.range = VIDEO_RANGE_FULL;
  if (obs_reset_video(&ovi) != OBS_VIDEO_SUCCESS)
    return 1;

  int total = 0, passed = 0, failed = 0;
  obs_enter_graphics();
  printf("\n=== Scanning data/examples ===\n");
  test_directory("data/examples", &total, &passed, &failed);
  printf("\n=== Scanning data/internal ===\n");
  test_directory("data/internal", &total, &passed, &failed);
  obs_leave_graphics();

  printf("\n============================================\n");
  printf("Total Shaders Scanned: %d | Passed: %d | Failed: %d\n", total, passed, failed);
  printf("============================================\n");
  obs_shutdown();
  return failed > 0 ? 1 : 0;
}
