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
	ogg_int64_t cutpoint;
	unsigned int serial;
	vcut_packet **headers; //3
	vcut_packet **packets; //2
} vcut_state;

int vcut_process(FILE *in, FILE *first, FILE *second, ogg_int64_t cutpoint);

vcut_packet *vcut_save_packet(ogg_packet *packet);

void vcut_submit_headers_to_stream(ogg_stream_state *stream, vcut_state *s);
int vcut_process_headers(vcut_state *s, FILE *in);
void vcut_write_pages_to_file(ogg_stream_state *stream, FILE *file);
void vcut_flush_pages_to_file(ogg_stream_state *stream, FILE *file);
int vcut_update_sync(vcut_state *s, FILE *f);
vcut_state *vcut_new_state(void);
int vcut_process_first_stream(vcut_state *s, ogg_stream_state *stream, FILE *in, FILE *f);
int vcut_process_second_stream(vcut_state *s, ogg_stream_state *stream, FILE *in,FILE *f);
long vcut_get_blocksize(vcut_state *s, vorbis_info *vb, ogg_packet *p);

#endif /* __VCUT_H */
