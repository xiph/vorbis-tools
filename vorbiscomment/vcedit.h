/* This program is licensed under the GNU General Public License, version 2,
 * a copy of which is included with this program.
 *
 * (c) 2000-2001 Michael Smith <msmith@labyrinth.net.au>
 *
 * VCEdit header.
 *
 * last modified: $ID:$
 */

#ifndef __VCEDIT_H
#define __VCEDIT_H

#include <ogg/ogg.h>
#include <vorbis/codec.h>

typedef struct {
	ogg_sync_state		*oy;
	ogg_page		*og;
	ogg_stream_state	*os;

	vorbis_comment		*vc;

	FILE		*in;
	long		serial;
	unsigned char	*mainbuf;
	unsigned char	*bookbuf;
	int		mainlen;
	int		booklen;
} vcedit_state;

extern vcedit_state *	vcedit_new_state(void);
extern void		vcedit_clear(vcedit_state *state);
extern vorbis_comment *	vcedit_comments(vcedit_state *state);
extern int		vcedit_open(vcedit_state *state, FILE *in);
extern int		vcedit_write(vcedit_state *state, FILE *out);



#endif /* __VCEDIT_H */


