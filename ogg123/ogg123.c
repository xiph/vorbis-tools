/* ogg123.c by Kenneth Arnold <kcarnold@yahoo.com> */
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

 last mod: $Id: ogg123.c,v 1.11 2000/12/04 14:16:30 msmith Exp $

 ********************************************************************/

/* Takes a vorbis bitstream from stdin and writes raw stereo PCM to
   stdout.  Decodes simple and chained OggVorbis files from beginning
   to end.  Vorbisfile.a is somewhat more complex than the code below.  */

#define OGG123_VERSION "0.1"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h> /* !!! */
#include <unistd.h>
#include <netdb.h>
#include <netinet/in.h>
#include <errno.h>
#include <math.h>
#include <getopt.h> /* !!! */
#include <fcntl.h> /* !!! */
#include <time.h> /* !!! */
#include <sys/time.h> /* !!! */
#include <ogg/ogg.h>
#include <vorbis/codec.h>
#include <vorbis/vorbisfile.h>
#include <ao/ao.h>

char convbuffer[4096];	/* take 8k out of the data segment, not the stack */
int convsize = 4096;

/* For facilitating output to multiple devices */
typedef struct devices_struct 
{
  int driver_id;
  ao_device_t *device;
  ao_option_t *options;
  struct devices_struct *next_device;
} devices;

struct
{
  char *read_file;		/* File to decode */
  char shuffle; /* Should we shuffle playing? */
  signed short int verbose;	/* Verbose output if > 0, quiet if < 0 */
  signed short int quiet;	/* Be quiet (no title) */
  double seekpos;		/* Amount to seek by */
  FILE *instream;		/* Stream to read from. */
  devices *outdevices;		/* Streams to write to. */
}
param =
{
NULL, 0, 0, 0, 0, NULL, NULL};

struct
{
  ogg_int64_t c_length;	/* Compressed length */
  double u_time;	/* Uncompressed time */
  ogg_int64_t c_pos;	/* Position in the compressed file */
  double u_pos;		/* Position in the decoded output */
}
info =
{
-1, -1, 0, 0};

struct option long_options[] = {
  {"help", no_argument, 0, 'h'},
  {"version", no_argument, 0, 'V'},
  {"device", required_argument, 0, 'd'},
  {"skip", required_argument, 0, 'k'},
  {"device-option", required_argument, 0, 'o'},
  {"verbose", no_argument, 0, 'v'},
  {"quiet", no_argument, 0, 'q'},
  {"shuffle", no_argument, 0, 'z'},
  {0, 0, 0, 0}
};

void play_file (void);		/* This really needs to go in a header file. */
FILE *http_open (char *server, int port, char *path);

void
append_device (devices ** devices_list, int driver_id, ao_option_t *options)
{
  if (*devices_list != NULL)
    {
      while ((*devices_list)->next_device != NULL)
	{
	  // Beware of pointers to pointers!
	  devices_list = &((*devices_list)->next_device);
	}
      (*devices_list)->next_device = (devices *) malloc (sizeof (devices));
      devices_list = &((*devices_list)->next_device);
    }
  else
    {
      *devices_list = (devices *) malloc (sizeof (devices));
    }
  (*devices_list)->driver_id = driver_id;
  (*devices_list)->options = options;
  (*devices_list)->next_device = NULL;
}

void
free_devices (devices * devices_list)
{
  devices temp_device;
  while (devices_list->next_device)
    {
      temp_device = *devices_list;
      free (devices_list);
      devices_list = temp_device.next_device;
    }
  free (devices_list);
}

void
devices_write (void *ptr, size_t size, devices *d)
{
  while (d)
    {
      ao_play(d->device, ptr, size);
      d = d->next_device;
    }
}

void
usage ()
{
  FILE *o;
  o = stderr;

  fprintf (o, "Ogg123 %s\n", OGG123_VERSION);
  fprintf (o, " by Kenneth Arnold <kcarnold@yahoo.com> and crew\n\n");
  fprintf (o, "Usage: ogg123 [<options>] <input file> ...\n\n");
  fprintf (o, "  -h, --help     this help\n");
  fprintf (o, "  -V, --version  display Ogg123 version\n");
  fprintf (o, "  -d, --device=d uses 'd' as an output device)\n");
  fprintf (o, "      Possible devices are (some may not be compiled):\n");
  fprintf (o, "      null (output nothing), oss (for Linux and *BSD),\n");
  fprintf (o, "      irix, solaris, wav (write to a .WAV file)\n");
  fprintf (o, "  -k n, --skip n  Skip the first 'n' seconds\n");
  fprintf (o, 
           "  -o, --device-option=k:v passes special option k with value\n");
  fprintf (o, 
           "      v to previously specified device (with -d).  See\n");
  fprintf (o, 
           "      man page for more info.\n");

  fprintf (o,
	   "  -v, --verbose  display progress and other useful stuff (not yet)\n");
  fprintf (o, "  -q, --quiet    don't display anything (no title)\n");
  fprintf (o, "  -z, --shuffle  shuffle play\n");
}

int get_default_device(void)
{
	FILE *fp;
	char filename[NAME_MAX];
	char line[100];
	char *device = NULL;
	int i;

	strncpy(filename, getenv("HOME"), NAME_MAX);
	strcat(filename, "/.ogg123rc");

	fp = fopen(filename, "r");
	if (fp) {
		if (fgets(line, 100, fp)) {
			if (strncmp(line, "default_device=", 15) == 0) {
				device = &line[15];
				for (i = 0; i < strlen(device); i++)
					if (device[i] == '\n' || device[i] == '\r')
						device[i] = 0;
			}
		}
		fclose(fp);
	}

	if (device) return ao_get_driver_id(device);

	return -1;
}

int
main (int argc, char **argv)
{
  /* Parse command line */

  int ret;
  int option_index = 1;
  ao_option_t *temp_options = NULL;
  int temp_driver_id = -1;
  devices *current;
  int bits, rate, channels;

  ao_initialize();

  temp_driver_id = get_default_device();

  while (-1 != (ret = getopt_long (argc, argv, "d:hqk:o:vV:z",
				   long_options, &option_index)))
    {
      switch (ret)
	{
	case 0:
	  fprintf (stderr,
		   "Internal error: long option given when none expected.\n");
	  exit (1);
	case 'd':
	  /* Need to store previous device before gathering options for
	     this device */
	  if (temp_driver_id != -1)
	    {
	      append_device(&param.outdevices, temp_driver_id, temp_options);
	      temp_options = NULL;
	    }
	  temp_driver_id = ao_get_driver_id(optarg);
	  if (temp_driver_id < 0)
	    {
	      fprintf(stderr, "No such device %s.\n", optarg);
	      exit(1);
	    }
	  break;
	case 'k':
	  param.seekpos = atof (optarg);
	  break;
	case 'o':
	  if (optarg && !ao_append_option(&temp_options, optarg))
	    {
	      fprintf(stderr, "Incorrect option format: %s.\n", optarg);
	      exit(1);
	    }
	  break;
	case 'h':
	  usage ();
	  exit (0);
	case 'q':
	  param.quiet++;
	  break;
	case 'v':
	  param.verbose++;
	  break;
	case 'V':
	  fprintf (stderr, "Ogg123 %s\n", OGG123_VERSION);
	  exit (0);
	case 'z':
	  param.shuffle = 1;
	  break;
	case '?':
	  break;
	default:
	  usage ();
	  exit (1);
	}
    }
  
  /* Add last device to device list or use the default device */
  if (temp_driver_id < 0)
    {
      temp_driver_id = ao_get_driver_id(NULL);
      if (temp_driver_id < 0)
	{
	  fprintf(stderr, "Internal Error: Could not load default driver.\n");
	  exit(1);
	}
    }

  append_device(&param.outdevices, temp_driver_id, temp_options);
  
  if (optind == argc)
    {
      fprintf (stderr,
	       "Please specify a file to decode on the command line.\n");
      exit (1);
    }

  /* Open all of the devices */
  bits = 16;
  rate = 44100;
  channels = 2;
  current = param.outdevices;

  if (param.quiet < 1)
	  fprintf(stderr, "Opening devices...\n");

  while (current != NULL) {
    ao_info_t *info = ao_get_driver_info(current->driver_id);

    if (param.quiet < 1) {
    	fprintf(stderr, "Device:   %s\n", info->name);
    	fprintf(stderr, "Author:   %s\n", info->author);
    	fprintf(stderr, "Comments: %s\n", info->comment);
    }
    
    current->device = ao_open(current->driver_id,bits,rate,channels,
			     current->options);
    if (current->device == NULL)
      {
	fprintf(stderr, "Error opening device.\n");
	exit(1);
      }
    if (param.quiet < 1)
    	fprintf(stderr, "\n"); // Gotta keep 'em separated ...
 
    current = current->next_device;
  }

  if (param.shuffle)
    {
      int i=optind, j=0;
      for (i = optind; i < argc; i++)
	{
	  srand (time (NULL));
	  j = (int) ((float) (argc - optind) * rand () / (RAND_MAX + 1.0));
	  param.read_file = argv[j+optind];
	  play_file ();
	}
    }
  else
    {
      while (optind < argc)
	{
	  param.read_file = argv[optind];
	  play_file ();
	  optind++;
	}
    }

  current = param.outdevices;

  while(current != NULL)
  {
	  ao_close(current->device);
	  current = current->next_device;
  }

  ao_shutdown();

  return (0);
}

void play_file(void)
{
	OggVorbis_File vf;
	int current_section = -1, eof = 0, eos = 0, ret;
	int old_section = -1;
	long t_min = 0, c_min = 0, r_min = 0;
	double t_sec = 0, c_sec = 0, r_sec = 0;
	int is_big_endian = ao_is_big_endian();
	double realseekpos = param.seekpos;

  if (strcmp (param.read_file, "-"))	/* input file not stdin */
    {
      if (!strncmp (param.read_file, "http://", 7))
	{
	  /* Stream down over http */
	  char *temp = NULL, *server = NULL, *port = NULL, *path = NULL;
	  int index;
	  long iport;
	  
	  temp = param.read_file + 7;
	  for (index = 0; temp[index] != '/' && temp[index] != ':'; index++);
	  server = (char *) malloc (index + 1);
	  strncpy (server, temp, index);
	  server[index] = '\0';

	  /* Was a port specified? */
	  if (temp[index] == ':')
	    {
	      /* Grab the port. */
	      temp += index + 1;
	      for (index = 0; temp[index] != '/'; index++);
	      port = (char *) malloc (index + 1);
	      strncpy (port, temp, index);
	      port[index] = '\0';
	      if ((iport = atoi (port)) <= 0 || iport > 65535)
		{
		  fprintf (stderr, "%s is not a valid port.\n", port);
		  exit (1);
		}
	    }
	  else iport = 80;
	  
	  path = strdup (temp + index);

	  if ((param.instream = http_open (server, iport, path)) == NULL)
	    {
	      fprintf (stderr, "Error while connecting to server!\n");
	      exit (1);
	    }
	  /* Send HTTP header */
	  fprintf (param.instream, 
		   "GET %s HTTP/1.0\r\n"
		   "Accept: */*\r\n"
		   "User-Agent: ogg123\r\n"
		   "Host: %s\r\n\r\n\r\n", 
		   path, server);

	  /* Dump headers */
	  {
	    char last = 0, in = 0;
	    int eol = 0;
		
	    /* Need to 'quiet' this header dump */
	    fprintf (stderr, "HTTP Headers:\n");
	    for (;;)
	      {
		last = in;
		in = getc (param.instream);
		putc (in, stderr);
		if (last == 13 && in == 10)
		  {
		    if (eol)
		      break;
		    eol = 1;
		  }
		else if (in != 10 && in != 13)
		  eol = 0;
	      }
	  }
	  free (server);
	  free (path);
	}
      else
	{
	  if (param.quiet < 1)
	  	fprintf (stderr, "Playing from file %s.\n", param.read_file);
	  /* Open the file. */
	  if ((param.instream = fopen (param.read_file, "rb")) == NULL)
	    {
	      fprintf (stderr, "Error opening input file.\n");
	      exit (1);
	    }
	}
    }
  else
    {
      if (param.quiet < 1)
	      fprintf (stderr, "Playing from standard input.\n");
      param.instream = stdin;
    }

	if ((ov_open(param.instream, &vf, NULL, 0)) < 0) {
		fprintf(stderr, "E: input not an Ogg Vorbis audio stream.\n");
		return;
	}
  
	/* Throw the comments plus a few lines about the bitstream we're
	** decoding */

	while (!eof) {
		int i;
		vorbis_comment *vc = ov_comment(&vf, -1);
		vorbis_info *vi = ov_info(&vf, -1);

		if (param.quiet < 1) {
			for (i = 0; i < vc->comments; i++) {
				char *cc = vc->user_comments[i]; /* current comment */
				if (!strncasecmp ("ARTIST=", cc, 7))
					fprintf(stderr, "Artist: %s\n", cc + 7);
				else if (!strncasecmp("ALBUM=", cc, 6))
					fprintf(stderr, "Album: %s\n", cc + 6);
				else if (!strncasecmp("TITLE=", cc, 6))
					fprintf(stderr, "Title: %s\n", cc + 6);
				else if (!strncasecmp("VERSION=", cc, 8))
					fprintf(stderr, "Version: %s\n", cc + 8);
				else if (!strncasecmp("ORGANIZATION=", cc, 13))
					fprintf(stderr, "Organization: %s\n", cc + 13);
				else if (!strncasecmp("GENRE=", cc, 6))
					fprintf(stderr, "Genre: %s\n", cc + 6);
				else if (!strncasecmp("DESCRIPTION=", cc, 12))
					fprintf(stderr, "Description: %s\n", cc + 12);
				else if (!strncasecmp("DATE=", cc, 5))
					fprintf(stderr, "Date: %s\n", cc + 5);
				else if (!strncasecmp("LOCATION=", cc, 9))
					fprintf(stderr, "Location: %s\n", cc + 9);
				else if (!strncasecmp("COPYRIGHT=", cc, 10))
					fprintf(stderr, "Copyright: %s\n", cc + 10);
				else
					fprintf(stderr, "Unrecognized comment: %s\n", cc);
			}
    
		fprintf(stderr, "\nBitstream is %d channel, %ldHz\n", vi->channels, vi->rate);
		fprintf(stderr, "Encoded by: %s\n\n", vc->vendor);
		}

		if (param.verbose > 0) {
			/* Seconds with double precision */
			info.u_time = ov_time_total(&vf, -1);
			t_min = (long)info.u_time / (long)60;
			t_sec = info.u_time - 60 * t_min;
		}

		if ((realseekpos > ov_time_total (&vf, -1)) || (realseekpos < 0))
	         /* If we're out of range set it to right before the end. If we set it
        	  * right to the end when we seek it will go to the beginning of the sond */
			realseekpos = ov_time_total (&vf, -1) - 0.01;
 
		if (realseekpos > 0)
			ov_time_seek (&vf, realseekpos);

		eos = 0;
		while (!eos) {
			old_section = current_section;
			ret = ov_read(&vf, convbuffer, sizeof (convbuffer), is_big_endian, 2, 1, &current_section);
			if (ret == 0) {
				/* End of file */
				eof = eos = 1;
			} else if (ret < 0) {
				/* Stream error */
				fprintf(stderr, "W: Stream error\n");
			} else {
				/* less bytes than we asked for */
				/* did we enter a new logical bitstream */
				if (old_section != current_section && old_section != -1)
					eos = 1;

				devices_write(convbuffer, ret, param.outdevices);
				if (param.verbose > 0) {
					info.u_pos = ov_time_tell(&vf);
					c_min = (long)info.u_pos / (long)60;
					c_sec = info.u_pos - 60 * c_min;
					r_min = (long)(info.u_time - info.u_pos) / (long)60;
					r_sec = (info.u_time - info.u_pos) - 60 * r_min;
					fprintf (stderr, "\rTime: %02li:%05.2f [%02li:%05.2f] of %02li:%05.2f, Bitrate: %.1f   \r", c_min, c_sec, r_min, r_sec, t_min, t_sec, (float)ov_bitrate_instant(&vf) / 1000.0F);
				}
			}
		}
	}

	ov_clear(&vf);
      
	if (param.quiet < 1)
		fprintf (stderr, "\nDone.\n");
}

int get_tcp_socket(void)
{
	return socket(AF_INET, SOCK_STREAM, 0);
}

FILE *http_open (char *server, int port, char *path)
{
  int sockfd = get_tcp_socket ();
  struct hostent *host;
  struct sockaddr_in sock_name;

  if (sockfd == -1)
    return NULL;

  if (!(host = gethostbyname (server)))
    {
      fprintf (stderr, "Unknown host: %s\n", server);
      return NULL;
    }

  memcpy (&sock_name.sin_addr, host->h_addr, host->h_length);
  sock_name.sin_family = AF_INET;
  sock_name.sin_port = htons (port);

  if (connect (sockfd, (struct sockaddr *) &sock_name, sizeof (sock_name)))
    {
      if (errno == ECONNREFUSED)
	  fprintf (stderr, "Connection refused\n");
      return NULL;
    }
  return fdopen (sockfd, "r+b");
}  
