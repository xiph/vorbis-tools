/* oggchain 0.1
     Hacked together by Kenneth Arnold <oggchain@arnoldnet.net>
   
   A simple tool to work with Ogg files as ordered archives. Currently
   only works with Vorbis (because that's all vorbisfile supports),
   but is designed with expandibility to all Ogg chained streams in
   mind.

   This currently uses libvorbisfile for the real work, with all
   intents to drop that once a general Ogg file access layer is
   implemented.
*/

#include <string.h>
#include <stdio.h>

#include "oggchain.h"

#define VERSION "0.1"

void usage (const char *name) {
  printf ("%s " VERSION " - a simple tool for working with Ogg archives\n"
	  " Kenneth Arnold <oggchain@arnoldnet.net>\n\n"
	  "Usage:\n"
	  " List the streams in an Ogg file:\n"
	  "  %s -l file.ogg\n"
	  " Concatenate several Ogg streams (equivalent to Unix cat(1)):\n"
	  "  %s -c out.ogg [file1.ogg file2.ogg ...]\n"
	  " Extract substreams of an Ogg file into other Ogg files:\n"
	  "  %s -x file.ogg [SECTION outfile.ogg ...]\n"
	  "  Where SECTION is a single argument specifying which parts of the source file\n"
	  "  are to be copied to the output file, and has the format:\n"
	  "  stream[:stream...], where 'stream' is a number or range. In future versions\n"
	  "  it will also be able to include sample number or time.\n",
	  name, name, name, name);
}

int main (int argc, char *argv[]) {
  /* we do not use getopt at the moment because there are only one or
     two flags that we'll need and ordering is much more important.
     (KISS also.) */

  if (argc < 2 ||
      (argc == 2 && 
       (strcmp(argv[1], "-h") == 0 ||
	strcmp(argv[1], "--help") == 0))) {
    usage(argv[0]);
    exit(0);
  }

  if (strcmp(argv[1], "-l") == 0) {
    /* list the substreams in the following file. */
    FILE *infile;
    OggVorbis_File vf;
    int ret;
    long i, nstreams;

    if (argc != 3) {
      usage(argv[0]);
      exit(0);
    }
    
    infile = fopen (argv[2], "rb");
    if (!infile) {
      perror ("opening input file");
      exit(1);
    }

    ret = ov_open (infile, &vf, NULL, 0);
    if (ret < 0) {
      fprintf (stderr, "Error opening Ogg stream. Input may not be an Ogg Vorbis file.\n");
      exit (1);
    }

    if (!ov_seekable(&vf)) {
      fprintf (stderr, "Input file not seekable, so it cannot currently be used as an archive.\n");
      exit (1);
    }
    
    nstreams = ov_streams (&vf);
    
    printf ("Input file has %ld stream%s:\n", nstreams, nstreams == 1 ? "" : "s");
    printf ("  Type            Raw Length              Samples\n");
    for (i = 0; i < nstreams; i++) {
      ogg_int64_t rawtotal, pcmtotal;

      rawtotal = ov_raw_total (&vf, i);
      pcmtotal = ov_pcm_total (&vf, i);

      /* a 64-bit integer can theoretically take up to 20 digits to
	 represent in base 10. */
      printf (" Vorbis %20lld %20lld\n", rawtotal, pcmtotal);
    }
  } else {
    fprintf (stderr, "other options currently unimplemented.\n");
  }
  return 0;
}
