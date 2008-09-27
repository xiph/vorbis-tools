/* This program is licensed under the GNU General Public License, version 2,
 * a copy of which is included with this program.
 *
 * (c) 2000-2001 Michael Smith <msmith@xiph.org>
 *
 *
 * Simple application to cut an ogg at a specified frame, and produce two
 * output files.
 *
 * last modified: $Id: vcut.c,v 1.9 2003/09/03 07:58:05 calc Exp $
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <ogg/ogg.h>
#include <vorbis/codec.h>
#include <time.h>
#include <errno.h>
#include <string.h>

#include "vcut.h"

#include <locale.h>
#include "i18n.h"

#ifdef HAVE_INTTYPES_H
#include <inttypes.h>
#define FORMAT_INT64 "%" PRId64
#define FORMAT_INT64_TIME "+%" PRId64
#else

#ifdef _WIN32
#define FORMAT_INT64	  "%I64d"
#define FORMAT_INT64_TIME "+%I64d"
#else
#if LONG_MAX!=2147483647L
#define FORMAT_INT64      "%ld"
#define FORMAT_INT64_TIME "+%ld"
#else
#define FORMAT_INT64	  "%lld"
#define FORMAT_INT64_TIME "+%lld"
#endif
#endif
#endif

static vcut_packet *save_packet(ogg_packet *packet)
{
	vcut_packet *p = malloc(sizeof(vcut_packet));

	p->length = packet->bytes;
	p->packet = malloc(p->length);
	memcpy(p->packet, packet->packet, p->length);

	return p;
}

static void free_packet(vcut_packet *p)
{
	if(p)
	{
		if(p->packet)
			free(p->packet);
		free(p);
	}
}

static long get_blocksize(vcut_state *s, vorbis_info *vi, ogg_packet *op)
{
	int this = vorbis_packet_blocksize(vi, op);
	int ret = (this+s->prevW)/4;

	s->prevW = this;
	return ret;
}

static int update_sync(vcut_state *s, FILE *f)
{
	char *buffer = ogg_sync_buffer(s->sync_in, 4096);
	int bytes = fread(buffer,1,4096,f);
	ogg_sync_wrote(s->sync_in, bytes);
	return bytes;
}

/* Returns 0 for success, or -1 on failure. */
static int write_pages_to_file(ogg_stream_state *stream, 
		FILE *file, int flush)
{
	ogg_page page;

	if(flush)
	{
		while(ogg_stream_flush(stream, &page))
		{
			if(fwrite(page.header,1,page.header_len, file) != page.header_len)
				return -1;
			if(fwrite(page.body,1,page.body_len, file) != page.body_len)
				return -1;
		}
	}
	else
	{
		while(ogg_stream_pageout(stream, &page))
		{
			if(fwrite(page.header,1,page.header_len, file) != page.header_len)
				return -1;
			if(fwrite(page.body,1,page.body_len, file) != page.body_len)
				return -1;
		}
	}

	return 0;
}


/* Write the first stream to output file until we get to the appropriate
 * cut point.
 *
 * We need to do the following:
 *   - Adjust the end of the stream to note the new end of stream.
 *   - Change the final granulepos to be the cutpoint value, so that we don't
 *     decode the extra data past this.
 *   - Save the final two packets in the stream to temporary buffers.
 *     These two packets then become the first two packets in the 2nd stream
 *     (we need two packets because of the overlap-add nature of vorbis).
 *   - For each packet, buffer it (it could be the 2nd last packet, we don't
 *     know yet (but we could optimise this decision based on known maximum
 *     block sizes, and call get_blocksize(), because this updates internal
 *     state needed for sample-accurate block size calculations.
 */
static int process_first_stream(vcut_state *s, ogg_stream_state *stream, 
		FILE *in, FILE *f)
{
	int eos=0;
	ogg_page page;
	ogg_packet packet;
	ogg_int64_t granpos, prevgranpos = -1;
	int result;

	while(!eos)
	{
		while(!eos)
		{
			int result = ogg_sync_pageout(s->sync_in, &page);
			if(result==0) break;
			else if(result<0) fprintf(stderr, _("Page error. Corrupt input.\n"));
			else
			{
				granpos = ogg_page_granulepos(&page);
				ogg_stream_pagein(s->stream_in, &page);

				if(granpos < s->cutpoint)
				{
					while(1)
					{
						result=ogg_stream_packetout(s->stream_in, &packet);

						/* throw away result, but update state */
						get_blocksize(s,s->vi,&packet);

						if(result==0) break;
						else if(result==-1)
							fprintf(stderr, _("Bitstream error, continuing\n"));
						else
						{
							if(packet.e_o_s)
								s->e_o_s=1;

							/* We need to save the last packet in the first
							 * stream - but we don't know when we're going
							 * to get there. So we have to keep every packet
							 * just in case.
							 */
							if(s->packets[0])
								free_packet(s->packets[0]);
							s->packets[0] = save_packet(&packet);

							ogg_stream_packetin(stream, &packet);

							/* Flush the stream after the second audio
							 * packet, which is necessary if we need the
							 * decoder to discard some samples from the
							 * beginning of this packet.
							 */
							if(packet.packetno == 4
									&& packet.granulepos != -1)
							{
								if(write_pages_to_file(stream, f,1))
									return -1;
							}
							else if(write_pages_to_file(stream, f,0))
								return -1;
						}
					}
					prevgranpos = granpos;
				}
				else
					eos=1; /* First stream ends somewhere in this page.
							  We break of out this loop here. */

				if(ogg_page_eos(&page) && !eos)
				{
					fprintf(stderr, _("Found EOS before cut point.\n"));
					eos=1;
				}
			}
		}
		if(!eos)
		{
			if(update_sync(s,in)==0) 
			{
				fprintf(stderr, _("Setting EOS: update sync returned 0\n"));
				eos=1;
			}
		}
	}

	/* Now, check to see if we reached a real EOS */
	if(granpos < s->cutpoint)
	{
		fprintf(stderr, 
				_("Cutpoint not within stream. Second file will be empty\n"));
		write_pages_to_file(stream, f,0);

		return -1;
	}

	while((result = ogg_stream_packetout(s->stream_in, &packet))!=0)
	{
		int bs;

		if(packet.e_o_s)
			s->e_o_s=1;
		bs = get_blocksize(s, s->vi, &packet);
		if(prevgranpos == -1)
		{
			/* this is the first audio packet; the second one normally
			 * starts at position 0 */
			prevgranpos = 0;
		}
		else if(prevgranpos == 0 && !packet.e_o_s)
		{
			/* the second packet; if our calculated granule position is
			 * greater than granpos, it means some audio samples must be
			 * discarded from the beginning when decoding (in this case,
			 * the Vorbis I spec. requires that this is the last packet
			 * on its page) */
			prevgranpos = bs;
			if(prevgranpos > granpos)
				prevgranpos = granpos;
		}
		else prevgranpos += bs;

		if(prevgranpos >= s->cutpoint && s->packets[0])
		{
			s->packets[1] = save_packet(&packet);
			packet.granulepos = s->cutpoint; /* Set it! This 'truncates' the 
											  * final packet, as needed. */
			packet.e_o_s = 1;
			ogg_stream_packetin(stream, &packet);
			break;
		}
		if(s->packets[0])
			free_packet(s->packets[0]);
		s->packets[0] = save_packet(&packet);
		ogg_stream_packetin(stream, &packet);
		if(write_pages_to_file(stream,f, 0))
			return -1;
	}

	/* Check that we got at least two packets here, which we need later */
	if(!s->packets[0] || !s->packets[1])
	{
		fprintf(stderr, _("Unhandled special case: first file too short?\n"));
		return -1;
	}

	if(write_pages_to_file(stream,f, 0))
		return -1;

	s->pagegranpos = granpos;
	/* Remaining samples in first packet */
	s->initialgranpos = prevgranpos - s->cutpoint; 

	return 0;
}

/* Process second stream.
 *
 * We need to do more packet manipulation here, because we need to calculate
 * a new granulepos for every packet, since the old ones are now invalid.
 * Start by placing the modified first and second packets into the stream.
 * Then just proceed through the stream modifying packno and granulepos for
 * each packet, using the granulepos which we track block-by-block.
 */
static int process_second_stream(vcut_state *s, ogg_stream_state *stream, 
		FILE *in, FILE *f)
{
	ogg_packet packet;
	ogg_page page;
	int eos=s->e_o_s;
	int result;
	ogg_int64_t page_granpos, current_granpos=s->initialgranpos;
	ogg_int64_t packetnum=0; /* Should this start from 0 or 3 ? */

	packet.bytes = s->packets[0]->length;
	packet.packet = s->packets[0]->packet;
	packet.b_o_s = 0;
	packet.e_o_s = eos;
	packet.granulepos = 0;
	packet.packetno = packetnum++; 
	ogg_stream_packetin(stream,&packet);

	if(eos)
	{
		/* Don't write the second file. Normally, we set the granulepos
		 * of its second audio packet so audio samples will be discarded
		 * from the beginning when decoding; but if that's also the last
		 * packet, the samples will be discarded from the end instead,
		 * which would corrupt the audio. */

		/* We'll still consider this a success; even if we could create
		 * such a short file, it would probably be useless. */
		fprintf(stderr, _("Cutpoint too close to end of file."
				" Second file will be empty.\n"));
	}
	else
	{
		packet.bytes = s->packets[1]->length;
		packet.packet = s->packets[1]->packet;
		packet.b_o_s = 0;
		packet.e_o_s = 0;
		packet.granulepos = s->initialgranpos;
		packet.packetno = packetnum++;
		ogg_stream_packetin(stream,&packet);
	}

	if(ogg_stream_flush(stream, &page)!=0)
	{
		fwrite(page.header,1,page.header_len,f);
		fwrite(page.body,1,page.body_len,f);
	}

	while(ogg_stream_flush(stream, &page)!=0)
	{
		/* Might this happen for _really_ high bitrate modes, if we're
		 * spectacularly unlucky? Doubt it, but let's check for it just
		 * in case.
		 */
		fprintf(stderr, _("ERROR: First two audio packets did not fit into one\n"
				        "       Ogg page. File may not decode correctly.\n"));
		fwrite(page.header,1,page.header_len,f);
		fwrite(page.body,1,page.body_len,f);
	}

	page_granpos = s->pagegranpos - s->cutpoint;
	while(!eos)
	{
		result = ogg_stream_packetout(s->stream_in, &packet);
		if(result==0)  /* another page is needed */
		{
			while(!eos)
			{
				result=ogg_sync_pageout(s->sync_in, &page);
				if(result==0)  /* need more data */
				{
					if(update_sync(s, in)==0)
					{
						fprintf(stderr,
							_("Update sync returned 0, setting EOS\n"));
						eos=1;
					}
				}
				else if(result==-1)
					fprintf(stderr, _("Recoverable bitstream error\n"));
				else  /* got a page */
				{
					ogg_stream_pagein(s->stream_in, &page);
					page_granpos = ogg_page_granulepos(&page)-s->cutpoint;
					break;
				}
			}
		}
		else if(result==-1) fprintf(stderr, _("Bitstream error\n"));
		else  /* got a packet */
		{
			int bs = get_blocksize(s, s->vi, &packet);
			current_granpos += bs;
			if(current_granpos > page_granpos)
			{
				current_granpos = page_granpos;
			}

			if(packet.e_o_s) eos=1;
			packet.granulepos = current_granpos;
			packet.packetno = packetnum++;
			ogg_stream_packetin(stream, &packet);
			if(write_pages_to_file(stream,f, 0))
				return -1;
		}
	}

	return 0;
}

static void submit_headers_to_stream(ogg_stream_state *stream, vcut_state *s) 
{
	int i;
	for(i=0;i<3;i++)
	{
		ogg_packet p;
		p.bytes = s->headers[i]->length;
		p.packet = s->headers[i]->packet;
		p.b_o_s = ((i==0)?1:0);
		p.e_o_s = 0;
		p.granulepos=0;

		ogg_stream_packetin(stream, &p);
	}
}

/* Pull out and save the 3 header packets from the input file.
 * If the cutpoint arg was given as seconds, find the number
 * of samples.
 */
static int process_headers(vcut_state *s)
{
	vorbis_comment vc;
	ogg_page page;
	ogg_packet packet;
	int bytes;
	int i;
	char *buffer;
	ogg_int64_t samples;

	ogg_sync_init(s->sync_in);
	
	vorbis_info_init(s->vi);
	vorbis_comment_init(&vc);

	buffer = ogg_sync_buffer(s->sync_in, 4096);
	bytes = fread(buffer, 1, 4096, s->in);
	ogg_sync_wrote(s->sync_in, bytes);

	if(ogg_sync_pageout(s->sync_in, &page)!=1){
		fprintf(stderr, _("Input not ogg.\n"));
		return -1;
	}

	s->serial = ogg_page_serialno(&page);

	ogg_stream_init(s->stream_in, s->serial);

	if(ogg_stream_pagein(s->stream_in, &page) <0)
	{
		fprintf(stderr, _("Error in first page\n"));
		return -1;
	}

	if(ogg_stream_packetout(s->stream_in, &packet)!=1){
		fprintf(stderr, _("Error in first packet\n"));
		return -1;
	}

	if(vorbis_synthesis_headerin(s->vi, &vc, &packet)<0)
	{
		fprintf(stderr, _("Error in primary header: not Vorbis?\n"));
		return -1;
	}

	s->headers[0] = save_packet(&packet);

	i=0;
	while(i<2)
	{
		while(i<2) {
			int res = ogg_sync_pageout(s->sync_in, &page);
			if(res==0)break;
			if(res==1)
			{
				ogg_stream_pagein(s->stream_in, &page);
				while(i<2)
				{
					res = ogg_stream_packetout(s->stream_in, &packet);
					if(res==0)break;
					if(res<0){
						fprintf(stderr, _("Secondary header corrupt\n"));
						return -1;
					}
					s->headers[i+1] = save_packet(&packet);
					vorbis_synthesis_headerin(s->vi,&vc,&packet);
					i++;
				}
			}
		}
		buffer=ogg_sync_buffer(s->sync_in, 4096);
		bytes=fread(buffer,1,4096,s->in);
		if(bytes==0 && i<2)
		{
			fprintf(stderr, _("EOF in headers\n"));
			return -1;
		}
		ogg_sync_wrote(s->sync_in, bytes);
	}

	vorbis_comment_clear(&vc);

	if(s->time) {
	  samples = s->cutpoint * s->vi->rate;
	  s->cutpoint = samples;
	}

	return 0;
}


int main(int argc, char **argv)
{
	ogg_int64_t cutpoint;
	FILE *in,*out1,*out2;
	int ret=0;
	int time=0;
	vcut_state *state;

	setlocale(LC_ALL, "");
	bindtextdomain(PACKAGE, LOCALEDIR);
	textdomain(PACKAGE);

	if(argc<5)
	{
		fprintf(stderr, 
				_("Usage: vcut infile.ogg outfile1.ogg outfile2.ogg [cutpoint | +cutpoint]\n"));
		exit(1);
	}

	fprintf(stderr, _("WARNING: vcut is still experimental code.\n"
		"Check that the output files are correct before deleting sources.\n\n"));

	in = fopen(argv[1], "rb");
	if(!in) {
		fprintf(stderr, _("Couldn't open %s for reading\n"), argv[1]);
		exit(1);
	}
	out1 = fopen(argv[2], "wb");
	if(!out1) {
		fprintf(stderr, _("Couldn't open %s for writing\n"), argv[2]);
		exit(1);
	}
	out2 = fopen(argv[3], "wb");
	if(!out2) {
		fprintf(stderr, _("Couldn't open %s for writing\n"), argv[3]);
		exit(1);
	}

	if(strchr(argv[4], '+') != NULL) {
	  if(sscanf(argv[4], FORMAT_INT64_TIME, &cutpoint) != 1) {
	    fprintf(stderr, _("Couldn't parse cutpoint \"%s\"\n"), argv[4]);
            exit(1);
	  }
	  time = 1;
	} else if(sscanf(argv[4], FORMAT_INT64, &cutpoint) != 1) {
	    fprintf(stderr, _("Couldn't parse cutpoint \"%s\"\n"), argv[4]);
            exit(1);
	}

	if(time) {
	  fprintf(stderr, _("Processing: Cutting at %lld seconds\n"), (long long)cutpoint);
	} else {
	  fprintf(stderr, _("Processing: Cutting at %lld samples\n"), (long long)cutpoint);
	}

	state = vcut_new();

	vcut_set_files(state, in,out1,out2);
	vcut_set_cutpoint(state, cutpoint, time);

	if(vcut_process(state))
	{
		fprintf(stderr, _("Processing failed\n"));
		ret = 1;
	}


	vcut_free(state);

	fclose(in);
	fclose(out1);
	fclose(out2);

	return ret;
}

int vcut_process(vcut_state *s)
{
	ogg_stream_state  stream_out_first;
	ogg_stream_state  stream_out_second;

	/* Read headers in, and save them */
	if(process_headers(s))
	{
		fprintf(stderr, _("Error reading headers\n"));
		return -1;
	}

	/* ok, headers are all in, and saved */
	vorbis_synthesis_init(s->vd,s->vi);
	vorbis_block_init(s->vd,s->vb);

	ogg_stream_init(&stream_out_first,s->serial); /* first file gets original */
	srand(time(NULL));
	ogg_stream_init(&stream_out_second, rand()); /* second gets random */

	submit_headers_to_stream(&stream_out_first, s);
	if(write_pages_to_file(&stream_out_first, s->out1, 1))
		return -1;

	submit_headers_to_stream(&stream_out_second, s);
	if(write_pages_to_file(&stream_out_second, s->out2, 1))
		return -1;

	
	if(process_first_stream(s, &stream_out_first, s->in, s->out1))
	{
		fprintf(stderr, _("Error writing first output file\n"));
		return -1;
	}

	ogg_stream_clear(&stream_out_first);

	if(process_second_stream(s, &stream_out_second, s->in, s->out2))
	{
		fprintf(stderr, _("Error writing second output file\n"));
		return -1;
	}
	ogg_stream_clear(&stream_out_second);

	return 0;
}

vcut_state *vcut_new(void)
{
	vcut_state *s = malloc(sizeof(vcut_state));
	memset(s,0,sizeof(vcut_state));

	s->sync_in = malloc(sizeof(ogg_sync_state));
	s->stream_in = malloc(sizeof(ogg_stream_state));
	s->vd = malloc(sizeof(vorbis_dsp_state));
	s->vi = malloc(sizeof(vorbis_info));
	s->vb = malloc(sizeof(vorbis_block));

	s->headers = malloc(sizeof(vcut_packet)*3);
	memset(s->headers, 0, sizeof(vcut_packet)*3);
	s->packets = malloc(sizeof(vcut_packet)*2);
	memset(s->packets, 0, sizeof(vcut_packet)*2);

	return s;
}

/* Full cleanup of internal state and vorbis/ogg structures */
void vcut_free(vcut_state *s)
{
	if(s)
	{
		if(s->packets)
		{
			if(s->packets[0])
				free_packet(s->packets[0]);
			if(s->packets[1])
				free_packet(s->packets[1]);
			free(s->packets);
		}

		if(s->headers)
		{
			int i;
			for(i=0; i < 3; i++)
				if(s->headers[i])
					free_packet(s->headers[i]);
			free(s->headers);
		}

		if(s->vb)
		{
			vorbis_block_clear(s->vb);
			free(s->vb);
		}
		if(s->vd)
		{
			vorbis_dsp_clear(s->vd);
			free(s->vd);
		}
		if(s->vi)
		{
			vorbis_info_clear(s->vi);
			free(s->vi);
		}
		if(s->stream_in)
		{
			ogg_stream_clear(s->stream_in);
			free(s->stream_in);
		}
		if(s->sync_in)
		{
			ogg_sync_clear(s->sync_in);
			free(s->sync_in);
		}

		free(s);
	}
}

void vcut_set_files(vcut_state *s, FILE *in, FILE *out1, FILE *out2)
{
	s->in = in;
	s->out1 = out1;
	s->out2 = out2;
}

void vcut_set_cutpoint(vcut_state *s, ogg_int64_t cutpoint, int time)
{
	s->cutpoint = cutpoint;
	s->time = time;
}

void vcut_time_to_samples(ogg_int64_t *time, ogg_int64_t *samples, FILE *in)
{

}
