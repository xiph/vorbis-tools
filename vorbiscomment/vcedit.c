/* This program is licensed under the GNU General Public License, version 2,
 * a copy of which is included with this program.
 *
 * (c) 2000-2001 Michael Smith <msmith@labyrinth.net.au>
 *
 *
 * Comment editing backend, suitable for use by nice frontend interfaces.
 *
 * last modified: $Id: vcedit.c,v 1.3 2001/01/22 11:44:04 msmith Exp $
 */


#include <stdio.h>
#include <stdlib.h>
#include <ogg/ogg.h>
#include <vorbis/codec.h>

#include "vcedit.h"

#define CHUNKSIZE 4096

vcedit_state *vcedit_new_state(void)
{
	vcedit_state *state = malloc(sizeof(vcedit_state));
	memset(state, 0, sizeof(vcedit_state));

	return state;
}

void vcedit_clear(vcedit_state *state)
{
	if(state)
		free(state);
}


vorbis_comment *vcedit_comments(vcedit_state *state)
{
	return state->vc;
}

static void vcedit_clear_internals(vcedit_state *state)
{
	if(state->vc)
	{
		vorbis_comment_clear(state->vc);
		free(state->vc);
		state->vc=NULL;
	}
	if(state->os)
	{
		ogg_stream_clear(state->os);
		free(state->os);
		state->os=NULL;
	}
	if(state->oy)
	{
		ogg_sync_clear(state->oy);
		free(state->oy);
		state->oy=NULL;
	}
}


int vcedit_open(vcedit_state *state, FILE *in)
{

	char *buffer;
	int bytes,i;
	ogg_packet *header;
	ogg_packet	header_main;
	ogg_packet  header_comments;
	ogg_packet	header_codebooks;
	ogg_page    og;
	vorbis_info vi;


	state->in = in;

	state->oy = malloc(sizeof(ogg_sync_state));
	ogg_sync_init(state->oy);

	buffer = ogg_sync_buffer(state->oy, CHUNKSIZE);
	bytes = fread(buffer, 1, CHUNKSIZE, state->in);

	ogg_sync_wrote(state->oy, bytes);

	if(ogg_sync_pageout(state->oy, &og) != 1)
	{
		if(bytes<CHUNKSIZE)
			fprintf(stderr, "Input truncated or empty.\n");
		else
			fprintf(stderr, "Input does not appear to be an Ogg bitstream.\n");
		goto err;
	}

	state->serial = ogg_page_serialno(&og);

	state->os = malloc(sizeof(ogg_stream_state));
	ogg_stream_init(state->os, state->serial);

	vorbis_info_init(&vi);

	state->vc = malloc(sizeof(vorbis_comment));
	vorbis_comment_init(state->vc);

	if(ogg_stream_pagein(state->os, &og) < 0)
	{
		fprintf(stderr, "Error reading first page of Ogg bitstream.\n");
		goto err;
	}

	if(ogg_stream_packetout(state->os, &header_main) != 1)
	{
		fprintf(stderr, "Error reading initial header packet.\n");
		goto err;
	}

	if(vorbis_synthesis_headerin(&vi, state->vc, &header_main) < 0)
	{
		fprintf(stderr, "Ogg bitstream does not contain vorbis data.\n");
		goto err;
	}

	state->mainlen = header_main.bytes;
	state->mainbuf = malloc(state->mainlen);
	memcpy(state->mainbuf, header_main.packet, header_main.bytes);

	i = 0;
	header = &header_comments;
	while(i<2) {
		while(i<2) {
			int result = ogg_sync_pageout(state->oy, &og);
			if(result == 0) break; /* Too little data so far */
			else if(result == 1)
			{
				ogg_stream_pagein(state->os, &og);
				while(i<2)
				{
					result = ogg_stream_packetout(state->os, header);
					if(result == 0) break;
					if(result == -1)
					{
						fprintf(stderr, "Corrupt secondary header.\n");
						goto err;
					}
					vorbis_synthesis_headerin(&vi, state->vc, header);
					if(i==1)
					{
						state->booklen = header->bytes;
						state->bookbuf = malloc(state->booklen);
						memcpy(state->bookbuf, header->packet, 
								header->bytes);
					}
					i++;
					header = &header_codebooks;
				}
			}
		}

		buffer = ogg_sync_buffer(state->oy, CHUNKSIZE);
		bytes = fread(buffer, 1, CHUNKSIZE, state->in);
		if(bytes == 0 && i < 2)
		{
			fprintf(stderr, "EOF before end of vorbis headers.\n");
			goto err;
		}
		ogg_sync_wrote(state->oy, bytes);
	}

	/* Headers are done! */
	vorbis_info_clear(&vi);
	return 0;

err:
	vcedit_clear_internals(state);
	return -1;
}

int vcedit_write(vcedit_state *state, FILE *out)
{
	ogg_stream_state streamout;
	ogg_packet header_main;
	ogg_packet header_comments;
	ogg_packet header_codebooks;

	ogg_page ogout, ogin;
	ogg_packet op;
	int result;
	char *buffer;
	int bytes, eosin=0, eosout=0;

	header_main.bytes = state->mainlen;
	header_main.packet = state->mainbuf;
	header_main.b_o_s = 1;
	header_main.e_o_s = 0;
	header_main.granulepos = 0;

	header_codebooks.bytes = state->booklen;
	header_codebooks.packet = state->bookbuf;
	header_codebooks.b_o_s = 0;
	header_codebooks.e_o_s = 0;
	header_codebooks.granulepos = 0;

	ogg_stream_init(&streamout, state->serial);

	vorbis_commentheader_out(state->vc, &header_comments);

	ogg_stream_packetin(&streamout, &header_main);
	ogg_stream_packetin(&streamout, &header_comments);
	ogg_stream_packetin(&streamout, &header_codebooks);

	while((result = ogg_stream_flush(&streamout, &ogout)))
	{
		fwrite(ogout.header,1,ogout.header_len, out);
		fwrite(ogout.body,1,ogout.body_len, out);
	}

	while(!(eosin && eosout))
	{
		while(!(eosin && !eosout))
		{
			result = ogg_sync_pageout(state->oy, &ogin);
			if(result==0) break; /* Need more data... */
			else if(result ==-1)
				fprintf(stderr, "Recoverable error in bitstream\n");
			else
			{
				ogg_stream_pagein(state->os, &ogin);
	
				while(1)
				{
					result = ogg_stream_packetout(state->os, &op);
					if(result==0)break;
					else if(result==-1) 
						fprintf(stderr, "Recoverable error in bitstream\n");
					else
					{
						ogg_stream_packetin(&streamout, &op);
	
						while(!(eosin && eosout))
						{
							int result = ogg_stream_pageout(&streamout, &ogout);
							if(result==0)break;
	
							fwrite(ogout.header,1,ogout.header_len, out);
							fwrite(ogout.body,1,ogout.body_len, out);
	
							if(ogg_page_eos(&ogout)) eosout = 1;
						}
					}
				}
				if(ogg_page_eos(&ogin)) eosin = 1;
			}
		}
		if(!(eosin && eosout))
		{
			buffer = ogg_sync_buffer(state->oy, CHUNKSIZE);
			bytes = fread(buffer,1, CHUNKSIZE, state->in);
			ogg_sync_wrote(state->oy, bytes);
			if(bytes == 0) eosin = 1;
		}
	}

	ogg_stream_clear(&streamout);
	ogg_packet_clear(&header_comments);

	free(state->mainbuf);
	free(state->bookbuf);

	vcedit_clear_internals(state);
	return 0;
}
	
