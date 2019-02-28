/*  bitcorder A video streaming application
 *  Copyright (C) 2019 Daniel Patrick Johnson
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU Lesser Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU Lesser Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser Public License
 *  along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#include <config.h>
#include <stdio.h>
#include <error.h>
#include <argp.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <strings.h>
#include <gst/gst.h>

/* Avoiding heap allocation. This might be dumb */
enum default_names { DFT_EMPTY = 0, DFT_LOCALHOST, DFT_EXAMPLE_COM, DFT_KEY, DFT_FLASHVER };
static char* default_strings[] = {
	[DFT_EMPTY] = "",
	[DFT_LOCALHOST] = "localhost",
	[DFT_EXAMPLE_COM] = "http://example.com/app",
	[DFT_KEY] = "XXXX-XXXX-XXXX-XXXX",
	[DFT_FLASHVER] = "FME/3.0%20(compatible;%20FMSc%201.0)"
};

/* argp from glibc requires these to be externally linked globals */
/* can't even use static to limit scope to file */
const char * argp_program_bug_address = "Daniel Patrick Johnson <teknotus@gmail.com>";
const char * argp_program_version = "zero";

enum primary_opts { CAPTURE = 256, CAMERA, IMAGE, MONITOR, OUTPUT, AUDIO, VIDEO_BITRATE, AUDIO_BITRATE, RTP, RTMP, SAVE, RPI };

enum subopt_keys { XID=0, XNAME, DISPLAY, FRAMERATE, SHOW_POINTER, CHOOSE_WINDOW,
	MONITOR_SINK, CHOOSE_DEVICE, HOST, PORT, SERVICE, URL, STREAM_KEY, TEST, FILENAME,
	// FIXME add WIDTH, HEIGHT for camera
	DEVICE, FOURCC, // PNG, JPEG,
	LEFT, TOP, RIGHT, BOTTOM, SCALE_WIDTH, SCALE_HEIGHT,
        XPOS, YPOS, ZORDER, ALPHA, EFFECT,
	FORMAT,
	THE_END };
/* xpos ypos zorder alpha effect device png */
char * subopt_names[] = {
	[XID] = "xid",
	[XNAME] = "xname",
	[DISPLAY] = "display",
	[FRAMERATE] = "framerate",
	[SHOW_POINTER] = "show-pointer",
	[CHOOSE_WINDOW] = "choose_window", // interactive window choosing
	[MONITOR_SINK] = "monitor_sink", // xvimage gtk glimage?
	[CHOOSE_DEVICE] = "choose_device", // interactive camera/microphone choosing
	[HOST] = "host",
	[PORT] = "port",
	[SERVICE] = "service",
	[URL] = "url",
	[STREAM_KEY] = "key",
	[TEST] = "test",
	[FILENAME] = "filename",
	[DEVICE] = "device",
	[FOURCC] = "fourcc",
	/*[PNG] = "png", * Autodetected!
	[JPEG] = "jpeg", * Wheeeeeeeeee */
	[LEFT] = "left", // crop left right top bottom
	[TOP] = "top",
	[RIGHT] = "right",
	[BOTTOM] = "bottom",
	[SCALE_WIDTH] = "scale_width", // scale image to size
	[SCALE_HEIGHT] = "scale_height",
	[XPOS] = "xpos", // where to render in composite image
	[YPOS] = "ypos",
	[ZORDER] = "zorder", // layer in composite
	[ALPHA] = "alpha", // composite alpha blend
	[EFFECT] = "effect", // gleffect #
	[FORMAT] = "format", // audio format ogg, mp3, aac, etc
	[THE_END] = NULL
};
struct composite_options {
	bool use_crop;
	bool use_scale;
	enum primary_opts type; // FIXME hack. Gotta be a better way.
	// crop
	uint32_t left;
	uint32_t top;
	uint32_t right;
	uint32_t bottom;
	// scale
	int32_t scale_width;
	int32_t scale_height;
	// position
	int32_t xpos;
	int32_t ypos;
	// stacking order
	int zorder;
	double alpha;
	// Optional GL Effect
	// Possibly better to put this someplace else
	int effect;
};
struct camera_options {
	char * device;
	uint32_t width; // need different parse names
	uint32_t height; // than scale
	uint32_t framerate; // cannot handle 29.97 fps FIXME
	char * fourcc;
	struct composite_options composite;
};
struct image_options {
	char * filename;
	struct composite_options composite;
};
struct window_options {
	uint32_t xid;
	char * xname;
	char * display;
	uint32_t framerate;
	bool show_pointer;
	struct composite_options composite;
};
struct output_options {				// Kind of opposite of composite
	uint32_t framerate;
	struct composite_options composite;	// But mostly the same stuff
};						// Hack FIXME
enum audio_format { AAC = 0, MP3, INVALID_FORMAT }; //FIXME add ogg flac opus
struct audio_options {
	enum audio_format format;
};// FIXME add alsa/jack/pulse
char * audio_format_names[] = {
	[AAC] = "aac",
	[MP3] = "mp3",
	[INVALID_FORMAT] = "invalid_format"
};

struct rtp_options {
	char * host;
	uint32_t port;
};
enum rtmp_service { YOUTUBE = 0, TWITCH, INVALID_SERVICE };
char * rtmp_service_names[] = {
	[YOUTUBE] = "youtube",
	[TWITCH] = "twitch",
	[INVALID_SERVICE] = "invalid_service"
};
struct rtmp_options {
	enum rtmp_service service;
	char *url;
	char *key;
	bool test;
};
struct save_options {
			// probably add some kind of format picking
	char * filename;
};
struct arguments {
	bool use_monitor;
	bool use_rtmp;
	bool use_rtp;
	bool use_save;
	bool use_audio;
	uint32_t video_bitrate;
	uint32_t audio_bitrate;
	struct camera_options camera;
	struct image_options image;
	struct window_options window;
	struct output_options output;
	struct audio_options audio;
	struct rtp_options rtp;
	struct rtmp_options rtmp;
	struct save_options save;
};

void parse_composite(struct arguments * args, enum primary_opts source,  enum subopt_keys key, char *value){
	printf("parse_composite\n");
	struct composite_options *opt = NULL;
	double alpha = 1.0;
	int64_t num = 0;
	switch(source){
		case CAPTURE:
			opt = &args->window.composite;
			break;
		case CAMERA:
			opt = &args->camera.composite;
			break;
		case IMAGE:
			opt = &args->image.composite;
			break;
		case OUTPUT:
			opt = &args->output.composite;
		default:
			printf("unknown source\n");
	}
	if(key == ALPHA){
		alpha = strtod(value, NULL);
	} else {
		num = strtol(value, NULL, 0);
	}
	switch(key){
		case LEFT:
			printf("crop left\n");
			opt->use_crop = true;
			opt->left = num;
			break;
		case TOP:
			printf("crop top\n");
			opt->use_crop = true;
			opt->top = num;
			break;
		case RIGHT:
			printf("crop right\n");
			opt->use_crop = true;
			opt->right = num;
			break;
		case BOTTOM:
			printf("crop bottom\n");
			opt->use_crop = true;
			opt->bottom = num;
			break;
		case SCALE_WIDTH:
			printf("scale width\n");
			if(num > 0 && opt->scale_height > 0) opt->use_scale = true;
			opt->scale_width = num;
			break;
		case SCALE_HEIGHT:
			printf("scale height\n");
			if(num > 0 && opt->scale_width > 0) opt->use_scale = true;
			opt->scale_height = num;
			break;
		case XPOS:
			printf("composite x\n");
			opt->xpos = num;
			break;
		case YPOS:
			printf("composite y\n");
			opt->ypos = num;
			break;
		case ZORDER:
			printf("zorder\n");
			opt->zorder = num;
			break;
		case ALPHA:
			printf("alpha\n");
			opt->alpha = alpha;
			break;
		case EFFECT:
			printf("gleffect\n");
			opt->effect = num;
			break;
		default:
			printf("parse_composite huh?\n");
	
	}
};

struct arguments init_args(){
	struct arguments args = { 0 };
	struct window_options winopt = { 0 };
	struct camera_options camopt = { 0 };
	struct image_options imgopt = { 0 };
	struct output_options outopt = { 0 };
	struct rtp_options rtpopt = { 0 };
	struct rtmp_options rtmpopt = { 0 };
	struct save_options saveopt = { 0 };
	winopt.xid = 0;
	winopt.xname = default_strings[DFT_EMPTY];
	winopt.display = default_strings[DFT_EMPTY];
	winopt.framerate = 30;
	winopt.show_pointer = false;
	winopt.composite.alpha = 1.0;
	winopt.composite.type = CAPTURE;

	camopt.composite.alpha = 1.0;
	camopt.composite.type = CAMERA;

	imgopt.composite.alpha = 1.0;
	imgopt.composite.type = IMAGE;

	outopt.composite.type = OUTPUT;

	rtpopt.host = default_strings[DFT_LOCALHOST];
	rtpopt.port = 6970;

	rtmpopt.service = INVALID_SERVICE;
	rtmpopt.url = default_strings[DFT_EXAMPLE_COM];
	rtmpopt.key = default_strings[DFT_KEY];

	saveopt.filename = default_strings[DFT_EMPTY];

	args.use_monitor = false;
	args.use_rtmp = false;
	args.use_rtp = false;
	args.use_save = false;
	args.use_audio = false;
	args.video_bitrate = 0;
	args.audio_bitrate = 0;
	args.window = winopt;
	args.camera = camopt;
	args.image = imgopt;
	args.output = outopt;
	args.rtp = rtpopt;
	args.rtmp = rtmpopt;
	args.save = saveopt;
	return args;
}

/* this doesn't have to be global, but putting it here made it easier to keep option parsing on same screen. */
struct argp_option options[] = {
	{ "win", CAPTURE, "xid=...,xname...", 0, "Which window to capture", 0 },
	{ "      --win xid=...", 0, 0, OPTION_DOC, "Specify by Xwindows ID", 1 },
	{ "      --win xname=...", 0, 0, OPTION_DOC, "Specify by Xwindows Title", 2 },
	{ "      --win framerate=...", 0, 0, OPTION_DOC, "Frames per second", 3 },
	{ "      --win show_pointer", 0, 0, OPTION_DOC, "include pointer in window capture", 4 },
	{ "cam", CAMERA, "device=/dev/videoX,...", 0, "Which camera to capture", 5 },
	{ "      --cam device=...", 0, 0, OPTION_DOC, "Specify by /dev/videoX", 6 },
	{ "      --cam framerate=...", 0, 0, OPTION_DOC, "frames per second", 7 },
	{ "      --cam width=...", 0, 0, OPTION_DOC, "capture width", 8 },
	{ "      --cam height=...", 0, 0, OPTION_DOC, "capture height", 9 },
	{ "      --cam fourcc=...", 0, 0, OPTION_DOC, "Example YUY2", 10 },
	{ "img", IMAGE, "filename=exampe.png", 0, "filename for static image png/jpeg", 11 },
	{ "out", OUTPUT, "filename=vid.mkv,scale_...", 0, "output filters/filename", 12 },
	{ "Common options win, cam, img, out", 0, 0, OPTION_DOC, "Compositing", 13 },
	{ "  left=...", 0, 0, OPTION_DOC, "x of left crop", 14 },
	{ "  top=...", 0, 0, OPTION_DOC, "y of top crop", 15 },
	{ "  right=...", 0, 0, OPTION_DOC, "x of right crop", 16 },
	{ "  bottom=...", 0, 0, OPTION_DOC, "y of bottom crop", 17 },
	{ "  scale_width=...", 0, 0, OPTION_DOC, "width to scale to", 18 },
	{ "  scale_height=...", 0, 0, OPTION_DOC, "height to scale to", 19 },
	{ "  effect=...", 0, 0, OPTION_DOC, "gleffect number to apply", 20 },
	{ "  ***  following not for out", 0, 0, OPTION_DOC, "***", 21 },
	{ "  xpos=...", 0, 0, OPTION_DOC, "x of top left corner in composite", 22 },
	{ "  ypos=...", 0, 0, OPTION_DOC, "y of top left corner in composite", 23 },
	{ "  zorder=...", 0, 0, OPTION_DOC, "position in image stack of composite", 24 },
	{ "  alpha=...", 0, 0, OPTION_DOC, "alpha blend value in composite", 25 },
	{ "vid_rate", VIDEO_BITRATE, "...", 0, "video bitrate",  26 },
	{ "aud_rate", AUDIO_BITRATE, "...", 0, "audio bitrate", 27 },
	{ "audio", AUDIO, "format=mp3", 0, "audio encoding format", 28 },
	{ "rtp", RTP, "host=...,port...", 0, "stream to real time protocol", 29 },
	{ "      --rtp host=...", 0, 0, OPTION_DOC, "hostname or IP address", 30 },
	{ "      --rtp port=...", 0, 0, OPTION_DOC, "port IETF rec 6970...6999", 31 },
	{ "rtmp", RTMP, "url=...,key...", 0, "Stream video to distribution network", 32 },
	{ "      --rtmp service=...", 0, 0, OPTION_DOC, "youtube or twitch", 33 },
	{ "      --rtmp url=...", 0, 0, OPTION_DOC, "rtmp://...", 34 },
	{ "      --rtmp key=...", 0, 0, OPTION_DOC, "XXXX-XXXX-XXXX-XXXX", 35 },
	{ "save", SAVE, "filename=...mkv", 0, "save video to file", 36 },
	{ 0 }
};
error_t argp_callback(int key, char *arg, struct argp_state *state){
	struct arguments * arrrgs = state->input;
	char *empty = default_strings[DFT_EMPTY];
	char *subopts = empty, *value;
	if(arg != NULL)
		subopts = arg;
	int num = 0;
	int subkey;
	printf("argp callback called key: ");
	if(key >= 20 && key <= 126)
		printf("%c ", key);
	printf("\t0x%08x\n", key);

	switch(key) {
	case CAPTURE:
		printf("capture window\n");
		while(*subopts != '\0'){
			subkey = getsubopt(&subopts, subopt_names, &value);
			printf("subkey: %d value: %s\n", subkey, value);
			if(subkey >= LEFT){
				printf("Common option %d\n", subkey);
				parse_composite(arrrgs, key, subkey, value);
			}
			else
			switch(subkey){
			case XID:
				if(value != NULL){
					printf("XID: %s\n", value);
					num = strtol(value, NULL, 0);
					printf("XID as number %d 0x%08x\n", num, num);
					arrrgs->window.xid = num;
				}
				break;
			case XNAME:
				if(value != NULL){
					printf("XNAME: %s\n", value);
					arrrgs->window.xname = value;
				}
				break;
			case DISPLAY:
				if(value != NULL){
					printf("DISPLAY: %s\n", value);
					arrrgs->window.display = value;
				}
				break;
			case FRAMERATE:
				if(value != NULL){
					printf("FRAMERATE: %s\n", value);
					num = strtol(value, NULL, 0);
					printf("FRAMERATE as number %d\n", num);
					arrrgs->window.framerate = num;
				}
				break;
			case SHOW_POINTER:
				arrrgs->window.show_pointer = true;
				break;
			default:
				printf("Something is broken\n");
			}
		}
		break;
	case CAMERA:
		printf("camera\n");
		while(*subopts != '\0'){
			subkey = getsubopt(&subopts, subopt_names, &value);
			printf("subkey: %d value: %s\n", subkey, value);
			if(subkey >= LEFT){
				printf("Common option %d\n", subkey);
				parse_composite(arrrgs, key, subkey, value);
			}
			else
			switch(subkey){
			case DEVICE:
				if(value != NULL){
					printf("DEVICE: %s\n", value);
					arrrgs->camera.device = value;
				}
				break;
			case FRAMERATE:
				if(value != NULL){
					num = strtol(value, NULL, 0);
					printf("FRAMERATE: as number %d\n", num);
					arrrgs->camera.framerate = num;
				}
			case FOURCC:
				if(value != NULL){
					printf("FOURCC: %s\n", value);
					arrrgs->camera.fourcc = value;
				}
				break;
			default:
				printf("camera option: %d not implemented yet\n", subkey);
			}
		}
		break;
	case IMAGE:
		printf("image\n");
		while(*subopts != '\0'){
			subkey = getsubopt(&subopts, subopt_names, &value);
			printf("subkey: %d value: %s\n", subkey, value);
			if(subkey >= LEFT){
				printf("Common option %d\n", subkey);
				parse_composite(arrrgs, key, subkey, value);
			}
			else
			switch(subkey){// probably more options in future
			case FILENAME:
				if(value != NULL){
					printf("FILENAME: %s\n", value);
					arrrgs->image.filename = value;
				}
				break;
			}
		}
		break;
	case OUTPUT:
		printf("output\n");
		while(*subopts != '\0'){
			subkey = getsubopt(&subopts, subopt_names, &value);
			printf("subkey: %d value: %s\n", subkey, value);
			if(subkey >= LEFT){
				printf("Common option %d\n", subkey);
				parse_composite(arrrgs, key, subkey, value);
			}
			else
			switch(subkey){// undo composite hack in future
			case FRAMERATE:
				if(value != NULL){
					num = strtol(value, NULL, 0);
					arrrgs->output.framerate = num;
				}
			default:
				printf("output unknown option\n");
			}
		}
		break;
	case AUDIO:
		printf("audio\n");
		while(*subopts != '\0'){
			subkey = getsubopt(&subopts, subopt_names, &value);
			printf("subkey: %d value: %s\n", subkey, value);
			switch(subkey){
			case FORMAT:
				if(value != NULL){
					printf("FORMAT: %s\n", value);
					for(int i=0 ; i < 2 ; i++){
						if(strcasecmp(value, audio_format_names[i]) == 0){
							printf("found format %d\n", i);
							arrrgs->audio.format = i;
							break;
						}
					}
				}
				break;
			default:
				printf("audio unknown option\n");
			}
		}
		break;
	case VIDEO_BITRATE:
		arrrgs->video_bitrate = strtol(subopts, NULL, 0);
		break;
	case AUDIO_BITRATE:
		arrrgs->audio_bitrate = strtol(subopts, NULL, 0);
		break;
	case RTP:
		printf("RTP\n");
		arrrgs->use_rtp = true;
		arrrgs->use_audio = true;
		while(*subopts != '\0'){
			subkey = getsubopt(&subopts, subopt_names, &value);
			printf("subkey: %d value: %s\n", subkey, value);
			switch(subkey){
			case HOST:
				if(value != NULL){
					printf("HOST: %s\n", value);
					arrrgs->rtp.host = value;
				}
				break;
			case PORT:
				if(value != NULL){
					printf("PORT: %s\n", value);
					arrrgs->rtp.port = strtol(value, NULL, 0);
				}
				break;
			}
		}
		break;
	case RTMP:
		printf("RTMP\n");
		arrrgs->use_rtmp = true;
		arrrgs->use_audio = true;
		while(*subopts != '\0'){
			subkey = getsubopt(&subopts, subopt_names, &value);
			printf("subkey: %d value: %s\n", subkey, value);
			switch(subkey){
			case SERVICE:
				if(value != NULL){
					printf("SERVICE: %s\n", value);
					for(int i=0 ; i < 2 ; i++){
						if(strcasecmp(value, rtmp_service_names[i]) == 0){
							printf("found service %d\n", i);
							arrrgs->rtmp.service = i;
							break;
						}
					}
				}
				break;
			case URL:
				if(value != NULL){
					printf("URL: %s\n", value);
					arrrgs->rtmp.url = value;
				}
				break;
			case STREAM_KEY:
				if(value != NULL){
					printf("STREAM_KEY: %s\n", value);
					arrrgs->rtmp.key = value;
				}
				break;
			case TEST:
				arrrgs->rtmp.test = true;
				break;
			}
		}
		break;
	case SAVE:
		printf("SAVE\n");
		while(*subopts != '\0'){
			subkey = getsubopt(&subopts, subopt_names, &value);
			printf("subkey: %d value: %s\n", subkey, value);
			switch(subkey){
			case FILENAME:
				if(value != NULL){
					arrrgs->use_save = true;
					arrrgs->use_audio = true;
					printf("SERVICE: %s\n", value);
					arrrgs->save.filename = value;
				}
				break;
			default:
				printf("unknown save option\n");
			}
		}
		break;
	case ARGP_KEY_END:
		printf("END\n");
		break;
	case ARGP_KEY_NO_ARGS:
		printf("NO_ARGS\n");
		break;
	case ARGP_KEY_INIT:
     		printf("INIT\n");
		break;
	case ARGP_KEY_SUCCESS:
		printf("SUCCESS\n");
		break;
	case ARGP_KEY_ERROR:
		printf("ERROR\n");
		break;
	case ARGP_KEY_ARGS:
     		printf("ARGS\n");
		break;
	case ARGP_KEY_FINI:
     		printf("FINI\n");
	}
	return 0;
}
GstElement * add_composite_pipeline(GstElement *pipeline, GstElement *mixer, struct composite_options *opt){
	// FIXME free or save pointers
	GstElement * last_element;

	GstElement *vidqueue = gst_element_factory_make("queue", NULL);
	gst_bin_add(GST_BIN(pipeline), vidqueue);
	last_element = vidqueue;

	if(opt->use_crop && (opt->type != CAPTURE)){
		GstElement *crop = gst_element_factory_make("videocrop", NULL);
		g_object_set(G_OBJECT(crop), "left", opt->left, "top", opt->top,
			"right", opt->right, "bottom", opt->bottom, NULL);
		gst_bin_add(GST_BIN(pipeline), crop);
		gst_element_link(last_element, crop);
		last_element = crop;
	}
	GstElement *winup = gst_element_factory_make("glupload", NULL);
	GstElement *colorcvt = gst_element_factory_make("glcolorconvert", NULL);
	gst_bin_add_many(GST_BIN(pipeline), winup, colorcvt, NULL);
	gst_element_link_many(last_element, winup, colorcvt, NULL);
	last_element = colorcvt;

	if(opt->use_scale){
		printf("Using scale\n");
		GstElement *colorscale = gst_element_factory_make("glcolorscale", NULL);
		GstCaps *resize_caps;
		resize_caps = gst_caps_new_simple("video/x-raw",
			"width", G_TYPE_INT, opt->scale_width, "height", G_TYPE_INT, opt->scale_height, NULL);
		GstCapsFeatures * feature = gst_caps_features_from_string("memory:GLMemory");
		gst_caps_set_features(resize_caps, 0, feature);
		GstElement *scale_capsfilter = gst_element_factory_make("capsfilter", NULL);
		g_object_set(G_OBJECT(scale_capsfilter), "caps", resize_caps, NULL);

		gst_bin_add_many(GST_BIN(pipeline), colorscale, scale_capsfilter, NULL);
		gst_element_link_many(last_element, colorscale, scale_capsfilter,  NULL);
		last_element = scale_capsfilter;
	}
	if(opt->effect > 0){
		GstElement *effect = gst_element_factory_make("gleffects", NULL);
		g_object_set(G_OBJECT(effect), "effect", opt->effect, NULL);
		gst_bin_add(GST_BIN(pipeline), effect);
		gst_element_link(last_element, effect);
		last_element = effect;
	}

	gst_element_link(last_element, mixer);
	GstPad *capspad = gst_element_get_static_pad(last_element, "src");
	GstPad *mixpad0 = gst_pad_get_peer(capspad);
	g_object_set(G_OBJECT(mixpad0), "xpos", opt->xpos, "ypos", opt->ypos, "zorder", opt->zorder,
		"alpha", opt->alpha, NULL);
	return vidqueue;
}
static void image_decode_new_pad (GstElement *dec, GstPad *decpad, gpointer usrptr){
	GstElement *freeze = (GstElement *)usrptr;
	gst_element_link(dec, freeze);
}

// Block fixed caps from downstream causing crop instead of scale
// FIXME refactor more?
GstPadProbeReturn block_caps_probe (GstPad *pad, GstPadProbeInfo *info, gpointer data){
	GQuark capsq = g_quark_try_string("caps");
	GstPadProbeType type = GST_PAD_PROBE_INFO_TYPE(info);
	//printf("caps block probe ====================== ");

	if(type & GST_PAD_PROBE_TYPE_QUERY_DOWNSTREAM){
		//printf("Query Downstream %s\n", GST_QUERY_TYPE_NAME(GST_PAD_PROBE_INFO_QUERY(info)));
		GstQuery * query = gst_pad_probe_info_get_query(info);
		GstQueryType query_type = GST_QUERY_TYPE(query);
		GQuark query_quark = gst_query_type_to_quark(query_type);
		if(capsq == query_quark){
			//printf("query struct unfiltered: %s\n", gst_structure_to_string(gst_query_get_structure (GST_PAD_PROBE_INFO_QUERY(info))));
			GQuark g_int = g_type_from_name("gint");
			GQuark width_quark = g_quark_try_string("width");
			GQuark height_quark = g_quark_try_string("height");
			GstCaps * caps;
			gst_query_parse_caps_result(query, &caps);
			int capssize = caps != NULL ? gst_caps_get_size(caps) : 0;
			// FIXME maybe be one loop instead of a check loop, and a filter loop
			// Should at least not need to do full check on second pass
			// Really should fix code upstream in gstreamer to not need this hack
			bool replace_query = false;
			for(int i=0 ; i < capssize ; i++){
				GstStructure * cap = gst_caps_get_structure(caps, i);
				const GValue * width_value = gst_structure_id_get_value(cap, width_quark);
				const GValue * height_value = gst_structure_id_get_value(cap, height_quark);

				if(G_VALUE_TYPE(width_value) == g_int || G_VALUE_TYPE(height_value) == g_int){
					printf("index: %d width or height is fixed\n", i);
					replace_query = true;
				}
			}
			if(replace_query){
				GstCaps * nocrop_caps = gst_caps_new_empty();
				for(int i=0 ; i < capssize ; i++){
					GstStructure * cap = gst_caps_get_structure(caps, i);
					GstCapsFeatures * features = gst_caps_get_features(caps, i);
					const GValue * width_value = gst_structure_id_get_value(cap, width_quark);
					const GValue * height_value = gst_structure_id_get_value(cap, height_quark);
					if(G_VALUE_TYPE(width_value) != g_int && G_VALUE_TYPE(height_value) != g_int){
						gst_caps_append_structure_full(nocrop_caps, gst_structure_copy(cap),
							gst_caps_features_copy(features));
					}
				}
				gst_query_set_caps_result(query, nocrop_caps);
			}
		}
		//printf("query struct filtered: %s\n", gst_structure_to_string(gst_query_get_structure (GST_PAD_PROBE_INFO_QUERY(info))));
	}
	return GST_PAD_PROBE_OK;
}

int main(int argc, char *argv[])
{
	uint32_t default_audio_bitrate = 128000;
	uint32_t default_rtp_port = 6970;
	char doc[] = "I bet this program does something useful.";
	char more_doc[] = "what does this do?";
	struct arguments arrrgs = init_args();
	struct argp argp_stuff = { options, argp_callback, more_doc, doc, 0, 0, 0};
	argp_parse(&argp_stuff, argc, argv, 0, 0, &arrrgs);
	printf("Parsed Options\n");
	printf("xid: 0x%08x\n", arrrgs.window.xid);
	printf("xname: %s\n", arrrgs.window.xname);
	printf("display: %s\n", arrrgs.window.display);
	printf("framerate: %d\n", arrrgs.window.framerate);
	printf("pointer: %s\n", arrrgs.window.show_pointer ? "TRUE" : "FALSE");
	printf("video_bitrate: %d\n", arrrgs.video_bitrate);
	printf("audio_bitrate: %d\n", arrrgs.audio_bitrate);
	printf("use_rtp: %s\n", arrrgs.use_rtp ? "TRUE" : "FALSE");
	printf("rtp host: %s\n", arrrgs.rtp.host);
	printf("rtp port: %d\n", arrrgs.rtp.port);
	printf("use_rtmp: %s\n", arrrgs.use_rtmp ? "TRUE" : "FALSE");
	printf("rtmp service: %s\n", rtmp_service_names[arrrgs.rtmp.service]);
	printf("rtmp url: %s\n", arrrgs.rtmp.url);
	printf("rtmp stream_key: %s\n", arrrgs.rtmp.key);
	printf("window composite use crop: %s\n", arrrgs.window.composite.use_crop ? "TRUE" : "FALSE");
	printf("window composite left: %d\n", arrrgs.window.composite.left);
	printf("output scale_width: %d\n", arrrgs.output.composite.scale_width);
	if(arrrgs.use_save){
		printf("save filename: %s\n", arrrgs.save.filename);
	}

	GstClock *clock;
	GMainLoop *loop;
	GstElement *pipeline;
	GstElement *audiobin;
	GstElement *audiotee;
	GstElement *audio_rtp_queue;
	GstElement *audio_rtmp_queue;
	GstElement *audio_save_queue;
	GstElement *videncbin;
	GstElement *videnctee;
	GstElement *video_rtp_queue;
	GstElement *video_rtmp_queue;
	GstElement *video_save_queue;
	GstElement *h264enc;
	GstElement *rtpbin;
	GstElement *rtpsink;
	GstElement *tsmux;
	GstElement *flashmux;
	GstElement *savemux;
	GstElement *preenc;
	GstElement *rtmpbin;
	GstElement *savebin;
	GstElement *savesink;
	GstElement *streamsink;
	GstElement *window_el, *vidqueue;
	GstElement *audio_enc;

	GstCaps *framerate_caps;
	GstCaps *resize_caps;

	gst_init(NULL,NULL);

	/* audio pipeline */
	switch(arrrgs.audio.format){
		case AAC:
			printf("case AAC\n");
			audiobin = gst_parse_bin_from_description(
			"pulsesrc ! queue ! audioconvert ! avenc_aac name=audio_enc ! aacparse "
			"! audio/mpeg,mpegversion=4,stream-format=raw ! queue",
			true, NULL);
			break;
		case MP3:
			printf("case MP3\n");
			audiobin = gst_parse_bin_from_description(
			"pulsesrc ! queue ! audioconvert ! lamemp3enc name=audio_enc target=1 ! mpegaudioparse ! queue",
			true, NULL);
			break;
		default:
			printf("switch unknown audio format\n");
	}

	audio_enc = gst_bin_get_by_name(GST_BIN(audiobin), "audio_enc");
	g_object_set(G_OBJECT(audio_enc), "bitrate", arrrgs.audio_bitrate != 0 ? arrrgs.audio_bitrate : default_audio_bitrate, NULL);
	if(arrrgs.audio_bitrate == 0)
		arrrgs.audio_bitrate = default_audio_bitrate;

	/* video compress pipeline */
	videncbin = gst_parse_bin_from_description(
		"queue ! videoconvert ! queue ! vaapih264enc name=h264enc max-bframes=0 tune=3"
		" ! queue ! h264parse config-interval=1 ! queue", true, NULL);
		/* need to expose all of the compression tuning controls */
	h264enc = gst_bin_get_by_name(GST_BIN(videncbin), "h264enc");
	if(arrrgs.video_bitrate > 0){
		g_object_set(G_OBJECT(h264enc), "bitrate", arrrgs.video_bitrate, NULL);
	} else {
		g_object_get(G_OBJECT(h264enc), "bitrate", &arrrgs.video_bitrate, NULL);
		/* well crap I should have known this would not have a number before video starts */
		/* I guess fix this when adding interactive features */
		// FIXME
	}

	/* rtp pipeline */
	rtpbin = gst_parse_bin_from_description(
		"queue ! rtpmp2tpay ! udpsink name=rtpsink", true, NULL );

	/* rtmp pipeline */
	rtmpbin = gst_parse_bin_from_description("queue leaky=downstream ! rtmpsink name=streamsink", true, NULL);

	/* save pipeline */
	savebin = gst_parse_bin_from_description("queue ! filesink name=savesink", true, NULL);

	/* main pipeline */
	pipeline = gst_parse_launch(
		"glcolorconvert name=glcc ! gldownload ! queue ! tee name=vid_raw_tee ! queue ! gtksink "
		"vid_raw_tee. ! queue name=preenc", NULL);
	GstElement *glcc = gst_bin_get_by_name(GST_BIN(pipeline), "glcc");
	GstElement *mix = gst_element_factory_make("glvideomixerelement", NULL);
	GstPad *mixsrc = gst_element_get_static_pad(mix, "src");
	gst_bin_add(GST_BIN(pipeline), mix);

	if(arrrgs.output.framerate > 0 || arrrgs.output.composite.use_scale){
		GstElement *out_filter = gst_element_factory_make("capsfilter", NULL);
		gst_bin_add(GST_BIN(pipeline), out_filter);
		GstCaps *out_caps = gst_caps_new_empty();
		GstStructure *out_caps_struct = gst_structure_new_empty("video/x-raw");
		GstCapsFeatures * out_caps_feature = gst_caps_features_from_string("memory:GLMemory");

		if(arrrgs.output.framerate > 0){
			printf("set output framerate: %d\n", arrrgs.output.framerate);
			GValue rate = G_VALUE_INIT;
			g_value_init(&rate, GST_TYPE_FRACTION);
			gst_value_set_fraction(&rate, arrrgs.output.framerate, 1);
			gst_structure_set_value(out_caps_struct, "framerate", &rate);
		}
		if(arrrgs.output.composite.use_scale){
			printf("scalefilter\n");
			gst_pad_add_probe(mixsrc, GST_PAD_PROBE_TYPE_QUERY_DOWNSTREAM,
				(GstPadProbeCallback) block_caps_probe, NULL, NULL);
			GstElement *out_scale = gst_element_factory_make("glcolorscale", NULL);
			GValue width = G_VALUE_INIT;
			GValue height = G_VALUE_INIT;
			g_value_init(&width, G_TYPE_INT);
			g_value_init(&height, G_TYPE_INT);
			g_value_set_int(&width, arrrgs.output.composite.scale_width);
			g_value_set_int(&height, arrrgs.output.composite.scale_height);
			gst_structure_set_value(out_caps_struct, "width", &width);
			gst_structure_set_value(out_caps_struct, "height", &height);
			gst_bin_add(GST_BIN(pipeline), out_scale);
			gst_element_link_many(mix, out_scale, out_filter, glcc, NULL);
		} else {
			gst_element_link_many(mix, out_filter, glcc, NULL);
		}
		gst_caps_append_structure_full(out_caps, out_caps_struct, out_caps_feature);
		g_object_set(G_OBJECT(out_filter), "caps", out_caps, NULL);
	} else {
		gst_element_link(mix, glcc);
	}
	// FIXME add glfilter
	preenc = gst_bin_get_by_name(GST_BIN(pipeline), "preenc");
	
	// FIXME Should really only need one vidqueue
	vidqueue = add_composite_pipeline(pipeline, mix, &arrrgs.window.composite);
	GstElement *vidqueue2 = add_composite_pipeline(pipeline, mix, &arrrgs.camera.composite);
	GstElement *vidqueue3 = add_composite_pipeline(pipeline, mix, &arrrgs.image.composite);

	if(arrrgs.image.filename != NULL){
		GstElement *image = gst_element_factory_make("filesrc", NULL);
		printf("image location: %s\n", arrrgs.image.filename);
		g_object_set(G_OBJECT(image), "location", arrrgs.image.filename, NULL);
		//GstElement *image_dec = gst_element_factory_make("jpegdec", NULL);
		GstElement *image_dec = gst_element_factory_make("decodebin", NULL);
		GstElement *freeze = gst_element_factory_make("imagefreeze", NULL);

		g_signal_connect(image_dec, "pad-added", G_CALLBACK(image_decode_new_pad), freeze);

		GstElement *freeze_convert = gst_element_factory_make("videoconvert", NULL);
		gst_bin_add_many(GST_BIN(pipeline), image, image_dec, freeze, freeze_convert, NULL);
		gst_element_link(image, image_dec);
		gst_element_link_many(freeze, freeze_convert, vidqueue3, NULL);
	}

	if(arrrgs.camera.device != NULL){
		GstElement *cam = gst_element_factory_make("v4l2src", NULL);
		g_object_set(G_OBJECT(cam), "device",  arrrgs.camera.device , NULL);
		gst_bin_add(GST_BIN(pipeline), cam);
		gst_element_link(cam, vidqueue2);
	}

	// FIXME make this work if not recording any windows
	window_el = gst_element_factory_make("ximagesrc", "window_el");
	g_object_set(G_OBJECT(window_el),"use-damage", FALSE, NULL);
	if(arrrgs.window.display[0] != '\0')
		g_object_set(G_OBJECT(window_el),"display-name", arrrgs.window.display, NULL);
	g_object_set(G_OBJECT(window_el),"show-pointer", arrrgs.window.show_pointer, NULL);
	g_object_set(G_OBJECT(window_el),"xid", arrrgs.window.xid, NULL);
	if(arrrgs.window.composite.use_crop){
		g_object_set(G_OBJECT(window_el),
			"startx", arrrgs.window.composite.left,
			"starty", arrrgs.window.composite.top,
			"endx", arrrgs.window.composite.right,
			"endy", arrrgs.window.composite.bottom, NULL);
	}
	gst_bin_add(GST_BIN(pipeline), window_el);

	framerate_caps = gst_caps_new_simple("video/x-raw",
					     "framerate", GST_TYPE_FRACTION, arrrgs.window.framerate, 1,
					     NULL);
	gst_element_link_filtered(window_el, vidqueue, framerate_caps);

	/* add audio to pipeline */
	if(arrrgs.use_audio){
		audiotee = gst_element_factory_make("tee", "audiotee");
		gst_bin_add_many(GST_BIN(pipeline), audiobin, audiotee, NULL);
		gst_element_link(audiobin, audiotee);
	}

	/* add video encoder to pipeline */
	if(arrrgs.use_rtp || arrrgs.use_rtmp || arrrgs.use_save){
		videnctee = gst_element_factory_make("tee", "videnctee");
		gst_bin_add_many(GST_BIN(pipeline), videncbin, videnctee, NULL);
		gst_element_link(preenc, videncbin);
		gst_element_link(videncbin,videnctee);
	}

	/* add rtp to pipeline */
	if(arrrgs.use_rtp){
		rtpsink = gst_bin_get_by_name(GST_BIN(rtpbin),"rtpsink");
		g_object_set(G_OBJECT(rtpsink), "host", arrrgs.rtp.host, NULL);
		g_object_set(G_OBJECT(rtpsink), "port", arrrgs.rtp.port, NULL);
		tsmux = gst_element_factory_make("mpegtsmux", "tsmux");
		video_rtp_queue = gst_element_factory_make("queue", "video_rtp_queue");
		gst_bin_add_many(GST_BIN(pipeline), video_rtp_queue, tsmux, rtpbin, NULL);
		gst_element_link(videnctee, video_rtp_queue);
		gst_element_link(video_rtp_queue, tsmux);
		gst_element_link(tsmux, rtpbin);

		/* link audio */
		audio_rtp_queue = gst_element_factory_make("queue", "audio_rtp_queue");
		gst_bin_add(GST_BIN(pipeline), audio_rtp_queue);
		gst_element_link(audiotee, audio_rtp_queue);
		gst_element_link(audio_rtp_queue, tsmux);
	}

	/* rtmp AKA YouTube/Twitch */

	/* find part after last slash in URL */
	char * service_app;
	int lastslash = 0;
	if(arrrgs.use_rtmp){
		int url_length = strlen(arrrgs.rtmp.url);
		for(int i = 0 ; i < url_length ; i++){
			// printf("url char: %c\n", arrrgs.rtmp.url[i]);
			if(arrrgs.rtmp.url[i] == '/'){
				// printf("found / at: %d\n", i);
				lastslash = i;
			}
		}
		service_app = &arrrgs.rtmp.url[lastslash + 1];
		printf("service_app: %s\n", service_app);
		printf("audio_bitrate: %d video_bitrate: %d\n", arrrgs.audio_bitrate, arrrgs.video_bitrate);
		// mp3 bitrate settings are already divided, aac are not
		uint32_t datarate = (arrrgs.audio_bitrate + arrrgs.video_bitrate)/1000; // 1024? // FIXME

		char rtmp_sink_location[512] = {0};
		switch(arrrgs.rtmp.service){
			case YOUTUBE:
				printf("RTMP service YouTube\n");
				snprintf(rtmp_sink_location, 512,
				"%s/x/%s?videoKeyframeFrequency=1&totalDatarate=%d "
				"app=%s flashVer=%s swfUrl=%s",
				arrrgs.rtmp.url, arrrgs.rtmp.key, datarate, service_app,
				default_strings[DFT_FLASHVER], arrrgs.rtmp.url);
				break;
			case TWITCH:
				printf("RTMP service twitch\n");
				snprintf(rtmp_sink_location, 512,
				"%s/%s%s "
				"app=%s live=1 flashVer=%s",
				arrrgs.rtmp.url, arrrgs.rtmp.key, arrrgs.rtmp.test ? "?bandwidthtest=true" : "", service_app,
				default_strings[DFT_FLASHVER]);
				break;
		}

		printf("rtmp_sink_location: %s\n", rtmp_sink_location);

		// set sink properties
		streamsink = gst_bin_get_by_name(GST_BIN(rtmpbin), "streamsink");
		g_object_set(G_OBJECT(streamsink), "location", rtmp_sink_location, NULL);
		// add mux flvmux streamable=true
		// flashmux
		flashmux = gst_element_factory_make("flvmux", "flashmux");
		g_object_set(G_OBJECT(flashmux), "streamable", true, NULL);
		// add queues
		// audio_rtmp_queue
		audio_rtmp_queue = gst_element_factory_make("queue", "audio_rtmp_queue");
		// vidio_rtmp_queue
		video_rtmp_queue = gst_element_factory_make("queue", "video_rtmp_queue");
		// add rtmp bin
		gst_bin_add_many(GST_BIN(pipeline), audio_rtmp_queue, video_rtmp_queue, flashmux, rtmpbin, NULL);
		// link
		
		// link audio
		gst_element_link(audiotee, audio_rtmp_queue);
		gst_element_link(audio_rtmp_queue, flashmux);

		// link video
		gst_element_link(videnctee, video_rtmp_queue);
		gst_element_link(video_rtmp_queue, flashmux);

		// link mux to stream
		gst_element_link(flashmux, rtmpbin);

	}
	/* add save to pipeline */
	if(arrrgs.use_save){
		savesink = gst_bin_get_by_name(GST_BIN(savebin),"savesink");
		g_object_set(G_OBJECT(savesink), "location", arrrgs.save.filename, NULL);
		savemux = gst_element_factory_make("matroskamux", "savemux");
		video_save_queue = gst_element_factory_make("queue", "video_save_queue");
		gst_bin_add_many(GST_BIN(pipeline), video_save_queue, savemux, savebin, NULL);
		gst_element_link(videnctee, video_save_queue);
		gst_element_link(video_save_queue, savemux);
		gst_element_link(savemux, savebin);

		/* link audio */
		audio_save_queue = gst_element_factory_make("queue", "audio_save_queue");
		gst_bin_add(GST_BIN(pipeline), audio_save_queue);
		gst_element_link(audiotee, audio_save_queue);
		gst_element_link(audio_save_queue, savemux);
	}


	loop = g_main_loop_new(NULL, FALSE);

	/* use system clock for timestamps instead of random start clock */
	clock = gst_system_clock_obtain ();
	g_object_set(G_OBJECT(clock), "clock-type", GST_CLOCK_TYPE_REALTIME, NULL);

	gst_element_set_state(pipeline, GST_STATE_PAUSED);
	gst_element_set_state(pipeline, GST_STATE_PLAYING);
	g_main_loop_run(loop);
	gst_element_set_state(pipeline, GST_STATE_NULL);
	gst_object_unref(pipeline);
	return 0;
}

