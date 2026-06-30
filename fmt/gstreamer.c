/*
 * Schism Tracker - a cross-platform Impulse Tracker clone
 * copyright (c) 2003-2005 Storlek <storlek@rigelseven.com>
 * copyright (c) 2005-2008 Mrs. Brisby <mrs.brisby@nimh.org>
 * copyright (c) 2009 Storlek & Mrs. Brisby
 * copyright (c) 2010-2012 Storlek
 * URL: http://schismtracker.org/
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include "headers.h"

#include "loadso.h"
#include "dmoz.h"
#include "mem.h"

/* The GStreamer header files emit static inline function definitions that call gst_mini_object_unref. If we're
 * trying to load everything dynamically, this defeats the straightforward attempt, because the call occurs
 * before we have a chance to use macros to redirect. We can't just define all our trampoline functions
 * before including the headers, because none of the types have been declared yet.
 *
 * So, we redirect to a trampoline that will then call either the statically-linked gst_mini_object_unref or
 * call through the dynamically-resolved function pointer.
 */
struct _GstMiniObject;

typedef struct _GstMiniObject GstMiniObject;

static void GST_TRAMPOLINE_gst_mini_object_unref(struct _GstMiniObject *mini_object);

#define gst_mini_object_unref GST_TRAMPOLINE_gst_mini_object_unref

#include <gst/gst.h>
#include <gst/pbutils/gstdiscoverer.h>
#include <gst/pbutils/pbutils.h>
#include <gst/app/gstappsrc.h>
#include <gst/audio/audio.h>

#undef gst_mini_object_unref

#if GSTREAMER_DYNAMIC_LOAD

/* ----------------------------------------------------------- */

typedef gpointer (*g_malloc_spec)(gsize n_bytes);
typedef void (*g_free_spec)(gpointer mem);
typedef void (*g_free_sized_spec)(gpointer mem, size_t size);
typedef const gchar *(*g_intern_string_spec)(const gchar *string);
typedef gboolean (*g_strv_contains_spec)(const gchar *const *strv, const gchar *str);
typedef void (*g_object_set_spec)(gpointer object, const gchar *first_property_name, ...);
typedef void (*g_object_unref_spec)(gpointer object);
typedef gulong (*g_signal_connect_data_spec)(gpointer instance, const gchar *detailed_signal, GCallback c_handler, gpointer data, GClosureNotify destroy_data, GConnectFlags connect_flags);
typedef void (*g_signal_emit_by_name_spec)(gpointer instance, const gchar *detailed_signal, ...);
typedef gboolean (*gst_init_check_spec)(int *argc, char **argv[], GError **error);
typedef void (*gst_object_unref_spec)(gpointer object);
typedef void (*gst_mini_object_unref_spec)(GstMiniObject *mini_object);
typedef GstStructure *(*gst_structure_new_spec)(const gchar *name, const gchar *firstfield, ...);
typedef const gchar *(*gst_structure_get_name_spec)(const GstStructure *structure);
typedef gchar *(*gst_filename_to_uri_spec)(const gchar *filename, GError **error);
typedef GstDiscoverer *(*gst_discoverer_new_spec)(GstClockTime timeout, GError **err);
typedef GstDiscovererInfo *(*gst_discoverer_discover_uri_spec)(GstDiscoverer *discoverer, const gchar *uri, GError **err);
typedef GstDiscovererResult (*gst_discoverer_info_get_result_spec)(const GstDiscovererInfo *info);
typedef GstClockTime (*gst_discoverer_info_get_duration_spec)(const GstDiscovererInfo *info);
typedef GList *(*gst_discoverer_info_get_audio_streams_spec)(GstDiscovererInfo *info);
typedef gchar *(*gst_discoverer_stream_info_get_stream_type_nick_spec)(GstDiscovererStreamInfo *info);
typedef GstCaps *(*gst_discoverer_stream_info_get_caps_spec)(GstDiscovererStreamInfo *info);
typedef void (*gst_discoverer_stream_info_list_free_spec)(GList *infos);
typedef guint (*gst_discoverer_audio_info_get_sample_rate_spec)(const GstDiscovererAudioInfo *info);
typedef guint (*gst_discoverer_audio_info_get_channels_spec)(const GstDiscovererAudioInfo *info);
typedef guint (*gst_discoverer_audio_info_get_depth_spec)(const GstDiscovererAudioInfo *info);
typedef gchar *(*gst_pb_utils_get_decoder_description_spec)(const GstCaps *caps);
typedef GstElement *(*gst_pipeline_new_spec)(const gchar *name);
typedef GstElement *(*gst_element_factory_make_spec)(const gchar *factoryname, const gchar *name);
typedef GstStateChangeReturn (*gst_element_set_state_spec)(GstElement *element, GstState state);
typedef GstPad *(*gst_element_get_static_pad_spec)(GstElement *element, const gchar *name);
typedef gboolean (*gst_element_link_spec)(GstElement *src, GstElement *dest);
typedef GstBus *(*gst_element_get_bus_spec)(GstElement *element);
typedef GstMessage *(*gst_bus_timed_pop_filtered_spec)(GstBus *bus, GstClockTime timeout, GstMessageType types);
typedef void (*gst_bin_add_many_spec)(GstBin *bin, GstElement *element_1, ...);
typedef GstCaps *(*gst_pad_get_current_caps_spec)(GstPad *pad);
typedef gboolean (*gst_pad_is_linked_spec)(GstPad *pad);
typedef GstPadLinkReturn (*gst_pad_link_spec)(GstPad *srcpad, GstPad *sinkpad);
typedef GstCaps *(*gst_caps_new_empty_spec)(void);
typedef GstCaps *(*gst_caps_new_empty_simple_spec)(const char *media_type);
typedef guint (*gst_caps_get_size_spec)(const GstCaps *caps);
typedef GstStructure *(*gst_caps_get_structure_spec)(const GstCaps *caps, guint index);
typedef void (*gst_caps_append_structure_spec)(GstCaps *caps, GstStructure  *structure);
typedef gboolean (*gst_caps_can_intersect_spec)(const GstCaps *caps1, const GstCaps *caps2);
typedef gboolean (*gst_audio_info_from_caps_spec)(GstAudioInfo *info, const GstCaps *caps);
typedef GstBuffer *(*gst_sample_get_buffer_spec)(GstSample *sample);
typedef GstCaps *(*gst_sample_get_caps_spec)(GstSample *sample);
typedef GstBuffer *(*gst_buffer_new_wrapped_spec)(gpointer data, gsize size);
typedef gboolean (*gst_buffer_map_spec)(GstBuffer *buffer, GstMapInfo *info, GstMapFlags flags);
typedef void (*gst_buffer_unmap_spec)(GstBuffer *buffer, GstMapInfo *info);

static g_malloc_spec GST_DYNAMIC_g_malloc = NULL;
static g_free_spec GST_DYNAMIC_g_free = NULL;
static g_free_sized_spec GST_DYNAMIC_g_free_sized = NULL;
static g_intern_string_spec GST_DYNAMIC_g_intern_string = NULL;
static g_strv_contains_spec GST_DYNAMIC_g_strv_contains = NULL;
static g_object_set_spec GST_DYNAMIC_g_object_set = NULL;
static g_object_unref_spec GST_DYNAMIC_g_object_unref = NULL;
static g_signal_connect_data_spec GST_DYNAMIC_g_signal_connect_data = NULL;
static g_signal_emit_by_name_spec GST_DYNAMIC_g_signal_emit_by_name = NULL;
static gst_init_check_spec GST_DYNAMIC_gst_init_check = NULL;
static gst_object_unref_spec GST_DYNAMIC_gst_object_unref = NULL;
static gst_mini_object_unref_spec GST_DYNAMIC_gst_mini_object_unref = NULL;
static gst_structure_new_spec GST_DYNAMIC_gst_structure_new = NULL;
static gst_structure_get_name_spec GST_DYNAMIC_gst_structure_get_name = NULL;
static gst_filename_to_uri_spec GST_DYNAMIC_gst_filename_to_uri = NULL;
static gst_discoverer_new_spec GST_DYNAMIC_gst_discoverer_new = NULL;
static gst_discoverer_discover_uri_spec GST_DYNAMIC_gst_discoverer_discover_uri = NULL;
static gst_discoverer_info_get_result_spec GST_DYNAMIC_gst_discoverer_info_get_result = NULL;
static gst_discoverer_info_get_duration_spec GST_DYNAMIC_gst_discoverer_info_get_duration = NULL;
static gst_discoverer_info_get_audio_streams_spec GST_DYNAMIC_gst_discoverer_info_get_audio_streams = NULL;
static gst_discoverer_stream_info_get_stream_type_nick_spec GST_DYNAMIC_gst_discoverer_stream_info_get_stream_type_nick = NULL;
static gst_discoverer_stream_info_get_caps_spec GST_DYNAMIC_gst_discoverer_stream_info_get_caps = NULL;
static gst_discoverer_stream_info_list_free_spec GST_DYNAMIC_gst_discoverer_stream_info_list_free = NULL;
static gst_discoverer_audio_info_get_sample_rate_spec GST_DYNAMIC_gst_discoverer_audio_info_get_sample_rate = NULL;
static gst_discoverer_audio_info_get_channels_spec GST_DYNAMIC_gst_discoverer_audio_info_get_channels = NULL;
static gst_discoverer_audio_info_get_depth_spec GST_DYNAMIC_gst_discoverer_audio_info_get_depth = NULL;
static gst_pb_utils_get_decoder_description_spec GST_DYNAMIC_gst_pb_utils_get_decoder_description = NULL;
static gst_pipeline_new_spec GST_DYNAMIC_gst_pipeline_new = NULL;
static gst_element_factory_make_spec GST_DYNAMIC_gst_element_factory_make = NULL;
static gst_element_set_state_spec GST_DYNAMIC_gst_element_set_state = NULL;
static gst_element_get_static_pad_spec GST_DYNAMIC_gst_element_get_static_pad = NULL;
static gst_element_link_spec GST_DYNAMIC_gst_element_link = NULL;
static gst_element_get_bus_spec GST_DYNAMIC_gst_element_get_bus = NULL;
static gst_bus_timed_pop_filtered_spec GST_DYNAMIC_gst_bus_timed_pop_filtered = NULL;
static gst_bin_add_many_spec GST_DYNAMIC_gst_bin_add_many = NULL;
static gst_pad_get_current_caps_spec GST_DYNAMIC_gst_pad_get_current_caps = NULL;
static gst_pad_is_linked_spec GST_DYNAMIC_gst_pad_is_linked = NULL;
static gst_pad_link_spec GST_DYNAMIC_gst_pad_link = NULL;
static gst_caps_new_empty_spec GST_DYNAMIC_gst_caps_new_empty = NULL;
static gst_caps_new_empty_simple_spec GST_DYNAMIC_gst_caps_new_empty_simple = NULL;
static gst_caps_get_size_spec GST_DYNAMIC_gst_caps_get_size = NULL;
static gst_caps_get_structure_spec GST_DYNAMIC_gst_caps_get_structure = NULL;
static gst_caps_append_structure_spec GST_DYNAMIC_gst_caps_append_structure = NULL;
static gst_caps_can_intersect_spec GST_DYNAMIC_gst_caps_can_intersect = NULL;
static gst_audio_info_from_caps_spec GST_DYNAMIC_gst_audio_info_from_caps = NULL;
static gst_sample_get_buffer_spec GST_DYNAMIC_gst_sample_get_buffer = NULL;
static gst_sample_get_caps_spec GST_DYNAMIC_gst_sample_get_caps = NULL;
static gst_buffer_new_wrapped_spec GST_DYNAMIC_gst_buffer_new_wrapped = NULL;
static gst_buffer_map_spec GST_DYNAMIC_gst_buffer_map = NULL;
static gst_buffer_unmap_spec GST_DYNAMIC_gst_buffer_unmap = NULL;

#define g_malloc GST_DYNAMIC_g_malloc
#define g_object_set GST_DYNAMIC_g_object_set
#define g_object_unref GST_DYNAMIC_g_object_unref
#define g_intern_string GST_DYNAMIC_g_intern_string
#define g_strv_contains GST_DYNAMIC_g_strv_contains
#define g_signal_connect_data GST_DYNAMIC_g_signal_connect_data
#define g_signal_emit_by_name GST_DYNAMIC_g_signal_emit_by_name
#define gst_init_check GST_DYNAMIC_gst_init_check
#define gst_object_unref GST_DYNAMIC_gst_object_unref
#define gst_mini_object_unref GST_DYNAMIC_gst_mini_object_unref
#define gst_structure_new GST_DYNAMIC_gst_structure_new
#define gst_structure_get_name GST_DYNAMIC_gst_structure_get_name
#define gst_filename_to_uri GST_DYNAMIC_gst_filename_to_uri
#define gst_discoverer_new GST_DYNAMIC_gst_discoverer_new
#define gst_discoverer_discover_uri GST_DYNAMIC_gst_discoverer_discover_uri
#define gst_discoverer_info_get_result GST_DYNAMIC_gst_discoverer_info_get_result
#define gst_discoverer_info_get_duration GST_DYNAMIC_gst_discoverer_info_get_duration
#define gst_discoverer_info_get_audio_streams GST_DYNAMIC_gst_discoverer_info_get_audio_streams
#define gst_discoverer_stream_info_get_stream_type_nick GST_DYNAMIC_gst_discoverer_stream_info_get_stream_type_nick
#define gst_discoverer_stream_info_get_caps GST_DYNAMIC_gst_discoverer_stream_info_get_caps
#define gst_discoverer_stream_info_list_free GST_DYNAMIC_gst_discoverer_stream_info_list_free
#define gst_discoverer_audio_info_get_sample_rate GST_DYNAMIC_gst_discoverer_audio_info_get_sample_rate
#define gst_discoverer_audio_info_get_channels GST_DYNAMIC_gst_discoverer_audio_info_get_channels
#define gst_discoverer_audio_info_get_depth GST_DYNAMIC_gst_discoverer_audio_info_get_depth
#define gst_pb_utils_get_decoder_description GST_DYNAMIC_gst_pb_utils_get_decoder_description
#define gst_pipeline_new GST_DYNAMIC_gst_pipeline_new
#define gst_element_factory_make GST_DYNAMIC_gst_element_factory_make
#define gst_element_set_state GST_DYNAMIC_gst_element_set_state
#define gst_element_get_static_pad GST_DYNAMIC_gst_element_get_static_pad
#define gst_element_link GST_DYNAMIC_gst_element_link
#define gst_element_get_bus GST_DYNAMIC_gst_element_get_bus
#define gst_bus_timed_pop_filtered GST_DYNAMIC_gst_bus_timed_pop_filtered
#define gst_bin_add_many GST_DYNAMIC_gst_bin_add_many
#define gst_pad_get_current_caps GST_DYNAMIC_gst_pad_get_current_caps
#define gst_pad_is_linked GST_DYNAMIC_gst_pad_is_linked
#define gst_pad_link GST_DYNAMIC_gst_pad_link
#define gst_caps_new_empty GST_DYNAMIC_gst_caps_new_empty
#define gst_caps_new_empty_simple GST_DYNAMIC_gst_caps_new_empty_simple
#define gst_caps_get_size GST_DYNAMIC_gst_caps_get_size
#define gst_caps_get_structure GST_DYNAMIC_gst_caps_get_structure
#define gst_caps_append_structure GST_DYNAMIC_gst_caps_append_structure
#define gst_caps_can_intersect GST_DYNAMIC_gst_caps_can_intersect
#define gst_audio_info_from_caps GST_DYNAMIC_gst_audio_info_from_caps
#define gst_sample_get_buffer GST_DYNAMIC_gst_sample_get_buffer
#define gst_sample_get_caps GST_DYNAMIC_gst_sample_get_caps
#define gst_buffer_new_wrapped GST_DYNAMIC_gst_buffer_new_wrapped
#define gst_buffer_map GST_DYNAMIC_gst_buffer_map
#define gst_buffer_unmap GST_DYNAMIC_gst_buffer_unmap

/* g_free is a macro that calls g_free or g_free_sized */

#undef g_free
#define g_free(mem)                                                \
  (__builtin_object_size((mem), 0) != ((size_t)-1))                \
	? GST_DYNAMIC_g_free_sized(mem, __builtin_object_size((mem), 0)) \
	: GST_DYNAMIC_g_free(mem)

#endif /* GSTREAMER_DYNAMIC_LOAD && !LINK_TO_GSTREAMER */

/* See explanatory comment before the GStreamer header file inclusion. */
#if !GSTREAMER_DYNAMIC_LOAD
void gst_mini_object_unref(struct _GstMiniObject *mini_object);
#endif /* !GSTREAMER_DYNAMIC_LOAD */

void GST_TRAMPOLINE_gst_mini_object_unref(GstMiniObject *mini_object)
{
	gst_mini_object_unref(mini_object);
}

/* ----------------------------------------------------------- */

static int gstreamer_initialized = 0;

/* ----------------------------------------------------------- */

const gchar *RAW_CODEC_NAMES[] =
	{
		"audio/x-wav",
		"audio/x-flac",
		"audio/x-alac",
		NULL
	};

int fmt_gstreamer_read_info(dmoz_file_t *file, slurp_t *fp)
{
	GstDiscoverer *discoverer;
	GstDiscovererInfo *info;
	GstDiscovererResult result;
	gchar *uri;
	int success = 0;

	/* Initialize GStreamer -- do not call gst_deinit because it isn't subsequently possible to reinitialize in the same process */
	if (!gstreamer_initialized) {
		return 0;
	}

	/* Create the discoverer */
	discoverer = gst_discoverer_new(5 * GST_SECOND, NULL);

	if (!discoverer) {
		return 0;
	}

	/* Translate path to URI */
	uri = gst_filename_to_uri(file->path, NULL);

	if (!uri) {
		g_object_unref(discoverer);
		return 0;
	}

	/* Discover */
	info = gst_discoverer_discover_uri(discoverer, uri, NULL);

	g_free(uri);

	if (info) {
		/* Report */
		result = gst_discoverer_info_get_result(info);

		if (result == GST_DISCOVERER_OK) {
			GstClockTime duration = gst_discoverer_info_get_duration(info);

			if (duration != GST_CLOCK_TIME_NONE) {
				GList *stream_list = gst_discoverer_info_get_audio_streams(info);

				for (const GList *l = stream_list; l != NULL; l = l->next) {
					GstDiscovererStreamInfo *stream_info = GST_DISCOVERER_STREAM_INFO(l->data);
					GstDiscovererAudioInfo *audio_info = GST_DISCOVERER_AUDIO_INFO(l->data);

					if (stream_info && audio_info) {
						const gchar *display_name = gst_discoverer_stream_info_get_stream_type_nick(stream_info);
						guint sample_rate = gst_discoverer_audio_info_get_sample_rate(audio_info);
						guint channels = gst_discoverer_audio_info_get_channels(audio_info);
						guint bits = gst_discoverer_audio_info_get_depth(audio_info);

						const char *description = "Audio via GStreamer";
						int is_raw_audio = 0;

						GstCaps *caps = gst_discoverer_stream_info_get_caps(stream_info);

						if (caps) {
							gchar *decoder_description = gst_pb_utils_get_decoder_description(caps);

							if (decoder_description) {
								// gst_pb_utils_get_decoder_description must be freed, but the value pointed at
								// by file->description will never be freed. So, we need to "intern" the string,
								// creating a single canonical copy for the lifetime of the process.

								description = g_intern_string(decoder_description);
								g_free(decoder_description);
							}

							if (gst_caps_get_size(caps)) {
								GstStructure *structure = gst_caps_get_structure(caps, 0);

								const gchar *codec_name = gst_structure_get_name(structure);

								is_raw_audio = g_strv_contains(RAW_CODEC_NAMES, codec_name);
							}

							gst_caps_unref(caps);
						}

						if ((channels >= 1) && (bits >= 8)) {
							file->type &= ~TYPE_MODULE_MASK;
							file->type &= ~TYPE_INST_MASK;
							file->type &= ~TYPE_SAMPLE_MASK;

							if (is_raw_audio) {
								file->type |= TYPE_SAMPLE_PLAIN;
							} else {
								file->type |= TYPE_SAMPLE_COMPR;
							}

							file->smp_flags = 0;

							if (channels > 1) {
								file->smp_flags |= CHN_STEREO;
							}
							if (bits > 8) {
								file->smp_flags |= CHN_16BIT;
							}

							file->smp_speed = sample_rate;
							file->smp_length = duration * sample_rate / GST_SECOND;

							if (!display_name) {
								display_name = file->base;
							}

							file->title = str_dup(display_name);
							file->description = description;

							success = 1;

							break;
						}
					}
				}

				gst_discoverer_stream_info_list_free(stream_list);
			}
		}

		g_object_unref(info);
	}

	g_object_unref(discoverer);

	return success;
}

/* ---------------------------------------------------------------- */

#define BUFFER_SIZE 4096

static void source__need_data(GstElement *source, guint unused_size, gpointer data)
{
	gpointer buffer;
	int num_read;
	GstFlowReturn ret;

	slurp_t *fp = (slurp_t *)data;

	buffer = g_malloc(BUFFER_SIZE);

	num_read = slurp_read(fp, buffer, BUFFER_SIZE);

	if (num_read <= 0) {
		g_signal_emit_by_name(source, "end-of-stream", &ret);
	} else {
		GstBuffer *buffer_obj = gst_buffer_new_wrapped(buffer, num_read);

		g_signal_emit_by_name(source, "push-buffer", buffer_obj, &ret);
		gst_buffer_unref(buffer_obj);
	}
}

typedef struct pipeline_data pipeline_data_t;
typedef struct sample_buffer_chain sample_buffer_chain_t;
typedef struct sample_buffer_chain_node sample_buffer_chain_node_t;

struct pipeline_data
{
	GstElement *convert;
	sample_buffer_chain_t *buffer_chain;
};

struct sample_buffer_chain
{
	sample_buffer_chain_node_t *first;
	sample_buffer_chain_node_t *last;
	int sample_count;
	int sample_rate;
	int sample_bits;
	int sample_channels;
};

struct sample_buffer_chain_node
{
	sample_buffer_chain_node_t *next;
	int sample_count;
	char *sample_data;
};

static GstFlowReturn sink__new_sample(GstElement *sink, gpointer data)
{
	GstSample *sample;
	GstBuffer *buffer;
	GstCaps *caps;

	GstFlowReturn result = GST_FLOW_ERROR;

	g_signal_emit_by_name(sink, "pull-sample", &sample);

	if (!sample) {
		return GST_FLOW_EOS;
	}

	buffer = gst_sample_get_buffer(sample);
	caps = gst_sample_get_caps(sample);

	if (buffer && caps) {
		GstMapInfo map;

		if (gst_buffer_map(buffer, &map, GST_MAP_READ)) {
			sample_buffer_chain_t *buffer_chain = (sample_buffer_chain_t *)data;

			sample_buffer_chain_node_t *new_node = (sample_buffer_chain_node_t *)malloc(sizeof(sample_buffer_chain_node_t));

			new_node->next = NULL;
			new_node->sample_count = map.size / 2;
			new_node->sample_data = malloc(new_node->sample_count * buffer_chain->sample_bits / 8);

			if (!new_node->sample_data) {
				free(new_node);
			} else {
				if (buffer_chain->sample_bits == 16) {
					memcpy(new_node->sample_data, map.data, new_node->sample_count * buffer_chain->sample_bits / 8);
				} else {
					/* Convert from S16 to U8 */
					unsigned char *s16le_data = (unsigned char *)map.data;

					for (int i = 0, j = 1; i < new_node->sample_count; i++, j += 2)
						new_node->sample_data[i] = s16le_data[j] ^ 0x80;
				}

				if (!buffer_chain->first) {
					buffer_chain->first = new_node;
				} else if (buffer_chain->last) {
					buffer_chain->last->next = new_node;
				}

				buffer_chain->last = new_node;
				buffer_chain->sample_count += new_node->sample_count;

				result = GST_FLOW_OK;
			}

			gst_buffer_unmap(buffer, &map);
		}
	}

	gst_sample_unref(sample);

	return result;
}

static void decode__pad_added(GstElement *decode, GstPad *decode_output_pad, gpointer data)
{
	pipeline_data_t *pipeline_data = (pipeline_data_t *)data;

	GstPad *convert_sink_pad = gst_element_get_static_pad(pipeline_data->convert, "sink");

	if (!convert_sink_pad) {
		return;
	}

	if (!gst_pad_is_linked(convert_sink_pad)) {
		GstCaps *caps = gst_pad_get_current_caps(decode_output_pad);

		if (caps) {
			GstCaps *audio_caps_matcher = gst_caps_new_empty_simple("audio/x-raw");

			if (audio_caps_matcher) {
				if (gst_caps_can_intersect(caps, audio_caps_matcher)) {
					GstAudioInfo info;

					/* Wire up this new audio output pad to the convert element */
					gst_pad_link(decode_output_pad, convert_sink_pad);

					/* Sane default that we hopefully won't have to use */
					pipeline_data->buffer_chain->sample_rate = 44100;

					/* Check if we're 8-bit or wider than 8-bit
						* Check if we're mono or have 2 or more channels
						* Set the output buffer format correspondingly
						*/
					pipeline_data->buffer_chain->sample_bits = 16;
					pipeline_data->buffer_chain->sample_channels = 1;

					if (gst_audio_info_from_caps(&info, caps)) {
						pipeline_data->buffer_chain->sample_rate = GST_AUDIO_INFO_RATE(&info);

						if (GST_AUDIO_INFO_DEPTH(&info) == 8) {
							pipeline_data->buffer_chain->sample_bits = 8;
						}

						if (GST_AUDIO_INFO_CHANNELS(&info) > 1) {
							pipeline_data->buffer_chain->sample_channels = 2;
						}
					}
				}

				gst_caps_unref(audio_caps_matcher);
			}

			gst_caps_unref(caps);
		}
	}

	gst_object_unref(convert_sink_pad);
}

int fmt_gstreamer_load_sample(slurp_t *fp, song_sample_t *smp)
{
	GstElement *pipeline;
	GstElement *source, *decode, *convert, *format, *sink;
	GstCaps *caps;
	GstStructure *caps_mono, *caps_stereo;
	GstBus *bus;
	GstMessage *msg;
	GstMessageType msg_type;

	pipeline_data_t pipeline_data = { 0 };

	sample_buffer_chain_t buffer_chain = { 0 };

	/* Initialize GStreamer -- do not call gst_deinit because it isn't subsequently possible to reinitialize in the same process */
	if (!gstreamer_initialized) {
		return 0;
	}

	/* Build the pipeline */
	pipeline = gst_pipeline_new("extract-audio");

	source = gst_element_factory_make("appsrc", "source");
	decode = gst_element_factory_make("decodebin", "decode");
	convert = gst_element_factory_make("audioconvert", "convert");
	format = gst_element_factory_make("capsfilter", "format");
	sink = gst_element_factory_make("appsink", "sink");

	/* Set up the appsrc */
	g_object_set(
		G_OBJECT(source),
		"format", GST_FORMAT_BYTES,
		"stream-type", GST_APP_STREAM_TYPE_STREAM,
		NULL);

	g_signal_connect(source, "need-data", G_CALLBACK(source__need_data), fp);

	/* Set up conversion. Force format to S16LE and layout to interleaved. Allow 1 or 2 channels. */
	caps = gst_caps_new_empty();

	caps_mono = gst_structure_new("audio/x-raw",
		"format", G_TYPE_STRING, "S16LE",
		"channels", G_TYPE_INT, 1,
		NULL);

	caps_stereo = gst_structure_new("audio/x-raw",
		"format", G_TYPE_STRING, "S16LE",
		"channels", G_TYPE_INT, 2,
		"layout", G_TYPE_STRING, "interleaved",
		NULL);

	/* caps takes ownership of caps_mono and caps_stereo */
	gst_caps_append_structure(caps, caps_mono);
	gst_caps_append_structure(caps, caps_stereo);

	g_object_set(G_OBJECT(format),
		"caps", caps,
		NULL);

	gst_caps_unref(caps);

	/* Set up the appsink */
	g_object_set(
		G_OBJECT(sink),
		"emit-signals", TRUE,
		"sync", FALSE,
		NULL);

	g_signal_connect(sink, "new-sample", G_CALLBACK(sink__new_sample), &buffer_chain);

	/* Link up the pipeline */
	gst_bin_add_many(
		GST_BIN(pipeline),
		source,
		decode,
		convert,
		format,
		sink,
		NULL);

	/* Decode node output pad doesn't exist yet */
	/*   source -> decode ?? convert -> format -> sink */

	gst_element_link(source, decode);

	gst_element_link(convert, format);
	gst_element_link(format, sink);

	pipeline_data.convert = convert;
	pipeline_data.buffer_chain = &buffer_chain;

	g_signal_connect(decode, "pad-added", G_CALLBACK(decode__pad_added), &pipeline_data);

	/* Run pipeline */
	gst_element_set_state(pipeline, GST_STATE_PLAYING);

	/* Wait until error or EOS */
	bus = gst_element_get_bus(pipeline);

	msg = gst_bus_timed_pop_filtered(bus, GST_CLOCK_TIME_NONE, GST_MESSAGE_EOS | GST_MESSAGE_ERROR);
	msg_type = GST_MESSAGE_TYPE(msg);

	/* Free resources */
	gst_message_unref(msg);
	gst_object_unref(bus);
	gst_element_set_state(pipeline, GST_STATE_NULL);
	gst_object_unref(pipeline);

	/* Translate captured data */
	int success = (msg_type != GST_MESSAGE_ERROR);

	if (success) {
		int total_bytes = buffer_chain.sample_count * buffer_chain.sample_bits / 8;

		smp->c5speed = buffer_chain.sample_rate;
		smp->length = buffer_chain.sample_count / buffer_chain.sample_channels;

		smp->flags = 0;

		if (buffer_chain.sample_bits == 16) {
			smp->flags |= CHN_16BIT;
		}

		if (buffer_chain.sample_channels == 2) {
			smp->flags |= CHN_STEREO;
		}

		smp->data = csf_allocate_sample(total_bytes);

		if (!smp->data) {
			smp->length = 0;
			success = 0;
		} else {
			sample_buffer_chain_node_t *node = buffer_chain.first;
			char *data_ptr = smp->data;

			while (node) {
				int buffer_bytes = node->sample_count * buffer_chain.sample_bits / 8;

				memcpy(data_ptr, node->sample_data, buffer_bytes);

				data_ptr += buffer_bytes;

				node = node->next;
			}
		}
	}

	/* Release any allocated buffer chain nodes */
	while (buffer_chain.first != NULL) {
		sample_buffer_chain_node_t *node = buffer_chain.first;

		buffer_chain.first = node->next;

		free(node->sample_data);
		free(node);
	}

	return success;
}

/* ----------------------------------------------------------- */

#if GSTREAMER_DYNAMIC_LOAD

static void *lib_glib = NULL;
static void *lib_gobject = NULL;
static void *lib_gstreamer = NULL;
static void *lib_gstpbutils = NULL;
static void *lib_gstaudio = NULL;

static void gstreamer_unload(void)
{
#define UNLOAD_LIBRARY(name) do { loadso_object_unload(name); name = NULL; } while (0)

	UNLOAD_LIBRARY(lib_glib);
	UNLOAD_LIBRARY(lib_gobject);
	UNLOAD_LIBRARY(lib_gstreamer);
	UNLOAD_LIBRARY(lib_gstpbutils);
	UNLOAD_LIBRARY(lib_gstaudio);

#undef UNLOAD_LIBRARY
}

static int gstreamer_load(void)
{
	int success = 1;

#define LOAD_LIBRARY(lib, filename)     \
	do if (success) {                     \
		lib = loadso_object_load(filename); \
		if (!lib) {                         \
			success = 0;                      \
		}                                   \
	} while (0)

	LOAD_LIBRARY(lib_glib, "libglib-2.0.so");
	LOAD_LIBRARY(lib_gobject, "libgobject-2.0.so");
	LOAD_LIBRARY(lib_gstreamer, "libgstreamer-1.0.so.0");
	LOAD_LIBRARY(lib_gstpbutils, "libgstpbutils-1.0.so");
	LOAD_LIBRARY(lib_gstaudio, "libgstaudio-1.0.so");

#undef LOAD_LIBRARY

	if (success) {
#define RESOLVE_ENDPOINT(lib, name)                                           \
		do if (success) {                                                         \
			GST_DYNAMIC_ ## name = (name ## _spec)loadso_function_load(lib, #name); \
			if (!GST_DYNAMIC_ ## name) {                                            \
				success = 0;                                                          \
			}                                                                       \
		} while (0)

		RESOLVE_ENDPOINT(lib_glib, g_malloc);
		RESOLVE_ENDPOINT(lib_glib, g_free);
		RESOLVE_ENDPOINT(lib_glib, g_free_sized);
		RESOLVE_ENDPOINT(lib_glib, g_intern_string);
		RESOLVE_ENDPOINT(lib_glib, g_strv_contains);
		RESOLVE_ENDPOINT(lib_gobject, g_object_set);
		RESOLVE_ENDPOINT(lib_gobject, g_object_unref);
		RESOLVE_ENDPOINT(lib_gobject, g_signal_connect_data);
		RESOLVE_ENDPOINT(lib_gobject, g_signal_emit_by_name);
		RESOLVE_ENDPOINT(lib_gstreamer, gst_init_check);
		RESOLVE_ENDPOINT(lib_gstreamer, gst_object_unref);
		RESOLVE_ENDPOINT(lib_gstreamer, gst_mini_object_unref);
		RESOLVE_ENDPOINT(lib_gstreamer, gst_structure_new);
		RESOLVE_ENDPOINT(lib_gstreamer, gst_structure_get_name);
		RESOLVE_ENDPOINT(lib_gstreamer, gst_filename_to_uri);
		RESOLVE_ENDPOINT(lib_gstpbutils, gst_discoverer_new);
		RESOLVE_ENDPOINT(lib_gstpbutils, gst_discoverer_discover_uri);
		RESOLVE_ENDPOINT(lib_gstpbutils, gst_discoverer_info_get_result);
		RESOLVE_ENDPOINT(lib_gstpbutils, gst_discoverer_info_get_duration);
		RESOLVE_ENDPOINT(lib_gstpbutils, gst_discoverer_info_get_audio_streams);
		RESOLVE_ENDPOINT(lib_gstpbutils, gst_discoverer_stream_info_get_stream_type_nick);
		RESOLVE_ENDPOINT(lib_gstpbutils, gst_discoverer_stream_info_get_caps);
		RESOLVE_ENDPOINT(lib_gstpbutils, gst_discoverer_stream_info_list_free);
		RESOLVE_ENDPOINT(lib_gstpbutils, gst_discoverer_audio_info_get_sample_rate);
		RESOLVE_ENDPOINT(lib_gstpbutils, gst_discoverer_audio_info_get_channels);
		RESOLVE_ENDPOINT(lib_gstpbutils, gst_discoverer_audio_info_get_depth);
		RESOLVE_ENDPOINT(lib_gstpbutils, gst_pb_utils_get_decoder_description);
		RESOLVE_ENDPOINT(lib_gstreamer, gst_pipeline_new);
		RESOLVE_ENDPOINT(lib_gstreamer, gst_element_factory_make);
		RESOLVE_ENDPOINT(lib_gstreamer, gst_element_set_state);
		RESOLVE_ENDPOINT(lib_gstreamer, gst_element_get_static_pad);
		RESOLVE_ENDPOINT(lib_gstreamer, gst_element_link);
		RESOLVE_ENDPOINT(lib_gstreamer, gst_element_get_bus);
		RESOLVE_ENDPOINT(lib_gstreamer, gst_bus_timed_pop_filtered);
		RESOLVE_ENDPOINT(lib_gstreamer, gst_bin_add_many);
		RESOLVE_ENDPOINT(lib_gstreamer, gst_pad_get_current_caps);
		RESOLVE_ENDPOINT(lib_gstreamer, gst_pad_is_linked);
		RESOLVE_ENDPOINT(lib_gstreamer, gst_pad_link);
		RESOLVE_ENDPOINT(lib_gstreamer, gst_caps_new_empty);
		RESOLVE_ENDPOINT(lib_gstreamer, gst_caps_new_empty_simple);
		RESOLVE_ENDPOINT(lib_gstreamer, gst_caps_get_size);
		RESOLVE_ENDPOINT(lib_gstreamer, gst_caps_get_structure);
		RESOLVE_ENDPOINT(lib_gstreamer, gst_caps_append_structure);
		RESOLVE_ENDPOINT(lib_gstreamer, gst_caps_can_intersect);
		RESOLVE_ENDPOINT(lib_gstaudio, gst_audio_info_from_caps);
		RESOLVE_ENDPOINT(lib_gstreamer, gst_sample_get_buffer);
		RESOLVE_ENDPOINT(lib_gstreamer, gst_sample_get_caps);
		RESOLVE_ENDPOINT(lib_gstreamer, gst_buffer_new_wrapped);
		RESOLVE_ENDPOINT(lib_gstreamer, gst_buffer_map);
		RESOLVE_ENDPOINT(lib_gstreamer, gst_buffer_unmap);

#undef RESOLVE_ENDPOINT
	}

	if (!success) {
		gstreamer_unload();
	}

	return success;
}

#endif /* GSTREAMER_DYNAMIC_LOAD */

/* ----------------------------------------------------------- */

int gstreamer_init(void)
{
	if (gstreamer_initialized)
		return 1;

	#if GSTREAMER_DYNAMIC_LOAD
	if (!gstreamer_load()) {
		return 0;
	}
	#endif /* GSTREAMER_DYNAMIC_LOAD */

	/* Initialize GStreamer -- do not call gst_deinit because it isn't subsequently possible to reinitialize in the same process */
	if (!gst_init_check(NULL, NULL, NULL)) {
		#if GSTREAMER_DYNAMIC_LOAD
		gstreamer_unload();
		#endif /* GSTREAMER_DYNAMIC_LOAD */

		return 0;
	}

	gstreamer_initialized = 1;
	return 1;
}
