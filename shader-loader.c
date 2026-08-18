#include "obs-shaderfilter.h"

#define MAX_INCLUDE_DEPTH 16

static const char *get_path_dir_end(const char *path)
{
	const char *slash = strrchr(path, '/');
	const char *bslash = strrchr(path, '\\');
	if (!slash)
		return bslash;
	if (!bslash)
		return slash;
	return (slash > bslash) ? slash : bslash;
}

static bool is_file_visited(const struct dstr *visited, size_t count, const char *abs_path)
{
	for (size_t i = 0; i < count; i++) {
		if (visited[i].array && strcmp(visited[i].array, abs_path) == 0)
			return true;
	}
	return false;
}

static char *load_shader_from_file_internal(const char *file_name, struct dstr **visited, size_t *visited_count,
					    size_t *visited_cap, int depth)
{
	if (!file_name || !*file_name || depth > MAX_INCLUDE_DEPTH) {
		if (depth > MAX_INCLUDE_DEPTH)
			blog(LOG_WARNING, "[obs-shaderfilter] Maximum include depth reached while including '%s'",
			     file_name ? file_name : "");
		return NULL;
	}

	char *abs_file_path = os_get_abs_path_ptr(file_name);
	const char *lookup_path = abs_file_path ? abs_file_path : file_name;

	if (is_file_visited(*visited, *visited_count, lookup_path)) {
		bfree(abs_file_path);
		return bstrdup("");
	}

	if (*visited_count >= *visited_cap) {
		*visited_cap = (*visited_cap == 0) ? 8 : (*visited_cap * 2);
		*visited = brealloc(*visited, sizeof(struct dstr) * (*visited_cap));
	}
	dstr_init_copy(&(*visited)[*visited_count], lookup_path);
	(*visited_count)++;

	char *file_ptr = os_quick_read_utf8_file(file_name);
	if (file_ptr == NULL) {
		blog(LOG_WARNING, "[obs-shaderfilter] failed to read file: %s", file_name);
		bfree(abs_file_path);
		return NULL;
	}

	char **lines = strlist_split(file_ptr, '\n', true);
	struct dstr shader_file;
	dstr_init(&shader_file);

	size_t line_i = 0;
	while (lines && lines[line_i] != NULL) {
		char *line = lines[line_i];
		line_i++;

		char *trimmed = line;
		while (*trimmed == ' ' || *trimmed == '\t')
			trimmed++;

		if (strncmp(trimmed, "#include", 8) == 0 &&
		    (trimmed[8] == ' ' || trimmed[8] == '\t' || trimmed[8] == '"')) {
			char *start = strchr(trimmed, '"');
			char *end = start ? strrchr(start + 1, '"') : NULL;

			if (start && end && end > start + 1) {
				struct dstr include_path = {0};
				const char *dir_end = get_path_dir_end(file_name);
				if (dir_end) {
					dstr_ncopy(&include_path, file_name, dir_end - file_name + 1);
				}
				dstr_ncat(&include_path, start + 1, end - (start + 1));

				char *abs_include_path = os_get_abs_path_ptr(include_path.array);
				const char *target_path = abs_include_path ? abs_include_path : include_path.array;

				char *file_contents = load_shader_from_file_internal(target_path, visited, visited_count,
										     visited_cap, depth + 1);
				if (file_contents) {
					dstr_cat(&shader_file, file_contents);
					dstr_cat(&shader_file, "\n");
					bfree(file_contents);
				}
				bfree(abs_include_path);
				dstr_free(&include_path);
			} else {
				dstr_cat(&shader_file, line);
				dstr_cat(&shader_file, "\n");
			}
		} else {
			dstr_cat(&shader_file, line);
			dstr_cat(&shader_file, "\n");
		}
	}

	bfree(abs_file_path);
	bfree(file_ptr);
	strlist_free(lines);
	return shader_file.array;
}

char *load_shader_from_file(const char *file_name)
{
	struct dstr *visited = NULL;
	size_t visited_count = 0;
	size_t visited_cap = 0;

	char *result = load_shader_from_file_internal(file_name, &visited, &visited_count, &visited_cap, 0);

	for (size_t i = 0; i < visited_count; i++) {
		dstr_free(&visited[i]);
	}
	bfree(visited);

	return result;
}

void load_output_effect(struct shader_filter_data *filter)
{
	if (filter->output_effect != NULL) {
		obs_enter_graphics();
		gs_effect_destroy(filter->output_effect);
		filter->output_effect = NULL;
		obs_leave_graphics();
	}

	char *shader_text = NULL;
	struct dstr filename = {0};
	dstr_cat(&filename, obs_get_module_data_path(obs_current_module()));
	dstr_cat(&filename, "/internal/render_output.effect");
	char *abs_path = os_get_abs_path_ptr(filename.array);
	if (abs_path) {
		shader_text = load_shader_from_file(abs_path);
		bfree(abs_path);
	}
	if (!shader_text)
		shader_text = load_shader_from_file(filename.array);

	char *errors = NULL;
	dstr_free(&filename);

	obs_enter_graphics();
	filter->output_effect = gs_effect_create(shader_text, NULL, &errors);
	obs_leave_graphics();

	bfree(shader_text);
	if (filter->output_effect == NULL) {
		blog(LOG_WARNING, "[obs-shaderfilter] Unable to load render_output.effect file.  Errors:\n%s",
		     (errors == NULL || strlen(errors) == 0 ? "(None)" : errors));
		bfree(errors);
	} else {
		size_t effect_count = gs_effect_get_num_params(filter->output_effect);
		for (size_t effect_index = 0; effect_index < effect_count; effect_index++) {
			gs_eparam_t *param = gs_effect_get_param_by_idx(filter->output_effect, effect_index);
			struct gs_effect_param_info info;
			gs_effect_get_param_info(param, &info);
			if (strcmp(info.name, "output_image") == 0) {
				filter->param_output_image = param;
			}
		}
	}
}

void load_sprite_buffer(struct shader_filter_data *filter)
{
	if (filter->sprite_buffer)
		return;
	struct gs_vb_data *vbd = gs_vbdata_create();
	vbd->num = 4;
	vbd->points = bmalloc(sizeof(struct vec3) * 4);
	vbd->num_tex = 1;
	vbd->tvarray = bmalloc(sizeof(struct gs_tvertarray));
	vbd->tvarray[0].width = 2;
	vbd->tvarray[0].array = bmalloc(sizeof(struct vec2) * 4);
	memset(vbd->points, 0, sizeof(struct vec3) * 4);
	memset(vbd->tvarray[0].array, 0, sizeof(struct vec2) * 4);
	filter->sprite_buffer = gs_vertexbuffer_create(vbd, GS_DYNAMIC);
}

static char *adjust_error_line_numbers(const char *errors, bool use_template)
{
	if (!errors || !use_template)
		return bstrdup(errors ? errors : "");

	size_t template_lines = 0;
	const char *p = effect_template_begin;
	while (*p) {
		if (*p == '\n')
			template_lines++;
		p++;
	}

	struct dstr adjusted = {0};
	dstr_init(&adjusted);

	char **lines = strlist_split(errors, '\n', true);
	size_t i = 0;
	while (lines && lines[i]) {
		const char *line = lines[i];
		i++;

		struct dstr new_line = {0};
		dstr_init_copy(&new_line, line);

		const char *cur = line;
		while (*cur) {
			if ((*cur == '(' || *cur == ':') && (*(cur + 1) >= '0' && *(cur + 1) <= '9')) {
				char delim = *cur;
				char *end = NULL;
				long line_num = strtol(cur + 1, &end, 10);
				if (end && (*end == ',' || *end == ':' || *end == ')')) {
					if (line_num > (long)template_lines) {
						long user_line = line_num - (long)template_lines;
						struct dstr old_num_str = {0};
						struct dstr new_num_str = {0};
						dstr_printf(&old_num_str, "%c%ld%c", delim, line_num, *end);
						dstr_printf(&new_num_str, "%c%ld%c", delim, user_line, *end);
						dstr_replace(&new_line, old_num_str.array, new_num_str.array);
						dstr_free(&old_num_str);
						dstr_free(&new_num_str);
					}
				}
				cur = end ? end : cur + 1;
			} else {
				cur++;
			}
		}

		dstr_cat_dstr(&adjusted, &new_line);
		dstr_cat(&adjusted, "\n");
		dstr_free(&new_line);
	}
	strlist_free(lines);

	return adjusted.array;
}

void shader_filter_reload_effect(struct shader_filter_data *filter)
{
	obs_data_t *settings = obs_source_get_settings(filter->context);

	// First, clean up the old effect and all references to it.
	filter->shader_start_time = 0.0f;
	shader_filter_clear_params(filter);

	if (filter->effect != NULL) {
		obs_enter_graphics();
		gs_effect_destroy(filter->effect);
		filter->effect = NULL;
		obs_leave_graphics();
	}

	// Load text and build the effect from the template, if necessary.
	char *shader_text = NULL;
	bool use_template = !obs_data_get_bool(settings, "override_entire_effect");

	if (obs_data_get_bool(settings, "from_file")) {
		const char *file_name = obs_data_get_string(settings, "shader_file_name");
		if (!strlen(file_name)) {
			obs_data_unset_user_value(settings, "last_error");
			goto end;
		}
		shader_text = load_shader_from_file(file_name);
		if (!shader_text) {
			obs_data_set_string(settings, "last_error", obs_module_text("ShaderFilter.FileLoadFailed"));
			goto end;
		}
		filter->last_file_time = get_file_mod_time(file_name);
	} else {
		shader_text = bstrdup(obs_data_get_string(settings, "shader_text"));
		use_template = true;
	}
	filter->use_template = use_template;

	struct dstr effect_text = {0};

	if (use_template) {
		dstr_cat(&effect_text, effect_template_begin);
	}

	if (shader_text) {
		dstr_cat(&effect_text, shader_text);
		bfree(shader_text);
	}

	if (use_template) {
		dstr_cat(&effect_text, effect_template_end);
	}

	// Create the effect.
	char *errors = NULL;

	obs_enter_graphics();
	int device_type = gs_get_device_type();
	if (device_type == GS_DEVICE_OPENGL) {
		dstr_replace(&effect_text, "[loop]", "");
		dstr_insert(&effect_text, 0, "#define OPENGL 1\n");
	}

	if (effect_text.len && dstr_find(&effect_text, "#define USE_PM_ALPHA 1")) {
		filter->use_pm_alpha = true;
	} else {
		filter->use_pm_alpha = false;
	}

	if (filter->effect)
		gs_effect_destroy(filter->effect);
	filter->effect = gs_effect_create(effect_text.array, NULL, &errors);
	obs_leave_graphics();

	if (filter->effect == NULL) {
		char *adjusted_errors = adjust_error_line_numbers(errors, use_template);
		blog(LOG_WARNING, "[obs-shaderfilter] Unable to create effect. Errors returned from parser:\n%s",
		     (adjusted_errors == NULL || strlen(adjusted_errors) == 0 ? "(None)" : adjusted_errors));
		if (adjusted_errors && strlen(adjusted_errors)) {
			obs_data_set_string(settings, "last_error", adjusted_errors);
		} else {
			obs_data_set_string(settings, "last_error", obs_module_text("ShaderFilter.Unknown"));
		}
		bfree(adjusted_errors);
		dstr_free(&effect_text);
		bfree(errors);
		goto end;
	} else {
		dstr_free(&effect_text);
		obs_data_unset_user_value(settings, "last_error");
	}

	// Store references to the new effect's parameters.
	da_free(filter->stored_param_list);

	size_t effect_count = gs_effect_get_num_params(filter->effect);
	for (size_t effect_index = 0; effect_index < effect_count; effect_index++) {
		gs_eparam_t *param = gs_effect_get_param_by_idx(filter->effect, effect_index);
		if (!param)
			continue;
		struct gs_effect_param_info info;
		gs_effect_get_param_info(param, &info);

		if (strcmp(info.name, "uv_offset") == 0) {
			filter->param_uv_offset = param;
		} else if (strcmp(info.name, "uv_scale") == 0) {
			filter->param_uv_scale = param;
		} else if (strcmp(info.name, "uv_pixel_interval") == 0) {
			filter->param_uv_pixel_interval = param;
		} else if (strcmp(info.name, "uv_size") == 0) {
			filter->param_uv_size = param;
		} else if (strcmp(info.name, "canvas_size") == 0) {
			filter->param_canvas_size = param;
		} else if (strcmp(info.name, "delta_time") == 0) {
			filter->param_delta_time = param;
		} else if (strcmp(info.name, "frame_count") == 0) {
			filter->param_frame_count = param;
		} else if (strcmp(info.name, "color_space") == 0) {
			filter->param_color_space = param;
		} else if (strcmp(info.name, "current_time_ms") == 0) {
			filter->param_current_time_ms = param;
		} else if (strcmp(info.name, "current_time_sec") == 0) {
			filter->param_current_time_sec = param;
		} else if (strcmp(info.name, "current_time_min") == 0) {
			filter->param_current_time_min = param;
		} else if (strcmp(info.name, "current_time_hour") == 0) {
			filter->param_current_time_hour = param;
		} else if (strcmp(info.name, "current_time_day_of_week") == 0) {
			filter->param_current_time_day_of_week = param;
		} else if (strcmp(info.name, "current_time_day_of_month") == 0) {
			filter->param_current_time_day_of_month = param;
		} else if (strcmp(info.name, "current_time_month") == 0) {
			filter->param_current_time_month = param;
		} else if (strcmp(info.name, "current_time_day_of_year") == 0) {
			filter->param_current_time_day_of_year = param;
		} else if (strcmp(info.name, "current_time_year") == 0) {
			filter->param_current_time_year = param;
		} else if (strcmp(info.name, "elapsed_time") == 0) {
			filter->param_elapsed_time = param;
		} else if (strcmp(info.name, "elapsed_time_start") == 0) {
			filter->param_elapsed_time_start = param;
		} else if (strcmp(info.name, "elapsed_time_show") == 0) {
			filter->param_elapsed_time_show = param;
		} else if (strcmp(info.name, "elapsed_time_active") == 0) {
			filter->param_elapsed_time_active = param;
		} else if (strcmp(info.name, "elapsed_time_enable") == 0) {
			filter->param_elapsed_time_enable = param;
		} else if (strcmp(info.name, "rand_f") == 0) {
			filter->param_rand_f = param;
		} else if (strcmp(info.name, "rand_activation_f") == 0) {
			filter->param_rand_activation_f = param;
		} else if (strcmp(info.name, "rand_instance_f") == 0) {
			filter->param_rand_instance_f = param;
		} else if (strcmp(info.name, "loops") == 0) {
			filter->param_loops = param;
		} else if (strcmp(info.name, "loop_second") == 0) {
			filter->param_loop_second = param;
		} else if (strcmp(info.name, "local_time") == 0) {
			filter->param_local_time = param;
		} else if (strcmp(info.name, "audio_peak") == 0) {
			filter->param_audio_peak = param;
		} else if (strcmp(info.name, "audio_magnitude") == 0) {
			filter->param_audio_magnitude = param;
		} else if (strcmp(info.name, "ViewProj") == 0) {
			// Nothing.
		} else if (strcmp(info.name, "image") == 0) {
			filter->param_image = param;
		} else if (strcmp(info.name, "previous_image") == 0) {
			filter->param_previous_image = param;
		} else if (strcmp(info.name, "previous_output") == 0) {
			filter->param_previous_output = param;
		} else if (strcmp(info.name, "pass_texture") == 0 || strcmp(info.name, "pass0_texture") == 0 ||
			   strcmp(info.name, "intermediate_texture") == 0) {
			filter->param_pass_texture = param;
		} else if (filter->transition && strcmp(info.name, "image_a") == 0) {
			filter->param_image_a = param;
		} else if (filter->transition && strcmp(info.name, "image_b") == 0) {
			filter->param_image_b = param;
		} else if (filter->transition && strcmp(info.name, "transition_time") == 0) {
			filter->param_transition_time = param;
		} else if (filter->transition && strcmp(info.name, "convert_linear") == 0) {
			filter->param_convert_linear = param;
		} else {
			struct effect_param_data *cached_data = da_push_back_new(filter->stored_param_list);
			dstr_copy(&cached_data->name, info.name);
			cached_data->type = info.type;
			cached_data->param = param;
			da_init(cached_data->option_values);
			da_init(cached_data->option_labels);
			const size_t annotation_count = gs_param_get_num_annotations(param);
			for (size_t annotation_index = 0; annotation_index < annotation_count; annotation_index++) {
				gs_eparam_t *annotation = gs_param_get_annotation_by_idx(param, annotation_index);
				void *annotation_default = gs_effect_get_default_val(annotation);
				gs_effect_get_param_info(annotation, &info);
				if (strcmp(info.name, "name") == 0 && info.type == GS_SHADER_PARAM_STRING) {
					dstr_copy(&cached_data->display_name, (const char *)annotation_default);
				} else if (strcmp(info.name, "label") == 0 && info.type == GS_SHADER_PARAM_STRING) {
					dstr_copy(&cached_data->display_name, (const char *)annotation_default);
				} else if (strcmp(info.name, "widget_type") == 0 && info.type == GS_SHADER_PARAM_STRING) {
					dstr_copy(&cached_data->widget_type, (const char *)annotation_default);
				} else if (strcmp(info.name, "group") == 0 && info.type == GS_SHADER_PARAM_STRING) {
					dstr_copy(&cached_data->group, (const char *)annotation_default);
				} else if (strcmp(info.name, "minimum") == 0) {
					if (info.type == GS_SHADER_PARAM_FLOAT || info.type == GS_SHADER_PARAM_VEC2 ||
					    info.type == GS_SHADER_PARAM_VEC3 || info.type == GS_SHADER_PARAM_VEC4) {
						cached_data->minimum.f = *(float *)annotation_default;
					} else if (info.type == GS_SHADER_PARAM_INT) {
						cached_data->minimum.i = *(int *)annotation_default;
					}
				} else if (strcmp(info.name, "maximum") == 0) {
					if (info.type == GS_SHADER_PARAM_FLOAT || info.type == GS_SHADER_PARAM_VEC2 ||
					    info.type == GS_SHADER_PARAM_VEC3 || info.type == GS_SHADER_PARAM_VEC4) {
						cached_data->maximum.f = *(float *)annotation_default;
					} else if (info.type == GS_SHADER_PARAM_INT) {
						cached_data->maximum.i = *(int *)annotation_default;
					}
				} else if (strcmp(info.name, "step") == 0) {
					if (info.type == GS_SHADER_PARAM_FLOAT || info.type == GS_SHADER_PARAM_VEC2 ||
					    info.type == GS_SHADER_PARAM_VEC3 || info.type == GS_SHADER_PARAM_VEC4) {
						cached_data->step.f = *(float *)annotation_default;
					} else if (info.type == GS_SHADER_PARAM_INT) {
						cached_data->step.i = *(int *)annotation_default;
					}
				} else if (strncmp(info.name, "option_", 7) == 0) {
					int id = atoi(info.name + 7);
					if (info.type == GS_SHADER_PARAM_INT) {
						int val = *(int *)annotation_default;
						int *cd = da_insert_new(cached_data->option_values, id);
						*cd = val;

					} else if (info.type == GS_SHADER_PARAM_STRING) {
						struct dstr val = {0};
						dstr_copy(&val, (const char *)annotation_default);
						struct dstr *cs = da_insert_new(cached_data->option_labels, id);
						*cs = val;
					}
				}
				bfree(annotation_default);
			}
		}
	}

end:
	obs_data_release(settings);
}
