#ifndef __VCUT_H
#define __VCUT_H

#include <stdio.h>
#include <ogg/ogg.h>
#include <vorbis/codec.h>

typedef struct {
	int length;
	unsigned char *packet;
} vcut_packet;

typedef struct {
	ogg_sync_state *sync_in;
	ogg_stream_state *stream_in;
	vorbis_dsp_state *vd;
	vorbis_block *vb;
	vorbis_info *vi;
	int prevW;
	ogg_int64_t initialgranpos;
	int time;
	ogg_int64_t cutpoint;
	unsigned int serial;
	vcut_packet **headers; /* 3 */
	vcut_packet **packets; /* 2 */

	FILE *in,*out1,*out2;
} vcut_state;

int vcut_process(vcut_state *state);
void vcut_set_files(vcut_state *s, FILE *in, FILE *out1, FILE *out2);
void vcut_set_cutpoint(vcut_state *s, ogg_int64_t cutpoint, int time);
vcut_state *vcut_new(void);
void vcut_free(vcut_state *state);

#endif /* __VCUT_H */
