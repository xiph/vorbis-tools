/* This program is licensed under the GNU General Public License, version 2,
 * a copy of which is included with this program.
 *
 * (c) 2000-2001 Michael Smith <msmith@labyrinth.net.au>
 *
 *
 * Simple application to cut an ogg at a specified frame, and produce two
 * output files.
 *
 * last modified: $Id: vcut.c,v 1.1 2001/03/24 14:12:35 msmith Exp $
 */

#include <stdio.h>
#include <stdlib.h>
#include <ogg/ogg.h>
#include <vorbis/codec.h>
#include <time.h>
#include <errno.h>
#include <string.h>

/* Erk. We need some internal headers too (at the moment, anyway) */
#include "lib/codec_internal.h"
#include "lib/misc.h"

#include "vcut.h"

int main(int argc, char **argv)
{
	ogg_int64_t cutpoint;
	FILE *in,*out1,*out2;

	if(argc<5)
	{
		fprintf(stderr, 
				"Usage: vcut infile.ogg outfile1.ogg outfile2.ogg cutpoint\n");
		exit(1);
	}

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
		exit(1);
	}

	fclose(in);
	fclose(out1);
	fclose(out2);

	return 0;
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
	vcut_process_headers(s, in);

	/* ok, headers are all in, and saved */
	fprintf(stderr, "Header reading complete\n");

	vorbis_synthesis_init(s->vd,s->vi);
	vorbis_block_init(s->vd,s->vb);

	ogg_stream_init(&stream_out_first, s->serial); /* first file gets original */
	srand(time(NULL));
	ogg_stream_init(&stream_out_second, rand()); /* second gets random */

	vcut_submit_headers_to_stream(&stream_out_first, s);
	vcut_submit_headers_to_stream(&stream_out_second, s);

	vcut_flush_pages_to_file(&stream_out_first, first);
	vcut_flush_pages_to_file(&stream_out_second, second);

	vcut_process_first_stream(s, &stream_out_first, in, first);
	ogg_stream_clear(&stream_out_first);
	vcut_process_second_stream(s, &stream_out_second, in, second);
	ogg_stream_clear(&stream_out_second);

	/* Free some memory! */
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

	return s;
}


void vcut_process_first_stream(vcut_state *s, ogg_stream_state *stream, FILE *in, FILE *f)
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
				fprintf(stderr, "Input page: %lld\n", granpos);
				ogg_stream_pagein(s->stream_in, &page);

				fprintf(stderr, "granpos: %lld, cutpoint; %lld\n", granpos, s->cutpoint);
				if(granpos < s->cutpoint)
				{
					while(1)
					{
						result=ogg_stream_packetout(s->stream_in, &packet);

						/* throw away result */
						vcut_get_blocksize(s,s->vb,&packet);

						if(result==0) break;
						else if(result==-1)
							fprintf(stderr, "Bitstream error, continuing\n");
						else
						{
							/* Free these somewhere: FIXME!!! */
							s->packets[0] = vcut_save_packet(&packet);
							// Count pcm?? 
							ogg_stream_packetin(stream, &packet);
							vcut_write_pages_to_file(stream, f);
						}
					}
					prevgranpos = granpos;
				}
				else
				{
					fprintf(stderr, "Page passes cutpoint\n");
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

	while((result = ogg_stream_packetout(s->stream_in, &packet))!=0)
	{
		int bs = vcut_get_blocksize(s, s->vb, &packet);
		prevgranpos += bs;

		fprintf(stderr, "prevgranpos += %d -> %lld\n", bs, prevgranpos);
		if(prevgranpos > s->cutpoint)
		{
			fprintf(stderr, "Passed cutpoint: %lld\n", s->cutpoint);
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

	vcut_write_pages_to_file(stream,f);

	/* Remaining samples in first packet */
	s->initialgranpos = prevgranpos - s->cutpoint; 
	fprintf(stderr, "Trimming %lld samples from end of first stream (placing on second stream\n", s->initialgranpos);

	fprintf(stderr, "Completed first stream!\n");
}

void vcut_process_second_stream(vcut_state *s, ogg_stream_state *stream, FILE *in, FILE *f)
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
		fprintf(stderr, "Flushing (should be only one page)\n");
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
						int bs = vcut_get_blocksize(s, s->vb, &packet);
						current_granpos += bs;
						if(current_granpos > page_granpos)
						{
							fprintf(stderr, "INFO: Truncating at granpos=%lld->%lld\n", current_granpos, page_granpos);
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
}			

long vcut_get_blocksize(vcut_state *s, vorbis_block *vb, ogg_packet *op)
{
	vorbis_info *vi = vb->vd->vi;
	oggpack_buffer *opb = &vb->opb;
	int mode;
	codec_setup_info *ci = vi->codec_setup;
	int ret;

	oggpack_readinit(opb,op->packet, op->bytes);

	if(oggpack_read(opb,1)!=0) return 0;

	mode = oggpack_read(opb, 
			((backend_lookup_state *)vb->vd->backend_state)->modebits);

	ret = (ci->blocksizes[ci->mode_param[mode]->blockflag] + 
			ci->blocksizes[s->prevW])/4;

	/* remember to init prevW to appropriate thing */
	s->prevW = ci->mode_param[mode]->blockflag;
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
									

void vcut_process_headers(vcut_state *s, FILE *in)
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
		exit(1);
	}

	s->serial = ogg_page_serialno(&page);

	ogg_stream_init(s->stream_in, s->serial);

	if(ogg_stream_pagein(s->stream_in, &page) <0)
	{
		fprintf(stderr, "Error in first page\n");
		exit(1);
	}

	if(ogg_stream_packetout(s->stream_in, &packet)!=1){
		fprintf(stderr, "error in first packet\n");
		exit(1);
	}

	if(vorbis_synthesis_headerin(s->vi, &vc, &packet)<0)
	{
		fprintf(stderr, "Error in primary header: not vorbis?\n");
		exit(1);
	}

	fprintf(stderr, "headerin: read primary header\n");
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
						exit(1);
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
			exit(1);
		}
		ogg_sync_wrote(s->sync_in, bytes);
	}
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




