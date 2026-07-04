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

static void GST_TRAMPOLINE_gst_mini_object_unref(struct _GstMiniObject *mini_object);

#define gst_mini_object_unref GST_TRAMPOLINE_gst_mini_object_unref

#ifdef GSTREAMER_DYNAMIC_LOAD
/* disable these :) */
# define G_DISABLE_CAST_CHECKS 1
#endif

#include <gst/gst.h>
#include <gst/pbutils/gstdiscoverer.h>
#include <gst/pbutils/pbutils.h>
#include <gst/app/gstappsrc.h>
#include <gst/audio/audio.h>

#undef gst_mini_object_unref

#define LIBS \
	LIB(glib, "glib-2.0", 0, 0) \
	LIB(gobject, "gobject-2.0", 0, 0) \
	LIB(gstreamer, "gstreamer-1.0", 0, 0) \
	LIB(gstpbutils, "gstpbutils-1.0", 0, 0) \
	LIB(gstaudio, "gstaudio-1.0", 0, 0)

#define SYMBOLS \
	SYMBOL(glib, gpointer, g_malloc, (gsize n_bytes)) \
	SYMBOL(glib, void, g_free, (gpointer mem)) \
	SYMBOL(glib, void, g_free_sized, (gpointer mem, size_t size)) \
	SYMBOL(glib, const gchar *, g_intern_string, (const gchar *string)) \
	SYMBOL(glib, gboolean, g_strv_contains, (const gchar *const *strv, const gchar *str)) \
	SYMBOL(gobject, void, g_object_set, (gpointer object, const gchar *first_property_name, ...)) \
	SYMBOL(gobject, void, g_object_unref, (gpointer object)) \
	SYMBOL(gobject, gulong, g_signal_connect_data, (gpointer instance, const gchar *detailed_signal, GCallback c_handler, gpointer data, GClosureNotify destroy_data, GConnectFlags connect_flags)) \
	SYMBOL(gobject, void, g_signal_emit_by_name, (gpointer instance, const gchar *detailed_signal, ...)) \
	SYMBOL(gstreamer, gboolean, gst_init_check, (int *argc, char **argv[], GError **error)) \
	SYMBOL(gstreamer, void, gst_object_unref, (gpointer object)) \
	SYMBOL(gstreamer, void, gst_mini_object_unref, (GstMiniObject *mini_object)) \
	SYMBOL(gstreamer, GstStructure *, gst_structure_new, (const gchar *name, const gchar *firstfield, ...)) \
	SYMBOL(gstreamer, const gchar *, gst_structure_get_name, (const GstStructure *structure)) \
	SYMBOL(gstreamer, gchar *, gst_filename_to_uri, (const gchar *filename, GError **error)) \
	SYMBOL(gstpbutils, GstDiscoverer *, gst_discoverer_new, (GstClockTime timeout, GError **err)) \
	SYMBOL(gstpbutils, GstDiscovererInfo *, gst_discoverer_discover_uri, (GstDiscoverer *discoverer, const gchar *uri, GError **err)) \
	SYMBOL(gstpbutils, GstDiscovererResult, gst_discoverer_info_get_result, (const GstDiscovererInfo *info)) \
	SYMBOL(gstpbutils, GstClockTime, gst_discoverer_info_get_duration, (const GstDiscovererInfo *info)) \
	SYMBOL(gstpbutils, GList *, gst_discoverer_info_get_audio_streams, (GstDiscovererInfo *info)) \
	SYMBOL(gstpbutils, gchar *, gst_discoverer_stream_info_get_stream_type_nick, (GstDiscovererStreamInfo *info)) \
	SYMBOL(gstpbutils, GstCaps *, gst_discoverer_stream_info_get_caps, (GstDiscovererStreamInfo *info)) \
	SYMBOL(gstpbutils, void, gst_discoverer_stream_info_list_free, (GList *infos)) \
	SYMBOL(gstpbutils, guint, gst_discoverer_audio_info_get_sample_rate, (const GstDiscovererAudioInfo *info)) \
	SYMBOL(gstpbutils, guint, gst_discoverer_audio_info_get_channels, (const GstDiscovererAudioInfo *info)) \
	SYMBOL(gstpbutils, guint, gst_discoverer_audio_info_get_depth, (const GstDiscovererAudioInfo *info)) \
	SYMBOL(gstpbutils, gchar *, gst_pb_utils_get_decoder_description, (const GstCaps *caps)) \
	SYMBOL(gstreamer, GstElement *, gst_pipeline_new, (const gchar *name)) \
	SYMBOL(gstreamer, GstElement *, gst_element_factory_make, (const gchar *factoryname, const gchar *name)) \
	SYMBOL(gstreamer, GstStateChangeReturn, gst_element_set_state, (GstElement *element, GstState state)) \
	SYMBOL(gstreamer, GstPad *, gst_element_get_static_pad, (GstElement *element, const gchar *name)) \
	SYMBOL(gstreamer, gboolean, gst_element_link, (GstElement *src, GstElement *dest)) \
	SYMBOL(gstreamer, GstBus *, gst_element_get_bus, (GstElement *element)) \
	SYMBOL(gstreamer, GstMessage *, gst_bus_timed_pop_filtered, (GstBus *bus, GstClockTime timeout, GstMessageType types)) \
	SYMBOL(gstreamer, void, gst_bin_add_many, (GstBin *bin, GstElement *element_1, ...)) \
	SYMBOL(gstreamer, GstCaps *, gst_pad_get_current_caps, (GstPad *pad)) \
	SYMBOL(gstreamer, gboolean, gst_pad_is_linked, (GstPad *pad)) \
	SYMBOL(gstreamer, GstPadLinkReturn, gst_pad_link, (GstPad *srcpad, GstPad *sinkpad)) \
	SYMBOL(gstreamer, GstCaps *, gst_caps_new_empty, (void)) \
	SYMBOL(gstreamer, GstCaps *, gst_caps_new_empty_simple, (const char *media_type)) \
	SYMBOL(gstreamer, guint, gst_caps_get_size, (const GstCaps *caps)) \
	SYMBOL(gstreamer, GstStructure *, gst_caps_get_structure, (const GstCaps *caps, guint index)) \
	SYMBOL(gstreamer, void, gst_caps_append_structure, (GstCaps *caps, GstStructure  *structure)) \
	SYMBOL(gstreamer, gboolean, gst_caps_can_intersect, (const GstCaps *caps1, const GstCaps *caps2)) \
	SYMBOL(gstaudio, gboolean, gst_audio_info_from_caps, (GstAudioInfo *info, const GstCaps *caps)) \
	SYMBOL(gstreamer, GstBuffer *, gst_sample_get_buffer, (GstSample *sample)) \
	SYMBOL(gstreamer, GstCaps *, gst_sample_get_caps, (GstSample *sample)) \
	SYMBOL(gstreamer, GstBuffer *, gst_buffer_new_wrapped, (gpointer data, gsize size)) \
	SYMBOL(gstreamer, gboolean, gst_buffer_map, (GstBuffer *buffer, GstMapInfo *info, GstMapFlags flags)) \
	SYMBOL(gstreamer, void, gst_buffer_unmap, (GstBuffer *buffer, GstMapInfo *info))

#ifdef GSTREAMER_DYNAMIC_LOAD

#define LIB(sym, name, ver, age) static void *lib_##sym = NULL;
LIBS
#undef LIB

#define SYMBOL(lib, ret, name, params) static ret (*GST_##name) params = NULL;
SYMBOLS
#undef SYMBOL

static void gstreamer_unload(void)
{
#define LIB(sym, name, ver, age) do { loadso_object_unload(lib_##sym); lib_##sym = NULL; } while (0);
	LIBS
#undef LIB
}

static int gstreamer_load(void)
{
#define LIB(sym, name, ver, age)     \
	do {                                  \
		lib_##sym = library_load(name, ver, age); \
		if (!lib_##sym)                         \
			goto fail;                        \
	} while (0);

		LIBS

#undef LIB

#define SYMBOL(lib, ret, name, params)                                         \
	do {                                                                      \
		GST_##name = (ret (*) params)loadso_function_load(lib_##lib, #name); \
		if (!GST_##name)                                            \
			goto fail;                                                            \
	} while (0);

		SYMBOLS

#undef SYMBOL

	return 1;

fail:
	gstreamer_unload();
	return 0;
}

/* extra hack here */
# if G_GNUC_CHECK_VERSION (4, 1) && GLIB_VERSION_MAX_ALLOWED >= GLIB_VERSION_2_78 && defined(G_HAVE_FREE_SIZED)
#  undef g_free
#  define g_free(mem)                                                \
  (__builtin_object_size((mem), 0) != ((size_t)-1))                \
	? GST_g_free_sized(mem, __builtin_object_size((mem), 0)) \
	: GST_g_free(mem)
# else
#  define g_free GST_g_free
# endif /* G_GNUC_CHECK_VERSION (4, 1) && GLIB_VERSION_MAX_ALLOWED >= GLIB_VERSION_2_78 && defined(G_HAVE_FREE_SIZED) */

#undef g_signal_connect
#define g_signal_connect(instance, detailed_signal, c_handler, data) \
	GST_g_signal_connect_data ((instance), (detailed_signal), (c_handler), (data), NULL, (GConnectFlags) 0)

#else /* !defined(GSTREAMER_DYNAMIC_LOAD) */

/* this is the easiest way to do this... */
# define SYMBOL(lib, ret, name, params) static ret (*const GST_##name) params = name;
SYMBOLS
# undef SYMBOL

#endif /* GSTREAMER_DYNAMIC_LOAD */

/* See explanatory comment before the GStreamer header file inclusion. */
#if !GSTREAMER_DYNAMIC_LOAD
void gst_mini_object_unref(struct _GstMiniObject *mini_object);
#endif /* !GSTREAMER_DYNAMIC_LOAD */

static void GST_TRAMPOLINE_gst_mini_object_unref(GstMiniObject *mini_object)
{
	GST_gst_mini_object_unref(mini_object);
}

/* inline functions */
#define GST_gst_caps_unref    gst_caps_unref
#define GST_gst_buffer_unref  gst_buffer_unref
#define GST_gst_sample_unref  gst_sample_unref
#define GST_g_signal_connect  g_signal_connect
#define GST_gst_message_unref gst_message_unref

/* ----------------------------------------------------------- */

static int gstreamer_initialized = 0;

/* ----------------------------------------------------------- */

/* XXX can we merge this code with the below?
 * dealing with the file path as a URI is a stupid hack,
 * we should only be dealing with `slurp_t` ideally.
 *
 * also doing it this way can probably lead to different results
 * depending on whether we are reading info or loading it as a
 * samples which is Bad */
int fmt_gstreamer_read_info(dmoz_file_t *file, slurp_t *fp)
{
	GstDiscoverer *discoverer;
	GstDiscovererInfo *info;
	GstDiscovererResult result;
	gchar *uri;
	int success = 0;

	if (!gstreamer_initialized)
		return 0;

	/* Create the discoverer */
	discoverer = GST_gst_discoverer_new(5 * GST_SECOND, NULL);
	if (!discoverer)
		return 0;

	/* Translate path to URI */
	uri = GST_gst_filename_to_uri(file->path, NULL);
	if (!uri) {
		GST_g_object_unref(discoverer);
		return 0;
	}

	/* Discover */
	info = GST_gst_discoverer_discover_uri(discoverer, uri, NULL);

	GST_g_free(uri);

	if (info) {
		/* Report */
		result = GST_gst_discoverer_info_get_result(info);

		if (result == GST_DISCOVERER_OK) {
			GstClockTime duration = GST_gst_discoverer_info_get_duration(info);

			if (duration != GST_CLOCK_TIME_NONE) {
				GList *stream_list = GST_gst_discoverer_info_get_audio_streams(info);

				for (const GList *l = stream_list; l != NULL; l = l->next) {
					GstDiscovererStreamInfo *stream_info = GST_DISCOVERER_STREAM_INFO(l->data);
					GstDiscovererAudioInfo *audio_info = GST_DISCOVERER_AUDIO_INFO(l->data);

					if (stream_info && audio_info) {
						const gchar *display_name = GST_gst_discoverer_stream_info_get_stream_type_nick(stream_info);
						guint sample_rate = GST_gst_discoverer_audio_info_get_sample_rate(audio_info);
						guint channels = GST_gst_discoverer_audio_info_get_channels(audio_info);
						guint bits = GST_gst_discoverer_audio_info_get_depth(audio_info);

						const char *description = "Audio via GStreamer";
						int is_raw_audio = 0;

						GstCaps *caps = GST_gst_discoverer_stream_info_get_caps(stream_info);

						if (caps) {
							gchar *decoder_description = GST_gst_pb_utils_get_decoder_description(caps);

							if (decoder_description) {
								// gst_pb_utils_get_decoder_description must be freed, but the value pointed at
								// by file->description will never be freed. So, we need to "intern" the string,
								// creating a single canonical copy for the lifetime of the process.

								description = GST_g_intern_string(decoder_description);
								GST_g_free(decoder_description);
							}

							if (GST_gst_caps_get_size(caps)) {
								static const gchar *RAW_CODEC_NAMES[] = {
									"audio/x-wav",
									"audio/x-flac",
									"audio/x-alac",
									NULL
								};

								GstStructure *structure = GST_gst_caps_get_structure(caps, 0);

								const gchar *codec_name = GST_gst_structure_get_name(structure);

								is_raw_audio = GST_g_strv_contains(RAW_CODEC_NAMES, codec_name);
							}

							GST_gst_caps_unref(caps);
						}

						if ((channels >= 1) && (bits >= 8)) {
							file->type &= ~TYPE_MODULE_MASK;
							file->type &= ~TYPE_INST_MASK;
							file->type &= ~TYPE_SAMPLE_MASK;

							file->type |= (is_raw_audio ? TYPE_SAMPLE_PLAIN : TYPE_SAMPLE_COMPR);

							file->smp_flags = 0;

							if (channels > 1)
								file->smp_flags |= CHN_STEREO;

							if (bits > 8)
								file->smp_flags |= CHN_16BIT;

							file->smp_speed = sample_rate;
							file->smp_length = duration * sample_rate / GST_SECOND;

							file->title = str_dup(display_name ? display_name : file->base);
							file->description = description;

							success = 1;

							break;
						}
					}
				}

				GST_gst_discoverer_stream_info_list_free(stream_list);
			}
		}

		GST_g_object_unref(info);
	}

	GST_g_object_unref(discoverer);

	return success;
}

/* ---------------------------------------------------------------- */

/* Pull nice big chunks so the need-data signal doesn't need to fire too often. */
#define BUFFER_SIZE 262144

static void source__need_data(GstElement *source, guint unused_size, gpointer data)
{
	gpointer buffer;
	size_t num_read;
	GstFlowReturn ret;

	slurp_t *fp = (slurp_t *)data;

	buffer = GST_g_malloc(BUFFER_SIZE);

	num_read = slurp_read(fp, buffer, BUFFER_SIZE);

	if (num_read == 0) {
		GST_g_signal_emit_by_name(source, "end-of-stream", &ret);
	} else {
		GstBuffer *buffer_obj = GST_gst_buffer_new_wrapped(buffer, num_read); /* takes ownership of buffer's lifetime */
		GST_g_signal_emit_by_name(source, "push-buffer", buffer_obj, &ret);
		GST_gst_buffer_unref(buffer_obj);
	}
}

typedef struct pipeline_data pipeline_data_t;
typedef struct sample_buffer sample_buffer_t;

struct pipeline_data {
	GstElement *convert;
	sample_buffer_t *buffer;
};

/* FIXME the way this is done is kind of disgusting.
 * we should have constructors and destructors for this kind of thing
 * so that they don't fall out of sync */
struct sample_buffer {
	/* whether membuf is a valid disko buffer.
	 * this is a bit dumb, as really it should be initialized by
	 * the caller instead. */
	int is_populated;
	disko_t membuf;
	int sample_rate;
	int sample_bytes; /* bytes per sample, valid values are 2 (16bit) and 1(8bit) */
	int sample_channels;
};

static GstFlowReturn sink__new_sample(GstElement *sink, gpointer data)
{
	GstSample *sample = NULL;
	GstBuffer *buffer = NULL;
	GstCaps *caps = NULL;
	GstMapInfo map;
	sample_buffer_t *capture_buffer = (sample_buffer_t *)data;

	GstFlowReturn result = GST_FLOW_ERROR;

	GST_g_signal_emit_by_name(sink, "pull-sample", &sample);

	if (!sample)
		return GST_FLOW_EOS;

	buffer = GST_gst_sample_get_buffer(sample);
	caps = GST_gst_sample_get_caps(sample);

	if (!buffer || !caps)
		goto fail;

	if (!GST_gst_buffer_map(buffer, &map, GST_MAP_READ))
		goto fail;

	disko_write(&capture_buffer->membuf, map.data, map.size);

	result = GST_FLOW_OK;

	GST_gst_buffer_unmap(buffer, &map);
fail:
	GST_gst_sample_unref(sample);
	return result;
}

static void decode__pad_added(GstElement *decode, GstPad *decode_output_pad, gpointer data)
{
	GstPad *convert_sink_pad = NULL;
	GstCaps *caps = NULL;
	GstCaps *audio_caps_matcher = NULL;
	pipeline_data_t *pipeline_data = (pipeline_data_t *)data;
	GstAudioInfo info;

	convert_sink_pad = GST_gst_element_get_static_pad(pipeline_data->convert, "sink");
	if (!convert_sink_pad)
		goto fail;

	if (GST_gst_pad_is_linked(convert_sink_pad))
		goto fail;

	caps = GST_gst_pad_get_current_caps(decode_output_pad);
	if (!caps)
		goto fail;

	audio_caps_matcher = GST_gst_caps_new_empty_simple("audio/x-raw");
	if (!audio_caps_matcher)
		goto fail;

	if (!GST_gst_caps_can_intersect(caps, audio_caps_matcher))
		goto fail;

	/* Wire up this new audio output pad to the convert element */
	GST_gst_pad_link(decode_output_pad, convert_sink_pad);

	/* Sane default that we hopefully won't have to use */
	pipeline_data->buffer->sample_rate = 44100;

	/* Check if we're 8-bit or wider than 8-bit
	 * Check if we're mono or have 2 or more channels
	 * Set the output buffer format correspondingly
	 *
	 * maybe should default to stereo instead of mono?  --paper
	 */
	pipeline_data->buffer->sample_bytes = 2;
	pipeline_data->buffer->sample_channels = 1;

	if (GST_gst_audio_info_from_caps(&info, caps)) {
		pipeline_data->buffer->sample_rate = GST_AUDIO_INFO_RATE(&info);

		if (GST_AUDIO_INFO_DEPTH(&info) == 8)
			pipeline_data->buffer->sample_bytes = 1;

		if (GST_AUDIO_INFO_CHANNELS(&info) > 1)
			pipeline_data->buffer->sample_channels = 2;
	}

	disko_memopen(&pipeline_data->buffer->membuf);
	pipeline_data->buffer->is_populated = 1;

fail:
	if (audio_caps_matcher) GST_gst_caps_unref(audio_caps_matcher);
	if (caps) GST_gst_caps_unref(caps);
	if (convert_sink_pad) GST_gst_object_unref(convert_sink_pad);
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

	sample_buffer_t buffer = { 0 };

	/* do nothing if gstreamer isn't initialized */
	if (!gstreamer_initialized)
		return 0;

	/* Build the pipeline */
	pipeline = GST_gst_pipeline_new("extract-audio");

	source = GST_gst_element_factory_make("appsrc", "source");
	decode = GST_gst_element_factory_make("decodebin", "decode");
	convert = GST_gst_element_factory_make("audioconvert", "convert");
	format = GST_gst_element_factory_make("capsfilter", "format");
	sink = GST_gst_element_factory_make("appsink", "sink");

	/* Set up the appsrc */
	GST_g_object_set(
		G_OBJECT(source),
		"format", GST_FORMAT_BYTES,
		"stream-type", GST_APP_STREAM_TYPE_STREAM,
		NULL);

	GST_g_signal_connect(source, "need-data", G_CALLBACK(source__need_data), fp);

	caps = GST_gst_caps_new_empty();

#define ADD_CAPS(FORMAT) \
do { \
	GST_gst_caps_append_structure(caps, GST_gst_structure_new("audio/x-raw", \
		"format", G_TYPE_STRING, FORMAT, \
		"channels", G_TYPE_INT, 1, \
		NULL)); \
	\
	GST_gst_caps_append_structure(caps, GST_gst_structure_new("audio/x-raw", \
		"format", G_TYPE_STRING, FORMAT, \
		"channels", G_TYPE_INT, 2, \
		"layout", G_TYPE_STRING, "interleaved", \
		NULL)); \
} while (0)

	ADD_CAPS("S8");
	/* Convert to native-endian */
#ifdef WORDS_BIGENDIAN
	ADD_CAPS("S16BE");
#else
	ADD_CAPS("S16BE");
#endif

#undef ADD_CAPS

	GST_g_object_set(G_OBJECT(format),
		"caps", caps,
		NULL);

	GST_gst_caps_unref(caps);

	/* Set up the appsink */
	GST_g_object_set(
		G_OBJECT(sink),
		"emit-signals", TRUE,
		"sync", FALSE,
		NULL);

	GST_g_signal_connect(sink, "new-sample", G_CALLBACK(sink__new_sample), &buffer);

	/* Link up the pipeline */
	GST_gst_bin_add_many(
		GST_BIN(pipeline),
		source,
		decode,
		convert,
		format,
		sink,
		NULL);

	/* Decode node output pad doesn't exist yet */
	/*   source -> decode ?? convert -> format -> sink */

	GST_gst_element_link(source, decode);

	GST_gst_element_link(convert, format);
	GST_gst_element_link(format, sink);

	pipeline_data.convert = convert;
	pipeline_data.buffer = &buffer;

	GST_g_signal_connect(decode, "pad-added", G_CALLBACK(decode__pad_added), &pipeline_data);

	/* Run pipeline */
	GST_gst_element_set_state(pipeline, GST_STATE_PLAYING);

	/* Wait until error or EOS */
	bus = GST_gst_element_get_bus(pipeline);

	msg = GST_gst_bus_timed_pop_filtered(bus, GST_CLOCK_TIME_NONE, GST_MESSAGE_EOS | GST_MESSAGE_ERROR);
	msg_type = GST_MESSAGE_TYPE(msg);

	/* Free resources */
	GST_gst_message_unref(msg);
	GST_gst_object_unref(bus);
	GST_gst_element_set_state(pipeline, GST_STATE_NULL);
	GST_gst_object_unref(pipeline);

	/* Translate captured data */
	if (msg_type == GST_MESSAGE_ERROR || !buffer.is_populated)
		return 0;

	smp->c5speed = buffer.sample_rate;

	smp->length = buffer.membuf.length;
	smp->flags = 0;

	if (buffer.sample_bytes == 2) {
		smp->flags |= CHN_16BIT;
		smp->length >>= 1;
	}

	if (buffer.sample_channels == 2) {
		smp->flags |= CHN_STEREO;
		smp->length >>= 1;
	}

	/* would be nice to have a csf disko api so we can write directly into
	 * the sample buffer instead of having to copy it */
	smp->data = csf_allocate_sample(buffer.membuf.length);

	if (smp->data) {
		memcpy(smp->data, buffer.membuf.data, buffer.membuf.length);
	} else {
		smp->length = 0;
	}

	disko_memclose(&buffer.membuf, 1 /* free buffer */);

	return !!(smp->data);
}

/* ----------------------------------------------------------- */

int gstreamer_init(void)
{
	if (gstreamer_initialized)
		return 1;

#if GSTREAMER_DYNAMIC_LOAD
	if (!gstreamer_load())
		return 0;
#endif /* GSTREAMER_DYNAMIC_LOAD */

	/* Initialize GStreamer -- do not call gst_deinit because it isn't subsequently possible to reinitialize in the same process */
	if (!GST_gst_init_check(NULL, NULL, NULL)) {
#if GSTREAMER_DYNAMIC_LOAD
		gstreamer_unload();
#endif /* GSTREAMER_DYNAMIC_LOAD */

		return 0;
	}

	gstreamer_initialized = 1;
	return 1;
}
