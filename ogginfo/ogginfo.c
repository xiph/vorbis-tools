/* ogginfo
 *
 * This program is distributed under the GNU General Public License, version 2.
 * A copy of this license is included with this source.
 *
 * Copyright 2001, JAmes Atwill <ogg@linuxstuff.org>
 *
 * Portions from libvorbis examples, (c) Monty <monty@xiph.org>
 */

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <ogg/ogg.h>
#include <vorbis/codec.h>
#include <vorbis/vorbisfile.h>

void doinfo(char *);
int  dointegritycheck(char *);
int  test_header(FILE *fp, ogg_sync_state *oy, ogg_stream_state *os,
		 vorbis_info    *vi,  vorbis_comment  *vc);
int  test_stream(FILE *fp, ogg_sync_state *oy, ogg_stream_state *os);

int main(int ac,char **av)
{
  int i;
  int header_state;

  if ( ac < 2 ) {
    fprintf(stderr,"Usage: %s [filename1.ogg] ... [filenameN.ogg]\n",av[0]);
    return(0);
  }

  for(i=1;i!=ac;i++) {
    printf("filename=%s\n",av[i]);
    header_state = dointegritycheck(av[i]);
    if (header_state == 1)
      doinfo(av[i]);
  }
  return(0);
  
}

void doinfo(char *filename)
{
  FILE *fp;
  OggVorbis_File vf;
  int rc,i;
  vorbis_comment *vc;
  double playtime;
  long playmin,playsec;

  memset(&vf,0,sizeof(OggVorbis_File));
  
  fp = fopen(filename,"rb");
  if (!fp) {
    fprintf(stderr,"Unable to open \"%s\": %s\n",
	    filename,
	    strerror(errno));
  }

  rc = ov_open(fp,&vf,NULL,0);

  if (rc < 0) {
    fprintf(stderr,"Unable to understand \"%s\", errorcode=%d\n",
	    filename,rc);
    return;
  }
  
  vc = ov_comment(&vf,-1);

  for (i=0; i < vc->comments; i++) {
    printf("%s\n",vc->user_comments[i]);
  }

  playtime = ov_time_total(&vf,-1);

  playmin = (long)playtime / (long)60;
  playsec = (long)playtime - (playmin*60);
  printf("length=%f\n",playtime);
  printf("playtime=%ld:%02ld\n",playmin,playsec);

  ov_clear(&vf);

  return;
}

/* Tests the integrity of a vorbis stream.  Returns 1 if the header is good
   (but not necessarily the rest of the stream) and 0 otherwise.
   
   Huge chunks of this function are from decoder_example.c (Copyright
   1994-2001 Xiphophorus Company). */
int dointegritycheck(char *filename)
{
  int header_state = -1; /* Assume the worst */
  int stream_state = -1;

  ogg_sync_state   oy; /* sync and verify incoming physical bitstream */
  ogg_stream_state os; /* take physical pages, weld into a logical
			  stream of packets */
  vorbis_info      vi; /* struct that stores all the static vorbis bitstream
			  settings */
  vorbis_comment   vc; /* struct that stores all the bitstream user comments */
  
  FILE *fp;
  
  /********** Decode setup ************/

  fp = fopen(filename,"rb");
  if (!fp) {
    fprintf(stderr,"Unable to open \"%s\": %s\n",
	    filename,
	    strerror(errno));
    return 0;
  }

  ogg_sync_init(&oy); /* Now we can read pages */
  
  if ( (header_state = test_header(fp, &oy, &os, &vi, &vc)) == 1 ) {

    stream_state = test_stream(fp, &oy, &os);
  }

  /* Output test results */
  if (header_state == 1)
    printf("header_integrity=pass\n");
  else
    printf("header_integrity=fail\n");

  if (stream_state >= 0)
    printf("stream_integrity=pass\n");
  else
    printf("stream_integrity=fail\n");

  if (stream_state > 0)
    printf("file_truncated=false\n");
  else
    printf("file_truncated=true\n");

  
  /* clean up this logical bitstream; before exit we see if we're
     followed by another [chained] */

  if (header_state == 0) {
    /* We got far enough to initialize these structures */

    ogg_stream_clear(&os);
    
    /* ogg_page and ogg_packet structs always point to storage in
       libvorbis.  They're never freed or manipulated directly */
    
    vorbis_comment_clear(&vc);
    vorbis_info_clear(&vi);  /* must be called last */
  }

  /* OK, clean up the framer */
  ogg_sync_clear(&oy);
  
  fclose(fp);

  return header_state > 0 ? 1 : 0;
}

/* Test the integrity of the stream header.  
   Return:
     1 if it is good
     0 if it is corrupted and os, vi, and vc were initialized
    -1 if it is corrupted and os, vi, and vc were not initialized 
       (don't clear them) */
int test_header (FILE *fp, ogg_sync_state *oy, ogg_stream_state *os,
		 vorbis_info *vi,  vorbis_comment  *vc)
{
  ogg_page         og; /* one Ogg bitstream page.  Vorbis packets are inside */
  ogg_packet       op; /* one raw packet of data for decode */
  char *buffer;
  int bytes;
  int eos=0;
  int i;
  
  /* grab some data at the head of the stream.  We want the first page
     (which is guaranteed to be small and only contain the Vorbis
     stream initial header) We need the first page to get the stream
     serialno. */
  
  /* submit a 4k block to libvorbis' Ogg layer */
  buffer=ogg_sync_buffer(oy,4096);
  bytes=fread(buffer,1,4096,fp);
  ogg_sync_wrote(oy,bytes);
  
  /* Get the first page. */
  if(ogg_sync_pageout(oy,&og)!=1){
    /* error case.  Must not be Vorbis data */
    return -1;
  }
  
  /* Get the serial number and set up the rest of decode. */
  /* serialno first; use it to set up a logical stream */
  ogg_stream_init(os,ogg_page_serialno(&og));
  
  /* extract the initial header from the first page and verify that the
     Ogg bitstream is in fact Vorbis data */
  
  /* I handle the initial header first instead of just having the code
     read all three Vorbis headers at once because reading the initial
     header is an easy way to identify a Vorbis bitstream and it's
     useful to see that functionality seperated out. */
  
  vorbis_info_init(vi);
  vorbis_comment_init(vc);
  if(ogg_stream_pagein(os,&og)<0){ 
    /* error; stream version mismatch perhaps */
    return 0;
  }
    
  if(ogg_stream_packetout(os,&op)!=1){ 
    /* no page? must not be vorbis */
    return 0;
  }
  
  if(vorbis_synthesis_headerin(vi,vc,&op)<0){ 
    /* error case; not a vorbis header */
    return 0;
  }
    
  /* At this point, we're sure we're Vorbis.  We've set up the logical
     (Ogg) bitstream decoder.  Get the comment and codebook headers and
     set up the Vorbis decoder */
  
  /* The next two packets in order are the comment and codebook headers.
     They're likely large and may span multiple pages.  Thus we reead
     and submit data until we get our two pacakets, watching that no
     pages are missing.  If a page is missing, error out; losing a
     header page is the only place where missing data is fatal. */
  
  i=0;
  while(i<2){
    while(i<2){
      int result=ogg_sync_pageout(oy,&og);
      if(result==0)break; /* Need more data */
      /* Don't complain about missing or corrupt data yet.  We'll
	 catch it at the packet output phase */
      if(result==1){
	ogg_stream_pagein(os,&og); /* we can ignore any errors here
				       as they'll also become apparent
				       at packetout */
	while(i<2){
	  result=ogg_stream_packetout(os,&op);
	  if(result==0)break;
	  if(result<0){
	    /* Uh oh; data at some point was corrupted or missing!
	       We can't tolerate that in a header. */
	    return 0;
	  }
	  vorbis_synthesis_headerin(vi,vc,&op);
	  i++;
	}
      }
    }

    /* no harm in not checking before adding more */
    buffer=ogg_sync_buffer(oy,4096);
    bytes=fread(buffer,1,4096,fp);
    if(bytes==0 && i<2){
      return 0;
    }
    ogg_sync_wrote(oy,bytes);
  }
  
  /* If we made it this far, the header must be good. */
  return 1;
}

/* Test the integrity of the vorbis stream after the header.
   Return:
     >0 if the stream is correct and complete
     0 if the stream is correct but truncated
    -1 if the stream is corrupted somewhere in the middle */
int test_stream (FILE *fp, ogg_sync_state *oy, ogg_stream_state *os)
{
  int eos = 0;
  ogg_page         og; /* one Ogg bitstream page.  Vorbis packets are inside */
  ogg_packet       op; /* one raw packet of data for decode */
  int bytes;
  char *buffer;

  /* Just a straight decode loop until end of stream */
    while(!eos){
      while(!eos){
	int result=ogg_sync_pageout(oy,&og);
	if(result==0)break; /* need more data */
	if(result<0){ /* missing or corrupt data at this page position */
	  return -1;
	}else{
	  if (ogg_stream_pagein(os,&og) < 0) {
	    return -1;
	  }

	  while(1){
	    result=ogg_stream_packetout(os,&op);

	    if(result==0)break; /* need more data */
	    if(result<0){ /* missing or corrupt data at this page position */
	      return -1;
	    }else{
	      /* Normally we would do decode here, but we're just checking
		 packet integrity */
	    }
	  }
	  if(ogg_page_eos(&og))eos=1;
	}
      }
      if(!eos){
	buffer=ogg_sync_buffer(oy,4096);
	bytes=fread(buffer,1,4096,fp);
	ogg_sync_wrote(oy,bytes);
	if(bytes==0)eos=1;
      }
    }

    /* Make sure that the last page is marked as the end-of-stream */
    return ogg_page_eos(&og);
}


