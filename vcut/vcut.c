/* This program is licensed under the GNU General Public License, version 2,
 * a copy of which is included with this program.
 *
 * (c) 2000-2001 Michael Smith <msmith@labyrinth.net.au>
 *
 *
 * Simple application to cut an ogg at a specified frame, and produce two
 * output files.
 *
 * last modified: $Id: vcut.c,v 1.3 2001/07/16 13:09:57 msmith Exp $
 */

#include <stdio.h>
#include <stdlib.h>
#include <ogg/ogg.h>
#include <vorbis/codec.h>
#include <time.h>
#include <errno.h>
#include <string.h>

#include "vcut.h"

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
	unsigned char *buffer = ogg_sync_buffer(s->sync_in, 4096);
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
	ogg_int64_t granpos, prevgranpos;
	int result;

	while(!eos)
	{
		while(!eos)
		{
			int result = ogg_sync_pageout(s->sync_in, &page);
			if(result==0) break;
			else if(result<0) fprintf(stderr, "Page error. Corrupt input.\n");
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
							fprintf(stderr, "Bitstream error, continuing\n");
						else
						{
							/* We need to save the last packet in the first
							 * stream - but we don't know when we're going
							 * to get there. So we have to keep every packet
							 * just in case.
							 */
							if(s->packets[0])
								free_packet(s->packets[0]);
							s->packets[0] = save_packet(&packet);

							ogg_stream_packetin(stream, &packet);
							if(write_pages_to_file(stream, f,0))
								return -1;
						}
					}
					prevgranpos = granpos;
				}
				else
					eos=1; /* First stream ends somewhere in this page.
							  We break of out this loop here. */

				if(ogg_page_eos(&page))
				{
					fprintf(stderr, "Found EOS before cut point.\n");
					eos=1;
				}
			}
		}
		if(!eos)
		{
			if(update_sync(s,in)==0) 
			{
				fprintf(stderr, "Setting eos: update sync returned 0\n");
				eos=1;
			}
		}
	}

	/* Now, check to see if we reached a real EOS */
	if(granpos < s->cutpoint)
	{
		fprintf(stderr, 
				"Cutpoint not within stream. Second file will be empty\n");
		write_pages_to_file(stream, f,0);

		return -1;
	}

	while((result = ogg_stream_packetout(s->stream_in, &packet))!=0)
	{
		int bs;
		
		bs = get_blocksize(s, s->vi, &packet);
		prevgranpos += bs;

		if(prevgranpos > s->cutpoint)
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
		fprintf(stderr, "Unhandled special case: first file too short?\n");
		return -1;
	}

	if(write_pages_to_file(stream,f, 0))
		return -1;

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
	int eos=0;
	int result;
	ogg_int64_t page_granpos, current_granpos=s->initialgranpos;
	ogg_int64_t packetnum=0; /* Should this start from 0 or 3 ? */

	packet.bytes = s->packets[0]->length;
	packet.packet = s->packets[0]->packet;
	packet.b_o_s = 0;
	packet.e_o_s = 0;
	packet.granulepos = 0;
	packet.packetno = packetnum++; 
	ogg_stream_packetin(stream,&packet);

	packet.bytes = s->packets[1]->length;
	packet.packet = s->packets[1]->packet;
	packet.b_o_s = 0;
	packet.e_o_s = 0;
	packet.granulepos = s->initialgranpos;
	packet.packetno = packetnum++;
	ogg_stream_packetin(stream,&packet);

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
		fprintf(stderr, "ERROR: First two audio packets did not fit into one\n"
				        "       ogg page. File may not decode correctly.\n");
		fwrite(page.header,1,page.header_len,f);
		fwrite(page.body,1,page.body_len,f);
	}

	while(!eos)
	{
		while(!eos)
		{
			result=ogg_sync_pageout(s->sync_in, &page);
			if(result==0) break;
			else if(result==-1)
				fprintf(stderr, "Recoverable bitstream error\n");
			else
			{
				page_granpos = ogg_page_granulepos(&page) - s->cutpoint;
				if(ogg_page_eos(&page))eos=1;
				ogg_stream_pagein(s->stream_in, &page);
				while(1)
				{
					result = ogg_stream_packetout(s->stream_in, &packet);
					if(result==0) break;
					else if(result==-1) fprintf(stderr, "Bitstream error\n");
					else
					{
						int bs = get_blocksize(s, s->vi, &packet);
						current_granpos += bs;
						if(current_granpos > page_granpos)
						{
							current_granpos = page_granpos;
						}

						packet.granulepos = current_granpos;
						packet.packetno = packetnum++;
						ogg_stream_packetin(stream, &packet);
						if(write_pages_to_file(stream,f, 0))
							return -1;
					}
				}
			}
		}
		if(!eos)
		{
			if(update_sync(s, in)==0)
			{
				fprintf(stderr, "Update sync returned 0, setting eos\n");
				eos=1;
			}
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
 */
static int process_headers(vcut_state *s)
{
	vorbis_comment vc;
	ogg_page page;
	ogg_packet packet;
	int bytes;
	int i;
	unsigned char *buffer;

	ogg_sync_init(s->sync_in);
	
	vorbis_info_init(s->vi);
	vorbis_comment_init(&vc);

	buffer = ogg_sync_buffer(s->sync_in, 4096);
	bytes = fread(buffer, 1, 4096, s->in);
	ogg_sync_wrote(s->sync_in, bytes);

	if(ogg_sync_pageout(s->sync_in, &page)!=1){
		fprintf(stderr, "Input not ogg.\n");
		return -1;
	}

	s->serial = ogg_page_serialno(&page);

	ogg_stream_init(s->stream_in, s->serial);

	if(ogg_stream_pagein(s->stream_in, &page) <0)
	{
		fprintf(stderr, "Error in first page\n");
		return -1;
	}

	if(ogg_stream_packetout(s->stream_in, &packet)!=1){
		fprintf(stderr, "error in first packet\n");
		return -1;
	}

	if(vorbis_synthesis_headerin(s->vi, &vc, &packet)<0)
	{
		fprintf(stderr, "Error in primary header: not vorbis?\n");
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
						fprintf(stderr, "Secondary header corrupt\n");
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
			fprintf(stderr, "EOF in headers\n");
			return -1;
		}
		ogg_sync_wrote(s->sync_in, bytes);
	}

	vorbis_comment_clear(&vc);

	return 0;
}


int main(int argc, char **argv)
{
	ogg_int64_t cutpoint;
	FILE *in,*out1,*out2;
	int ret=0;
	vcut_state *state;

	if(argc<5)
	{
		fprintf(stderr, 
				"Usage: vcut infile.ogg outfile1.ogg outfile2.ogg cutpoint\n");
		exit(1);
	}

	fprintf(stderr, "WARNING: vcut is still experimental code.\n"
		"Check that the output files are correct before deleting sources.\n\n");

	in = fopen(argv[1], "rb");
	if(!in) {
		fprintf(stderr, "Couldn't open %s for reading\n", argv[1]);
		exit(1);
	}
	out1 = fopen(argv[2], "wb");
	if(!out1) {
		fprintf(stderr, "Couldn't open %s for writing\n", argv[2]);
		exit(1);
	}
	out2 = fopen(argv[3], "wb");
	if(!out2) {
		fprintf(stderr, "Couldn't open %s for writing\n", argv[3]);
		exit(1);
	}

	sscanf(argv[4], "%Ld", &cutpoint);

	fprintf(stderr, "Processing: Cutting at %lld\n", cutpoint);

	state = vcut_new();

	vcut_set_files(state, in,out1,out2);
	vcut_set_cutpoint(state, cutpoint);

	if(vcut_process(state))
	{
		fprintf(stderr, "Processing failed\n");
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
		fprintf(stderr, "Error reading headers\n");
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
		fprintf(stderr, "Error writing first output file\n");
		return -1;
	}

	ogg_stream_clear(&stream_out_first);

	if(process_second_stream(s, &stream_out_second, s->in, s->out2))
	{
		fprintf(stderr, "Error writing second output file\n");
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

void vcut_set_cutpoint(vcut_state *s, ogg_int64_t cutpoint)
{
	s->cutpoint = cutpoint;
}



