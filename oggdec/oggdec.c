/* OggDec
 *
 * This program is distributed under the GNU General Public License, version 2.
 * A copy of this license is included with this source.
 *
 * Copyright 2002, Michael Smith <msmith@xiph.org>
 *
 */

#include <stdlib.h>
#include <stdio.h>
#include <getopt.h>
#include <errno.h>
#include <string.h>

#if defined(_WIN32) || defined(__EMX__) || defined(__WATCOMC__)
#include <fcntl.h>
#include <io.h>
#endif

#include <vorbis/vorbisfile.h>

struct option long_options[] = {
    {"quiet", 0,0,'Q'},
    {"help",0,0,'h'},
    {"version", 0, 0, 'v'},
    {"bits", 1, 0, 'b'},
    {"endianness", 1, 0, 'e'},
    {"raw", 0, 0, 'R'},
    {"sign", 1, 0, 's'},
    {"output", 1, 0, 'o'},
    {NULL,0,0,0}
};

#define VERSIONSTRING "OggDec 1.0.1\n"

static int quiet = 0;
static int bits = 16;
static int endian = 0;
static int raw = 0;
static int sign = 1;
unsigned char headbuf[44]; /* The whole buffer */
char *outfilename = NULL;

static void usage(void) {
    fprintf(stderr, "Usage: oggdec [flags] file1.ogg [file2.ogg ... fileN.ogg]\n"
                    "\n"
                    "Supported flags:\n"
                    " --quiet, -Q      Quiet mode. No console output.\n"
                    " --help,  -h      Produce this help message.\n"
                    " --version, -v    Print out version number.\n"
                    " --bits, -b       Bit depth for output (8 and 16 supported)\n"
                    " --endianness, -e Output endianness for 16 bit output. 0 for\n"
                    "                  little endian (default), 1 for big endian\n"
                    " --sign, -s       Sign for output PCM, 0 for unsigned, 1 for\n"
                    "                  signed (default 1)\n"
                    " --raw, -R        Raw (headerless) output.\n"
                    " --output, -o     Output to given filename. May only be used\n"
                    "                  if there is only one input file\n"

            );

}


static void parse_options(int argc, char **argv)
{
    int option_index = 1;
    int ret;

    while((ret = getopt_long(argc, argv, "Qhvb:e:Rs:o:", 
                    long_options, &option_index)) != -1)
    {
        switch(ret)
        {
            case 'Q':
                quiet = 1;
                break;
            case 'h':
                usage();
                exit(0);
                break;
            case 'v':
                fprintf(stderr, VERSIONSTRING);
                exit(0);
                break;
            case 's':
                sign = atoi(optarg);
                break;
            case 'b':
                bits = atoi(optarg);
                if(bits <= 8)
                    bits = 8;
                else
                    bits = 16;
                break;
            case 'e':
                endian = atoi(optarg);
                break;
            case 'o':
                outfilename = strdup(optarg);
                break;
            case 'R':
                raw = 1;
                break;
            default:
                fprintf(stderr, "Internal error: Unrecognised argument\n");
                break;
        }
    }
}

#define WRITE_U32(buf, x) *(buf)     = (unsigned char)((x)&0xff);\
                          *((buf)+1) = (unsigned char)(((x)>>8)&0xff);\
                          *((buf)+2) = (unsigned char)(((x)>>16)&0xff);\
                          *((buf)+3) = (unsigned char)(((x)>>24)&0xff);

#define WRITE_U16(buf, x) *(buf)     = (unsigned char)((x)&0xff);\
                          *((buf)+1) = (unsigned char)(((x)>>8)&0xff);

/* Some of this based on ao/src/ao_wav.c */
int write_prelim_header(OggVorbis_File *vf, FILE *out, ogg_int64_t knownlength) {
    unsigned int size = 0x7fffffff;
    int channels = ov_info(vf,0)->channels;
    int samplerate = ov_info(vf,0)->rate;
    int bytespersec = channels*samplerate*bits/8;
    int align = channels*bits/8;
    int samplesize = bits;

    if(knownlength && knownlength*bits/8*channels < size)
        size = (unsigned int)(knownlength*bits/8*channels+44) ;

    memcpy(headbuf, "RIFF", 4);
    WRITE_U32(headbuf+4, size-8);
    memcpy(headbuf+8, "WAVE", 4);
    memcpy(headbuf+12, "fmt ", 4);
    WRITE_U32(headbuf+16, 16);
    WRITE_U16(headbuf+20, 1); /* format */
    WRITE_U16(headbuf+22, channels);
    WRITE_U32(headbuf+24, samplerate);
    WRITE_U32(headbuf+28, bytespersec);
    WRITE_U16(headbuf+32, align);
    WRITE_U16(headbuf+34, samplesize);
    memcpy(headbuf+36, "data", 4);
    WRITE_U32(headbuf+40, size - 44);

    if(fwrite(headbuf, 1, 44, out) != 44) {
        fprintf(stderr, "ERROR: Failed to write wav header: %s\n", strerror(errno));
        return 1;
    }

    return 0;
}

int rewrite_header(FILE *out, unsigned int written) 
{
    unsigned int length = written;

    length += 44;

    WRITE_U32(headbuf+4, length-8);
    WRITE_U32(headbuf+40, length-44);
    if(fseek(out, 0, SEEK_SET) != 0)
        return 1;

    if(fwrite(headbuf, 1, 44, out) != 44) {
        fprintf(stderr, "ERROR: Failed to write wav header: %s\n", strerror(errno));
        return 1;
    }
    return 0;
}

static int decode_file(char *infile, char *outfile)
{
    FILE *in, *out=NULL;
    OggVorbis_File vf;
    int bs = 0;
    char buf[8192];
    int buflen = 8192;
    unsigned int written = 0;
    int ret;
    ogg_int64_t length = 0;
    ogg_int64_t done = 0;
    int size;
    int seekable = 0;
    int percent = 0;

    if(!infile) {
#ifdef __BORLANDC__
        setmode(fileno(stdin), O_BINARY);
#elif _WIN32
        _setmode(_fileno(stdin), _O_BINARY);
#endif
        in = stdin;
    }
    else {
        in = fopen(infile, "rb");
        if(!in) {
            fprintf(stderr, "ERROR: Failed to open input file: %s\n", strerror(errno));
            return 1;
        }
    }


    if(ov_open(in, &vf, NULL, 0) < 0) {
        fprintf(stderr, "ERROR: Failed to open input as vorbis\n");
        fclose(in);
        return 1;
    }

    if(!outfile) {
#ifdef __BORLANDC__
        setmode(fileno(stdout), O_BINARY);
#elif _WIN32
        _setmode(_fileno(stdout), _O_BINARY);
#endif
        out = stdout;
    }
    else {
        out = fopen(outfile, "wb");
        if(!out) {
            fprintf(stderr, "ERROR: Failed to open output file: %s\n", strerror(errno));
            return 1;
        }
    }

    if(ov_seekable(&vf)) {
        seekable = 1;
        length = ov_pcm_total(&vf, 0);
        size = bits/8 * ov_info(&vf, 0)->channels;
        if(!quiet)
            fprintf(stderr, "Decoding \"%s\" to \"%s\"\n", 
                    infile?infile:"standard input", 
                    outfile?outfile:"standard output");
    }

    if(!raw) {
        if(write_prelim_header(&vf, out, length)) {
            ov_clear(&vf);
            fclose(out);
            return 1;
        }
    }

    while((ret = ov_read(&vf, buf, buflen, endian, bits/8, sign, &bs)) != 0) {
        if(bs != 0) {
            fprintf(stderr, "Only one logical bitstream currently supported\n");
            break;
        }

        if(ret < 0 ) {
           if( !quiet ) {
               fprintf(stderr, "Warning: hole in data\n");
           }
            continue;
        }

        if(fwrite(buf, 1, ret, out) != ret) {
            fprintf(stderr, "Error writing to file: %s\n", strerror(errno));
            ov_clear(&vf);
            fclose(out);
            return 1;
        }

        written += ret;
        if(!quiet && seekable) {
            done += ret/size;
            if((double)done/(double)length * 200. > (double)percent) {
                percent = (double)done/(double)length *200;
                fprintf(stderr, "\r\t[%5.1f%%]", (double)percent/2.);
            }
        }
    }

    if(seekable && !quiet)
        fprintf(stderr, "\n");

    if(!raw)
        rewrite_header(out, written); /* We don't care if it fails, too late */

    ov_clear(&vf);

    fclose(out);
    return 0;
}

int main(int argc, char **argv)
{
    int i;

    if(argc == 1) {
        fprintf(stderr, VERSIONSTRING);
        usage();
        return 1;
    }

    parse_options(argc,argv);

    if(!quiet)
        fprintf(stderr, VERSIONSTRING);

    if(optind >= argc) {
        fprintf(stderr, "ERROR: No input files specified. Use -h for help\n");
        return 1;
    }

    if(argc - optind > 1 && outfilename) {
        fprintf(stderr, "ERROR: Can only specify one input file if output filename is specified\n");
        return 1;
    }
    
    for(i=optind; i < argc; i++) {
        char *in, *out;
        if(!strcmp(argv[i], "-"))
            in = NULL;
        else
            in = argv[i];

        if(outfilename) {
            if(!strcmp(outfilename, "-"))
                out = NULL;
            else
                out = outfilename;
        }
        else {
            char *end = strrchr(argv[i], '.');
            end = end?end:(argv[i] + strlen(argv[i]) + 1);

            out = malloc(strlen(argv[i]) + 10);
            strncpy(out, argv[i], end-argv[i]);
            out[end-argv[i]] = 0;
            if(raw)
                strcat(out, ".raw");
            else
                strcat(out, ".wav");
        }

        if(decode_file(in,out))
            return 1;
    }

    if(outfilename)
        free(outfilename);

    return 0;
}
    

