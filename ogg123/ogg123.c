/* ogg123.c by Kenneth Arnold <kcarnold@arnoldnet.net> */
/* Modified to use libao by Stan Seibert <volsung@asu.edu> */

/********************************************************************
 *                                                                  *
 * THIS FILE IS PART OF THE OggVorbis SOFTWARE CODEC SOURCE CODE.   *
 * USE, DISTRIBUTION AND REPRODUCTION OF THIS SOURCE IS GOVERNED BY *
 * THE GNU PUBLIC LICENSE 2, WHICH IS INCLUDED WITH THIS SOURCE.    *
 * PLEASE READ THESE TERMS BEFORE DISTRIBUTING.                     *
 *                                                                  *
 * THE OggVorbis SOURCE CODE IS (C) COPYRIGHT 1994-2000             *
 * by Monty <monty@xiph.org> and the XIPHOPHORUS Company            *
 * http://www.xiph.org/                                             *
 *                                                                  *
 ********************************************************************

 last mod: $Id: ogg123.c,v 1.44 2001/08/07 21:37:50 volsung Exp $

 ********************************************************************/

/* FIXME : That was a messy message. Fix it. */

#include <sys/types.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <netdb.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <errno.h>
#include <time.h>
#include <getopt.h>

#include <signal.h>

#include "ogg123.h"

/* take buffer out of the data segment, not the stack */
char convbuffer[BUFFER_CHUNK_SIZE];
int convsize = BUFFER_CHUNK_SIZE;
buf_t * buffer = NULL;

static char skipfile_requested;
static void (*old_sig)(int);

struct {
    char *key;			/* includes the '=' for programming convenience */
    char *formatstr;		/* formatted output */
} ogg_comment_keys[] = {
  {"ARTIST=", "Artist: %s\n"},
  {"ALBUM=", "Album: %s\n"},
  {"TITLE=", "Title: %s\n"},
  {"VERSION=", "Version: %s\n"},
  {"TRACKNUMBER=", "Track number: %s\n"},
  {"ORGANIZATION=", "Organization: %s\n"},
  {"GENRE=", "Genre: %s\n"},
  {"DESCRIPTION=", "Description: %s\n"},
  {"DATE=", "Date: %s\n"},
  {"LOCATION=", "Location: %s\n"},
  {"COPYRIGHT=", "Copyright %s\n"},
  {NULL, NULL}
};

struct option long_options[] = {
    {"help", no_argument, 0, 'h'},
    {"version", no_argument, 0, 'V'},
    {"device", required_argument, 0, 'd'},
    {"file", required_argument, 0, 'f'},
    {"skip", required_argument, 0, 'k'},
    {"device-option", required_argument, 0, 'o'},
    {"verbose", no_argument, 0, 'v'},
    {"quiet", no_argument, 0, 'q'},
    {"shuffle", no_argument, 0, 'z'},
    {"buffer", required_argument, 0, 'b'},
    {"delay", required_argument, 0, 'l'},
    {0, 0, 0, 0}
};

void usage(void)
{
    FILE *o = stderr;
    int i, driver_count;
    ao_info **devices = ao_driver_info_list(&driver_count);

    fprintf(o,
	    "Ogg123 from " PACKAGE " " VERSION "\n"
	    " by Kenneth Arnold <kcarnold@arnoldnet.net> and others\n\n"
	    "Usage: ogg123 [<options>] <input file> ...\n\n"
	    "  -h, --help     this help\n"
	    "  -V, --version  display Ogg123 version\n"
	    "  -d, --device=d uses 'd' as an output device\n"
	    "      Possible devices are:\n"
	    "        ");

    for(i = 0; i < driver_count; i++)
      fprintf(o,"%s ",devices[i]->short_name);

    fprintf(o,"\n");

    fprintf(o,
	    "  -f, --file=filename  Set the output filename for a previously\n"
	    "      specified file device (with -d).\n"
	    "  -k n, --skip n  Skip the first 'n' seconds\n"
	    "  -o, --device-option=k:v passes special option k with value\n"
	    "      v to previously specified device (with -d).  See\n"
	    "      man page for more info.\n"
	    "  -b n, --buffer n  use a buffer of approximately 'n' kilobytes\n"
	    "  -v, --verbose  display progress and other useful stuff\n"
	    "  -q, --quiet    don't display anything (no title)\n"
	    "  -z, --shuffle  shuffle play\n"
	    "\n"
	    "ogg123 will skip to the next song on SIGINT (Ctrl-C) after s seconds after\n"
	    "song start."
	    "  -l, --delay=s  set s (default 1). If s=-1, disable song skip.\n");
}

int main(int argc, char **argv)
{
    ogg123_options_t opt;
    int ret;
    int option_index = 1;
    ao_option *temp_options = NULL;
    ao_option ** current_options = &temp_options;
    ao_info *info;
    int temp_driver_id = -1;
    devices_t *current;
    
    opt.read_file = NULL;
    opt.shuffle = 0;
    opt.verbose = 0;
    opt.quiet = 0;
    opt.seekpos = 0;
    opt.instream = NULL;
    opt.outdevices = NULL;
    opt.buffer_size = 0;
    opt.delay = 1;

    ao_initialize();

    while (-1 != (ret = getopt_long(argc, argv, "b:d:f:hl:k:o:qvVz",
				    long_options, &option_index))) {
	switch (ret) {
	case 0:
	    fprintf(stderr,
		    "Internal error: long option given when none expected.\n");
	    exit(1);
	case 'b':
	  opt.buffer_size = atoi(optarg) / (BUFFER_CHUNK_SIZE / 1024);
	  if (opt.buffer_size == 1)
	    opt.buffer_size = 0; /* Hack to work around boundary case in
				    buffering code.  FIXME! */
	  break;
	case 'd':
	    temp_driver_id = ao_driver_id(optarg);
	    if (temp_driver_id < 0) {
		fprintf(stderr, "No such device %s.\n", optarg);
		exit(1);
	    }
	    current = append_device(opt.outdevices, temp_driver_id, 
				    NULL, NULL);
	    if(opt.outdevices == NULL)
		    opt.outdevices = current;
	    current_options = &current->options;
	    break;
	case 'f':
	    info = ao_driver_info(temp_driver_id);
	    if (info->type == AO_TYPE_FILE) {
	        free(current->filename);
		current->filename = strdup(optarg);
	    } else {
	        fprintf(stderr, "Driver %s is not a file output driver.\n",
			info->short_name);
	        exit(1);
	    }
	    break;
	case 'k':
	    opt.seekpos = atof(optarg);
	    break;
	case 'l':
	    opt.delay = atoi(optarg);
	    break;
	case 'o':
	    if (optarg && !add_option(current_options, optarg)) {
		fprintf(stderr, "Incorrect option format: %s.\n", optarg);
		exit(1);
	    }
	    break;
	case 'h':
	    usage();
	    exit(0);
	case 'q':
	    opt.quiet++;
	    break;
	case 'v':
	    opt.verbose++;
	    break;
	case 'V':
	    fprintf(stderr, "Ogg123 from " PACKAGE " " VERSION "\n");
	    exit(0);
	case 'z':
	    opt.shuffle = 1;
	    break;
	case '?':
	    break;
	default:
	    usage();
	    exit(1);
	}
    }

    /* Add last device to device list or use the default device */
    if (temp_driver_id < 0) {
	temp_driver_id = get_default_device();
	if(temp_driver_id < 0) {
		temp_driver_id = ao_default_driver_id();
	}
	if (temp_driver_id < 0) {
	    fprintf(stderr,
		    "Could not load default driver and no ~/.ogg123rc found. Exiting.\n");
	    exit(1);
	}
	opt.outdevices = append_device(opt.outdevices, temp_driver_id, 
				       temp_options, NULL);
    }

    if (optind == argc) {
	usage();
	exit(1);
    }

    if (opt.shuffle) {
	int i;
	
	srand(time(NULL));

	for (i = optind; i < argc; i++) {
		int j = optind + rand() % (argc - i);
		char *temp = argv[i];
		argv[i] = argv[j];
		argv[j] = temp;
	}
    }

    while (optind < argc) {
	opt.read_file = argv[optind];
	play_file(opt);
	optind++;
    }

    while (opt.outdevices != NULL) {
      if (opt.outdevices->device)
        ao_close(opt.outdevices->device);
      current = opt.outdevices->next_device;
      free(opt.outdevices);
      opt.outdevices = current;
    }

    if (buffer != NULL)
	    buffer_shutdown(buffer);
    
    ao_shutdown();

    return (0);
}

/* Two signal handlers, one for SIGINT, and the second for
 * SIGALRM.  They are de/activated on an as-needed basis by the
 * player to allow the user to stop ogg123 or skip songs.
 */

void signal_skipfile(int which_signal)
{
  skipfile_requested = 1;

  /* libao, when writing wav's, traps SIGINT so it correctly
   * closes things down in the event of an interrupt.  We
   * honour this.   libao will re-raise SIGINT once it cleans
   * up properly, causing the application to exit.  This is 
   * desired since we would otherwise re-open output.wav 
   * and blow away existing "output.wav" file.
   */

  if (old_sig != NULL) {
    signal(which_signal,old_sig);
    raise(which_signal);
  }

}

void signal_activate_skipfile(int ignored)
{
  old_sig = signal(SIGINT,signal_skipfile);
}


void play_file(ogg123_options_t opt)
{
    /* Oh my gosh this is disgusting. Big cleanups here will include an
       almost complete rewrite of the hacked-out HTTP streaming and a shift
       to using callbacks for the vorbisfile input.
    */

    OggVorbis_File vf;
    int current_section = -1, eof = 0, eos = 0, ret;
    int old_section = -1;
    long t_min = 0, c_min = 0, r_min = 0;
    double t_sec = 0, c_sec = 0, r_sec = 0;
    int is_big_endian = ao_is_big_endian();
    double realseekpos = opt.seekpos;

    /* Junk left over from the failed info struct */
    double u_time, u_pos;

    if (strcmp(opt.read_file, "-")) {	/* input file not stdin */
	if (!strncmp(opt.read_file, "http://", 7)) {
	    /* Stream down over http */
	    char *temp = NULL, *server = NULL, *port = NULL, *path = NULL;
	    int index;
	    long iport;

	    temp = opt.read_file + 7;
	    for (index = 0; temp[index] != '/' && temp[index] != ':';
		 index++);
	    server = (char *) malloc(index + 1);
	    strncpy(server, temp, index);
	    server[index] = '\0';

	    /* Was a port specified? */
	    if (temp[index] == ':') {
		/* Grab the port. */
		temp += index + 1;
		for (index = 0; temp[index] != '/'; index++);
		port = (char *) malloc(index + 1);
		strncpy(port, temp, index);
		port[index] = '\0';
		if ((iport = atoi(port)) <= 0 || iport > 65535) {
		    fprintf(stderr, "%s is not a valid port.\n", port);
		    exit(1);
		}
	    } else
		iport = 80;

	    path = strdup(temp + index);

	    if ((opt.instream = http_open(server, iport, path)) == NULL) {
		fprintf(stderr, "Error while connecting to server!\n");
		exit(1);
	    }
	    /* Send HTTP header */
	    fprintf(opt.instream,
		    "GET %s HTTP/1.0\r\n"
		    "Accept: */*\r\n"
		    "User-Agent: ogg123\r\n"
		    "Host: %s\r\n\r\n\r\n", path, server);

		fflush(opt.instream); /* Make sure these are all actually sent */

	    /* Dump headers */
	    {
		char last = 0, in = 0;
		int eol = 0;

		if (opt.verbose > 0)
		  fprintf(stderr, "HTTP Headers:\n");
		for (;;) {
		    last = in;
		    in = getc(opt.instream);
		    if (opt.verbose > 0)
		      putc(in, stderr);
		    if (last == 13 && in == 10) {
			if (eol)
			    break;
			eol = 1;
		    } else if (in != 10 && in != 13)
			eol = 0;
		}
	    }
	    free(server);
	    free(path);
	} else {
	    if (opt.quiet < 1)
		fprintf(stderr, "Playing from file %s.\n", opt.read_file);
	    /* Open the file. */
	    if ((opt.instream = fopen(opt.read_file, "rb")) == NULL) {
		fprintf(stderr, "Error opening input file.\n");
		exit(1);
	    }
	}
    } else {
	if (opt.quiet < 1)
	    fprintf(stderr, "Playing from standard input.\n");
	opt.instream = stdin;
    }

    if ((ov_open(opt.instream, &vf, NULL, 0)) < 0) {
	fprintf(stderr, "E: input not an Ogg Vorbis audio stream.\n");
	return;
    }

    /* Setup so that pressing ^C in the first second of playback
     * interrupts the program, but after the first second, skips
     * the song.  This functionality is similar to mpg123's abilities. */

    if (opt.delay > 0) {
        skipfile_requested = 0;
	signal(SIGALRM,signal_activate_skipfile);
	alarm(opt.delay);
    }

    while (!eof) {
	int i;
	vorbis_comment *vc = ov_comment(&vf, -1);
	vorbis_info *vi = ov_info(&vf, -1);

	if(open_audio_devices(&opt, vi->rate, vi->channels, &buffer) < 0)
		exit(1);

	if (opt.quiet < 1) {
	    if (eos && opt.verbose) fprintf (stderr, "\r                                                      \r\n");
	    for (i = 0; i < vc->comments; i++) {
		char *cc = vc->user_comments[i];	/* current comment */
		int i;

		for (i = 0; ogg_comment_keys[i].key != NULL; i++)
		    if (!strncasecmp
			(ogg_comment_keys[i].key, cc,
			 strlen(ogg_comment_keys[i].key))) {
			fprintf(stderr, ogg_comment_keys[i].formatstr,
				cc + strlen(ogg_comment_keys[i].key));
			break;
		    }
		if (ogg_comment_keys[i].key == NULL)
		    fprintf(stderr, "Unrecognized comment: '%s'\n", cc);
	    }

	    fprintf(stderr, "\nBitstream is %d channel, %ldHz\n",
		    vi->channels, vi->rate);
	    if (opt.verbose > 1)
	      fprintf(stderr, "Encoded by: %s\n\n", vc->vendor);
	}

	if (opt.verbose > 0 && ov_seekable(&vf)) {
	    /* Seconds with double precision */
	    u_time = ov_time_total(&vf, -1);
	    t_min = (long) u_time / (long) 60;
	    t_sec = u_time - 60 * t_min;
	}

	if ((realseekpos > ov_time_total(&vf, -1)) || (realseekpos < 0))
	    /* If we're out of range set it to right before the end. If we set it
	     * right to the end when we seek it will go to the beginning of the song */
	    realseekpos = ov_time_total(&vf, -1) - 0.01;

	if (realseekpos > 0)
	    ov_time_seek(&vf, realseekpos);

	eos = 0;

	while (!eos) {

	    if (skipfile_requested) {
	      eof = eos = 1;
	      skipfile_requested = 0;
	      signal(SIGALRM,signal_activate_skipfile);
	      alarm(opt.delay);
	      if (buffer) {
		buffer_flush (buffer);
	      }
	      break;
  	    }

	    old_section = current_section;
	    ret =
		ov_read(&vf, convbuffer, sizeof(convbuffer), is_big_endian,
			2, 1, &current_section);
	    if (ret == 0) {
		/* End of file */
		eof = eos = 1;
	    } else if (ret == OV_HOLE) {
	      if (opt.verbose > 1) 
		/* we should be able to resync silently; if not there are 
		   bigger problems. */
		fprintf (stderr, "Warning: hole in the stream; probably harmless\n");
	    } else if (ret < 0) {
	      /* Stream error */
	      fprintf(stderr, "Error: libvorbis reported a stream error.\n");
	    } else {
		/* did we enter a new logical bitstream */
		if (old_section != current_section && old_section != -1)
		    eos = 1;

		if (buffer)
		  {
		    chunk_t chunk;
		    chunk.len = ret;
		    memcpy (chunk.data, convbuffer, ret);
		    
		    submit_chunk (buffer, chunk);
		  }
		else
		  devices_write(convbuffer, ret, opt.outdevices);
		
		if (opt.verbose > 0) {
		    if (ov_seekable (&vf)) {
		      u_pos = ov_time_tell(&vf);
		      c_min = (long) u_pos / (long) 60;
		      c_sec = u_pos - 60 * c_min;
		      r_min = (long) (u_time - u_pos) / (long) 60;
		      r_sec = (u_time - u_pos) - 60 * r_min;
		      fprintf(stderr,
			      "\rTime: %02li:%05.2f [%02li:%05.2f] of %02li:%05.2f, Bitrate: %.1f   \r",
			      c_min, c_sec, r_min, r_sec, t_min, t_sec,
			      (float) ov_bitrate_instant(&vf) / 1000.0F);
		    } else {
		      /* working around a bug in vorbisfile */
		      u_pos = (double) ov_pcm_tell(&vf) / (double) vi->rate;
		      c_min = (long) u_pos / (long) 60;
		      c_sec = u_pos - 60 * c_min;
		      fprintf(stderr,
			      "\rTime: %02li:%05.2f, Bitrate: %.1f   \r",
			      c_min, c_sec,
			      (float) ov_bitrate_instant (&vf) / 1000.0F);
		    }
		}
	    }
	}
    }

    alarm(0);
    signal(SIGALRM,SIG_DFL);
    signal(SIGINT,old_sig);

    ov_clear(&vf);

    if (opt.quiet < 1)
	fprintf(stderr, "\nDone.\n");
}

int get_tcp_socket(void)
{
    return socket(AF_INET, SOCK_STREAM, 0);
}

FILE *http_open(char *server, int port, char *path)
{
    int sockfd = get_tcp_socket();
    struct hostent *host;
    struct sockaddr_in sock_name;

    if (sockfd == -1)
	return NULL;

    if (!(host = gethostbyname(server))) {
	fprintf(stderr, "Unknown host: %s\n", server);
	return NULL;
    }

    memcpy(&sock_name.sin_addr, host->h_addr, host->h_length);
    sock_name.sin_family = AF_INET;
    sock_name.sin_port = htons(port);

    if (connect(sockfd, (struct sockaddr *) &sock_name, sizeof(sock_name))) {
	if (errno == ECONNREFUSED)
	    fprintf(stderr, "Connection refused\n");
	return NULL;
    }
    return fdopen(sockfd, "r+b");
}

int open_audio_devices(ogg123_options_t *opt, int rate, int channels, buf_t **buffer)
{
  static int prevrate=0, prevchan=0;
  devices_t *current;
  ao_sample_format format;

  if(prevrate == rate && prevchan == channels)
    return 0;
  
  if(prevrate !=0 && prevchan!=0)
	{
	  if (buffer != NULL && *buffer != NULL) {
	    buffer_shutdown (*buffer);
	    *buffer = NULL;
	  }

	  current = opt->outdevices;
	  while (current != NULL) {
	    ao_close(current->device);
	    current = current->next_device;
	  }
	}
  
  format.rate = prevrate = rate;
  format.channels = prevchan = channels;
  format.bits = 16;
  format.byte_format = AO_FMT_NATIVE;

  current = opt->outdevices;
  while (current != NULL) {
    ao_info *info = ao_driver_info(current->driver_id);
    
    if (opt->verbose > 0) {
      fprintf(stderr, "Device:   %s\n", info->name);
      fprintf(stderr, "Author:   %s\n", info->author);
      fprintf(stderr, "Comments: %s\n", info->comment);
      fprintf(stderr, "\n");	
    }
    
    if (current->filename == NULL)
      current->device = ao_open_live(current->driver_id, &format,
				     current->options);
    else
      current->device = ao_open_file(current->driver_id, current->filename,
				     0, &format, current->options);

    if (current->device == NULL) {
      switch (errno) {
      case AO_ENODRIVER:
	fprintf(stderr, "Error: No device not available.\n");
	break;
      case AO_ENOTLIVE:
	fprintf(stderr, "Error: %s requires an output filename to be specified with -f.\n", info->short_name);
	break;
      case AO_EBADOPTION:
	fprintf(stderr, "Error: Unsupported option value to %s device.\n",
		info->short_name);
	break;
      case AO_EOPENDEVICE:
	fprintf(stderr, "Error: Cannot open device %s.\n",
		info->short_name);
	break;
      case AO_EFAIL:
	fprintf(stderr, "Error: Device failure.\n");
	break;
      case AO_ENOTFILE:
	fprintf(stderr, "Error: An output file cannot be given for %s device.\n", info->short_name);
	break;
      case AO_EOPENFILE:
	fprintf(stderr, "Error: Cannot open file %s for writing.\n",
		current->filename);
	break;
      case AO_EFILEEXISTS:
	fprintf(stderr, "Error: File %s already exists.\n", current->filename);
	break;
      default:
	fprintf(stderr, "Error: This error should never happen.  Panic!\n");
	break;
      }
	
      return -1;
    }
    current = current->next_device;
  }
  
  if (opt->buffer_size)
    *buffer = fork_writer (opt->buffer_size, opt->outdevices);
  
    return 0;
}
