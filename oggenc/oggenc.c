/* OggEnc
 *
 * This program is distributed under the GNU General Public License, version 2.
 * A copy of this license is included with this source.
 *
 * Copyright 2000, Michael Smith <msmith@labyrinth.net.au>
 *
 * Portions from Vorbize, (c) Kenneth Arnold <kcarnold@yahoo.com>
 * and libvorbis examples, (c) Monty <monty@xiph.org>
 */

#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include <getopt.h>
#include <string.h>
#include <time.h>

#include "platform.h"
#include <vorbis/modes.h>
#include "encode.h"
#include "audio.h"

#define VERSION_STRING "OggEnc v0.5\n"
#define CHUNK 4096 /* We do reads, etc. in multiples of this */

/* Define supported formats here */
input_format formats[] = {
	{wav_open, wav_close, "wav", "WAV file reader"},
	{NULL, NULL, NULL, NULL}
};

struct option long_options[] = {
	{"quiet",0,0,'q'},
	{"help",0,0,'h'},
	{"comment",1,0,'c'},
	{"artist",1,0,'a'},
	{"album",1,0,'l'},
	{"title",1,0,'t'},
	{"names",1,0,'n'},
	{"output",1,0,'o'},
	{"version",0,0,'v'},
	{"raw",0,0,'r'},
	{"mode",1,0,'m'},
	{"date",1,0,'d'},
	{"tracknum",1,0,'N'},
	{NULL,0,0,0}
};
	
char *generate_name_string(char *format, char *artist, char *title, char *album, char *track, char *date);
void parse_options(int argc, char **argv, oe_options *opt);
void build_comments(vorbis_comment *vc, oe_options *opt, int filenum, 
		char **artist, char **album, char **title, char **tracknum, char **date);
void usage(void);
vorbis_info *choose_mode(int modenum, int channels);

int main(int argc, char **argv)
{
	oe_options opt = {NULL, 0, NULL, 0, NULL, 0, NULL, 0, NULL, 0, NULL, 0, 
		0,0, NULL,NULL,0}; /* Default values */
	int i;

	char **infiles;
	int numfiles;
	long nextserial;

	parse_options(argc, argv, &opt);

	if(optind >= argc)
	{
		fprintf(stderr, "ERROR: No input files specified. Use -h for help.\n");
		return 1;
	}
	else
	{
		infiles = argv + optind;
		numfiles = argc - optind;
	}

	/* Now, do some checking for illegal argument combinations */

	for(i = 0; i < numfiles; i++)
	{
		if(!strcmp(infiles[i], "-") && numfiles > 1)
		{
			fprintf(stderr, "ERROR: Multiple files specified when using stdin\n");
			exit(1);
		}
	}

	if(numfiles > 1 && opt.outfile)
	{
		fprintf(stderr, "ERROR: Multiple input files with specified output filename: suggest using -n\n");
		exit(1);
	}

	/* We randomly pick a serial number. This is then incremented for each file */
	srand(time(NULL));
	nextserial = rand();

	for(i = 0; i < numfiles; i++)
	{
		/* Once through the loop for each file */

		oe_enc_opt      enc_opts;
		vorbis_comment  vc;
		char *out_fn = NULL;
		FILE *in, *out = NULL;
		int foundformat = 0;
		int closeout = 0, closein = 0;
		int j=0;
		char *artist=NULL, *album=NULL, *title=NULL, *track=NULL, *date=NULL;



		/* Set various encoding defaults */

		enc_opts.serialno = nextserial++;
		enc_opts.quiet = opt.quiet;
		enc_opts.progress_update = update_statistics_full;
		enc_opts.end_encode = final_statistics;
		
		/* OK, let's build the vorbis_comments structure */
		build_comments(&vc, &opt, i, &artist, &album, &title, &track, &date);

		if(!strcmp(infiles[i], "-"))
		{
			setbinmode(stdin);
			in = stdin;
			if(!opt.outfile)
			{
				setbinmode(stdout);
				out = stdout;
			}
		}
		else
		{
			in = fopen(infiles[i], "rb");

			if(in == NULL)
			{
				fprintf(stderr, "ERROR: Cannot open input file \"%s\"\n", infiles[i]);
				free(out_fn);
				continue;
			}

			closein = 1;
		}

		if(opt.outfile && !strcmp(opt.outfile, "-"))
		{
			setbinmode(stdout);
			out = stdout;
		}
		else if(out == NULL)
		{
			if(opt.outfile)
			{
				out_fn = strdup(opt.outfile);
			}
			else if(opt.namefmt)
			{
				out_fn = generate_name_string(opt.namefmt, artist, title, album, track,date);
			}
			else if(opt.title)
			{
				out_fn = malloc(strlen(title) + 5);
				strcpy(out_fn, title);
				strcat(out_fn, ".ogg");
			}
			else
			{
				/* Create a filename from existing filename, replacing extension with .ogg */
				char *start, *end;

				start = infiles[i];
				end = rindex(infiles[i], '.');
				end = end?end:(start + strlen(infiles[i])+1);
			
				out_fn = malloc(end - start + 5);
				strncpy(out_fn, start, end-start);
				out_fn[end-start] = 0;
				strcat(out_fn, ".ogg");
			}


			out = fopen(out_fn, "wb");
			if(out == NULL)
			{
				if(closein)
					fclose(in);
				fprintf(stderr, "ERROR: Cannot open output file \"%s\"\n", out_fn);
				free(out_fn);
				continue;
			}	
			closeout = 1;
		}

		/* Now, set the rest of the options */
		enc_opts.out = out;
		enc_opts.comments = &vc;
		enc_opts.filename = out_fn;

		/* Now, we need to select an input audio format */

		if(opt.rawmode)
		{
			raw_open(in, &enc_opts);
			foundformat=1;
		}
		else
		{
			while(formats[j].open_func)
			{
				if(formats[j].open_func(in, &enc_opts))
				{
					foundformat = 1;
					break;
				}
				j++;
			}
		}

		if(!foundformat)
		{
			fprintf(stderr, "ERROR: Input file \"%s\" is not a supported format\n", infiles[i]);
			free(out_fn);
			continue;
		}

		enc_opts.mode = choose_mode(opt.modenum, enc_opts.channels);
		if(!enc_opts.total_samples_per_channel)
			enc_opts.progress_update = update_statistics_notime;

		if(!opt.quiet)
			fprintf(stderr, "Encoding using encoder mode %d\n", opt.modenum?opt.modenum:3);

		oe_encode(&enc_opts);

		if(out_fn) free(out_fn);
		free(enc_opts.mode);
		vorbis_comment_clear(&vc);
		if(!opt.rawmode)
			formats[j].close_func(enc_opts.readdata);

		if(closein)
			fclose(in);
		if(closeout)
			fclose(out);
	}/* Finished this file, loop around to next... */

	return 0;

}

void usage(void)
{
	fprintf(stdout, 
		VERSION_STRING
		"(c) 2000 Michael Smith <msmith@labyrinth.net.au)\n"
		"\n"
		"Usage: oggenc [options] input.wav [...]\n"
		"\n"
		"OPTIONS:\n"
		" General:\n"
		" -q, --quiet          Produce no output to stderr\n"
		" -h, --help           Print this help text\n"
		" -r, --raw            Raw mode. Input files are read directly as PCM data\n"
		" -m, --mode           Select encoding mode, takes an argument between 1 and 6\n"
		"                      The default is mode 3. Higher numbers are higher quality.\n"
		"\n"
		" Naming:\n"
		" -o, --output=fn      Write file to fn (only valid in single-file mode)\n"
		" -n, --names=string   Produce filenames as this string, with %%a, %%t, %%l,\n"
		"                      %%n, %%d replaces by artist, title, album, track number,\n"
		"                      and date, respectively (see below for specifying these).\n"
		"                      %%%% gives a literal %%.\n"
		" -c, --comment=c      Add the given string as an extra comment. This may be\n"
		"                      used multiple times.\n"
		" -d, --date           Date for track (usually date of performance)\n"
		" -N, --tracknum       Track number for this track\n"
		" -t, --title          Title for this track\n"
		" -l, --album          Name of album\n"
		" -a, --artist         Name of artist\n"
		"                      If multiple input files are given, then multiple\n"
		"                      instances of the previous five arguments will be used,\n"
		"                      in the order they are given. If fewer titles are\n"
		"                      specified than files, OggEnc will print a warning, and\n"
		"                      reuse the final one for the remaining files. If fewer\n"
		"                      track numbers are given, the remaining files will be\n"
		"                      unnumbered. For the others, the final tag will be reused\n"
		"                      for all others without warning (so you can specify a date\n"
		"                      once, for example, and have it used for all the files)\n"
		"\n"
		"INPUT FILES:\n"
		" OggEnc input files must currently be 44.1kHz, 16 bit stereo WAV files.\n"
		" You can specify taking the file from stdin by using - as the input filename.\n"
		" Alternatively, the --raw option may be used to use a raw PCM data file, with\n"
		" the same restrictions as above.\n"
		" In this mode, output is to stdout unless an outfile filename is specified\n"
		" with -o\n"
		"\n"
		"MODES:\n"
		" OggEnc support multiple different modes. Each of these is a fully VBR mode,\n"
		" but they vary in intended (average) bitrate. The following table shows \n"
		" approximate average bitrates (note that mode 3 is the default)\n"
		"     Mode    |    Bitrate (stereo)\n"
		"       1     |       N/A\n"
		"       2     |    128 kbps\n"
		"      *3     |    160 kbps\n"
		"       4     |    192 kbps\n"
		"       5     |    256 kbps\n"
		"       6     |    350 kbps\n\n");
}

char *generate_name_string(char *format, 
		char *artist, char *title, char *album, char *track, char *date)
{
	char *buffer;
	char *cur;
	char next;

	buffer = calloc(CHUNK,1);

	cur = buffer;


	while(*format)
	{
		next = *format++;

		if(next == '%')
		{
			switch(*format++)
			{
				case '%':
					*cur++ = '%';
					break;
				case 'a':
					strcat(buffer, artist?artist:"(none)");
					cur += strlen(artist?artist:"(none)");
					break;
				case 'd':
					strcat(buffer, date?date:"(none)");
					cur += strlen(date?date:"(none)");
					break;
				case 't':
					strcat(buffer, title?title:"(none)");
					cur += strlen(title?title:"(none)");
					break;
				case 'l':
					strcat(buffer, album?album:"(none)");
					cur += strlen(album?album:"(none)");
					break;
				case 'n':
					strcat(buffer, track?track:"(none)");
					cur += strlen(track?track:"(none)");
					break;
				default:
					fprintf(stderr, "WARNING: Ignoring illegal escape character '%c' in name format\n", *(format - 1));
					break;
			}
		}
		else
			*cur++ = next;
	}

	return buffer;
}

void parse_options(int argc, char **argv, oe_options *opt)
{
	int ret;
	int option_index = 1;

	while((ret = getopt_long(argc, argv, "a:c:d:hl:m:n:N:o:qrt:v", 
					long_options, &option_index)) != -1)
	{
		switch(ret)
		{
			case 0:
				fprintf(stderr, "Internal error parsing command line options\n");
				exit(1);
				break;
			case 'a':
				opt->artist = realloc(opt->artist, (++opt->artist_count)*sizeof(char *));
				opt->artist[opt->artist_count - 1] = strdup(optarg);
				break;
			case 'c':
				opt->comments = realloc(opt->comments, (++opt->comment_count)*sizeof(char *));
				opt->comments[opt->comment_count - 1] = strdup(optarg);
				break;
			case 'd':
				opt->dates = realloc(opt->dates, (++opt->date_count)*sizeof(char *));
				opt->dates[opt->date_count - 1] = strdup(optarg);
				break;
			case 'l':
				opt->album = realloc(opt->album, (++opt->album_count)*sizeof(char *));
				opt->album[opt->album_count - 1] = strdup(optarg);
				break;
			case 't':
				opt->title = realloc(opt->title, (++opt->title_count)*sizeof(char *));
				opt->title[opt->title_count - 1] = strdup(optarg);
				break;
			case 'm':
				opt->modenum = atoi(optarg);
				break;
			case 'n':
				if(opt->namefmt)
				{
					fprintf(stderr, "WARNING: Multiple name formats specified, using final\n");
					free(opt->namefmt);
				}
				opt->namefmt = strdup(optarg);
				break;
			case 'o':
				if(opt->outfile)
				{
					fprintf(stderr, "WARNING: Multiple output files specified, suggest using -n\n");
					free(opt->outfile);
				}
				opt->outfile = strdup(optarg);
				break;
			case 'h':
				usage();
				exit(0);
				break;
			case 'q':
				opt->quiet = 1;
				break;
			case 'r':
				opt->rawmode = 1;
				break;
			case 'v':
				fprintf(stderr, VERSION_STRING);
				exit(0);
				break;
			case 'N':
				opt->tracknum = realloc(opt->tracknum, (++opt->track_count)*sizeof(char *));
				opt->tracknum[opt->track_count - 1] = strdup(optarg);
				break;
			case '?':
				fprintf(stderr, "WARNING: Unknown option specified, ignoring->\n");
				break;
			default:
				usage();
				exit(0);
		}
	}
}

void build_comments(vorbis_comment *vc, oe_options *opt, int filenum, 
		char **artist, char **album, char **title, char **tracknum, char **date)
{
	int i;

	vorbis_comment_init(vc);

	for(i = 0; i < opt->comment_count; i++)
		vorbis_comment_add(vc, opt->comments[i]);

	if(opt->title_count)
	{
		if(filenum >= opt->title_count)
		{
			if(!opt->quiet)
				fprintf(stderr, "WARNING: Insufficient titles specified, defaulting to final title.\n");
			i = opt->title_count-1;
		}
		else
			i = filenum;

		*title = opt->title[i];
		vorbis_comment_add_tag(vc, "title", opt->title[i]);
	}

	if(opt->artist_count)
	{
		if(filenum >= opt->artist_count)
			i = opt->artist_count-1;
		else
			i = filenum;
	
		*artist = opt->artist[i];
		vorbis_comment_add_tag(vc, "artist", opt->artist[i]);
	}

	if(opt->date_count)
	{
		if(filenum >= opt->date_count)
			i = opt->date_count-1;
		else
			i = filenum;
	
		*date = opt->dates[i];
		vorbis_comment_add_tag(vc, "date", opt->dates[i]);
	}
	
	if(opt->album_count)
	{
		if(filenum >= opt->album_count)
		{
			i = opt->album_count-1;
		}
		else
			i = filenum;

		*album = opt->album[i];	
		vorbis_comment_add_tag(vc, "album", opt->album[i]);
	}

	if(filenum < opt->track_count)
	{
		i = filenum;
		*tracknum = opt->tracknum[i];
		vorbis_comment_add_tag(vc, "tracknumber", opt->tracknum[i]);
	}
}

vorbis_info *choose_mode(int modenum, int channels)
{
	vorbis_info *mode = malloc(sizeof(vorbis_info));

	if(modenum > 6 || modenum < 0)
		fprintf(stderr, "Warning: Unrecognised mode (choose from 1-6), defaulting to mode B (3)\n");

	switch(modenum)
	{
	case 0:
		memcpy(mode, &info_B, sizeof(vorbis_info)); /* Default */
		break;
	case 1:
		fprintf(stderr, "Warning: mode AA (1) is not yet implemented. Increasing to mode A (2)\n");
		memcpy(mode, &info_A, sizeof(vorbis_info)); 
		break;
	case 2:
		memcpy(mode, &info_A, sizeof(vorbis_info)); 
		break;
	case 3:
		memcpy(mode, &info_B, sizeof(vorbis_info)); 
		break;
	case 4:
		memcpy(mode, &info_C, sizeof(vorbis_info)); 
		break;
	case 5:
		memcpy(mode, &info_D, sizeof(vorbis_info)); 
		break;
	case 6:
		memcpy(mode, &info_E, sizeof(vorbis_info)); 
		break;
	default:
		memcpy(mode, &info_B, sizeof(vorbis_info)); 
	}

	if(channels ==1)
		mode->channels = 1; /* Edit the mode struct to reflect 
							   mono rather than stereo */
	return mode;
}

