// gcc ogginfo.c -o ogginfo -lvorbisfile -lvorbis -Wall

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <ogg/ogg.h>
#include <vorbis/codec.h>
#include <vorbis/vorbisfile.h>
#include <ao/ao.h>

void doinfo(char *);

int main(int ac,char **av)
{
  int i;

  if ( ac < 2 ) {
    fprintf(stderr,"Usage: %s [filename1.ogg] ... [filenameN.ogg]\n",av[0]);
    return(0);
  }

  for(i=1;i!=ac;i++) {
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
  
  fp = fopen(filename,"r");
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
  
  printf("filename=%s\n",filename);
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
