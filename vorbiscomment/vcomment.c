/* This program is licensed under the GNU General Public License, 
 * version 2, a copy of which is included with this program.
 *
 * (c) 2000-2001 Michael Smith <msmith@labyrinth.net.au>
 * (c) 2001 Ralph Giles <giles@ashlu.bc.ca>
 *
 * Front end to show how to use vcedit;
 * note that it's not very useful on its own.
 */


#include <stdio.h>
#include <string.h>
#include <getopt.h>

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
} param_t;

#define MODE_NONE  0
#define MODE_LIST  1
#define MODE_WRITE 2

/* prototypes */
void usage(void);
void print_comments(FILE *out, vorbis_comment *vc);
int  add_comment(char *line, vorbis_comment *vc);

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
	FILE *in, *out;

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
			fprintf(stderr, "Failed to open file as vorbis.\n");
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

	if (param->mode = MODE_WRITE) {

		state = vcedit_new_state();

		if(vcedit_open(state, param->in) < 0)
		{
			fprintf(stderr, "Failed to open file as vorbis.\n");
			return 1;
		}

		/* grab and clear the exisiting comments */
		vc = vcedit_comments(state);
		vorbis_comment_clear(vc);
		vorbis_comment_init(vc);

		/* build the replacement structure */
		while (!feof(param->com)) {
			/* FIXME should use a resizeable buffer! */
			char *buf = (char *)malloc(1024*sizeof(char));

			fgets(buf, 1024, param->com);
			if (add_comment(buf, vc) < 0) {
				fprintf(stderr,
					"bad comment:\n\t\"%s\"\n",
					buf);
			}

			free(buf);
		}

		/* write out the modified stream */
		if(vcedit_write(state, param->out) < 0)
		{
			fprintf(stderr, "Failed to write comments to output file\n");
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

   Take a line of the form "TAG=value string", parse it,
   and add it to the vorbis_comment structure. Error checking
   is performed.

   Note that this assumes a null-terminated string, which may cause
   problems with > 8-bit character sets!

***********/

int  add_comment(char *line, vorbis_comment *vc)
{
	char	*mark, *value;

	/* strip any terminal newline */
	{
		int len = strlen(line);
		if (line[len-1] == '\n') line[len-1] = '\0';
	}

	/* validation: basically, we assume it's a tag
	/* if it has an '=' after one or more alpha chars */

	mark = index(line, '=');
	if (mark == NULL) return -1;

	value = line;
	while (value < mark) {
		if (!isalpha(*value++)) return -1;
	}

	/* split the line by turning the '=' in to a null */
	*mark = '\0';	
	value++;

	/* append the comment and return */
	vorbis_comment_add_tag(vc, line, value);

	return 0;
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
		"  vorbiscomment -w in.ogg out.ogg (to modify comments)\n"
		"	in the write case, a new set of comments in the form\n"
		"	'TAG=value' is expected on stdin. This set will\n"
		"	completely replace the existing set.\n"
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

	while ((ret = getopt_long(argc, argv, "lwhqc:",
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
			default:
				usage();
				exit(1);
		}
	}

	/* remaining bits must be the filenames */
	if ((param->mode == MODE_LIST && (argc - optind) != 1) ||
		(param->mode == MODE_WRITE && (argc - optind) != 2)) {
			usage();
			exit(1);
	}
	param->infilename = strdup(argv[optind]);
	if (param->mode == MODE_WRITE)
		param->outfilename = strdup(argv[optind+1]);
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

	if (p->mode == MODE_WRITE) { 

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
}
