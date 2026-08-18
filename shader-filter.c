#include "obs-shaderfilter.h"

const char *shader_filter_get_name(void *unused)
{
	UNUSED_PARAMETER(unused);
	return obs_module_text("ShaderFilter");
}

void *shader_filter_create(obs_data_t *settings, obs_source_t *source)
{
	struct shader_filter_data *filter = bzalloc(sizeof(struct shader_filter_data));
	filter->context = source;
	filter->reload_effect = true;

	dstr_init(&filter->last_path);
	dstr_copy(&filter->last_path, obs_data_get_string(settings, "shader_file_name"));
	filter->last_from_file = obs_data_get_bool(settings, "from_file");
	filter->rand_instance_f = (float)((double)rand_interval(0, 10000) / (double)10000);
	filter->rand_activation_f = (float)((double)rand_interval(0, 10000) / (double)10000);

	da_init(filter->stored_param_list);
	load_output_effect(filter);
	obs_source_update(source, settings);

	return filter;
}

void shader_filter_destroy(void *data)
{
	struct shader_filter_data *filter = data;
	shader_filter_clear_params(filter);

	obs_enter_graphics();
	if (filter->effect)
		gs_effect_destroy(filter->effect);
	if (filter->output_effect)
		gs_effect_destroy(filter->output_effect);
	if (filter->input_texrender)
		gs_texrender_destroy(filter->input_texrender);
	if (filter->output_texrender)
		gs_texrender_destroy(filter->output_texrender);
	if (filter->previous_input_texrender)
		gs_texrender_destroy(filter->previous_input_texrender);
	if (filter->previous_output_texrender)
		gs_texrender_destroy(filter->previous_output_texrender);
	if (filter->intermediate_texrender)
		gs_texrender_destroy(filter->intermediate_texrender);
	if (filter->sprite_buffer)
		gs_vertexbuffer_destroy(filter->sprite_buffer);
	obs_leave_graphics();

	dstr_free(&filter->last_path);
	da_free(filter->stored_param_list);

	shader_filter_cleanup_volmeter(filter);

	bfree(filter);
}

void shader_filter_update(void *data, obs_data_t *settings)
{
	struct shader_filter_data *filter = data;

	// Get expansions. Will be used in the video_tick() callback.
	filter->expand_left = (int)obs_data_get_int(settings, "expand_left");
	filter->expand_right = (int)obs_data_get_int(settings, "expand_right");
	filter->expand_top = (int)obs_data_get_int(settings, "expand_top");
	filter->expand_bottom = (int)obs_data_get_int(settings, "expand_bottom");
	filter->rand_activation_f = (float)((double)rand_interval(0, 10000) / (double)10000);
	filter->auto_reload = obs_data_get_bool(settings, "auto_reload");

	if (filter->reload_effect) {
		filter->reload_effect = false;
		shader_filter_reload_effect(filter);
		obs_source_update_properties(filter->context);
	}

	if (filter->param_audio_magnitude || filter->param_audio_peak) {
		const char *audio_source_name = obs_data_get_string(settings, "audio_source");
		if (filter->audio_source_name == NULL || strcmp(filter->audio_source_name, audio_source_name) != 0) {
			obs_source_t *audio_source = obs_get_source_by_name(audio_source_name);
			if (audio_source && ((obs_source_get_output_flags(audio_source) & OBS_SOURCE_AUDIO) == 0)) {
				obs_source_release(audio_source);
				audio_source = NULL;
			}
			if (audio_source) {
				if (filter->audio_source_name)
					bfree(filter->audio_source_name);
				filter->audio_source_name = bstrdup(audio_source_name);
			}

			if (!audio_source) {
				audio_source = obs_source_get_ref(obs_filter_get_parent(filter->context));
				if (audio_source && ((obs_source_get_output_flags(audio_source) & OBS_SOURCE_AUDIO) == 0)) {
					obs_source_release(audio_source);
					audio_source = NULL;
				}
			}
			if (audio_source) {
				if (!filter->volmeter) {
					filter->volmeter = obs_volmeter_create(OBS_FADER_LOG);
					obs_volmeter_add_callback(filter->volmeter, shader_filter_audio_callback, filter);
				}
				obs_volmeter_attach_source(filter->volmeter, audio_source);
				obs_source_release(audio_source);
			} else {
				shader_filter_cleanup_volmeter(filter);
			}
		}
	} else {
		shader_filter_cleanup_volmeter(filter);
	}

	size_t param_count = filter->stored_param_list.num;
	for (size_t param_index = 0; param_index < param_count; param_index++) {
		struct effect_param_data *param = (filter->stored_param_list.array + param_index);
		const char *param_name = param->name.array;
		struct dstr sources_name = {0};
		obs_source_t *source = NULL;

		void *default_value = gs_effect_get_default_val(param->param);

		switch (param->type) {
		case GS_SHADER_PARAM_BOOL:
			if (default_value != NULL) {
				obs_data_set_default_bool(settings, param_name, *(bool *)default_value);
				param->has_default = true;
			}
			param->value.i = obs_data_get_bool(settings, param_name);
			break;
		case GS_SHADER_PARAM_FLOAT:
			if (default_value != NULL) {
				obs_data_set_default_double(settings, param_name, *(float *)default_value);
				param->default_value.f = *(float *)default_value;
				param->has_default = true;
			}
			param->value.f = (float)obs_data_get_double(settings, param_name);
			break;
		case GS_SHADER_PARAM_INT:
			if (default_value != NULL) {
				obs_data_set_default_int(settings, param_name, *(int *)default_value);
				param->default_value.i = *(int *)default_value;
				param->has_default = true;
			}
			param->value.i = (int)obs_data_get_int(settings, param_name);
			break;
		case GS_SHADER_PARAM_VEC2:
			if (default_value != NULL) {
				struct vec2 *v2 = default_value;
				dstr_cat(&sources_name, param_name);
				dstr_cat(&sources_name, ".x");
				obs_data_set_default_double(settings, sources_name.array, v2->x);
				dstr_free(&sources_name);
				dstr_cat(&sources_name, param_name);
				dstr_cat(&sources_name, ".y");
				obs_data_set_default_double(settings, sources_name.array, v2->y);
				dstr_free(&sources_name);
				param->default_value.vec2.x = v2->x;
				param->default_value.vec2.y = v2->y;
				param->has_default = true;
			}
			dstr_cat(&sources_name, param_name);
			dstr_cat(&sources_name, ".x");
			param->value.vec2.x = (float)obs_data_get_double(settings, sources_name.array);
			dstr_free(&sources_name);
			dstr_cat(&sources_name, param_name);
			dstr_cat(&sources_name, ".y");
			param->value.vec2.y = (float)obs_data_get_double(settings, sources_name.array);
			dstr_free(&sources_name);
			break;
		case GS_SHADER_PARAM_VEC3:
			if (default_value != NULL) {
				struct vec3 *v3 = default_value;
				dstr_cat(&sources_name, param_name);
				dstr_cat(&sources_name, ".x");
				obs_data_set_default_double(settings, sources_name.array, v3->x);
				dstr_free(&sources_name);
				dstr_cat(&sources_name, param_name);
				dstr_cat(&sources_name, ".y");
				obs_data_set_default_double(settings, sources_name.array, v3->y);
				dstr_free(&sources_name);
				dstr_cat(&sources_name, param_name);
				dstr_cat(&sources_name, ".z");
				obs_data_set_default_double(settings, sources_name.array, v3->z);
				dstr_free(&sources_name);
				param->default_value.vec3.x = v3->x;
				param->default_value.vec3.y = v3->y;
				param->default_value.vec3.z = v3->z;
				param->has_default = true;
			}
			dstr_cat(&sources_name, param_name);
			dstr_cat(&sources_name, ".x");
			param->value.vec3.x = (float)obs_data_get_double(settings, sources_name.array);
			dstr_free(&sources_name);
			dstr_cat(&sources_name, param_name);
			dstr_cat(&sources_name, ".y");
			param->value.vec3.y = (float)obs_data_get_double(settings, sources_name.array);
			dstr_free(&sources_name);
			dstr_cat(&sources_name, param_name);
			dstr_cat(&sources_name, ".z");
			param->value.vec3.z = (float)obs_data_get_double(settings, sources_name.array);
			dstr_free(&sources_name);
			break;
		case GS_SHADER_PARAM_VEC4:
			if (default_value != NULL) {
				struct vec4 *v4 = default_value;
				obs_data_set_default_int(settings, param_name,
							 (int)color_to_int(v4->x, v4->y, v4->z, v4->w));
				param->default_value.vec4.x = v4->x;
				param->default_value.vec4.y = v4->y;
				param->default_value.vec4.z = v4->z;
				param->default_value.vec4.w = v4->w;
				param->has_default = true;
			}
			int_to_color((uint32_t)obs_data_get_int(settings, param_name), &param->value.vec4.x,
				     &param->value.vec4.y, &param->value.vec4.z, &param->value.vec4.w);
			break;
		case GS_SHADER_PARAM_TEXTURE:
			if (param->widget_type.array && strcmp(param->widget_type.array, "source") == 0) {
				const char *source_name = obs_data_get_string(settings, param_name);
				if (source_name && strlen(source_name) > 0) {
					source = obs_get_source_by_name(source_name);
				}
				obs_source_t *old_source = obs_weak_source_get_source(param->source);
				if (source != old_source) {
					if (old_source) {
						if ((!filter->transition || filter->prev_transitioning) &&
						    obs_source_active(filter->context))
							obs_source_dec_active(old_source);
						if ((!filter->transition || filter->prev_transitioning) &&
						    obs_source_showing(filter->context))
							obs_source_dec_showing(old_source);
						obs_source_release(old_source);
					}
					obs_weak_source_release(param->source);
					param->source = obs_source_get_weak_source(source);
					if (source) {
						if ((!filter->transition || filter->prev_transitioning) &&
						    obs_source_active(filter->context))
							obs_source_inc_active(source);
						if ((!filter->transition || filter->prev_transitioning) &&
						    obs_source_showing(filter->context))
							obs_source_inc_showing(source);
					}
				} else if (old_source) {
					obs_source_release(old_source);
				}
				if (source)
					obs_source_release(source);
			} else {
				const char *path = default_value;
				if (!obs_data_has_user_value(settings, param_name) && path && strlen(path)) {
					if (os_file_exists(path)) {
						char *abs_path = os_get_abs_path_ptr(path);
						obs_data_set_default_string(settings, param_name, abs_path);
						bfree(abs_path);
						param->has_default = true;
					} else {
						struct dstr texture_path = {0};
						dstr_init(&texture_path);
						dstr_cat(&texture_path, obs_get_module_data_path(obs_current_module()));
						dstr_cat(&texture_path, "/textures/");
						dstr_cat(&texture_path, path);
						char *abs_path = os_get_abs_path_ptr(texture_path.array);
						if (os_file_exists(abs_path)) {
							obs_data_set_default_string(settings, param_name, abs_path);
							param->has_default = true;
						}
						bfree(abs_path);
						dstr_free(&texture_path);
					}
				}
				path = obs_data_get_string(settings, param_name);
				bool n = false;
				if (param->image == NULL) {
					param->image = bzalloc(sizeof(gs_image_file_t));
					n = true;
				}
				if (n || !path || !param->path.array || strcmp(path, param->path.array) != 0) {
					if (!n) {
						obs_enter_graphics();
						gs_image_file_free(param->image);
						obs_leave_graphics();
					}
					gs_image_file_init(param->image, path);
					dstr_copy(&param->path, path);
					obs_enter_graphics();
					gs_image_file_init_texture(param->image);
					obs_leave_graphics();
				}
				obs_source_t *old_source = obs_weak_source_get_source(param->source);
				if (old_source) {
					if ((!filter->transition || filter->prev_transitioning) &&
					    obs_source_active(filter->context))
						obs_source_dec_active(old_source);
					if ((!filter->transition || filter->prev_transitioning) &&
					    obs_source_showing(filter->context))
						obs_source_dec_showing(old_source);
					obs_source_release(old_source);
				}
				obs_weak_source_release(param->source);
				param->source = NULL;
			}
			break;
		case GS_SHADER_PARAM_STRING:
			if (default_value != NULL) {
				obs_data_set_default_string(settings, param_name, (const char *)default_value);
				param->has_default = true;
			}
			param->value.string = (char *)obs_data_get_string(settings, param_name);
			break;
		default:;
		}
		bfree(default_value);
	}
}

void shader_filter_tick(void *data, float seconds)
{
	struct shader_filter_data *filter = data;
	obs_source_t *target = filter->transition ? filter->context : obs_filter_get_target(filter->context);
	if (!target)
		return;
	// Determine offsets from expansion values.
	int base_width = obs_source_get_base_width(target);
	int base_height = obs_source_get_base_height(target);

	filter->total_width = filter->expand_left + base_width + filter->expand_right;
	filter->total_height = filter->expand_top + base_height + filter->expand_bottom;

	filter->uv_size.x = (float)filter->total_width;
	filter->uv_size.y = (float)filter->total_height;

	filter->uv_scale.x = (float)filter->total_width / base_width;
	filter->uv_scale.y = (float)filter->total_height / base_height;

	filter->uv_offset.x = (float)(-filter->expand_left) / base_width;
	filter->uv_offset.y = (float)(-filter->expand_top) / base_height;

	filter->uv_pixel_interval.x = 1.0f / base_width;
	filter->uv_pixel_interval.y = 1.0f / base_height;

	if (filter->shader_start_time == 0.0f) {
		filter->shader_start_time = filter->elapsed_time + seconds;
	}
	filter->elapsed_time += seconds;
	filter->elapsed_time_loop += seconds;
	if (filter->elapsed_time_loop > 1.0f) {
		filter->elapsed_time_loop -= 1.0f;
		filter->loops++;
	}

	filter->local_time = (float)(os_gettime_ns() / 1000000000.0);
	if (filter->enabled != obs_source_enabled(filter->context)) {
		filter->enabled = !filter->enabled;
		if (filter->enabled)
			filter->shader_enable_time = filter->elapsed_time;
	}
	obs_source_t *parent = obs_filter_get_parent(filter->context);
	if (obs_source_enabled(filter->context) && parent && obs_source_active(parent)) {
		filter->shader_active_time += seconds;
	} else {
		filter->shader_active_time = 0.0f;
	}
	if (obs_source_enabled(filter->context) && parent && obs_source_showing(parent)) {
		filter->shader_show_time += seconds;
	} else {
		filter->shader_show_time = 0.0f;
	}

	filter->rand_f = (float)((double)rand_interval(0, 10000) / (double)10000);

	filter->delta_time = seconds;
	filter->frame_count++;

	struct obs_video_info ovi;
	if (obs_get_video_info(&ovi)) {
		filter->canvas_size.x = (float)ovi.base_width;
		filter->canvas_size.y = (float)ovi.base_height;
	}

	if (filter->auto_reload && filter->last_from_file && !dstr_is_empty(&filter->last_path)) {
		int64_t cur_time = get_file_mod_time(filter->last_path.array);
		if (cur_time > 0 && filter->last_file_time > 0 && cur_time > filter->last_file_time) {
			filter->reload_effect = true;
			filter->last_file_time = cur_time;
		}
	}

	filter->output_rendered = false;
	filter->input_rendered = false;
}

gs_texrender_t *create_or_reset_texrender(gs_texrender_t *render)
{
	if (!render) {
		render = gs_texrender_create(GS_RGBA, GS_ZS_NONE);
	} else {
		gs_texrender_reset(render);
	}
	return render;
}

static void get_input_source(struct shader_filter_data *filter)
{
	if (filter->input_rendered)
		return;

	// Use the OBS default effect file as our effect.
	gs_effect_t *pass_through = obs_get_base_effect(OBS_EFFECT_DEFAULT);

	// Set up our color space info.
	const enum gs_color_space preferred_spaces[] = {
		GS_CS_SRGB,
		GS_CS_SRGB_16F,
		GS_CS_709_EXTENDED,
	};

	const enum gs_color_space source_space =
		obs_source_get_color_space(obs_filter_get_target(filter->context), OBS_COUNTOF(preferred_spaces), preferred_spaces);

	const enum gs_color_format format = gs_get_format_from_space(source_space);

	if (filter->param_previous_image) {
		gs_texrender_t *temp = filter->input_texrender;
		filter->input_texrender = filter->previous_input_texrender;
		filter->previous_input_texrender = temp;
	}

	// Set up our input_texrender to catch the output texture.
	filter->input_texrender = create_or_reset_texrender(filter->input_texrender);

	// Start the rendering process with our correct color space params,
	// And set up your texrender to recieve the created texture.
	if (!filter->transition &&
	    !obs_source_process_filter_begin_with_color_space(filter->context, format, source_space, OBS_NO_DIRECT_RENDERING))
		return;

	if (gs_texrender_begin(filter->input_texrender, filter->total_width, filter->total_height)) {
		gs_blend_state_push();
		gs_reset_blend_state();
		gs_enable_blending(false);
		gs_blend_function(GS_BLEND_ONE, GS_BLEND_ZERO);

		gs_ortho(0.0f, (float)filter->total_width, 0.0f, (float)filter->total_height, -100.0f, 100.0f);
		// The incoming source is pre-multiplied alpha, so use the
		// OBS default effect "DrawAlphaDivide" technique to convert
		// the colors back into non-pre-multiplied space. If the shader
		// file has #define USE_PM_ALPHA 1, then use normal "Draw"
		// technique.
		const char *technique = filter->use_pm_alpha ? "Draw" : "DrawAlphaDivide";
		if (!filter->transition)
			obs_source_process_filter_tech_end(filter->context, pass_through, filter->total_width, filter->total_height,
							   technique);
		gs_texrender_end(filter->input_texrender);
		gs_blend_state_pop();
		filter->input_rendered = true;
	}
}

static void draw_output(struct shader_filter_data *filter)
{
	const enum gs_color_space preferred_spaces[] = {
		GS_CS_SRGB,
		GS_CS_SRGB_16F,
		GS_CS_709_EXTENDED,
	};

	const enum gs_color_space source_space =
		obs_source_get_color_space(obs_filter_get_target(filter->context), OBS_COUNTOF(preferred_spaces), preferred_spaces);

	const enum gs_color_format format = gs_get_format_from_space(source_space);

	if (!obs_source_process_filter_begin_with_color_space(filter->context, format, source_space, OBS_NO_DIRECT_RENDERING)) {
		return;
	}

	gs_texture_t *texture = gs_texrender_get_texture(filter->output_texrender);
	gs_effect_t *pass_through = filter->output_effect;
	if (!pass_through)
		pass_through = obs_get_base_effect(OBS_EFFECT_DEFAULT);

	if (filter->param_output_image) {
		gs_effect_set_texture(filter->param_output_image, texture);
	}

	obs_source_process_filter_end(filter->context, pass_through, filter->total_width, filter->total_height);
}

static void build_sprite(struct gs_vb_data *data, float fcx, float fcy, float start_u, float end_u, float start_v, float end_v)
{
	struct vec2 *tvarray = data->tvarray[0].array;

	vec3_zero(data->points);
	vec3_set(data->points + 1, fcx, 0.0f, 0.0f);
	vec3_set(data->points + 2, 0.0f, fcy, 0.0f);
	vec3_set(data->points + 3, fcx, fcy, 0.0f);
	vec2_set(tvarray, start_u, start_v);
	vec2_set(tvarray + 1, end_u, start_v);
	vec2_set(tvarray + 2, start_u, end_v);
	vec2_set(tvarray + 3, end_u, end_v);
}

static inline void build_sprite_norm(struct gs_vb_data *data, float fcx, float fcy)
{
	build_sprite(data, fcx, fcy, 0.0f, 1.0f, 0.0f, 1.0f);
}

static void render_shader(struct shader_filter_data *filter, float f, obs_source_t *filter_to)
{
	gs_texture_t *texture = gs_texrender_get_texture(filter->input_texrender);
	if (!texture) {
		return;
	}

	if (filter->param_previous_output) {
		gs_texrender_t *temp = filter->output_texrender;
		filter->output_texrender = filter->previous_output_texrender;
		filter->previous_output_texrender = temp;
	}
	filter->output_texrender = create_or_reset_texrender(filter->output_texrender);

	if (filter->param_image)
		gs_effect_set_texture(filter->param_image, texture);
	if (filter->param_previous_image)
		gs_effect_set_texture(filter->param_previous_image, gs_texrender_get_texture(filter->previous_input_texrender));
	filter->color_space = (int)gs_get_color_space();
	if (filter->param_previous_output)
		gs_effect_set_texture(filter->param_previous_output, gs_texrender_get_texture(filter->previous_output_texrender));

	shader_filter_set_effect_params(filter);

	if (f > 0.0f) {
		if (filter_to) {
			struct shader_filter_data *filter2 = obs_obj_get_data(filter_to);
			if (filter2) {
				for (size_t i = 0; i < filter->stored_param_list.num; i++) {
					struct effect_param_data *param = (filter->stored_param_list.array + i);
					if (!param->param || !param->name.array)
						continue;

					for (size_t j = 0; j < filter2->stored_param_list.num; j++) {
						struct effect_param_data *param2 = (filter2->stored_param_list.array + j);
						if (!param2->param || !param2->name.array)
							continue;
						if (param->type != param2->type)
							continue;
						if (strcmp(param->name.array, param2->name.array) != 0)
							continue;

						switch (param->type) {
						case GS_SHADER_PARAM_FLOAT:
							gs_effect_set_float(param->param, (float)param2->value.f * f +
											  (float)param->value.f * (1.0f - f));
							break;
						case GS_SHADER_PARAM_INT:
							gs_effect_set_int(param->param, (int)((double)param2->value.i * f +
												      (double)param->value.i * (1.0f - f)));
							break;
						case GS_SHADER_PARAM_VEC2: {
							struct vec2 v2;
							v2.x = (float)param2->value.vec2.x * f + (float)param->value.vec2.x * (1.0f - f);
							v2.y = (float)param2->value.vec2.y * f + (float)param->value.vec2.y * (1.0f - f);
							gs_effect_set_vec2(param->param, &v2);
							break;
						}
						case GS_SHADER_PARAM_VEC3: {
							struct vec3 v3;
							v3.x = (float)param2->value.vec3.x * f + (float)param->value.vec3.x * (1.0f - f);
							v3.y = (float)param2->value.vec3.y * f + (float)param->value.vec3.y * (1.0f - f);
							v3.z = (float)param2->value.vec3.z * f + (float)param->value.vec3.z * (1.0f - f);
							gs_effect_set_vec3(param->param, &v3);
							break;
						}
						case GS_SHADER_PARAM_VEC4: {
							struct vec4 v4;
							v4.x = (float)param2->value.vec4.x * f + (float)param->value.vec4.x * (1.0f - f);
							v4.y = (float)param2->value.vec4.y * f + (float)param->value.vec4.y * (1.0f - f);
							v4.z = (float)param2->value.vec4.z * f + (float)param->value.vec4.z * (1.0f - f);
							v4.w = (float)param2->value.vec4.w * f + (float)param->value.vec4.w * (1.0f - f);
							gs_effect_set_vec4(param->param, &v4);
							break;
						}
						default:;
						}
						break;
					}
				}
			}
		} else {
			for (size_t i = 0; i < filter->stored_param_list.num; i++) {
				struct effect_param_data *param = (filter->stored_param_list.array + i);
				if (!param->param || !param->has_default)
					continue;

				switch (param->type) {
				case GS_SHADER_PARAM_FLOAT:
					gs_effect_set_float(param->param,
							    (float)param->default_value.f * f + (float)param->value.f * (1.0f - f));
					break;
				case GS_SHADER_PARAM_INT:
					gs_effect_set_int(param->param, (int)((double)param->default_value.i * f +
									      (double)param->value.i * (1.0f - f)));
					break;
				case GS_SHADER_PARAM_VEC2: {
					struct vec2 v2;
					v2.x = param->default_value.vec2.x * f + param->value.vec2.x * (1.0f - f);
					v2.y = param->default_value.vec2.y * f + param->value.vec2.y * (1.0f - f);
					gs_effect_set_vec2(param->param, &v2);
					break;
				}
				case GS_SHADER_PARAM_VEC3: {
					struct vec3 v3;
					v3.x = param->default_value.vec3.x * f + param->value.vec3.x * (1.0f - f);
					v3.y = param->default_value.vec3.y * f + param->value.vec3.y * (1.0f - f);
					v3.z = param->default_value.vec3.z * f + param->value.vec3.z * (1.0f - f);
					gs_effect_set_vec3(param->param, &v3);
					break;
				}
				case GS_SHADER_PARAM_VEC4: {
					struct vec4 v4;
					v4.x = param->default_value.vec4.x * f + param->value.vec4.x * (1.0f - f);
					v4.y = param->default_value.vec4.y * f + param->value.vec4.y * (1.0f - f);
					v4.z = param->default_value.vec4.z * f + param->value.vec4.z * (1.0f - f);
					v4.w = param->default_value.vec4.w * f + param->value.vec4.w * (1.0f - f);
					gs_effect_set_vec4(param->param, &v4);
					break;
				}
				default:;
				}
			}
		}
	}

	gs_blend_state_push();
	gs_reset_blend_state();
	gs_enable_blending(false);
	gs_blend_function(GS_BLEND_ONE, GS_BLEND_ZERO);

	if (filter->param_pass_texture) {
		filter->intermediate_texrender = create_or_reset_texrender(filter->intermediate_texrender);
		if (gs_texrender_begin(filter->intermediate_texrender, filter->total_width, filter->total_height)) {
			gs_ortho(0.0f, (float)filter->total_width, 0.0f, (float)filter->total_height, -100.0f, 100.0f);
			if (filter->use_template) {
				gs_draw_sprite(texture, 0, filter->total_width, filter->total_height);
			} else {
				if (!filter->sprite_buffer)
					load_sprite_buffer(filter);

				struct gs_vb_data *data = gs_vertexbuffer_get_data(filter->sprite_buffer);
				build_sprite_norm(data, (float)filter->total_width, (float)filter->total_height);
				gs_vertexbuffer_flush(filter->sprite_buffer);
				gs_load_vertexbuffer(filter->sprite_buffer);
				gs_load_indexbuffer(NULL);
				gs_draw(GS_TRISTRIP, 0, 0);
			}
			gs_texrender_end(filter->intermediate_texrender);
		}
		gs_effect_set_texture(filter->param_pass_texture, gs_texrender_get_texture(filter->intermediate_texrender));
	}

	if (gs_texrender_begin(filter->output_texrender, filter->total_width, filter->total_height)) {
		gs_ortho(0.0f, (float)filter->total_width, 0.0f, (float)filter->total_height, -100.0f, 100.0f);
		while (gs_effect_loop(filter->effect, "Draw")) {
			if (filter->use_template) {
				gs_draw_sprite(texture, 0, filter->total_width, filter->total_height);
			} else {
				if (!filter->sprite_buffer)
					load_sprite_buffer(filter);

				struct gs_vb_data *data = gs_vertexbuffer_get_data(filter->sprite_buffer);
				build_sprite_norm(data, (float)filter->total_width, (float)filter->total_height);
				gs_vertexbuffer_flush(filter->sprite_buffer);
				gs_load_vertexbuffer(filter->sprite_buffer);
				gs_load_indexbuffer(NULL);
				gs_draw(GS_TRISTRIP, 0, 0);
			}
		}
		gs_texrender_end(filter->output_texrender);
	}

	gs_blend_state_pop();
}

static void shader_filter_render(void *data, gs_effect_t *effect)
{
	UNUSED_PARAMETER(effect);

	struct shader_filter_data *filter = data;

	float f = 0.0f;
	obs_source_t *filter_to = NULL;
	if (move_get_transition_filter)
		f = move_get_transition_filter(filter->context, &filter_to);

	if (f == 0.0f && filter->output_rendered) {
		draw_output(filter);
		return;
	}

	if (filter->effect == NULL || filter->rendering) {
		obs_source_skip_video_filter(filter->context);
		return;
	}

	get_input_source(filter);

	filter->rendering = true;
	render_shader(filter, f, filter_to);
	draw_output(filter);
	if (f == 0.0f)
		filter->output_rendered = true;
	filter->rendering = false;
}

static uint32_t shader_filter_getwidth(void *data)
{
	struct shader_filter_data *filter = data;
	return filter->total_width;
}

static uint32_t shader_filter_getheight(void *data)
{
	struct shader_filter_data *filter = data;
	return filter->total_height;
}

static void shader_filter_defaults(obs_data_t *settings)
{
	obs_data_set_default_string(settings, "shader_text", effect_template_default_image_shader);
	obs_data_set_default_bool(settings, "auto_reload", false);
}

static enum gs_color_space shader_filter_get_color_space(void *data, size_t count, const enum gs_color_space *preferred_spaces)
{
	UNUSED_PARAMETER(count);
	UNUSED_PARAMETER(preferred_spaces);
	struct shader_filter_data *filter = data;
	obs_source_t *target = obs_filter_get_target(filter->context);
	const enum gs_color_space potential_spaces[] = {
		GS_CS_SRGB,
		GS_CS_SRGB_16F,
		GS_CS_709_EXTENDED,
	};
	return obs_source_get_color_space(target, OBS_COUNTOF(potential_spaces), potential_spaces);
}

void shader_filter_activate(void *data)
{
	shader_filter_param_source_action(data, obs_source_inc_active);
}

void shader_filter_deactivate(void *data)
{
	shader_filter_param_source_action(data, obs_source_dec_active);
}

void shader_filter_show(void *data)
{
	shader_filter_param_source_action(data, obs_source_inc_showing);
}

void shader_filter_hide(void *data)
{
	shader_filter_param_source_action(data, obs_source_dec_showing);
}

struct obs_source_info shader_filter = {
	.id = "shader_filter",
	.type = OBS_SOURCE_TYPE_FILTER,
	.output_flags = OBS_SOURCE_VIDEO | OBS_SOURCE_SRGB | OBS_SOURCE_CUSTOM_DRAW,
	.create = shader_filter_create,
	.destroy = shader_filter_destroy,
	.update = shader_filter_update,
	.load = shader_filter_update,
	.video_tick = shader_filter_tick,
	.get_name = shader_filter_get_name,
	.get_defaults = shader_filter_defaults,
	.get_width = shader_filter_getwidth,
	.get_height = shader_filter_getheight,
	.video_render = shader_filter_render,
	.get_properties = shader_filter_properties,
	.video_get_color_space = shader_filter_get_color_space,
	.activate = shader_filter_activate,
	.deactivate = shader_filter_deactivate,
	.show = shader_filter_show,
	.hide = shader_filter_hide,
	.missing_files = shader_filter_missing_files,
};
