/* This program is licensed under the GNU General Public License, version 2,
 * a copy of which is included with this program.
 *
 * (c) 2000-2001 Michael Smith <msmith@labyrinth.net.au>
 *
 *
 * Simple application to cut an ogg at a specified frame, and produce two
 * output files.
 *
 * last modified: $Id: vcut.c,v 1.2 2001/07/04 14:48:55 msmith Exp $
 */

#include <stdio.h>
#include <stdlib.h>
#include <ogg/ogg.h>
#include <vorbis/codec.h>
#include <time.h>
#include <errno.h>
#include <string.h>

#include "vcut.h"

int main(int argc, char **argv)
{
	ogg_int64_t cutpoint;
	FILE *in,*out1,*out2;
	int ret=0;

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

	if(vcut_process(in,out1,out2,cutpoint))
	{
		fprintf(stderr, "Processing failed\n");
		ret = 1;
	}

	fclose(in);
	fclose(out1);
	fclose(out2);

	return ret;
}

int vcut_process(FILE *in, FILE *first, FILE *second, ogg_int64_t cutpoint)
{
	ogg_stream_state  stream_out_first;
	ogg_stream_state  stream_out_second;
	vcut_state *s;

	s = vcut_new_state();
	fprintf(stderr, "Processing: Cutting at %lld\n", cutpoint);
	s->cutpoint = cutpoint;

	/* Read headers in, and save them */
	if(vcut_process_headers(s, in))
	{
		fprintf(stderr, "Error reading headers\n");
		//cleanup();
	}

	/* ok, headers are all in, and saved */
	vorbis_synthesis_init(s->vd,s->vi);
	vorbis_block_init(s->vd,s->vb);

	ogg_stream_init(&stream_out_first,s->serial); /* first file gets original */
	srand(time(NULL));
	ogg_stream_init(&stream_out_second, rand()); /* second gets random */

	vcut_submit_headers_to_stream(&stream_out_first, s);
	vcut_flush_pages_to_file(&stream_out_first, first);

	vcut_submit_headers_to_stream(&stream_out_second, s);
	vcut_flush_pages_to_file(&stream_out_second, second);

	
	if(vcut_process_first_stream(s, &stream_out_first, in, first))
	{
		fprintf(stderr, "Error writing first output file\n");
		//cleanup();
	}

	ogg_stream_clear(&stream_out_first);

	if(vcut_process_second_stream(s, &stream_out_second, in, second))
	{
		fprintf(stderr, "Error writing second output file\n");
		//cleanup();
	}
	ogg_stream_clear(&stream_out_second);

	/* Free some memory! */
	vorbis_block_clear(s->vb);
	vorbis_dsp_clear(s->vd);
	vorbis_info_clear(s->vi);

	ogg_sync_clear(s->sync_in);

	return 0;
}

vcut_state *vcut_new_state()
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


int vcut_process_first_stream(vcut_state *s, ogg_stream_state *stream, 
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
			else if(result<0) fprintf(stderr, "Page error. May not work\n");
			else
			{
				granpos = ogg_page_granulepos(&page);
				ogg_stream_pagein(s->stream_in, &page);

				if(granpos < s->cutpoint)
				{
					while(1)
					{
						result=ogg_stream_packetout(s->stream_in, &packet);

						/* throw away result */
						vcut_get_blocksize(s,s->vi,&packet);

						if(result==0) break;
						else if(result==-1)
							fprintf(stderr, "Bitstream error, continuing\n");
						else
						{
							/* Free these somewhere: FIXME!!! */
							s->packets[0] = vcut_save_packet(&packet);
							ogg_stream_packetin(stream, &packet);
							vcut_write_pages_to_file(stream, f);
						}
					}
					prevgranpos = granpos;
				}
				else
				{
					fprintf(stderr, "DEBUG: Page passes cutpoint\n");
					eos=1; /* This way we break out */
				}


				if(ogg_page_eos(&page))
				{
					fprintf(stderr, "Found EOS page\n");
					eos=1;
				}
			}
		}
		if(!eos)
		{
			if(vcut_update_sync(s,in)==0) 
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
		vcut_write_pages_to_file(stream, f);
		return -1;
	}

	while((result = ogg_stream_packetout(s->stream_in, &packet))!=0)
	{
		int bs;
		
		bs = vcut_get_blocksize(s, s->vi, &packet);
		prevgranpos += bs;

		if(prevgranpos > s->cutpoint)
		{
			s->packets[1] = vcut_save_packet(&packet);
			packet.granulepos = s->cutpoint; /* Set it! */
			packet.e_o_s = 1;
			ogg_stream_packetin(stream, &packet);
			break;
		}
		/* Free these? */
		s->packets[0] = vcut_save_packet(&packet);
		ogg_stream_packetin(stream, &packet);
		vcut_write_pages_to_file(stream,f);
	}

	/* Check that we got at least two packets here */
	if(!s->packets[0] || !s->packets[1])
	{
		fprintf(stderr, "Unhandled special case: first file too short?\n");
		return -1;
	}

	vcut_write_pages_to_file(stream,f);

	/* Remaining samples in first packet */
	s->initialgranpos = prevgranpos - s->cutpoint; 
	fprintf(stderr, "DEBUG: Trimming %lld samples from end of first stream (placing on second stream\n", s->initialgranpos);

	return 0;
}

int vcut_process_second_stream(vcut_state *s, ogg_stream_state *stream, FILE *in, FILE *f)
{
	ogg_packet packet;
	ogg_page page;
	int eos=0;
	int result;
	ogg_int64_t page_granpos, current_granpos=s->initialgranpos;
	ogg_int64_t packetnum=0; /* Do we even care? */

	packet.bytes = s->packets[0]->length;
	packet.packet = s->packets[0]->packet;
	packet.b_o_s = 0;
	packet.e_o_s = 0;
	packet.granulepos = 0;
	packet.packetno = packetnum++; /* 0 or 3 for the first audio packet? */
	ogg_stream_packetin(stream,&packet);

	packet.bytes = s->packets[1]->length;
	packet.packet = s->packets[1]->packet;
	packet.b_o_s = 0;
	packet.e_o_s = 0;
	packet.granulepos = s->initialgranpos;
	packet.packetno = packetnum++;
	ogg_stream_packetin(stream,&packet);

	while(ogg_stream_flush(stream, &page)!=0)
	{
		/* What happens if this _IS_ more than one page? (e.g. really big 
		 * packets - high bitrate modes or whatever) */
		fprintf(stderr, "DEBUG: Flushing (should only happen _once_)\n");
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
						int bs = vcut_get_blocksize(s, s->vi, &packet);
						current_granpos += bs;
						if(current_granpos > page_granpos)
						{
							current_granpos = page_granpos;
						}

						packet.granulepos = current_granpos;
						packet.packetno = packetnum++;
						ogg_stream_packetin(stream, &packet);
						vcut_write_pages_to_file(stream,f);
					}
				}
			}
		}
		if(!eos)
		{
			if(vcut_update_sync(s, in)==0)
			{
				fprintf(stderr, "Update sync returned 0, setting eos\n");
				eos=1;
			}
		}
	}

	return 0;
}			

/* Do we want to move this functionality into core libvorbis? Then we 
 * wouldn't need the internal headers to be included. That would be good.
 */
long vcut_get_blocksize(vcut_state *s, vorbis_info *vi, ogg_packet *op)
{
	int this = vorbis_packet_blocksize(vi, op);
	int ret = (this+s->prevW)/4;

	s->prevW = this;
	return ret;
}

int vcut_update_sync(vcut_state *s, FILE *f)
{
	unsigned char *buffer = ogg_sync_buffer(s->sync_in, 4096);
	int bytes = fread(buffer,1,4096,f);
	ogg_sync_wrote(s->sync_in, bytes);
	return bytes;
}



void vcut_submit_headers_to_stream(ogg_stream_state *stream, vcut_state *s) 
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
									

int vcut_process_headers(vcut_state *s, FILE *in)
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
	bytes = fread(buffer, 1, 4096, in);
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

	s->headers[0] = vcut_save_packet(&packet);

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
					s->headers[i+1] = vcut_save_packet(&packet);
					vorbis_synthesis_headerin(s->vi,&vc,&packet);
					i++;
				}
			}
		}
		buffer=ogg_sync_buffer(s->sync_in, 4096);
		bytes=fread(buffer,1,4096,in);
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

vcut_packet *vcut_save_packet(ogg_packet *packet)
{
	vcut_packet *p = malloc(sizeof(vcut_packet));

	p->length = packet->bytes;
	p->packet = malloc(p->length);
	memcpy(p->packet, packet->packet, p->length);

	return p;
}

void vcut_write_pages_to_file(ogg_stream_state *stream, FILE *file)
{
	ogg_page page;

	while(ogg_stream_pageout(stream, &page))
	{
		fwrite(page.header,1,page.header_len, file);
		fwrite(page.body,1,page.body_len, file);
	}
}

void vcut_flush_pages_to_file(ogg_stream_state *stream, FILE *file)
{
	ogg_page page;

	while(ogg_stream_flush(stream, &page))
	{
		fwrite(page.header,1,page.header_len, file);
		fwrite(page.body,1,page.body_len, file);
	}
}




