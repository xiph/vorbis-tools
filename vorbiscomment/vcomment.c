/* This program is licensed under the GNU General Public License, version 2, a
 * copy of which is included with this program.
 *
 * (c) 2000-2001 Michael Smith <msmith@labyrinth.net.au>
 *
 * Front end to show how to use vcedit. Note that it's not actually useful.
 */

#include <stdio.h>
#include "vcedit.h"

int main(int argc, char **argv)
{
	vcedit_state *state;
	vorbis_comment *comment;
	FILE *in, *out;

	if(argc < 3)
	{
		fprintf(stderr, 
			"Usage: vcomment in.ogg out.ogg (to produce modified output)\n");
		return 1;
	}

	in = fopen(argv[1], "rb");
	if(!in)
	{
		fprintf(stderr, "Error opening input file \"%s\"\n", argv[1]);
		return 1;
	}

	out = fopen(argv[2], "wb");
	if(!out)
	{
		fprintf(stderr, "Error opening output file \"%s\"\n", argv[1]);
		return 1;
	}

	state = vcedit_new_state();

	if(vcedit_open(state, in) < 0)
	{
		fprintf(stderr, "Failed to open file as vorbis.\n");
		return 1;
	}

	comment = vcedit_comments(state);

	/* Read comments, present to user. */

	/* Now build new comments */
	vorbis_comment_clear(comment);
	vorbis_comment_init(comment);

	vorbis_comment_add_tag(comment, "COMMENTEDITED", 
			"This file has had the comments in it edited " 
			"(well, replaced by this useless text.");

	if(vcedit_write(state, out) < 0)
	{
		fprintf(stderr, "Failed to write comments to output file\n");
		return 1;
	}

	vcedit_clear(state);

	return 0;
}

