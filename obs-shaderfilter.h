#pragma once

#include <obs-module.h>
#include <graphics/graphics.h>
#include <graphics/image-file.h>
#include <graphics/math-extra.h>

#include <util/base.h>
#include <util/dstr.h>
#include <util/darray.h>
#include <util/platform.h>
#include <util/threading.h>
#include <float.h>
#include <limits.h>
#include <stdio.h>
#include <time.h>
#include <string.h>
#include <math.h>
#include <sys/stat.h>

#ifdef _WIN32
#include <windows.h>
#else
#include <sys/time.h>
#endif

#include "version.h"

#define nullptr ((void *)0)
#define MIN_AUDIO_THRESHOLD -60.0f

extern float (*move_get_transition_filter)(obs_source_t *filter_from, obs_source_t **filter_to);

extern const char *effect_template_begin;
extern const char *effect_template_default_image_shader;
extern const char *effect_template_default_transition_image_shader;
extern const char *effect_template_end;
extern const char *shader_filter_texture_file_filter;

struct effect_param_data {
	struct dstr name;
	struct dstr display_name;
	struct dstr widget_type;
	struct dstr group;
	struct dstr path;
	DARRAY(int) option_values;
	DARRAY(struct dstr) option_labels;

	enum gs_shader_param_type type;
	gs_eparam_t *param;

	gs_image_file_t *image;
	gs_texrender_t *render;
	obs_weak_source_t *source;

	union {
		long long i;
		double f;
		char *string;
		struct vec2 vec2;
		struct vec3 vec3;
		struct vec4 vec4;
	} value;
	union {
		long long i;
		double f;
		char *string;
		struct vec2 vec2;
		struct vec3 vec3;
		struct vec4 vec4;
	} default_value;
	bool has_default;
	char *label;
	union {
		long long i;
		double f;
	} minimum;
	union {
		long long i;
		double f;
	} maximum;
	union {
		long long i;
		double f;
	} step;
};

struct shader_filter_data {
	obs_source_t *context;
	gs_effect_t *effect;
	gs_effect_t *output_effect;
	gs_vertbuffer_t *sprite_buffer;

	gs_texrender_t *input_texrender;
	gs_texrender_t *previous_input_texrender;
	gs_texrender_t *output_texrender;
	gs_texrender_t *previous_output_texrender;
	gs_texrender_t *intermediate_texrender;
	gs_eparam_t *param_output_image;
	gs_eparam_t *param_pass_texture;

	bool reload_effect;
	struct dstr last_path;
	bool last_from_file;
	bool transition;
	bool transitioning;
	bool prev_transitioning;

	bool use_pm_alpha;
	bool output_rendered;
	bool input_rendered;

	float shader_start_time;
	float shader_show_time;
	float shader_active_time;
	float shader_enable_time;
	bool enabled;
	bool use_template;

	gs_eparam_t *param_uv_offset;
	gs_eparam_t *param_uv_scale;
	gs_eparam_t *param_uv_pixel_interval;
	gs_eparam_t *param_uv_size;
	gs_eparam_t *param_current_time_ms;
	gs_eparam_t *param_current_time_sec;
	gs_eparam_t *param_current_time_min;
	gs_eparam_t *param_current_time_hour;
	gs_eparam_t *param_current_time_day_of_week;
	gs_eparam_t *param_current_time_day_of_month;
	gs_eparam_t *param_current_time_month;
	gs_eparam_t *param_current_time_day_of_year;
	gs_eparam_t *param_current_time_year;
	gs_eparam_t *param_elapsed_time;
	gs_eparam_t *param_elapsed_time_start;
	gs_eparam_t *param_elapsed_time_show;
	gs_eparam_t *param_elapsed_time_active;
	gs_eparam_t *param_elapsed_time_enable;
	gs_eparam_t *param_loops;
	gs_eparam_t *param_loop_second;
	gs_eparam_t *param_local_time;
	gs_eparam_t *param_rand_f;
	gs_eparam_t *param_rand_instance_f;
	gs_eparam_t *param_rand_activation_f;
	gs_eparam_t *param_image;
	gs_eparam_t *param_previous_image;
	gs_eparam_t *param_image_a;
	gs_eparam_t *param_image_b;
	gs_eparam_t *param_transition_time;
	gs_eparam_t *param_convert_linear;
	gs_eparam_t *param_previous_output;
	gs_eparam_t *param_audio_peak;
	gs_eparam_t *param_audio_magnitude;
	gs_eparam_t *param_canvas_size;
	gs_eparam_t *param_delta_time;
	gs_eparam_t *param_frame_count;
	gs_eparam_t *param_color_space;

	int expand_left;
	int expand_right;
	int expand_top;
	int expand_bottom;

	int total_width;
	int total_height;
	bool no_repeat;
	bool rendering;
	bool auto_reload;
	int64_t last_file_time;

	struct vec2 uv_offset;
	struct vec2 uv_scale;
	struct vec2 uv_pixel_interval;
	struct vec2 uv_size;
	struct vec2 canvas_size;
	float elapsed_time;
	float elapsed_time_loop;
	float delta_time;
	int frame_count;
	int color_space;
	int loops;
	float local_time;
	float rand_f;
	float rand_instance_f;
	float rand_activation_f;
	float audio_peak;
	float audio_magnitude;

	char *audio_source_name;
	obs_volmeter_t *volmeter;
	volatile long current_audio_peak;
	volatile long current_audio_magnitude;

	DARRAY(struct effect_param_data) stored_param_list;
};

/* --- Inlines / Helpers --- */
static inline void set_atomic_float(volatile long *target, float value)
{
	union {
		float f;
		long l;
	} u;
	u.f = value;
	os_atomic_set_long(target, u.l);
}

static inline float get_atomic_float(const volatile long *target)
{
	union {
		float f;
		long l;
	} u;
	u.l = os_atomic_load_long(target);
	return u.f;
}

static inline unsigned int rand_interval(unsigned int min, unsigned int max)
{
	unsigned int r;
	const unsigned int range = 1 + max - min;
	const unsigned int buckets = RAND_MAX / range;
	const unsigned int limit = buckets * range;

	do {
		r = rand();
	} while (r >= limit);

	return min + (r / buckets);
}

static inline float convert_db_to_linear(float db_value)
{
	if (db_value <= MIN_AUDIO_THRESHOLD || db_value > 0.0f)
		return 0.0f;

	return fmaxf(0.0f, fminf(1.0f, (db_value - MIN_AUDIO_THRESHOLD) / (-MIN_AUDIO_THRESHOLD)));
}

static inline int64_t get_file_mod_time(const char *path)
{
	if (!path || !*path)
		return 0;
	struct stat st;
	if (os_stat(path, &st) == 0)
		return (int64_t)st.st_mtime;
	return 0;
}

static inline uint32_t color_to_int(float r, float g, float b, float a)
{
	uint32_t ur = (uint32_t)(r < 0.0f ? 0 : (r > 1.0f ? 255 : (int)(r * 255.0f + 0.5f)));
	uint32_t ug = (uint32_t)(g < 0.0f ? 0 : (g > 1.0f ? 255 : (int)(g * 255.0f + 0.5f)));
	uint32_t ub = (uint32_t)(b < 0.0f ? 0 : (b > 1.0f ? 255 : (int)(b * 255.0f + 0.5f)));
	uint32_t ua = (uint32_t)(a < 0.0f ? 0 : (a > 1.0f ? 255 : (int)(a * 255.0f + 0.5f)));
	return (ua << 24) | (ub << 16) | (ug << 8) | ur;
}

static inline void int_to_color(uint32_t val, float *r, float *g, float *b, float *a)
{
	*r = (float)(val & 0xFF) / 255.0f;
	*g = (float)((val >> 8) & 0xFF) / 255.0f;
	*b = (float)((val >> 16) & 0xFF) / 255.0f;
	*a = (float)((val >> 24) & 0xFF) / 255.0f;
}

/* --- Loader Module (shader-loader.c) --- */
char *load_shader_from_file(const char *file_name);
void load_output_effect(struct shader_filter_data *filter);
void load_sprite_buffer(struct shader_filter_data *filter);
void shader_filter_reload_effect(struct shader_filter_data *filter);

/* --- Converter Module (shader-convert.c) --- */
bool shader_filter_convert(obs_properties_t *props, obs_property_t *property, void *data);

/* --- Params & UI Module (shader-params.c) --- */
void shader_filter_clear_params(struct shader_filter_data *filter);
void shader_filter_set_effect_params(struct shader_filter_data *filter);
void shader_filter_param_source_action(void *data, void (*action)(obs_source_t *source));
obs_properties_t *shader_filter_properties(void *data);
bool shader_filter_from_file_changed(obs_properties_t *props, obs_property_t *p, obs_data_t *settings);
bool shader_filter_text_changed(obs_properties_t *props, obs_property_t *p, obs_data_t *settings);
bool shader_filter_file_name_changed(obs_properties_t *props, obs_property_t *p, obs_data_t *settings);
bool shader_filter_reload_effect_clicked(obs_properties_t *props, obs_property_t *property, void *data);
bool add_source_to_list(void *data, obs_source_t *source);
obs_missing_files_t *shader_filter_missing_files(void *data);

/* Audio callbacks & helpers */
void shader_filter_audio_callback(void *data, const float magnitude[MAX_AUDIO_CHANNELS],
				  const float peak[MAX_AUDIO_CHANNELS], const float input_peak[MAX_AUDIO_CHANNELS]);
void shader_filter_cleanup_volmeter(struct shader_filter_data *filter);
bool shader_filter_enum_audio_sources(void *data, obs_source_t *source);

/* --- Filter Module (shader-filter.c) --- */
extern struct obs_source_info shader_filter;
gs_texrender_t *create_or_reset_texrender(gs_texrender_t *render);
void shader_filter_destroy(void *data);
void shader_filter_update(void *data, obs_data_t *settings);
void shader_filter_tick(void *data, float seconds);

/* --- Transition Module (shader-transition.c) --- */
extern struct obs_source_info shader_transition;
