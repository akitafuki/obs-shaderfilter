#include "obs-shaderfilter.h"

static void *shader_transition_create(obs_data_t *settings, obs_source_t *source)
{
	struct shader_filter_data *filter = bzalloc(sizeof(struct shader_filter_data));
	filter->context = source;
	filter->reload_effect = true;
	filter->transition = true;

	dstr_init(&filter->last_path);
	dstr_copy(&filter->last_path, obs_data_get_string(settings, "shader_file_name"));
	filter->last_from_file = obs_data_get_bool(settings, "from_file");
	filter->rand_instance_f = (float)((double)rand_interval(0, 10000) / (double)10000);
	filter->rand_activation_f = (float)((double)rand_interval(0, 10000) / (double)10000);

	da_init(filter->stored_param_list);

	obs_source_update(source, settings);

	return filter;
}

static const char *shader_transition_get_name(void *unused)
{
	UNUSED_PARAMETER(unused);
	return obs_module_text("ShaderFilter");
}

static float mix_a(void *data, float t)
{
	UNUSED_PARAMETER(data);
	return 1.0f - t;
}

static float mix_b(void *data, float t)
{
	UNUSED_PARAMETER(data);
	return t;
}

static bool shader_transition_audio_render(void *data, uint64_t *ts_out, struct obs_source_audio_mix *audio, uint32_t mixers,
					   size_t channels, size_t sample_rate)
{
	struct shader_filter_data *filter = data;
	return obs_transition_audio_render(filter->context, ts_out, audio, mixers, channels, sample_rate, mix_a, mix_b);
}

static void shader_transition_video_callback(void *data, gs_texture_t *a, gs_texture_t *b, float t, uint32_t cx, uint32_t cy)
{
	if (!a && !b)
		return;

	struct shader_filter_data *filter = data;
	if (filter->effect == NULL || filter->rendering)
		return;

	if (!filter->prev_transitioning) {
		if (obs_source_active(filter->context))
			shader_filter_param_source_action(data, obs_source_inc_active);
		if (obs_source_showing(filter->context))
			shader_filter_param_source_action(data, obs_source_inc_showing);
	}
	filter->transitioning = true;

	const bool previous = gs_framebuffer_srgb_enabled();
	gs_enable_framebuffer_srgb(true);

	if (gs_get_color_space() == GS_CS_SRGB) {
		if (filter->param_image_a != NULL)
			gs_effect_set_texture(filter->param_image_a, a);
		if (filter->param_image_b != NULL)
			gs_effect_set_texture(filter->param_image_b, b);
		if (filter->param_image != NULL)
			gs_effect_set_texture(filter->param_image, t < 0.5 ? a : b);
		if (filter->param_convert_linear)
			gs_effect_set_bool(filter->param_convert_linear, true);
	} else {
		if (filter->param_image_a != NULL)
			gs_effect_set_texture_srgb(filter->param_image_a, a);
		if (filter->param_image_b != NULL)
			gs_effect_set_texture_srgb(filter->param_image_b, b);
		if (filter->param_image != NULL)
			gs_effect_set_texture_srgb(filter->param_image, t < 0.5 ? a : b);
		if (filter->param_convert_linear)
			gs_effect_set_bool(filter->param_convert_linear, false);
	}
	if (filter->param_transition_time != NULL)
		gs_effect_set_float(filter->param_transition_time, t);

	shader_filter_set_effect_params(filter);

	while (gs_effect_loop(filter->effect, "Draw"))
		gs_draw_sprite(NULL, 0, cx, cy);

	gs_enable_framebuffer_srgb(previous);
}

static void shader_transition_video_render(void *data, gs_effect_t *effect)
{
	UNUSED_PARAMETER(effect);

	const bool previous = gs_set_linear_srgb(true);

	struct shader_filter_data *filter = data;
	filter->transitioning = false;
	obs_transition_video_render2(filter->context, shader_transition_video_callback, NULL);
	if (!filter->transitioning && filter->prev_transitioning) {
		if (obs_source_active(filter->context))
			shader_filter_param_source_action(data, obs_source_dec_active);
		if (obs_source_showing(filter->context))
			shader_filter_param_source_action(data, obs_source_dec_showing);
	}
	filter->prev_transitioning = filter->transitioning;
	gs_set_linear_srgb(previous);
}

static void shader_transition_defaults(obs_data_t *settings)
{
	obs_data_set_default_string(settings, "shader_text", effect_template_default_transition_image_shader);
}

static enum gs_color_space shader_transition_get_color_space(void *data, size_t count, const enum gs_color_space *preferred_spaces)
{
	struct shader_filter_data *filter = data;
	const enum gs_color_space transition_space = obs_transition_video_get_color_space(filter->context);

	enum gs_color_space space = transition_space;
	for (size_t i = 0; i < count; ++i) {
		space = preferred_spaces[i];
		if (space == transition_space)
			break;
	}

	return space;
}

struct obs_source_info shader_transition = {
	.id = "shader_transition",
	.type = OBS_SOURCE_TYPE_TRANSITION,
	.create = shader_transition_create,
	.destroy = shader_filter_destroy,
	.update = shader_filter_update,
	.load = shader_filter_update,
	.video_tick = shader_filter_tick,
	.get_name = shader_transition_get_name,
	.audio_render = shader_transition_audio_render,
	.get_defaults = shader_transition_defaults,
	.video_render = shader_transition_video_render,
	.get_properties = shader_filter_properties,
	.video_get_color_space = shader_transition_get_color_space,
	.missing_files = shader_filter_missing_files,
};
