/* This program is licensed under the GNU General Public License, 
 * version 2, a copy of which is included with this program.
 *
 * (c) 2000-2001 Michael Smith <msmith@labyrinth.net.au>
 * (c) 2001 Ralph Giles <giles@ashlu.bc.ca>
 *
 * Front end to show how to use vcedit;
 * Of limited usability on its own, but could be useful.
 */


#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "getopt.h"
#include "utf8.h"

#include "vcedit.h"

/* getopt format struct */
struct option long_options[] = {
	{"list",0,0,'l'},
	{"write",0,0,'w'},
	{"help",0,0,'h'},
	{"quiet",0,0,'q'},
	{"commentfile",1,0,'c'},
	{NULL,0,0,0}
};

/* local parameter storage from parsed options */
typedef struct {
	int	mode;
	char	*infilename, *outfilename;
	char	*commentfilename;
	FILE	*in, *out, *com;
	int commentcount;
	char **comments;
	int tempoutfile;
	char *encoding;
} param_t;

#define MODE_NONE  0
#define MODE_LIST  1
#define MODE_WRITE 2
#define MODE_APPEND 3

/* prototypes */
void usage(void);
void print_comments(FILE *out, vorbis_comment *vc);
int  add_comment(char *line, vorbis_comment *vc, char *encoding);

param_t	*new_param(void);
void parse_options(int argc, char *argv[], param_t *param);
void open_files(param_t *p);
void close_files(param_t *p);


/**********
   main.c

   This is the main function where options are read and written
   you should be able to just read this function and see how
   to call the vcedit routines. Details of how to pack/unpack the
   vorbis_comment structure itself are in the following two routines.
   The rest of the file is ui dressing so make the program minimally
   useful as a command line utility and can generally be ignored.

***********/

int main(int argc, char **argv)
{
	vcedit_state *state;
	vorbis_comment *vc;
	param_t	*param;
	int i;

	/* initialize the cmdline interface */
	param = new_param();
	parse_options(argc, argv, param);

	/* take care of opening the requested files */
	/* relevent file pointers are returned in the param struct */
	open_files(param);

	/* which mode are we in? */

	if (param->mode == MODE_LIST) {
		
		state = vcedit_new_state();

		if(vcedit_open(state, param->in) < 0)
		{
			fprintf(stderr, "Failed to open file as vorbis: %s\n", 
					vcedit_error(state));
			return 1;
		}

		/* extract and display the comments */
		vc = vcedit_comments(state);
		print_comments(param->com, vc);

		/* done */
		vcedit_clear(state);

		close_files(param);
		return 0;		
	}

	if (param->mode == MODE_WRITE || param->mode == MODE_APPEND) {

		state = vcedit_new_state();

		if(vcedit_open(state, param->in) < 0)
		{
			fprintf(stderr, "Failed to open file as vorbis: %s\n", 
					vcedit_error(state));
			return 1;
		}

		/* grab and clear the exisiting comments */
		vc = vcedit_comments(state);
		if(param->mode != MODE_APPEND) 
		{
			vorbis_comment_clear(vc);
			vorbis_comment_init(vc);
		}

		for(i=0; i < param->commentcount; i++)
		{
			if(add_comment(param->comments[i], vc, param->encoding) < 0)
				fprintf(stderr, "Bad comment: \"%s\"\n", param->comments[i]);
		}

		/* build the replacement structure */
		if(param->commentcount==0)
		{
			/* FIXME should use a resizeable buffer! */
			char *buf = (char *)malloc(sizeof(char)*1024);

			while (fgets(buf, 1024, param->com))
				if (add_comment(buf, vc, param->encoding) < 0) {
					fprintf(stderr,
						"bad comment: \"%s\"\n",
						buf);
				}
			
			free(buf);
		}

		/* write out the modified stream */
		if(vcedit_write(state, param->out) < 0)
		{
			fprintf(stderr, "Failed to write comments to output file: %s\n", 
					vcedit_error(state));
			return 1;
		}

		/* done */
		vcedit_clear(state);
		
		close_files(param);
		return 0;
	}

	/* should never reach this point */
	fprintf(stderr, "no action specified\n");
	return 1;
}

/**********

   Print out the comments from the vorbis structure

   this version just dumps the raw strings
   a more elegant version would use vorbis_comment_query()

***********/

void print_comments(FILE *out, vorbis_comment *vc)
{
	int i;

	for (i = 0; i < vc->comments; i++)
		fprintf(out, "%s\n", vc->user_comments[i]);
}

/**********

   Take a line of the form "TAG=value string", parse it, convert the
   value to UTF-8 from the specified encoding, and add it to the
   vorbis_comment structure. Error checking is performed.

   Note that this assumes a null-terminated string, which may cause
   problems with > 8-bit character sets!

***********/

int  add_comment(char *line, vorbis_comment *vc, char *encoding)
{
	char	*mark, *value, *utf8_value;

	/* strip any terminal newline */
	{
		int len = strlen(line);
		if (line[len-1] == '\n') line[len-1] = '\0';
	}

	/* validation: basically, we assume it's a tag
	 * if it has an '=' after one or more valid characters,
	 * as the comment spec requires. For the moment, we
	 * also restrict ourselves to 0-terminated values */

	mark = strchr(line, '=');
	if (mark == NULL) return -1;

	value = line;
	while (value < mark) {
		if(*value < 0x20 || *value > 0x7d || *value == 0x3d) return -1;
		value++;
	}

	/* split the line by turning the '=' in to a null */
	*mark = '\0';	
	value++;

	/* convert the value from the native charset to UTF-8 */
	if (utf8_encode(value, &utf8_value, encoding) == 0) {
		
		/* append the comment and return */
		vorbis_comment_add_tag(vc, line, value);
		return 0;
	} else {
		fprintf(stderr, "Couldn't convert comment to UTF8, "
			"cannot add\n");
		return -1;
	}
}


/*** ui-specific routines ***/

/**********

   Print out to usage summary for the cmdline interface (ui)

***********/

void usage(void)
{
	fprintf(stderr, 
		"Usage: \n"
		"  vorbiscomment [-l] file.ogg (to list the comments)\n"
		"  vorbiscomment -a in.ogg out.ogg (to append comments)\n"
		"  vorbiscomment -w in.ogg out.ogg (to modify comments)\n"
		"	in the write case, a new set of comments in the form\n"
		"	'TAG=value' is expected on stdin. This set will\n"
		"	completely replace the existing set.\n"
		"   Either of -a and -w can take only a single filename,\n"
		"   in which case a temporary file will be used.\n"
		"   -c can be used to take comments from a specified file\n"
		"   instead of stdin.\n"
		"   Example: vorbiscomment -a in.ogg -c comments.txt\n"
		"   will append the comments in comments.txt to in.ogg\n"
		"   Finally, you may specify any number of tags to add on\n"
		"   the command line using the -t option. e.g.\n"
		"   vorbiscomment -a in.ogg -t \"ARTIST=Some Guy\" -t \"TITLE=A Title\"\n"
		"   (note that when using this, reading comments from the comment\n"
		"   file or stdin is disabled)\n"
	); 
}


/**********

   allocate and initialize a the parameter struct

***********/

param_t *new_param(void)
{
	param_t *param = (param_t *)malloc(sizeof(param_t));

	/* mode */
	param->mode = MODE_LIST;

	/* filenames */
	param->infilename  = NULL;
	param->outfilename = NULL;
	param->commentfilename = "-";	/* default */

	/* file pointers */
	param->in = param->out = NULL;
	param->com = NULL;

	param->commentcount=0;
	param->comments=NULL;
	param->tempoutfile=0;

	/* character encoding */
	param->encoding = "ISO-8859-1";

	return param;
}

/**********
   parse_options()

   This function takes care of parsing the command line options
   with getopt() and fills out the param struct with the mode,
   flags, and filenames.

***********/

void parse_options(int argc, char *argv[], param_t *param)
{
	int ret;
	int option_index = 1;

	while ((ret = getopt_long(argc, argv, "aelwhqc:t:",
			long_options, &option_index)) != -1) {
		switch (ret) {
			case 0:
				fprintf(stderr, "Internal error parsing command options\n");
				exit(1);
				break;
			case 'l':
				param->mode = MODE_LIST;
				break;
			case 'w':
				param->mode = MODE_WRITE;
				break;
			case 'a':
				param->mode = MODE_APPEND;
				break;
			case 'e':
				param->mode = strdup(optarg);
				break;
			case 'h':
				usage();
				exit(0);
				break;
			case 'q':
				/* set quiet flag */
				break;
			case 'c':
				param->commentfilename = strdup(optarg);
				break;
			case 't':
				param->comments = realloc(param->comments, 
						(param->commentcount+1)*sizeof(char *));
				param->comments[param->commentcount++] = strdup(optarg);
				break;
			default:
				usage();
				exit(1);
		}
	}

	/* remaining bits must be the filenames */
	if((param->mode == MODE_LIST && (argc-optind) != 1) ||
	   ((param->mode == MODE_WRITE || param->mode == MODE_APPEND) &&
	   ((argc-optind) < 1 || (argc-optind) > 2))) {
			usage();
			exit(1);
	}

	param->infilename = strdup(argv[optind]);
	if (param->mode == MODE_WRITE || param->mode == MODE_APPEND)
	{
		if(argc-optind == 1)
		{
			param->tempoutfile = 1;
			param->outfilename = malloc(strlen(param->infilename)+8);
			strcpy(param->outfilename, param->infilename);
			strcat(param->outfilename, ".vctemp");
		}
		else
			param->outfilename = strdup(argv[optind+1]);
	}
}

/**********
   open_files()

   This function takes care of opening the appropriate files
   based on the mode and filenames in the param structure.
   A filename of '-' is interpreted as stdin/out.

   The idea is just to hide the tedious checking so main()
   is easier to follow as an example.

***********/

void open_files(param_t *p)
{
	/* for all modes, open the input file */

	if (strncmp(p->infilename,"-",2) == 0) {
		p->in = stdin;
	} else {
		p->in = fopen(p->infilename, "rb");
	}
	if (p->in == NULL) {
		fprintf(stderr,
			"Error opening input file '%s'.\n",
			p->infilename);
		exit(1);
	}

	if (p->mode == MODE_WRITE || p->mode == MODE_APPEND) { 

		/* open output for write mode */

		if (strncmp(p->outfilename,"-",2) == 0) {
			p->out = stdout;
		} else {
			p->out = fopen(p->outfilename, "wb");
		}
		if(p->out == NULL) {
			fprintf(stderr,
				"Error opening output file '%s'.\n",
				p->outfilename);
			exit(1);
		}

		/* commentfile is input */
		
		if ((p->commentfilename == NULL) ||
				(strncmp(p->commentfilename,"-",2) == 0)) {
			p->com = stdin;
		} else {
			p->com = fopen(p->commentfilename, "rb");
		}
		if (p->com == NULL) {
			fprintf(stderr,
				"Error opening comment file '%s'.\n",
				p->commentfilename);
			exit(1);
		}

	} else {

		/* in list mode, commentfile is output */

		if ((p->commentfilename == NULL) ||
				(strncmp(p->commentfilename,"-",2) == 0)) {
			p->com = stdout;
		} else {
			p->com = fopen(p->commentfilename, "wb");
		}
		if (p->com == NULL) {
			fprintf(stderr,
				"Error opening comment file '%s'\n",
				p->commentfilename);
			exit(1);
		}
	}

	/* all done */
}

/**********
   close_files()

   Do some quick clean-up.

***********/

void close_files(param_t *p)
{
	/* FIXME: should handle stdin/out */

	if (p->in != NULL) fclose(p->in);
	if (p->out != NULL) fclose(p->out);
	if (p->com != NULL) fclose(p->com);

	if(p->tempoutfile)
		rename(p->outfilename, p->infilename);
}
