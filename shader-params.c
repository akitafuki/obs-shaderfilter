#include "obs-shaderfilter.h"

void shader_filter_clear_params(struct shader_filter_data *filter) {
  filter->param_current_time_ms = NULL;
  filter->param_current_time_sec = NULL;
  filter->param_current_time_min = NULL;
  filter->param_current_time_hour = NULL;
  filter->param_current_time_day_of_week = NULL;
  filter->param_current_time_day_of_month = NULL;
  filter->param_current_time_month = NULL;
  filter->param_current_time_day_of_year = NULL;
  filter->param_current_time_year = NULL;
  filter->param_elapsed_time = NULL;
  filter->param_elapsed_time_start = NULL;
  filter->param_elapsed_time_show = NULL;
  filter->param_elapsed_time_active = NULL;
  filter->param_elapsed_time_enable = NULL;
  filter->param_uv_offset = NULL;
  filter->param_uv_pixel_interval = NULL;
  filter->param_uv_scale = NULL;
  filter->param_uv_size = NULL;
  filter->param_rand_f = NULL;
  filter->param_rand_activation_f = NULL;
  filter->param_rand_instance_f = NULL;
  filter->param_loops = NULL;
  filter->param_loop_second = NULL;
  filter->param_local_time = NULL;
  filter->param_audio_peak = NULL;
  filter->param_audio_magnitude = NULL;
  filter->param_canvas_size = NULL;
  filter->param_delta_time = NULL;
  filter->param_frame_count = NULL;
  filter->param_color_space = NULL;
  filter->param_image = NULL;
  filter->param_previous_image = NULL;
  filter->param_image_a = NULL;
  filter->param_image_b = NULL;
  filter->param_transition_time = NULL;
  filter->param_convert_linear = NULL;
  filter->param_previous_output = NULL;
  filter->param_pass_texture = NULL;

  size_t param_count = filter->stored_param_list.num;
  for (size_t param_index = 0; param_index < param_count; param_index++) {
    struct effect_param_data *param = (filter->stored_param_list.array + param_index);
    if (param->image) {
      obs_enter_graphics();
      gs_image_file_free(param->image);
      obs_leave_graphics();

      bfree(param->image);
      param->image = NULL;
    }
    if (param->source) {
      obs_source_t *source = obs_weak_source_get_source(param->source);
      if (source) {
        if ((!filter->transition || filter->prev_transitioning) && obs_source_active(filter->context))
          obs_source_dec_active(source);
        if ((!filter->transition || filter->prev_transitioning) && obs_source_showing(filter->context))
          obs_source_dec_showing(source);
        obs_source_release(source);
      }
      obs_weak_source_release(param->source);
      param->source = NULL;
    }
    if (param->render) {
      obs_enter_graphics();
      gs_texrender_destroy(param->render);
      obs_leave_graphics();
      param->render = NULL;
    }
    dstr_free(&param->name);
    dstr_free(&param->display_name);
    dstr_free(&param->widget_type);
    dstr_free(&param->group);
    dstr_free(&param->path);
    da_free(param->option_values);
    for (size_t i = 0; i < param->option_labels.num; i++) {
      dstr_free(&param->option_labels.array[i]);
    }
    da_free(param->option_labels);
  }

  da_free(filter->stored_param_list);
}

void shader_filter_set_effect_params(struct shader_filter_data *filter) {
  if (filter->param_uv_scale != NULL) {
    gs_effect_set_vec2(filter->param_uv_scale, &filter->uv_scale);
  }
  if (filter->param_uv_offset != NULL) {
    gs_effect_set_vec2(filter->param_uv_offset, &filter->uv_offset);
  }
  if (filter->param_uv_pixel_interval != NULL) {
    gs_effect_set_vec2(filter->param_uv_pixel_interval, &filter->uv_pixel_interval);
  }
  if (filter->param_current_time_ms != NULL) {
#ifdef _WIN32
    SYSTEMTIME system_time;
    GetSystemTime(&system_time);
    gs_effect_set_int(filter->param_current_time_ms, system_time.wMilliseconds);
#else
    struct timeval tv;
    gettimeofday(&tv, NULL);
    gs_effect_set_int(filter->param_current_time_ms, tv.tv_usec / 1000);
#endif
  }
  if (filter->param_current_time_sec != NULL || filter->param_current_time_min != NULL || filter->param_current_time_hour != NULL ||
      filter->param_current_time_day_of_week != NULL || filter->param_current_time_day_of_month != NULL ||
      filter->param_current_time_month != NULL || filter->param_current_time_day_of_year != NULL ||
      filter->param_current_time_year != NULL) {
    time_t t = time(NULL);
    struct tm *lt = localtime(&t);
    if (filter->param_current_time_sec != NULL)
      gs_effect_set_int(filter->param_current_time_sec, lt->tm_sec);
    if (filter->param_current_time_min != NULL)
      gs_effect_set_int(filter->param_current_time_min, lt->tm_min);
    if (filter->param_current_time_hour != NULL)
      gs_effect_set_int(filter->param_current_time_hour, lt->tm_hour);
    if (filter->param_current_time_day_of_week != NULL)
      gs_effect_set_int(filter->param_current_time_day_of_week, lt->tm_wday);
    if (filter->param_current_time_day_of_month != NULL)
      gs_effect_set_int(filter->param_current_time_day_of_month, lt->tm_mday);
    if (filter->param_current_time_month != NULL)
      gs_effect_set_int(filter->param_current_time_month, lt->tm_mon);
    if (filter->param_current_time_day_of_year != NULL)
      gs_effect_set_int(filter->param_current_time_day_of_year, lt->tm_yday);
    if (filter->param_current_time_year != NULL)
      gs_effect_set_int(filter->param_current_time_year, lt->tm_year);
  }
  if (filter->param_elapsed_time != NULL) {
    gs_effect_set_float(filter->param_elapsed_time, filter->elapsed_time);
  }
  if (filter->param_elapsed_time_start != NULL) {
    gs_effect_set_float(filter->param_elapsed_time_start, filter->elapsed_time - filter->shader_start_time);
  }
  if (filter->param_elapsed_time_show != NULL) {
    gs_effect_set_float(filter->param_elapsed_time_show, filter->shader_show_time);
  }
  if (filter->param_elapsed_time_active != NULL) {
    gs_effect_set_float(filter->param_elapsed_time_active, filter->shader_active_time);
  }
  if (filter->param_elapsed_time_enable != NULL) {
    gs_effect_set_float(filter->param_elapsed_time_enable, filter->elapsed_time - filter->shader_enable_time);
  }
  if (filter->param_uv_size != NULL) {
    gs_effect_set_vec2(filter->param_uv_size, &filter->uv_size);
  }
  if (filter->param_canvas_size != NULL) {
    gs_effect_set_vec2(filter->param_canvas_size, &filter->canvas_size);
  }
  if (filter->param_delta_time != NULL) {
    gs_effect_set_float(filter->param_delta_time, filter->delta_time);
  }
  if (filter->param_frame_count != NULL) {
    gs_effect_set_int(filter->param_frame_count, filter->frame_count);
  }
  if (filter->param_color_space != NULL) {
    gs_effect_set_int(filter->param_color_space, filter->color_space);
  }
  if (filter->param_local_time != NULL) {
    gs_effect_set_float(filter->param_local_time, filter->local_time);
  }
  if (filter->param_audio_peak != NULL) {
    gs_effect_set_float(filter->param_audio_peak, filter->audio_peak);
  }
  if (filter->param_audio_magnitude != NULL) {
    gs_effect_set_float(filter->param_audio_magnitude, filter->audio_magnitude);
  }
  if (filter->param_loops != NULL) {
    gs_effect_set_int(filter->param_loops, filter->loops);
  }
  if (filter->param_loop_second != NULL) {
    gs_effect_set_float(filter->param_loop_second, filter->elapsed_time_loop);
  }
  if (filter->param_rand_f != NULL) {
    gs_effect_set_float(filter->param_rand_f, filter->rand_f);
  }
  if (filter->param_rand_activation_f != NULL) {
    gs_effect_set_float(filter->param_rand_activation_f, filter->rand_activation_f);
  }
  if (filter->param_rand_instance_f != NULL) {
    gs_effect_set_float(filter->param_rand_instance_f, filter->rand_instance_f);
  }

  size_t param_count = filter->stored_param_list.num;
  for (size_t param_index = 0; param_index < param_count; param_index++) {
    struct effect_param_data *param = (filter->stored_param_list.array + param_index);
    if (!param->param)
      continue;
    obs_source_t *source = NULL;

    switch (param->type) {
    case GS_SHADER_PARAM_BOOL:
      gs_effect_set_bool(param->param, param->value.i);
      break;
    case GS_SHADER_PARAM_FLOAT:
      gs_effect_set_float(param->param, (float)param->value.f);
      break;
    case GS_SHADER_PARAM_INT:
      gs_effect_set_int(param->param, (int)param->value.i);
      break;
    case GS_SHADER_PARAM_VEC2:
      gs_effect_set_vec2(param->param, &param->value.vec2);
      break;
    case GS_SHADER_PARAM_VEC3:
      gs_effect_set_vec3(param->param, &param->value.vec3);
      break;
    case GS_SHADER_PARAM_VEC4:
      gs_effect_set_vec4(param->param, &param->value.vec4);
      break;
    case GS_SHADER_PARAM_TEXTURE:
      source = obs_weak_source_get_source(param->source);
      if (source) {
        const enum gs_color_space preferred_spaces[] = {
            GS_CS_SRGB,
            GS_CS_SRGB_16F,
            GS_CS_709_EXTENDED,
        };
        const enum gs_color_space space = obs_source_get_color_space(source, OBS_COUNTOF(preferred_spaces), preferred_spaces);
        const enum gs_color_format format = gs_get_format_from_space(space);
        if (!param->render || gs_texrender_get_format(param->render) != format) {
          gs_texrender_destroy(param->render);
          param->render = gs_texrender_create(format, GS_ZS_NONE);
        } else {
          gs_texrender_reset(param->render);
        }
        uint32_t base_width = obs_source_get_base_width(source);
        uint32_t base_height = obs_source_get_base_height(source);
        gs_blend_state_push();
        gs_blend_function(GS_BLEND_ONE, GS_BLEND_ZERO);
        if (gs_texrender_begin_with_color_space(param->render, base_width, base_height, space)) {
          const float w = (float)base_width;
          const float h = (float)base_height;
          uint32_t flags = obs_source_get_output_flags(source);
          const bool custom_draw = (flags & OBS_SOURCE_CUSTOM_DRAW) != 0;
          const bool async = (flags & OBS_SOURCE_ASYNC) != 0;
          struct vec4 clear_color;

          vec4_zero(&clear_color);
          gs_clear(GS_CLEAR_COLOR, &clear_color, 0.0f, 0);
          gs_ortho(0.0f, w, 0.0f, h, -100.0f, 100.0f);

          if (!custom_draw && !async)
            obs_source_default_render(source);
          else
            obs_source_video_render(source);
          gs_texrender_end(param->render);
        }
        gs_blend_state_pop();
        obs_source_release(source);
        gs_texture_t *tex = gs_texrender_get_texture(param->render);
        gs_effect_set_texture(param->param, tex);
      } else if (param->image) {
        gs_effect_set_texture(param->param, param->image->texture);
      } else {
        gs_effect_set_texture(param->param, NULL);
      }

      break;
    case GS_SHADER_PARAM_STRING:
      gs_effect_set_val(param->param, (param->value.string ? param->value.string : NULL), gs_effect_get_val_size(param->param));
      break;
    default:;
    }
  }
}

void shader_filter_param_source_action(void *data, void (*action)(obs_source_t *source)) {
  struct shader_filter_data *filter = data;
  size_t param_count = filter->stored_param_list.num;
  for (size_t param_index = 0; param_index < param_count; param_index++) {
    struct effect_param_data *param = (filter->stored_param_list.array + param_index);
    if (!param->source)
      continue;
    obs_source_t *source = obs_weak_source_get_source(param->source);
    if (!source)
      continue;
    action(source);
    obs_source_release(source);
  }
}

bool shader_filter_from_file_changed(obs_properties_t *props, obs_property_t *p, obs_data_t *settings) {
  UNUSED_PARAMETER(p);
  struct shader_filter_data *filter = obs_properties_get_param(props);

  bool from_file = obs_data_get_bool(settings, "from_file");

  obs_property_set_visible(obs_properties_get(props, "shader_text"), !from_file);
  obs_property_set_visible(obs_properties_get(props, "shader_file_name"), from_file);

  if (from_file != filter->last_from_file) {
    filter->reload_effect = true;
  }
  filter->last_from_file = from_file;

  return true;
}

bool shader_filter_text_changed(obs_properties_t *props, obs_property_t *p, obs_data_t *settings) {
  UNUSED_PARAMETER(p);
  struct shader_filter_data *filter = obs_properties_get_param(props);
  if (!filter)
    return false;

  const char *shader_text = obs_data_get_string(settings, "shader_text");
  bool can_convert = strstr(shader_text, "void mainImage( out vec4") || strstr(shader_text, "void mainImage(out vec4") ||
                     strstr(shader_text, "void main()") || strstr(shader_text, "vec4 effect(vec4");
  obs_property_t *shader_convert = obs_properties_get(props, "shader_convert");
  bool visible = obs_property_visible(obs_properties_get(props, "shader_text"));
  if (obs_property_visible(shader_convert) != (can_convert && visible)) {
    obs_property_set_visible(shader_convert, can_convert && visible);
    return true;
  }
  return false;
}

bool shader_filter_file_name_changed(obs_properties_t *props, obs_property_t *p, obs_data_t *settings) {
  struct shader_filter_data *filter = obs_properties_get_param(props);
  const char *new_file_name = obs_data_get_string(settings, obs_property_name(p));

  if ((dstr_is_empty(&filter->last_path) && strlen(new_file_name)) ||
      (filter->last_path.array && dstr_cmp(&filter->last_path, new_file_name) != 0)) {
    filter->reload_effect = true;
    dstr_copy(&filter->last_path, new_file_name);
    size_t l = strlen(new_file_name);
    if (l > 7 && strncmp(new_file_name + l - 7, ".effect", 7) == 0) {
      obs_data_set_bool(settings, "override_entire_effect", true);
    } else if (l > 7 && strncmp(new_file_name + l - 7, ".shader", 7) == 0) {
      obs_data_set_bool(settings, "override_entire_effect", false);
    }
  }

  return false;
}

bool shader_filter_reload_effect_clicked(obs_properties_t *props, obs_property_t *property, void *data) {
  UNUSED_PARAMETER(props);
  UNUSED_PARAMETER(property);
  struct shader_filter_data *filter = data;

  filter->reload_effect = true;

  obs_source_update(filter->context, NULL);

  return false;
}

bool add_source_to_list(void *data, obs_source_t *source) {
  obs_property_t *p = data;
  const char *name = obs_source_get_name(source);
  size_t count = obs_property_list_item_count(p);
  size_t idx = 0;
  while (idx < count && strcmp(name, obs_property_list_item_string(p, idx)) > 0)
    idx++;
  obs_property_list_insert_string(p, idx, name, name);
  return true;
}

void shader_filter_audio_callback(void *data, const float magnitude[MAX_AUDIO_CHANNELS], const float peak[MAX_AUDIO_CHANNELS],
                                  const float input_peak[MAX_AUDIO_CHANNELS]) {
  UNUSED_PARAMETER(input_peak);
  struct shader_filter_data *filter = (struct shader_filter_data *)data;

  float max_peak = MIN_AUDIO_THRESHOLD;
  for (int i = 0; i < MAX_AUDIO_CHANNELS; i++) {
    if (peak[i] > max_peak && peak[i] != 0.0f) {
      max_peak = peak[i];
    }
  }

  float max_magnitude = MIN_AUDIO_THRESHOLD;
  for (int i = 0; i < MAX_AUDIO_CHANNELS; i++) {
    if (magnitude[i] > max_magnitude && magnitude[i] != 0.0f) {
      max_magnitude = magnitude[i];
    }
  }

  set_atomic_float(&filter->current_audio_peak, convert_db_to_linear(max_peak));
  set_atomic_float(&filter->current_audio_magnitude, convert_db_to_linear(max_magnitude));
}

void shader_filter_cleanup_volmeter(struct shader_filter_data *filter) {
  if (filter->volmeter) {
    obs_volmeter_remove_callback(filter->volmeter, shader_filter_audio_callback, filter);
    obs_volmeter_destroy(filter->volmeter);
    filter->volmeter = NULL;
  }
  if (filter->audio_source_name) {
    bfree(filter->audio_source_name);
    filter->audio_source_name = NULL;
  }
  set_atomic_float(&filter->current_audio_peak, 0.0f);
  set_atomic_float(&filter->current_audio_magnitude, 0.0f);
}

bool shader_filter_enum_audio_sources(void *data, obs_source_t *source) {
  obs_property_t *prop = (obs_property_t *)data;
  uint32_t flags = obs_source_get_output_flags(source);

  if ((flags & OBS_SOURCE_AUDIO) != 0) {
    const char *name = obs_source_get_name(source);
    obs_property_list_add_string(prop, name, name);
  }

  return true;
}

static void missing_file_callback(void *src, const char *new_path, void *data) {
  struct shader_filter_data *filter = src;
  const char *setting_name = data;
  obs_data_t *settings = obs_source_get_settings(filter->context);
  obs_data_set_string(settings, setting_name, new_path);
  obs_source_update(filter->context, settings);
  obs_data_release(settings);
}

obs_missing_files_t *shader_filter_missing_files(void *data) {
  struct shader_filter_data *filter = data;
  obs_missing_files_t *files = obs_missing_files_create();
  obs_data_t *settings = obs_source_get_settings(filter->context);
  if (obs_data_get_bool(settings, "from_file")) {
    const char *file_name = obs_data_get_string(settings, "shader_file_name");
    if (file_name && strlen(file_name) > 0 && !os_file_exists(file_name)) {
      obs_missing_file_t *file =
          obs_missing_file_create(file_name, missing_file_callback, OBS_MISSING_FILE_SOURCE, filter->context, "shader_file_name");
      obs_missing_files_add_file(files, file);
    }
  }
  size_t param_count = filter->stored_param_list.num;
  for (size_t param_index = 0; param_index < param_count; param_index++) {
    struct effect_param_data *param = (filter->stored_param_list.array + param_index);
    if (param->type != GS_SHADER_PARAM_TEXTURE)
      continue;
    const char *widget_type = param->widget_type.array;
    if (widget_type && strcmp(widget_type, "source") == 0)
      continue;
    const char *param_name = param->name.array;
    const char *path = obs_data_get_string(settings, param_name);
    if (path && strlen(path) > 0 && !os_file_exists(path)) {
      obs_missing_file_t *file =
          obs_missing_file_create(path, missing_file_callback, OBS_MISSING_FILE_SOURCE, filter->context, param->name.array);

      obs_missing_files_add_file(files, file);
    }
  }
  obs_data_release(settings);
  return files;
}

static bool shader_filter_preset_changed(obs_properties_t *props, obs_property_t *p, obs_data_t *settings) {
  const char *preset_path = obs_data_get_string(settings, "preset_shader");
  if (!preset_path || !*preset_path)
    return false;

  obs_data_set_string(settings, "shader_file_name", preset_path);
  obs_data_set_bool(settings, "from_file", true);
  bool is_effect = (strstr(preset_path, ".effect") != NULL);
  obs_data_set_bool(settings, "override_entire_effect", is_effect);

  struct shader_filter_data *filter = obs_properties_get_param(props);
  if (filter) {
    filter->reload_effect = true;
  }

  shader_filter_from_file_changed(props, p, settings);
  return true;
}

obs_properties_t *shader_filter_properties(void *data) {
  struct shader_filter_data *filter = data;

  struct dstr examples_path = {0};
  dstr_init(&examples_path);
  dstr_cat(&examples_path, obs_get_module_data_path(obs_current_module()));
  dstr_cat(&examples_path, "/examples");

  obs_properties_t *props = obs_properties_create();
  obs_properties_set_param(props, filter, NULL);

  obs_property_t *preset_list = obs_properties_add_list(props, "preset_shader", obs_module_text("ShaderFilter.Preset"),
                                                        OBS_COMBO_TYPE_LIST, OBS_COMBO_FORMAT_STRING);
  obs_property_list_add_string(preset_list, obs_module_text("ShaderFilter.PresetNone"), "");

  struct dstr glob_pattern = {0};
  dstr_copy_dstr(&glob_pattern, &examples_path);
  dstr_cat(&glob_pattern, "/*");

  os_glob_t *glob_info = NULL;
  if (os_glob(glob_pattern.array, 0, &glob_info) == 0 && glob_info) {
    for (size_t i = 0; i < glob_info->gl_pathc; i++) {
      const char *path = glob_info->gl_pathv[i].path;
      if (strstr(path, ".shader") || strstr(path, ".effect")) {
        const char *filename = strrchr(path, '/');
        if (!filename)
          filename = strrchr(path, '\\');
        filename = filename ? filename + 1 : path;
        obs_property_list_add_string(preset_list, filename, path);
      }
    }
    os_globfree(glob_info);
  }
  dstr_free(&glob_pattern);
  obs_property_set_modified_callback(preset_list, shader_filter_preset_changed);

  if (!filter || !filter->transition) {
    obs_properties_add_int(props, "expand_left", obs_module_text("ShaderFilter.ExpandLeft"), 0, 9999, 1);
    obs_properties_add_int(props, "expand_right", obs_module_text("ShaderFilter.ExpandRight"), 0, 9999, 1);
    obs_properties_add_int(props, "expand_top", obs_module_text("ShaderFilter.ExpandTop"), 0, 9999, 1);
    obs_properties_add_int(props, "expand_bottom", obs_module_text("ShaderFilter.ExpandBottom"), 0, 9999, 1);
  }

  obs_properties_add_bool(props, "override_entire_effect", obs_module_text("ShaderFilter.OverrideEntireEffect"));

  obs_property_t *from_file = obs_properties_add_bool(props, "from_file", obs_module_text("ShaderFilter.LoadFromFile"));
  obs_property_set_modified_callback(from_file, shader_filter_from_file_changed);

  obs_property_t *shader_text =
      obs_properties_add_text(props, "shader_text", obs_module_text("ShaderFilter.ShaderText"), OBS_TEXT_MULTILINE);
  obs_property_set_modified_callback(shader_text, shader_filter_text_changed);

  obs_properties_add_button2(props, "shader_convert", obs_module_text("ShaderFilter.Convert"), shader_filter_convert, data);

  char *abs_path = os_get_abs_path_ptr(examples_path.array);
  obs_property_t *file_name = obs_properties_add_path(props, "shader_file_name", obs_module_text("ShaderFilter.ShaderFileName"),
                                                      OBS_PATH_FILE, NULL, abs_path ? abs_path : examples_path.array);
  if (abs_path)
    bfree(abs_path);
  dstr_free(&examples_path);
  obs_property_set_modified_callback(file_name, shader_filter_file_name_changed);

  if (filter) {
    obs_data_t *settings = obs_source_get_settings(filter->context);
    const char *last_error = obs_data_get_string(settings, "last_error");
    if (last_error && strlen(last_error)) {
      obs_property_t *error = obs_properties_add_text(props, "last_error", obs_module_text("ShaderFilter.Error"), OBS_TEXT_INFO);
      obs_property_text_set_info_type(error, OBS_TEXT_INFO_ERROR);
    }
    obs_data_release(settings);
  }

  obs_properties_add_button(props, "reload_effect", obs_module_text("ShaderFilter.ReloadEffect"),
                            shader_filter_reload_effect_clicked);
  obs_properties_add_bool(props, "auto_reload", obs_module_text("ShaderFilter.AutoReload"));

  if (filter && (filter->param_audio_magnitude || filter->param_audio_peak)) {
    obs_property_t *audio_source =
        obs_properties_add_list(props, "audio_source", "Audio source", OBS_COMBO_TYPE_LIST, OBS_COMBO_FORMAT_STRING);
    obs_property_list_add_string(audio_source, "None", "");

    obs_enum_sources(shader_filter_enum_audio_sources, audio_source);
  }

  DARRAY(obs_property_t *) groups;
  da_init(groups);

  size_t param_count = filter ? filter->stored_param_list.num : 0;
  for (size_t param_index = 0; param_index < param_count; param_index++) {
    struct effect_param_data *param = (filter->stored_param_list.array + param_index);
    const char *param_name = param->name.array;
    const char *label = param->display_name.array;
    const char *widget_type = param->widget_type.array;
    const char *group_name = param->group.array;
    const int *options = param->option_values.array;
    const struct dstr *option_labels = param->option_labels.array;

    struct dstr display_name = {0};

    if (label == NULL) {
      dstr_ncat(&display_name, param_name, param->name.len);
      dstr_replace(&display_name, "_", " ");
    } else {
      dstr_ncat(&display_name, label, param->display_name.len);
    }
    obs_properties_t *group = NULL;
    if (group_name && strlen(group_name)) {
      for (size_t i = 0; i < groups.num; i++) {
        const char *n = obs_property_name(groups.array[i]);
        if (strcmp(n, group_name) == 0) {
          group = obs_property_group_content(groups.array[i]);
        }
      }
      if (!group) {
        group = obs_properties_create();
        obs_property_t *p = obs_properties_add_group(props, group_name, group_name, OBS_GROUP_NORMAL, group);
        da_push_back(groups, &p);
      }
    }
    if (!group)
      group = props;
    switch (param->type) {
    case GS_SHADER_PARAM_BOOL:
      obs_properties_add_bool(group, param_name, display_name.array);
      break;
    case GS_SHADER_PARAM_FLOAT: {
      double range_min = param->minimum.f;
      double range_max = param->maximum.f;
      double step = param->step.f;
      if (range_min == range_max) {
        range_min = -1000.0;
        range_max = 1000.0;
        step = 0.0001;
      }
      obs_properties_remove_by_name(props, param_name);
      if (widget_type != NULL && strcmp(widget_type, "slider") == 0) {
        obs_properties_add_float_slider(group, param_name, display_name.array, range_min, range_max, step);
      } else {
        obs_properties_add_float(group, param_name, display_name.array, range_min, range_max, step);
      }
      break;
    }
    case GS_SHADER_PARAM_INT: {
      int range_min = (int)param->minimum.i;
      int range_max = (int)param->maximum.i;
      int step = (int)param->step.i;
      if (range_min == range_max) {
        range_min = -1000;
        range_max = 1000;
        step = 1;
      }
      obs_properties_remove_by_name(props, param_name);

      if (widget_type != NULL && strcmp(widget_type, "slider") == 0) {
        obs_properties_add_int_slider(group, param_name, display_name.array, range_min, range_max, step);
      } else if (widget_type != NULL && strcmp(widget_type, "select") == 0) {
        obs_property_t *plist =
            obs_properties_add_list(group, param_name, display_name.array, OBS_COMBO_TYPE_LIST, OBS_COMBO_FORMAT_INT);
        for (size_t i = 0; i < param->option_values.num; i++) {
          obs_property_list_add_int(plist, option_labels[i].array, options[i]);
        }
      } else {
        obs_properties_add_int(group, param_name, display_name.array, range_min, range_max, step);
      }
      break;
    }
    case GS_SHADER_PARAM_INT3:

      break;
    case GS_SHADER_PARAM_VEC4:
      if (widget_type != NULL && strcmp(widget_type, "color") == 0) {
        obs_properties_add_color(group, param_name, display_name.array);
      } else if (widget_type != NULL && strcmp(widget_type, "color_alpha") == 0) {
        obs_properties_add_color_alpha(group, param_name, display_name.array);
      } else {
        obs_properties_add_color(group, param_name, display_name.array);
      }
      break;
    case GS_SHADER_PARAM_TEXTURE:
      if (widget_type != NULL && strcmp(widget_type, "source") == 0) {
        obs_property_t *p =
            obs_properties_add_list(group, param_name, display_name.array, OBS_COMBO_TYPE_LIST, OBS_COMBO_FORMAT_STRING);
        obs_enum_sources(add_source_to_list, p);
      } else {
        obs_properties_add_path(group, param_name, display_name.array, OBS_PATH_FILE, shader_filter_texture_file_filter, NULL);
      }
      break;
    case GS_SHADER_PARAM_STRING:
      if (widget_type != NULL && strcmp(widget_type, "info") == 0) {
        obs_properties_add_text(group, param_name, display_name.array, OBS_TEXT_INFO);
      } else {
        obs_properties_add_text(group, param_name, display_name.array, OBS_TEXT_MULTILINE);
      }
      break;
    default:;
    }
    dstr_free(&display_name);
  }
  da_free(groups);

  obs_properties_add_text(
      props, "plugin_info",
      "<a href=\"https://obsproject.com/forum/resources/obs-shaderfilter.1736/\">obs-shaderfilter</a> (" PROJECT_VERSION
      ") by <a href=\"https://www.exeldro.com\">Exeldro</a>",
      OBS_TEXT_INFO);
  return props;
}
