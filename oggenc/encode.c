/* OggEnc
 **
 ** This program is distributed under the GNU General Public License, version 2.
 ** A copy of this license is included with this source.
 **
 ** Copyright 2000-2002, Michael Smith <msmith@labyrinth.net.au>
 **
 ** Portions from Vorbize, (c) Kenneth Arnold <kcarnold@yahoo.com>
 ** and libvorbis examples, (c) Monty <monty@xiph.org>
 **/

#include <stdlib.h>
#include <stdio.h>
#include <math.h>

#include "platform.h"
#include <vorbis/vorbisenc.h>
#include "encode.h"
#include "i18n.h"


#define READSIZE 1024


int oe_write_page(ogg_page *page, FILE *fp);

int oe_encode(oe_enc_opt *opt)
{

	ogg_stream_state os;
	ogg_page 		 og;
	ogg_packet 		 op;

	vorbis_dsp_state vd;
	vorbis_block     vb;
	vorbis_info      vi;

	long samplesdone=0;
    int eos;
	long bytes_written = 0, packetsdone=0;
	double time_elapsed;
	int ret=0;

	TIMER *timer;


	/* get start time. */
	timer = timer_start();
    opt->start_encode(opt->infilename, opt->filename, opt->bitrate, 
            opt->quality, opt->managed);

	/* Have vorbisenc choose a mode for us */
	vorbis_info_init(&vi);

	if(!opt->managed)
	{
		if(vorbis_encode_init_vbr(&vi, opt->channels, opt->rate, opt->quality))
		{
			fprintf(stderr, _("Mode initialisation failed: invalid parameters for quality\n"));
			vorbis_info_clear(&vi);
			return 1;
		}
	}
	else
	{
		if(vorbis_encode_init(&vi, opt->channels, opt->rate, 
                    opt->max_bitrate>0?opt->max_bitrate*1000:-1,
				    opt->bitrate*1000, 
                    opt->min_bitrate>0?opt->min_bitrate*1000:-1))
		{
			fprintf(stderr, _("Mode initialisation failed: invalid parameters for bitrate\n"));
			vorbis_info_clear(&vi);
			return 1;
		}
	}

	/* Now, set up the analysis engine, stream encoder, and other
	   preparation before the encoding begins.
	 */

	vorbis_analysis_init(&vd,&vi);
	vorbis_block_init(&vd,&vb);

	ogg_stream_init(&os, opt->serialno);

	/* Now, build the three header packets and send through to the stream 
	   output stage (but defer actual file output until the main encode loop) */

	{
		ogg_packet header_main;
		ogg_packet header_comments;
		ogg_packet header_codebooks;
		int result;

		/* Build the packets */
		vorbis_analysis_headerout(&vd,opt->comments,
				&header_main,&header_comments,&header_codebooks);

		/* And stream them out */
		ogg_stream_packetin(&os,&header_main);
		ogg_stream_packetin(&os,&header_comments);
		ogg_stream_packetin(&os,&header_codebooks);

		while((result = ogg_stream_flush(&os, &og)))
		{
			if(!result) break;
			ret = oe_write_page(&og, opt->out);
			if(ret != og.header_len + og.body_len)
			{
				opt->error(_("Failed writing header to output stream\n"));
				ret = 1;
				goto cleanup; /* Bail and try to clean up stuff */
			}
			else
				bytes_written += ret;
		}
	}

	eos = 0;

	/* Main encode loop - continue until end of file */
	while(!eos)
	{
		float **buffer = vorbis_analysis_buffer(&vd, READSIZE);
		long samples_read = opt->read_samples(opt->readdata, 
				buffer, READSIZE);

		if(samples_read ==0)
			/* Tell the library that we wrote 0 bytes - signalling the end */
			vorbis_analysis_wrote(&vd,0);
		else
		{
			samplesdone += samples_read;

			/* Call progress update every 10 pages */
			if(packetsdone>=10)
			{
				double time;

				packetsdone = 0;
				time = timer_time(timer);

				opt->progress_update(opt->filename, opt->total_samples_per_channel, 
						samplesdone, time);
			}

			/* Tell the library how many samples (per channel) we wrote 
			   into the supplied buffer */
			vorbis_analysis_wrote(&vd, samples_read);
		}

		/* While we can get enough data from the library to analyse, one
		   block at a time... */
		while(vorbis_analysis_blockout(&vd,&vb)==1)
		{

			/* Do the main analysis, creating a packet */
			vorbis_analysis(&vb, NULL);
			vorbis_bitrate_addblock(&vb);

			while(vorbis_bitrate_flushpacket(&vd, &op)) 
			{
				/* Add packet to bitstream */
				ogg_stream_packetin(&os,&op);
				packetsdone++;

				/* If we've gone over a page boundary, we can do actual output,
				   so do so (for however many pages are available) */

				while(!eos)
				{
					int result = ogg_stream_pageout(&os,&og);
					if(!result) break;

					ret = oe_write_page(&og, opt->out);
					if(ret != og.header_len + og.body_len)
					{
						opt->error(_("Failed writing data to output stream\n"));
						ret = 1;
						goto cleanup; /* Bail */
					}
					else
						bytes_written += ret; 
	
					if(ogg_page_eos(&og))
						eos = 1;
				}
			}
		}
	}

	ret = 0; /* Success, set return value to 0 since other things reuse it
			  * for nefarious purposes. */

	/* Cleanup time */
cleanup:

	ogg_stream_clear(&os);

	vorbis_block_clear(&vb);
	vorbis_dsp_clear(&vd);
	vorbis_info_clear(&vi);

	time_elapsed = timer_time(timer);
	opt->end_encode(opt->filename, time_elapsed, opt->rate, samplesdone, bytes_written);

	timer_clear(timer);

	return ret;
}

void update_statistics_full(char *fn, long total, long done, double time)
{
	static char *spinner="|/-\\";
	static int spinpoint = 0;
	double remain_time;
	int minutes=0,seconds=0;
	
	remain_time = time/((double)done/(double)total) - time;
	minutes = ((int)remain_time)/60;
	seconds = (int)(remain_time - (double)((int)remain_time/60)*60);

	fprintf(stderr, "\r");
	fprintf(stderr, _("\t[%5.1f%%] [%2dm%.2ds remaining] %c"), 
			done*100.0/total, minutes, seconds, spinner[spinpoint++%4]);
}

void update_statistics_notime(char *fn, long total, long done, double time)
{
	static char *spinner="|/-\\";
	static int spinpoint =0;
	
	fprintf(stderr, "\r");
	fprintf(stderr, _("\tEncoding [%2dm%.2ds so far] %c"), 
            ((int)time)/60, (int)(time - (double)((int)time/60)*60),
			spinner[spinpoint++%4]);
}

int oe_write_page(ogg_page *page, FILE *fp)
{
	int written;
	written = fwrite(page->header,1,page->header_len, fp);
	written += fwrite(page->body,1,page->body_len, fp);

	return written;
}

void final_statistics(char *fn, double time, int rate, long samples, long bytes)
{
	double speed_ratio;
	if(fn)
		fprintf(stderr, _("\n\nDone encoding file \"%s\"\n"), fn);
	else
		fprintf(stderr, _("\n\nDone encoding.\n"));

	speed_ratio = (double)samples / (double)rate / time;
	
	fprintf(stderr, _("\n\tFile length:  %dm %04.1fs\n"),
			(int)(samples/rate/60),
			samples/rate - 
			floor(samples/rate/60)*60);
	fprintf(stderr, _("\tElapsed time: %dm %04.1fs\n"),
			(int)(time/60),
			time - floor(time/60)*60);
	fprintf(stderr, _("\tRate:         %.4f\n"), speed_ratio);
	fprintf(stderr, _("\tAverage bitrate: %.1f kb/s\n\n"), 
		8./1000.*((double)bytes/((double)samples/(double)rate)));
}

void final_statistics_null(char *fn, double time, int rate, long samples, 
		long bytes)
{
	/* Don't do anything, this is just a placeholder function for quiet mode */
}

void update_statistics_null(char *fn, long total, long done, double time)
{
	/* So is this */
}

void encode_error(char *errmsg)
{
	fprintf(stderr, "\n%s\n", errmsg);
}

void start_encode_full(char *fn, char *outfn, int bitrate, float quality, 
        int managed)
{
    if(!managed)
        fprintf(stderr, _("Encoding %s%s%s to \n         %s%s%s at quality %2.2f\n"),
			    fn?"\"":"", fn?fn:_("standard input"), fn?"\"":"",
                outfn?"\"":"", outfn?outfn:_("standard output"), outfn?"\"":"",
                quality * 10);
    else
        fprintf(stderr, _("Encoding %s%s%s to \n         "
                "%s%s%s at bitrate %d kbps,\n"
                "using full bitrate management engine\n"),
			    fn?"\"":"", fn?fn:_("standard input"), fn?"\"":"",
                outfn?"\"":"", outfn?outfn:_("standard output"), outfn?"\"":"",
                bitrate);
}

void start_encode_null(char *fn, char *outfn, int bitrate, float quality, 
        int managed)
{
}


