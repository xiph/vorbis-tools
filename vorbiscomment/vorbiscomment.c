/* This program is provided under the GNU General Public License, version 2, 
 * which is included with this program.
 *
 * (c) 2000 Michael Smith <msmith@labyrinth.net.au>
 * Portions (c) 2000 Kenneth C. Arnold <kcarnold@yahoo.com>
 * 
 * *********************************************************************
 * Vorbis comment field editor - designed to be wrapped by scripts, etc.
 * and as a proof of concept/example code/
 * 
 * last mod: $Id: vorbiscomment.c,v 1.3 2000/10/03 02:21:23 jack Exp $
 */


#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <ogg/ogg.h>
#include <vorbis/codec.h>

#define CHUNK 4096

int edit_comments(vorbis_comment *in, vorbis_comment *out);

int main(int argc, char **argv)
{

	char *buffer;
	int bytes;
	int i;
	int serial;
	int result;
	int eosin=0,eosout=0;
	FILE *input, *output;


	vorbis_comment   vc;
	vorbis_comment   vcnew;
	vorbis_info      vi;
	vorbis_dsp_state vd;
	ogg_sync_state   oy;
	ogg_stream_state os;
	ogg_stream_state out;
	ogg_page		 og;
	ogg_page		 outpage;
	ogg_packet		 op;

	ogg_packet	 	*header;
	ogg_packet header_main;
	ogg_packet header_main_original;
	ogg_packet header_comments;
	ogg_packet header_codebooks;
	ogg_packet header_codebooks_original;

	if(argc == 3)
	{
	  if (strcmp (argv[1], "-l") == 0 || strcmp (argv[1], "--list") == 0)
	    {
	      input = fopen (argv[2], "rb");
	      if (input == NULL)
		{
		  fprintf (stderr, "Cannot open input file\n");
		  exit (1);
		}
	      output = NULL;
	    }
	  else
	    {
	      input = fopen(argv[1], "r");
	      output = fopen(argv[2], "w");
	      
	      if(input == NULL || output == NULL)
		{
		  fprintf(stderr, "Error opening file\n");
		  exit(1);
		}
	    }
	}
	else if(argc == 2)
	{
	  input = fopen(argv[1],"rb");
	  if(input == NULL)
	    {
	      fprintf(stderr, "Cannot open input file\n");
	      exit(1);
	    }
	  remove(argv[1]);
	  output = fopen(argv[1], "w");
	  if(output == NULL)
	    {
	      fprintf(stderr, "Cannot open output file\n");
	      exit(1);
	    }
	}
	else
	  {
	    fprintf(stderr, "Usage: vorbiscomment infile.ogg outfile.ogg\n"
		    "       vorbiscomment file.ogg\n");
	    exit(1);
	  }
	
	
	
	
	ogg_sync_init(&oy);
	
	buffer=ogg_sync_buffer(&oy,CHUNK);
	bytes=fread(buffer,1,CHUNK,input);
	ogg_sync_wrote(&oy, bytes);
	
	if(ogg_sync_pageout(&oy,&og)!=1)
	  {
	    fprintf(stderr, "Some error\n");
	    exit(1);
	  }
	
	serial = ogg_page_serialno(&og);
	ogg_stream_init(&os,serial);
	
	vorbis_info_init(&vi);
	vorbis_comment_init(&vc);
	
	if(ogg_stream_pagein(&os,&og)<0)
	  {
	    fprintf(stderr, "couldn't read first page\n");
	    exit(1);
	  }
	
	if(ogg_stream_packetout(&os,&header_main_original)!=1)
	  {
	    fprintf(stderr, "First header broken\n");
	    exit(1);
	  }
	
	if(vorbis_synthesis_headerin(&vi,&vc,&header_main_original)<0)
	  {
	    fprintf(stderr, "ogg, but not vorbis\n");
	    exit(1);
	  }
	
	i = 0;
	header = &header_comments;
	while(i<2){
	  while(i<2){
	    int result = ogg_sync_pageout(&oy,&og);
	    if(result==0) break;
	    else if(result ==1)
	      {
		ogg_stream_pagein(&os,&og);
		
		while(i<2){
		  result = ogg_stream_packetout(&os,header);
		  if(result==0)break;
		  if(result==-1)
		    {
		      fprintf(stderr, "Failed horribly\n");
		      exit(1);
		    }
		  vorbis_synthesis_headerin(&vi,&vc,header);
		  i++;
		  header = &header_codebooks_original;
		}
	      }
	  }
	  buffer=ogg_sync_buffer(&oy,CHUNK);
	  bytes=fread(buffer,1,4096,input);
	  if(bytes==0 && i<2){
	    fprintf(stderr, "bits of header missing\n");
	    exit(1);
	  }
	  ogg_sync_wrote(&oy,bytes);
	}
	
	/* We now have full headers - so we can do what we want to them! */
	
	if (output == NULL)
	  {
	    for(i=0;i<vc.comments;i++)
	      {
		printf("%s\n",vc.user_comments[i]);
	      }
	    vorbis_comment_clear (&vc);
	  }
	else
	  {

	    edit_comments(&vc,&vcnew);
	    vorbis_comment_clear(&vc);
	    
	    

	    /* The following needs some explanation - the API doesn't currently give us
	     * a `clean' way of doing this.
	     *
	     * We need to call vorbis_analysis_headerout() to build the header packets,
	     * but the first argument to that is a vorbis_dsp_state *.
	     * So, we need to initialise this, for which we have vorbis_analysis_init(),
	     * but this doesn't work - because our vorbis_info struct is set up for 
	     * DECODE, and so is missing the encode-only parameters, so we crash.
	     *
	     * To avoid this, we take a look inside block.c, figure out what 
	     * vorbis_analysis_init() does, and what we actually NEED to do - it turns
	     * out that it's reasonably simple, we just need to clear everything else,
	     * and set the vorbis_info pointer to the one we decoded from the other
	     * header packets.
	     */
	    
	    //vorbis_analysis_init(&vd,&vi);
	    memset(&vd,0,sizeof(vd));
	    vd.vi = &vi; /* Ugly hack? Hell yeah! */
	    
	    /* End ugly hack */
	    
	    ogg_stream_init(&out,serial);
	    
	    vorbis_analysis_headerout(&vd,&vcnew,&header_main,&header_comments,&header_codebooks);
	    
	    ogg_stream_packetin(&out,&header_main);
	    ogg_stream_packetin(&out,&header_comments);
	    ogg_stream_packetin(&out,&header_codebooks);

	    while((result = ogg_stream_flush(&os, &og)))
	    {
		if(!result) break;
		fwrite(og.header,1, og.header_len, output);
		fwrite(og.body,1,og.body_len, output);
	    }
	    
	    /* Hard bit - stream ogg packets out, ogg packets in */
	    
	    while(!(eosin && eosout)){
	      while(!(eosin && !eosout)){

		result=ogg_sync_pageout(&oy,&og);
		if(result==0)break;
		else if(result==-1)fprintf(stderr, "Error in bitstream, continuing\n");
		else
		  {
		    ogg_stream_pagein(&os,&og);
		    
		    while(1)
		      {
			result = ogg_stream_packetout(&os,&op);
			if(result==0)break;
			else if(result ==-1) { }
			else
			  {
			    ogg_stream_packetin(&out,&op);
			    
			    while(!(eosin &&eosout)){
			      int result=ogg_stream_pageout(&out,&outpage);
			      if(result==0)break;
			      fwrite(outpage.header,1,outpage.header_len,output);
			      fwrite(outpage.body,1,outpage.body_len,output);
			      
			      if(ogg_page_eos(&outpage))eosout=1;
			    }
			  }
		      }
		    if(ogg_page_eos(&og))eosin=1;
		  }
	      }
	      if(!(eosin && eosout))
		{
		  buffer=ogg_sync_buffer(&oy,CHUNK);
		  bytes=fread(buffer,1,CHUNK,input);
		  ogg_sync_wrote(&oy,bytes);
		  if(bytes==0)eosin=1;
		}
	    }
	    
	    ogg_stream_clear(&os);
	    ogg_stream_clear(&out);
	    
	    ogg_sync_clear(&oy);
	    
	    fclose(output);
	  }
	fclose(input);
	
	return(0);
}	

int edit_comments(vorbis_comment *in, vorbis_comment *out)
{
  char comment[1024];
  vorbis_comment_init(out);
  
  while (1)
    {
      fgets (comment, sizeof (comment), stdin);
      if (feof (stdin))
	break;
      if (comment[strlen(comment) - 1] == '\n')
	comment[strlen(comment) - 1] = 0;
      printf ("%s\n", comment);
      vorbis_comment_add (out, comment);
    }
  return 0;
}

