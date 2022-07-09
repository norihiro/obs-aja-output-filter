#include <obs-module.h>
#include <media-io/video-io.h>
#include <media-io/video-frame.h>
#include "plugin-macros.generated.h"

#define OUTPUT_ID "aja_output"

struct aja_output_filter_s
{
	obs_source_t *context;
	obs_output_t *output;

	video_t *video_output;
	gs_texrender_t *texrender;
	gs_stagesurf_t *stagesurface;

	bool active;
	volatile bool need_restart;
};

static const char *get_name(void *type_data)
{
	UNUSED_PARAMETER(type_data);
	return obs_module_text("AJAOutput");
}

static void ajaof_render(void *data, uint32_t cx, uint32_t cy)
{
	UNUSED_PARAMETER(cx);
	UNUSED_PARAMETER(cy);

	struct aja_output_filter_s *filter = data;

	uint32_t width = gs_stagesurface_get_width(filter->stagesurface);
	uint32_t height = gs_stagesurface_get_height(filter->stagesurface);

	if (!gs_texrender_begin(filter->texrender, width, height))
		return;

	obs_source_t *parent = obs_filter_get_parent(filter->context);
	if (!parent)
		return;

	struct vec4 background;
	vec4_zero(&background);

	gs_clear(GS_CLEAR_COLOR, &background, 0.0f, 0);
	gs_ortho(0.0f, (float)width, 0.0f, (float)height, -100.0f, 100.0f);

	gs_blend_state_push();
	gs_blend_function(GS_BLEND_ONE, GS_BLEND_ZERO);

	obs_source_skip_video_filter(filter->context);

	gs_blend_state_pop();
	gs_texrender_end(filter->texrender);

	struct video_frame output_frame;
	if (!video_output_lock_frame(filter->video_output, &output_frame, 1, obs_get_video_frame_time()))
		return;

	gs_stage_texture(filter->stagesurface, gs_texrender_get_texture(filter->texrender));

	uint8_t *video_data;
	uint32_t video_linesize;
	if (!gs_stagesurface_map(filter->stagesurface, &video_data, &video_linesize))
		return;

	uint32_t linesize = output_frame.linesize[0];

	for (uint32_t i = 0; i < height; i++) {
		uint32_t dst_offset = linesize * i;
		uint32_t src_offset = video_linesize * i;
		memcpy(output_frame.data[0] + dst_offset, video_data + src_offset, linesize);
	}

	gs_stagesurface_unmap(filter->stagesurface);
	video_output_unlock_frame(filter->video_output);
}

static void ajaof_stop(void *data);

static void ajaof_start(void *data)
{
	struct aja_output_filter_s *filter = data;

	if (filter->active)
		return;

	if (!obs_source_enabled(filter->context))
		return;

	obs_source_t *parent = obs_filter_get_target(filter->context);
	uint32_t width = obs_source_get_base_width(parent);
	uint32_t height = obs_source_get_base_height(parent);

	if (!width || !height)
		return;

	obs_data_t *settings = obs_source_get_settings(filter->context);

	filter->output = obs_output_create(OUTPUT_ID, "aja_output_filter", settings, NULL);

	obs_data_release(settings);

	obs_enter_graphics();
	filter->texrender = gs_texrender_create(GS_BGRA, GS_ZS_NONE);
	filter->stagesurface = gs_stagesurface_create(width, height, GS_BGRA);
	obs_leave_graphics();

	const struct video_output_info *main_voi = video_output_get_info(obs_get_video());

	struct video_output_info vi = {0};
	vi.format = VIDEO_FORMAT_BGRA;
	vi.width = width;
	vi.height = height;
	vi.fps_den = main_voi->fps_den;
	vi.fps_num = main_voi->fps_num;
	vi.cache_size = 16;
	vi.colorspace = main_voi->colorspace;
	vi.range = main_voi->range;
	vi.name = obs_source_get_name(filter->context);

	video_output_open(&filter->video_output, &vi);
	obs_output_set_media(filter->output, filter->video_output, obs_get_audio());

	bool started = obs_output_start(filter->output);
	filter->active = true;

	if (!started)
		ajaof_stop(filter);
	else
		obs_add_main_render_callback(ajaof_render, filter);
}

static void ajaof_stop(void *data)
{
	struct aja_output_filter_s *filter = data;

	if (!filter->active)
		return;

	obs_remove_main_render_callback(ajaof_render, filter);

	obs_output_stop(filter->output);
	obs_output_release(filter->output);
	video_output_stop(filter->video_output);

	obs_enter_graphics();
	gs_stagesurface_destroy(filter->stagesurface);
	gs_texrender_destroy(filter->texrender);
	obs_leave_graphics();
	filter->stagesurface = NULL;
	filter->texrender = NULL;

	video_output_close(filter->video_output);
	filter->video_output = NULL;

	filter->active = false;
}

static obs_properties_t *get_properties(void *data)
{
	UNUSED_PARAMETER(data);
	obs_properties_t *props = obs_get_output_properties(OUTPUT_ID);
	obs_property_t *prop;

	prop = obs_properties_get(props, "ui_prop_auto_start_output");
	obs_property_set_visible(prop, false);

	return props;
}

static void update(void *data, obs_data_t *settings)
{
	UNUSED_PARAMETER(settings);

	struct aja_output_filter_s *s = data;

	s->need_restart = true;
}

#ifdef HAVE_FRONTEND
static void frontend_event(enum obs_frontend_event event, void *data)
{
	struct aja_output_filter_s *s = data;

	switch (event) {
	case OBS_FRONTEND_EVENT_FINISHED_LOADING:
		ajaof_start(s);
		break;
	case OBS_FRONTEND_EVENT_SCRIPTING_SHUTDOWN:
		ajaof_stop(s);
		break;
	}
}
#endif // HAVE_FRONTEND

static void *create(obs_data_t *settings, obs_source_t *source)
{
	UNUSED_PARAMETER(settings);

	struct aja_output_filter_s *s = bzalloc(sizeof(struct aja_output_filter_s));

	s->context = source;

#ifdef HAVE_FRONTEND
	obs_frontend_add_event_callback(frontend_event, s);
#endif

	return s;
}

static void destroy(void *data)
{
	struct aja_output_filter_s *s = data;

#ifdef HAVE_FRONTEND
	obs_frontend_remove_event_callback(frontend_event, s);
#endif
	ajaof_stop(s);

	bfree(s);
}

static void video_tick(void *data, float seconds)
{
	UNUSED_PARAMETER(seconds);
	struct aja_output_filter_s *s = data;

	if (s->texrender)
		gs_texrender_reset(s->texrender);

	bool need_start = s->need_restart;
	bool need_stop = s->need_restart;
	s->need_restart = false;

	if (!s->active && obs_source_enabled(s->context))
		need_start = true;
	else if (s->active && !obs_source_enabled(s->context))
		need_stop = true;

	if (need_stop)
		ajaof_stop(s);
	if (need_start)
		obs_queue_task(OBS_TASK_UI, ajaof_start, s, false);
}

const struct obs_source_info aja_output_filter = {
	.id = ID_PREFIX "filter",
	.type = OBS_SOURCE_TYPE_FILTER,
	.output_flags = OBS_SOURCE_VIDEO,
	.get_name = get_name,
	.create = create,
	.destroy = destroy,
	.update = update,
	.get_properties = get_properties,
	.video_tick = video_tick,
};
