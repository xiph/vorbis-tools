/* OggEnc
 **
 ** This program is distributed under the GNU General Public License, version 2.
 ** A copy of this license is included with this source.
 **
 ** Copyright 2000, Michael Smith <msmith@labyrinth.net.au>
 **
 ** Portions from Vorbize, (c) Kenneth Arnold <kcarnold@yahoo.com>
 ** and libvorbis examples, (c) Monty <monty@xiph.org>
 **/

#include <stdlib.h>
#include <stdio.h>
#include <math.h>

#include "platform.h"
#include <vorbis/codec.h>
#include "encode.h"


#define READSIZE 1024


static void update_statistics(char *fn, long total, long done, double time,long counter);
int oe_write_page(ogg_page *page, FILE *fp);

int oe_encode(oe_enc_opt *opt)
{

	ogg_stream_state os;
	ogg_page 		 og;
	ogg_packet 		 op;

	vorbis_dsp_state vd;
	vorbis_block     vb;

	long samplesdone=0;
	long counter=0,counter2=0;
    int eos;
	long bytes_written = 0;

	void *timer;


	/* get start time. */
	timer = timer_start();

	/* Now, set up the analysis engine, stream encoder, and other
	   preparation before the encoding begins.
	 */

	vorbis_analysis_init(&vd,opt->mode);
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
		/* Possibly need to flush page here? */
		ogg_stream_packetin(&os,&header_comments);
		ogg_stream_packetin(&os,&header_codebooks);

		/* Write out the headers - this ensures that the audio data starts at
		 * the start of a new page, making streaming easier */
		while((result = ogg_stream_flush(&os, &og)))
		{
			if(!result) break;
			bytes_written += oe_write_page(&og, opt->out);
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

			if((counter++%16==0) && !opt->quiet)
			{
				double time;

				time = timer_time(timer);

				update_statistics(opt->filename, 
						opt->total_samples_per_channel, samplesdone,time,counter2++);
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
			vorbis_analysis(&vb, &op);

			/* Add packet to bitstream */
			ogg_stream_packetin(&os,&op);

			/* If we've gone over a page boundary, we can do actual output,
			   so do so (for however many pages are available) */

			while(!eos)
			{
				int result = ogg_stream_pageout(&os,&og);
				if(!result) break;

				bytes_written += oe_write_page(&og, opt->out);

				if(ogg_page_eos(&og))
					eos = 1;
			}
		}
	}

	/* Cleanup time */

	ogg_stream_clear(&os);

	vorbis_block_clear(&vb);
	vorbis_dsp_clear(&vd);

	if(!opt->quiet)
	{
		double time_elapsed, speed_ratio;
		if(opt->filename)
			fprintf(stderr, "\n\nDone encoding file \"%s\"\n", opt->filename);
		else
			fprintf(stderr, "\n\nDone encoding.\n");

		time_elapsed = timer_time(timer);
		speed_ratio = (double)samplesdone / (double)opt->rate / time_elapsed;
		

		fprintf(stderr, "\n\tFile length:  %dm %04.1fs\n",
				(int)(samplesdone/opt->rate/60),
				samplesdone/opt->rate - 
				floor(samplesdone/opt->rate/60)*60);
		fprintf(stderr, "\tElapsed time: %dm %04.1fs\n",
				(int)(time_elapsed/60),
				time_elapsed - floor(time_elapsed/60)*60);
		fprintf(stderr, "\tRate:         %.4f\n", speed_ratio);
		fprintf(stderr, "\tAverage bitrate: %.1f kb/s\n\n", 
			8./1000.*((double)bytes_written/((double)samplesdone/(double)opt->rate)));

	}

	timer_clear(timer);

	return 0;
}

static void update_statistics(char *fn, long total, long done, double time,long counter)
{
	static char *spinner="|/-\\";
	double remain_time;
	int minutes=0,seconds=0;

	if(total)
	{
		remain_time = time/((double)done/(double)total) - time;
		minutes = ((int)remain_time)/60;
		seconds = (int)(remain_time - (float)((int)remain_time/60)*60);
	}

	if(fn && total && time > 1.0)
		fprintf(stderr, "\rEncoding \"%s\" [%5.1f%%] [%2dm%.2ds remaining] %c", fn, 
				done*100.0/total, minutes, seconds,
				spinner[counter%4]);
	else if(fn && total)
		fprintf(stderr, "\rEncoding \"%s\" [%5.1f%%] %c", fn, 
				done*100.0/total,
				spinner[counter%4]);
	else if(fn)
		fprintf(stderr, "\rEncoding \"%s\" [???%%] %c", fn, 
				spinner[counter%4]);
	else if(total && time > 1.0)
		fprintf(stderr, "\rEncoding to standard output [%5.1f%%] [%dm%ds remaining] %c", 
				done*100.0/total, minutes, seconds,
				spinner[counter%4]);
	else if(total)
		fprintf(stderr, "\rEncoding to standard output [%5.1f%%] %c", 
				done*100.0/total,
				spinner[counter%4]);
	else
		fprintf(stderr, "\rEncoding to standard output [???%%] %c",
				spinner[counter%4]);
}

int oe_write_page(ogg_page *page, FILE *fp)
{
	fwrite(page->header,1,page->header_len, fp);
	fwrite(page->body,1,page->body_len, fp);

	return page->header_len+page->body_len;
}



