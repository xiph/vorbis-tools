/********************************************************************
 *                                                                  *
 * THIS FILE IS PART OF THE OggVorbis SOFTWARE CODEC SOURCE CODE.   *
 * USE, DISTRIBUTION AND REPRODUCTION OF THIS SOURCE IS GOVERNED BY *
 * THE GNU PUBLIC LICENSE 2, WHICH IS INCLUDED WITH THIS SOURCE.    *
 * PLEASE READ THESE TERMS BEFORE DISTRIBUTING.                     *
 *                                                                  *
 * THE Ogg123 SOURCE CODE IS (C) COPYRIGHT 2000-2001                *
 * by Stan Seibert <volsung@xiph.org> AND OTHER CONTRIBUTORS        *
 * http://www.xiph.org/                                             *
 *                                                                  *
 ********************************************************************

 last mod: $Id: cmdline_options.c,v 1.9 2002/01/01 02:22:14 jack Exp $

 ********************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ao/ao.h>

#include "getopt.h"
#include "cmdline_options.h"
#include "status.h"

#define MIN_INPUT_BUFFER_SIZE 8

struct option long_options[] = {
  /* GNU standard options */
    {"help", no_argument, 0, 'h'},
    {"version", no_argument, 0, 'V'},
    /* ogg123-specific options */
    {"buffer", required_argument, 0, 'b'},
    {"config", optional_argument, 0, 'c'},
    {"device", required_argument, 0, 'd'},
    {"file", required_argument, 0, 'f'},
    {"skip", required_argument, 0, 'k'},
    {"delay", required_argument, 0, 'l'},
    {"device-option", required_argument, 0, 'o'},
    {"prebuffer", required_argument, 0, 'p'},
    {"quiet", no_argument, 0, 'q'},
    {"verbose", no_argument, 0, 'v'},
    {"nth", required_argument, 0, 'x'},
    {"ntimes", required_argument, 0, 'y'},
    {"shuffle", no_argument, 0, 'z'},
    {0, 0, 0, 0}
};


int parse_cmdline_options (int argc, char **argv,
			   ogg123_options_t *ogg123_opts,
			   file_option_t    *file_opts)
{
  int option_index = 1;
  ao_option *temp_options = NULL;
  ao_option ** current_options = &temp_options;
  ao_info *info;
  int temp_driver_id = -1;
  audio_device_t *current;
  int ret;

  while (-1 != (ret = getopt_long(argc, argv, "b:c::d:f:hl:k:o:p:qvVx:y:z",
				  long_options, &option_index))) {

      switch (ret) {
      case 0:
	status_error("Internal error: long option given when none expected.\n");
	exit(1);
	
      case 'b':
	ogg123_opts->input_buffer_size = atoi(optarg) * 1024;
	if (ogg123_opts->input_buffer_size < MIN_INPUT_BUFFER_SIZE * 1024) {
	  status_error("Input buffer size smaller than minimum size of %dkB.",
		       MIN_INPUT_BUFFER_SIZE);
	  ogg123_opts->input_buffer_size = MIN_INPUT_BUFFER_SIZE * 1024;
	}
	break;
	
      case 'c':
	if (optarg) {
	  char *tmp = strdup (optarg);
	  parse_code_t pcode = parse_line(file_opts, tmp);

	  if (pcode != parse_ok)
	    status_error("=== Error \"%s\" while parsing config option from command line.\n"
			 "=== Option was: %s\n",
			 parse_error_string(pcode), optarg);
	  free (tmp);
	}
	else {
	  /* not using the status interface here */
	  fprintf (stdout, "Available options:\n");
	  file_options_describe(file_opts, stdout);
	  exit (0);
	}
	break;
	
      case 'd':
	temp_driver_id = ao_driver_id(optarg);
	if (temp_driver_id < 0) {
	    status_error("=== No such device %s.\n", optarg);
	    exit(1);
	}

	current = append_audio_device(ogg123_opts->devices,
				      temp_driver_id, 
				      NULL, NULL);
	if(ogg123_opts->devices == NULL)
	  ogg123_opts->devices = current;
	current_options = &current->options;
	break;
	
      case 'f':
	if (temp_driver_id >= 0) {

	  info = ao_driver_info(temp_driver_id);
	  if (info->type == AO_TYPE_FILE) {
	    free(current->filename);
	    current->filename = strdup(optarg);
	  } else {
	    status_error("=== Driver %s is not a file output driver.\n",
			 info->short_name);
	    exit(1);
	  }
	} else {
	  status_error("=== Cannot specify output file without specifying a driver.\n");
	  exit (1);
	}
	break;

	case 'k':
	  ogg123_opts->seekpos = atof(optarg);
	  break;
	  
	case 'l':
	  ogg123_opts->delay = atoi(optarg);
	  break;
	  
	case 'o':
	  if (optarg && !add_ao_option(current_options, optarg)) {
	    status_error("=== Incorrect option format: %s.\n", optarg);
	    exit(1);
	  }
	  break;

	case 'h':
	  cmdline_usage();
	  exit(0);
	  break;
	  
	case 'p':
	  ogg123_opts->input_prebuffer = atof (optarg);
	  if (ogg123_opts->input_prebuffer < 0.0f || 
	      ogg123_opts->input_prebuffer > 100.0f) {

	    status_error ("--- Prebuffer value invalid. Range is 0-100.\n");
	    ogg123_opts->input_prebuffer = 
	      ogg123_opts->input_prebuffer < 0.0f ? 0.0f : 100.0f;
	  }
	  break;

      case 'q':
	ogg123_opts->verbosity = 0;
	break;
	
      case 'v':
	ogg123_opts->verbosity++;
	break;
	
      case 'V':
	status_error("ogg123 from " PACKAGE " " VERSION "\n");
	exit(0);
	break;

      case 'x':
	ogg123_opts->nth = atoi(optarg);
	if (ogg123_opts->nth == 0) {
	  status_error("--- Cannot play every 0th chunk!\n");
	  ogg123_opts->nth = 1;
	}
	break;
	  
      case 'y':
	ogg123_opts->ntimes = atoi(optarg);
	if (ogg123_opts->ntimes == 0) {
	  status_error("--- Cannot play every chunk 0 times.\n"
		 "--- To do a test decode, use the null output driver.\n");
	  ogg123_opts->ntimes = 1;
	}
	break;
	
      case 'z':
	ogg123_opts->shuffle = 1;
	break;
	
      case '?':
	break;
	
      default:
	cmdline_usage();
	exit(1);
      }
  }

  /* Add last device to device list or use the default device */
  if (temp_driver_id < 0) {

      /* First try config file setting */
      if (ogg123_opts->default_device) {
	  temp_driver_id = ao_driver_id(ogg123_opts->default_device);

	  if (temp_driver_id < 0)
	    status_error("--- Driver %s specified in configuration file invalid.\n",
			 ogg123_opts->default_device);
      }
      
      /* Then try libao autodetect */
      if (temp_driver_id < 0)
	temp_driver_id = ao_default_driver_id();

      /* Finally, give up */
      if (temp_driver_id < 0) {
	status_error("=== Could not load default driver and no driver specified in config file. Exiting.\n");
	exit(1);
      }

      ogg123_opts->devices = append_audio_device(ogg123_opts->devices,
					     temp_driver_id,
					     temp_options, 
					     NULL);
    }


  return optind;
}


void cmdline_usage (void)
{
  int i, driver_count;
  ao_info **devices = ao_driver_info_list(&driver_count);

  printf ( 
         "ogg123 from " PACKAGE " " VERSION "\n"
	 " by the Xiphophorus Team (http://www.xiph.org/)\n\n"
	 "Usage: ogg123 [<options>] <input file> ...\n\n"
	 "  -h, --help     this help\n"
	 "  -V, --version  display Ogg123 version\n"
	 "  -d, --device=d uses 'd' as an output device\n"
	 "      Possible devices are ('*'=live, '@'=file):\n"
	 "        ");
  
  for(i = 0; i < driver_count; i++) {
    printf ("%s", devices[i]->short_name);
    if (devices[i]->type == AO_TYPE_LIVE)
      printf ("*");
    else if (devices[i]->type == AO_TYPE_FILE)
      printf ("@");
    printf (" ");
  }

  printf ("\n");
  
  printf (
	 "  -f, --file=filename  Set the output filename for a previously\n"
	 "      specified file device (with -d).\n"
	 "  -k n, --skip n  Skip the first 'n' seconds\n"
	 "  -o, --device-option=k:v passes special option k with value\n"
	 "      v to previously specified device (with -d).  See\n"
	 "      man page for more info.\n"
	 "  -b n, --buffer n  use an input buffer of 'n' kilobytes\n"
	 "  -p n, --prebuffer n  load n%% of the input buffer before playing\n"
	 "  -v, --verbose  display progress and other status information\n"
	 "  -q, --quiet    don't display anything (no title)\n"
	 "  -x n, --nth    play every 'n'th block\n"
	 "  -y n, --ntimes repeat every played block 'n' times\n"
	 "  -z, --shuffle  shuffle play\n"
	 "\n"
	 "ogg123 will skip to the next song on SIGINT (Ctrl-C); two SIGINTs within\n"
	 "s milliseconds make ogg123 terminate.\n"
	 "  -l, --delay=s  set s [milliseconds] (default 500).\n");
}
