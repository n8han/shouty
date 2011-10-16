/*
 *      Command line parsing related functions
 *
 *      Copyright (c) 1999 Mark Taylor
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

/* $Id: parse.c,v 1.247.2.5 2008/09/14 11:51:49 robert Exp $ */

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#include <assert.h>
#include <ctype.h>

#ifdef STDC_HEADERS
# include <stdio.h>
# include <stdlib.h>
# include <string.h>
#else
# ifndef HAVE_STRCHR
#  define strchr index
#  define strrchr rindex
# endif
char   *strchr(), *strrchr();
# ifndef HAVE_MEMCPY
#  define memcpy(d, s, n) bcopy ((s), (d), (n))
#  define memmove(d, s, n) bcopy ((s), (d), (n))
# endif
#endif

#ifdef __OS2__
#include <os2.h>
#define PRTYC_IDLE 1
#define PRTYC_REGULAR 2
#define PRTYD_MINIMUM -31
#define PRTYD_MAXIMUM 31
#endif

#ifdef HAVE_LIMITS_H
# include <limits.h>
#endif

#include "lame.h"
#include "set_get.h"

#include "brhist.h"
#include "parse.h"
#include "main.h"
#include "get_audio.h"
#include "version.h"
#include "console.h"

#ifdef WITH_DMALLOC
#include <dmalloc.h>
#endif

                 
#ifdef HAVE_ICONV
#include <iconv.h>
#include <errno.h>
#endif

#if defined DEBUG || _DEBUG || _ALLOW_INTERNAL_OPTIONS
#define INTERNAL_OPTS 1
#else
#define INTERNAL_OPTS LAME_ALPHA_VERSION
#endif

#if (INTERNAL_OPTS!=0)
#define DEV_HELP(a) a
#else
#define DEV_HELP(a)
#endif



/* GLOBAL VARIABLES.  set by parse_args() */
/* we need to clean this up */
sound_file_format input_format;
int     swapbytes = 0;       /* force byte swapping   default=0 */
int     silent;              /* Verbosity */
int     ignore_tag_errors;   /* Ignore errors in values passed for tags */
int     brhist;
float   update_interval;     /* to use Frank's time status display */
int     mp3_delay;           /* to adjust the number of samples truncated
                                during decode */
int     mp3_delay_set;       /* user specified the value of the mp3 encoder
                                delay to assume for decoding */

int     disable_wav_header;
mp3data_struct mp3input_data; /* used by MP3 */
int     print_clipping_info; /* print info whether waveform clips */


int     in_signed = -1;

enum ByteOrder in_endian = ByteOrderLittleEndian;

int     in_bitwidth = 16;

int     flush_write = 0;



/**
 *  Long Filename support for the WIN32 platform
 *
 */
#ifdef WIN32
#include <winbase.h>
static void
dosToLongFileName(char *fn)
{
    const int MSIZE = PATH_MAX + 1 - 4; /*  we wanna add ".mp3" later */
    WIN32_FIND_DATAA lpFindFileData;
    HANDLE  h = FindFirstFileA(fn, &lpFindFileData);
    if (h != INVALID_HANDLE_VALUE) {
        int     a;
        char   *q, *p;
        FindClose(h);
        for (a = 0; a < MSIZE; a++) {
            if ('\0' == lpFindFileData.cFileName[a])
                break;
        }
        if (a >= MSIZE || a == 0)
            return;
        q = strrchr(fn, '\\');
        p = strrchr(fn, '/');
        if (p - q > 0)
            q = p;
        if (q == NULL)
            q = strrchr(fn, ':');
        if (q == NULL)
            strncpy(fn, lpFindFileData.cFileName, a);
        else {
            a += q - fn + 1;
            if (a >= MSIZE)
                return;
            strncpy(++q, lpFindFileData.cFileName, MSIZE - a);
        }
    }
}
#endif
#if defined(WIN32)
#include <windows.h>
BOOL
SetPriorityClassMacro(DWORD p)
{
    HANDLE  op = OpenProcess(PROCESS_ALL_ACCESS, TRUE, GetCurrentProcessId());
    return SetPriorityClass(op, p);
}

static void
setWin32Priority(lame_global_flags * gfp, int Priority)
{
    switch (Priority) {
    case 0:
    case 1:
        SetPriorityClassMacro(IDLE_PRIORITY_CLASS);
        console_printf("==> Priority set to Low.\n");
        break;
    default:
    case 2:
        SetPriorityClassMacro(NORMAL_PRIORITY_CLASS);
        console_printf("==> Priority set to Normal.\n");
        break;
    case 3:
    case 4:
        SetPriorityClassMacro(HIGH_PRIORITY_CLASS);
        console_printf("==> Priority set to High.\n");
        break;
    }
}
#endif


#if defined(__OS2__)
/* OS/2 priority functions */
static int
setOS2Priority(lame_global_flags * gfp, int Priority)
{
    int     rc;

    switch (Priority) {

    case 0:
        rc = DosSetPriority(0, /* Scope: only one process */
                            PRTYC_IDLE, /* select priority class (idle, regular, etc) */
                            0, /* set delta */
                            0); /* Assume current process */
        console_printf("==> Priority set to 0 (Low priority).\n");
        break;

    case 1:
        rc = DosSetPriority(0, /* Scope: only one process */
                            PRTYC_IDLE, /* select priority class (idle, regular, etc) */
                            PRTYD_MAXIMUM, /* set delta */
                            0); /* Assume current process */
        console_printf("==> Priority set to 1 (Medium priority).\n");
        break;

    case 2:
        rc = DosSetPriority(0, /* Scope: only one process */
                            PRTYC_REGULAR, /* select priority class (idle, regular, etc) */
                            PRTYD_MINIMUM, /* set delta */
                            0); /* Assume current process */
        console_printf("==> Priority set to 2 (Regular priority).\n");
        break;

    case 3:
        rc = DosSetPriority(0, /* Scope: only one process */
                            PRTYC_REGULAR, /* select priority class (idle, regular, etc) */
                            0, /* set delta */
                            0); /* Assume current process */
        console_printf("==> Priority set to 3 (High priority).\n");
        break;

    case 4:
        rc = DosSetPriority(0, /* Scope: only one process */
                            PRTYC_REGULAR, /* select priority class (idle, regular, etc) */
                            PRTYD_MAXIMUM, /* set delta */
                            0); /* Assume current process */
        console_printf("==> Priority set to 4 (Maximum priority). I hope you enjoy it :)\n");
        break;

    default:
        console_printf("==> Invalid priority specified! Assuming idle priority.\n");
    }


    return 0;
}
#endif


extern int
id3tag_set_textinfo_ucs2(lame_global_flags* gfp, char const* id, unsigned short const* text);

extern int
id3tag_set_comment_ucs2(lame_global_flags* gfp, char const* lng, unsigned short const* desc, unsigned short const* text);

/* possible text encodings */
typedef enum TextEncoding
{ TENC_RAW     /* bytes will be stored as-is into ID3 tags, which are Latin1/UCS2 per definition */
, TENC_LATIN1  /* text will be converted from local encoding to Latin1, as ID3 needs it */
, TENC_UCS2    /* text will be converted from local encoding to UCS-2, as ID3v2 wants it */
} TextEncoding;

#ifdef HAVE_ICONV

/* search for Zero termination in multi-byte strings */
static size_t
strlenMultiByte(char const* str, size_t w)
{    
    size_t n = 0;
    if (str != 0) {
        size_t i, x = 0;
        for (n = 0; ; ++n) {
            x = 0;
            for (i = 0; i < w; ++i) {
                x += *str++ == 0 ? 1 : 0;
            }
            if (x == w) {
                break;
            }
        }
    }
    return n;
}


static size_t
currCharCodeSize(void)
{
    size_t n = 1;
    char dst[32];
    char* src = "A";
    char* env_lang = getenv("LANG");
    char* xxx_code = env_lang == NULL ? NULL : strrchr(env_lang, '.');
    char* cur_code = xxx_code == NULL ? "" : xxx_code+1;
    iconv_t xiconv = iconv_open(cur_code, "ISO_8859-1");
    if (xiconv != (iconv_t)-1) {
        for (n = 0; n < 32; ++n) {
            char* i_ptr = src;
            char* o_ptr = dst;
            size_t srcln = 1;
            size_t avail = n;
            size_t rc = iconv(xiconv, &i_ptr, &srcln, &o_ptr, &avail);
            if (rc != (size_t)-1) {
                break;
            }
        }
        iconv_close(xiconv);
    }
    return n;
}

#if 0
static
char* fromLatin1( char* src )
{
    char* dst = 0;
    if (src != 0) {
        size_t const l = strlen(src);
        size_t const n = l*4;
        dst = calloc(n+4, 4);
        if (dst != 0) {
            char* env_lang = getenv("LANG");
            char* xxx_code = env_lang == NULL ? NULL : strrchr(env_lang, '.');
            char* cur_code = xxx_code == NULL ? "" : xxx_code+1;
            iconv_t xiconv = iconv_open(cur_code, "ISO_8859-1");
            if (xiconv != (iconv_t)-1) {
                char* i_ptr = src;
                char* o_ptr = dst;
                size_t srcln = l;
                size_t avail = n;                
                iconv(xiconv, &i_ptr, &srcln, &o_ptr, &avail);
                iconv_close(xiconv);
            }
        }
    }
    return dst;
}

static
char* fromUcs2( char* src )
{
    char* dst = 0;
    if (src != 0) {
        size_t const l = strlenMultiByte(src, 2);
        size_t const n = l*4;
        dst = calloc(n+4, 4);
        if (dst != 0) {
            char* env_lang = getenv("LANG");
            char* xxx_code = env_lang == NULL ? NULL : strrchr(env_lang, '.');
            char* cur_code = xxx_code == NULL ? "" : xxx_code+1;
            iconv_t xiconv = iconv_open(cur_code, "UCS-2LE");
            if (xiconv != (iconv_t)-1) {
                char* i_ptr = (char*)src;
                char* o_ptr = dst;
                size_t srcln = l*2;
                size_t avail = n;                
                iconv(xiconv, &i_ptr, &srcln, &o_ptr, &avail);
                iconv_close(xiconv);
            }
        }
    }
    return dst;
}
#endif

static
char* toLatin1( char* src )
{
    size_t w = currCharCodeSize();
    char* dst = 0;
    if (src != 0) {
        size_t const l = strlenMultiByte(src, w);
        size_t const n = l*4;
        dst = calloc(n+4, 4);
        if (dst != 0) {
            char* env_lang = getenv("LANG");
            char* xxx_code = env_lang == NULL ? NULL : strrchr(env_lang, '.');
            char* cur_code = xxx_code == NULL ? "" : xxx_code+1;
            iconv_t xiconv = iconv_open("ISO_8859-1", cur_code);
            if (xiconv != (iconv_t)-1) {
                char* i_ptr = (char*)src;
                char* o_ptr = dst;
                size_t srcln = l*w;
                size_t avail = n;                
                iconv(xiconv, &i_ptr, &srcln, &o_ptr, &avail);
                iconv_close(xiconv);
            }
        }
    }
    return dst;
}


static
char* toUcs2( char* src )
{
    size_t w = currCharCodeSize();
    char* dst = 0;
    if (src != 0) {
        size_t const l = strlenMultiByte(src, w);
        size_t const n = (l+1)*4;
        dst = calloc(n+4, 4);
        if (dst != 0) {
            char* env_lang = getenv("LANG");
            char* xxx_code = env_lang == NULL ? NULL : strrchr(env_lang, '.');
            char* cur_code = xxx_code == NULL ? "" : xxx_code+1;
            iconv_t xiconv = iconv_open("UCS-2LE", cur_code);
            dst[0] = 0xff;
            dst[1] = 0xfe;
            if (xiconv != (iconv_t)-1) {
                char* i_ptr = (char*)src;
                char* o_ptr = &dst[2];
                size_t srcln = l*w;
                size_t avail = n;                
                iconv(xiconv, &i_ptr, &srcln, &o_ptr, &avail);
                iconv_close(xiconv);
            }
        }
    }
    return dst;
}


static int
set_id3v2tag(lame_global_flags* gfp, int type, unsigned short const* str)
{
    switch (type)
    {
        case 'a': return id3tag_set_textinfo_ucs2(gfp, "TPE1", str);
        case 't': return id3tag_set_textinfo_ucs2(gfp, "TIT2", str);
        case 'l': return id3tag_set_textinfo_ucs2(gfp, "TALB", str);
        case 'g': return id3tag_set_textinfo_ucs2(gfp, "TCON", str);
        case 'c': return id3tag_set_comment_ucs2(gfp, 0, 0, str);
        case 'n': return id3tag_set_textinfo_ucs2(gfp, "TRCK", str);
    }
    return 0;
}
#endif

static int
set_id3tag(lame_global_flags* gfp, int type, char const* str)
{
    switch (type)
    {
        case 'a': return id3tag_set_artist(gfp, str), 0;
        case 't': return id3tag_set_title(gfp, str), 0;
        case 'l': return id3tag_set_album(gfp, str), 0;
        case 'g': return id3tag_set_genre(gfp, str);
        case 'c': return id3tag_set_comment(gfp, str), 0;
        case 'n': return id3tag_set_track(gfp, str);
        case 'y': return id3tag_set_year(gfp, str), 0;
        case 'v': return id3tag_set_fieldvalue(gfp, str);
    }
    return 0;
}

static int
id3_tag(lame_global_flags* gfp, int type, TextEncoding enc, char* str)
{
    void* x = 0;
    int result;
    switch (enc) 
    {
        default:
        case TENC_RAW:    x = strdup(str);   break;
#ifdef HAVE_ICONV
        case TENC_LATIN1: x = toLatin1(str); break;
        case TENC_UCS2:   x = toUcs2(str);   break;
#endif
    }
    switch (enc)
    {
        default:
        case TENC_RAW:
        case TENC_LATIN1: result = set_id3tag(gfp, type, x);   break;
#ifdef HAVE_ICONV
        case TENC_UCS2:   result = set_id3v2tag(gfp, type, x); break;
#endif
    }
    free(x);
    return result;
}




/************************************************************************
*
* license
*
* PURPOSE:  Writes version and license to the file specified by fp
*
************************************************************************/

static int
lame_version_print(FILE * const fp)
{
    const char *b = get_lame_os_bitness();
    const char *v = get_lame_version();
    const char *u = get_lame_url();
    const size_t lenb = strlen(b);
    const size_t lenv = strlen(v);
    const size_t lenu = strlen(u);
    const size_t lw = 80;       /* line width of terminal in characters */
    const size_t sw = 16;       /* static width of text */

    if (lw >= lenb + lenv + lenu + sw || lw < lenu + 2)
        /* text fits in 80 chars per line, or line even too small for url */
        if (lenb > 0)
            fprintf(fp, "LAME %s version %s (%s)\n\n", b, v, u);
        else
            fprintf(fp, "LAME version %s (%s)\n\n", v, u);
    else
        /* text too long, wrap url into next line, right aligned */
    if (lenb > 0)
        fprintf(fp, "LAME %s version %s\n%*s(%s)\n\n", b, v, lw - 2 - lenu, "", u);
    else
        fprintf(fp, "LAME version %s\n%*s(%s)\n\n", v, lw - 2 - lenu, "", u);

    if (LAME_ALPHA_VERSION)
        fprintf(fp, "warning: alpha versions should be used for testing only\n\n");


    return 0;
}

static int
print_license(FILE * const fp)
{                       /* print version & license */
    lame_version_print(fp);
    fprintf(fp,
            "Can I use LAME in my commercial program?\n"
            "\n"
            "Yes, you can, under the restrictions of the LGPL.  In particular, you\n"
            "can include a compiled version of the LAME library (for example,\n"
            "lame.dll) with a commercial program.  Some notable requirements of\n"
            "the LGPL:\n" "\n");
    fprintf(fp,
            "1. In your program, you cannot include any source code from LAME, with\n"
            "   the exception of files whose only purpose is to describe the library\n"
            "   interface (such as lame.h).\n" "\n");
    fprintf(fp,
            "2. Any modifications of LAME must be released under the LGPL.\n"
            "   The LAME project (www.mp3dev.org) would appreciate being\n"
            "   notified of any modifications.\n" "\n");
    fprintf(fp,
            "3. You must give prominent notice that your program is:\n"
            "      A. using LAME (including version number)\n"
            "      B. LAME is under the LGPL\n"
            "      C. Provide a copy of the LGPL.  (the file COPYING contains the LGPL)\n"
            "      D. Provide a copy of LAME source, or a pointer where the LAME\n"
            "         source can be obtained (such as www.mp3dev.org)\n"
            "   An example of prominent notice would be an \"About the LAME encoding engine\"\n"
            "   button in some pull down menu within the executable of your program.\n" "\n");
    fprintf(fp,
            "4. If you determine that distribution of LAME requires a patent license,\n"
            "   you must obtain such license.\n" "\n" "\n");
    fprintf(fp,
            "*** IMPORTANT NOTE ***\n"
            "\n"
            "The decoding functions provided in LAME use the mpglib decoding engine which\n"
            "is under the GPL.  They may not be used by any program not released under the\n"
            "GPL unless you obtain such permission from the MPG123 project (www.mpg123.de).\n"
            "\n");
    return 0;
}


/************************************************************************
*
* usage
*
* PURPOSE:  Writes command line syntax to the file specified by fp
*
************************************************************************/

int
usage(FILE * const fp, const char *ProgramName)
{                       /* print general syntax */
    lame_version_print(fp);
    fprintf(fp,
            "usage: %s [options] <infile> [outfile]\n"
            "\n"
            "    <infile> and/or <outfile> can be \"-\", which means stdin/stdout.\n"
            "\n"
            "Try:\n"
            "     \"%s --help\"           for general usage information\n"
            " or:\n"
            "     \"%s --preset help\"    for information on suggested predefined settings\n"
            " or:\n"
            "     \"%s --longhelp\"\n"
            "  or \"%s -?\"              for a complete options list\n\n",
            ProgramName, ProgramName, ProgramName, ProgramName, ProgramName);
    return 0;
}


/************************************************************************
*
* usage
*
* PURPOSE:  Writes command line syntax to the file specified by fp
*           but only the most important ones, to fit on a vt100 terminal
*
************************************************************************/

int
short_help(const lame_global_flags * gfp, FILE * const fp, const char *ProgramName)
{                       /* print short syntax help */
    lame_version_print(fp);
    fprintf(fp,
            "usage: %s [options] <infile> [outfile]\n"
            "\n"
            "    <infile> and/or <outfile> can be \"-\", which means stdin/stdout.\n"
            "\n" "RECOMMENDED:\n" "    lame -V2 input.wav output.mp3\n" "\n", ProgramName);
    fprintf(fp,
            "OPTIONS:\n"
            "    -b bitrate      set the bitrate, default 128 kbps\n"
            "    -h              higher quality, but a little slower.  Recommended.\n"
            "    -f              fast mode (lower quality)\n"
            "    -V n            quality setting for VBR.  default n=%i\n"
            "                    0=high quality,bigger files. 9=smaller files\n",
            lame_get_VBR_q(gfp));
    fprintf(fp,
            "    --preset type   type must be \"medium\", \"standard\", \"extreme\", \"insane\",\n"
            "                    or a value for an average desired bitrate and depending\n"
            "                    on the value specified, appropriate quality settings will\n"
            "                    be used.\n"
            "                    \"--preset help\" gives more info on these\n" "\n");
    fprintf(fp,
#if defined(WIN32)
            "    --priority type  sets the process priority\n"
            "                     0,1 = Low priority\n"
            "                     2   = normal priority\n"
            "                     3,4 = High priority\n" "\n"
#endif
#if defined(__OS2__)
            "    --priority type  sets the process priority\n"
            "                     0 = Low priority\n"
            "                     1 = Medium priority\n"
            "                     2 = Regular priority\n"
            "                     3 = High priority\n"
            "                     4 = Maximum priority\n" "\n"
#endif
            "    --longhelp      full list of options\n" "\n"
            "    --license       print License information\n\n"
            );

    return 0;
}

/************************************************************************
*
* usage
*
* PURPOSE:  Writes command line syntax to the file specified by fp
*
************************************************************************/

static void
wait_for(FILE * const fp, int lessmode)
{
    if (lessmode) {
        fflush(fp);
        getchar();
    }
    else {
        fprintf(fp, "\n");
    }
    fprintf(fp, "\n");
}

int
long_help(const lame_global_flags * gfp, FILE * const fp, const char *ProgramName, int lessmode)
{                       /* print long syntax help */
    lame_version_print(fp);
    fprintf(fp,
            "usage: %s [options] <infile> [outfile]\n"
            "\n"
            "    <infile> and/or <outfile> can be \"-\", which means stdin/stdout.\n"
            "\n" "RECOMMENDED:\n" "    lame -V2 input.wav output.mp3\n" "\n", ProgramName);
    fprintf(fp,
            "OPTIONS:\n"
            "  Input options:\n"
            "    --scale <arg>   scale input (multiply PCM data) by <arg>\n"
            "    --scale-l <arg> scale channel 0 (left) input (multiply PCM data) by <arg>\n"
            "    --scale-r <arg> scale channel 1 (right) input (multiply PCM data) by <arg>\n"
#if (defined HAVE_MPGLIB || defined AMIGA_MPEGA)
            "    --mp1input      input file is a MPEG Layer I   file\n"
            "    --mp2input      input file is a MPEG Layer II  file\n"
            "    --mp3input      input file is a MPEG Layer III file\n"
#endif
            "    --nogap <file1> <file2> <...>\n"
            "                    gapless encoding for a set of contiguous files\n"
            "    --nogapout <dir>\n"
            "                    output dir for gapless encoding (must precede --nogap)\n"
            "    --nogaptags     allow the use of VBR tags in gapless encoding\n"
           );
    fprintf(fp,
            "\n"
            "  Input options for RAW PCM:\n"
            "    -r              input is raw pcm\n"
            "    -x              force byte-swapping of input\n"
            "    -s sfreq        sampling frequency of input file (kHz) - default 44.1 kHz\n"
            "    --bitwidth w    input bit width is w (default 16)\n"
            "    --signed        input is signed (default)\n"
            "    --unsigned      input is unsigned\n"
            "    --little-endian input is little-endian (default)\n"
            "    --big-endian    input is big-endian\n"
           );

    wait_for(fp, lessmode);
    fprintf(fp,
            "  Operational options:\n"
            "    -a              downmix from stereo to mono file for mono encoding\n"
            "    -m <mode>       (j)oint, (s)imple, (f)orce, (d)dual-mono, (m)ono\n"
            "                    default is (j) or (s) depending on bitrate\n"
            "                    joint  = joins the best possible of MS and LR stereo\n"
            "                    simple = force LR stereo on all frames\n"
            "                    force  = force MS stereo on all frames.\n"
            "    --preset type   type must be \"medium\", \"standard\", \"extreme\", \"insane\",\n"
            "                    or a value for an average desired bitrate and depending\n"
            "                    on the value specified, appropriate quality settings will\n"
            "                    be used.\n"
            "                    \"--preset help\" gives more info on these\n"
            "    --comp  <arg>   choose bitrate to achive a compression ratio of <arg>\n");
    fprintf(fp, "    --replaygain-fast   compute RG fast but slightly inaccurately (default)\n"
#ifdef DECODE_ON_THE_FLY
            "    --replaygain-accurate   compute RG more accurately and find the peak sample\n"
#endif
            "    --noreplaygain  disable ReplayGain analysis\n"
#ifdef DECODE_ON_THE_FLY
            "    --clipdetect    enable --replaygain-accurate and print a message whether\n"
            "                    clipping occurs and how far the waveform is from full scale\n"
#endif
        );
    fprintf(fp,
            "    --flush         flush output stream as soon as possible\n"
            "    --freeformat    produce a free format bitstream\n"
            "    --decode        input=mp3 file, output=wav\n"
            "    -t              disable writing wav header when using --decode\n");

    wait_for(fp, lessmode);
    fprintf(fp,
            "  Verbosity:\n"
            "    --disptime <arg>print progress report every arg seconds\n"
            "    -S              don't print progress report, VBR histograms\n"
            "    --nohist        disable VBR histogram display\n"
            "    --silent        don't print anything on screen\n"
            "    --quiet         don't print anything on screen\n"
            "    --brief         print more useful information\n"
            "    --verbose       print a lot of useful information\n" "\n");
    fprintf(fp,
            "  Noise shaping & psycho acoustic algorithms:\n"
            "    -q <arg>        <arg> = 0...9.  Default  -q 5 \n"
            "                    -q 0:  Highest quality, very slow \n"
            "                    -q 9:  Poor quality, but fast \n"
            "    -h              Same as -q 2.   Recommended.\n"
            "    -f              Same as -q 7.   Fast, ok quality\n");

    wait_for(fp, lessmode);
    fprintf(fp,
            "  CBR (constant bitrate, the default) options:\n"
            "    -b <bitrate>    set the bitrate in kbps, default 128 kbps\n"
            "    --cbr           enforce use of constant bitrate\n"
            "\n"
            "  ABR options:\n"
            "    --abr <bitrate> specify average bitrate desired (instead of quality)\n" "\n");
    fprintf(fp,
            "  VBR options:\n"
            "    -V n            quality setting for VBR.  default n=%i\n"
            "                    0=high quality,bigger files. 9=smaller files\n"
            "    -v              the same as -V 4\n"
            "    --vbr-old       use old variable bitrate (VBR) routine\n"
            "    --vbr-new       use new variable bitrate (VBR) routine (default)\n"
            ,
            lame_get_VBR_q(gfp));
    fprintf(fp,
            "    -b <bitrate>    specify minimum allowed bitrate, default  32 kbps\n"
            "    -B <bitrate>    specify maximum allowed bitrate, default 320 kbps\n"
            "    -F              strictly enforce the -b option, for use with players that\n"
            "                    do not support low bitrate mp3\n"
            "    -t              disable writing LAME Tag\n"
            "    -T              enable and force writing LAME Tag\n");

    wait_for(fp, lessmode);
    DEV_HELP(fprintf(fp,
                     "  ATH related:\n"
                     "    --noath         turns ATH down to a flat noise floor\n"
                     "    --athshort      ignore GPSYCHO for short blocks, use ATH only\n"
                     "    --athonly       ignore GPSYCHO completely, use ATH only\n"
                     "    --athtype n     selects between different ATH types [0-4]\n"
                     "    --athlower x    lowers ATH by x dB\n");
             fprintf(fp, "    --athaa-type n  ATH auto adjust: 0 'no' else 'loudness based'\n"
/** OBSOLETE "    --athaa-loudapprox n   n=1 total energy or n=2 equal loudness curve\n"*/
                     "    --athaa-sensitivity x  activation offset in -/+ dB for ATH auto-adjustment\n"
                     "\n");
        )
        fprintf(fp,
                "  PSY related:\n"
                DEV_HELP(
                "    --short         use short blocks when appropriate\n"
                "    --noshort       do not use short blocks\n"
                "    --allshort      use only short blocks\n"
                )
        );
    fprintf(fp,
            "    --temporal-masking x   x=0 disables, x=1 enables temporal masking effect\n"
            "    --nssafejoint   M/S switching criterion\n"
            "    --nsmsfix <arg> M/S switching tuning [effective 0-3.5]\n"
            "    --interch x     adjust inter-channel masking ratio\n"
            "    --ns-bass x     adjust masking for sfbs  0 -  6 (long)  0 -  5 (short)\n"
            "    --ns-alto x     adjust masking for sfbs  7 - 13 (long)  6 - 10 (short)\n"
            "    --ns-treble x   adjust masking for sfbs 14 - 21 (long) 11 - 12 (short)\n");
    fprintf(fp,
            "    --ns-sfb21 x    change ns-treble by x dB for sfb21\n"
            DEV_HELP("    --shortthreshold x,y  short block switching threshold,\n"
                     "                          x for L/R/M channel, y for S channel\n"
                     "  Noise Shaping related:\n"
                     "    --substep n     use pseudo substep noise shaping method types 0-2\n")
        );

    wait_for(fp, lessmode);

    fprintf(fp,
            "  experimental switches:\n"
            DEV_HELP(
            "    -X n[,m]        selects between different noise measurements\n"
            "                    n for long block, m for short. if m is omitted, m = n\n"
            )
            "    -Y              lets LAME ignore noise in sfb21, like in CBR\n"
            DEV_HELP(
            "    -Z [n]          currently no effects\n"
            )
            );

    wait_for(fp, lessmode);

    fprintf(fp,
            "  MP3 header/stream options:\n"
            "    -e <emp>        de-emphasis n/5/c  (obsolete)\n"
            "    -c              mark as copyright\n"
            "    -o              mark as non-original\n"
            "    -p              error protection.  adds 16 bit checksum to every frame\n"
            "                    (the checksum is computed correctly)\n"
            "    --nores         disable the bit reservoir\n"
            "    --strictly-enforce-ISO   comply as much as possible to ISO MPEG spec\n" "\n");
    fprintf(fp,
            "  Filter options:\n"
            "  --lowpass <freq>        frequency(kHz), lowpass filter cutoff above freq\n"
            "  --lowpass-width <freq>  frequency(kHz) - default 15%% of lowpass freq\n"
            "  --highpass <freq>       frequency(kHz), highpass filter cutoff below freq\n"
            "  --highpass-width <freq> frequency(kHz) - default 15%% of highpass freq\n");
    fprintf(fp,
            "  --resample <sfreq>  sampling frequency of output file(kHz)- default=automatic\n");

    wait_for(fp, lessmode);
    fprintf(fp,
            "  ID3 tag options:\n"
            "    --tt <title>    audio/song title (max 30 chars for version 1 tag)\n"
            "    --ta <artist>   audio/song artist (max 30 chars for version 1 tag)\n"
            "    --tl <album>    audio/song album (max 30 chars for version 1 tag)\n"
            "    --ty <year>     audio/song year of issue (1 to 9999)\n"
            "    --tc <comment>  user-defined text (max 30 chars for v1 tag, 28 for v1.1)\n"
            "    --tn <track[/total]>   audio/song track number and (optionally) the total\n"
            "                           number of tracks on the original recording. (track\n"
            "                           and total each 1 to 255. just the track number\n"
            "                           creates v1.1 tag, providing a total forces v2.0).\n"
            "    --tg <genre>    audio/song genre (name or number in list)\n"
            "    --ti <file>     audio/song albumArt (jpeg/png/gif file, 128KB max, v2.3)\n"
            "    --tv <id=value> user-defined frame specified by id and value (v2.3 tag)\n");
    fprintf(fp,
            "    --add-id3v2     force addition of version 2 tag\n"
            "    --id3v1-only    add only a version 1 tag\n"
            "    --id3v2-only    add only a version 2 tag\n"
            "    --space-id3v1   pad version 1 tag with spaces instead of nulls\n"
            "    --pad-id3v2     same as '--pad-id3v2-size 128'\n"
            "    --pad-id3v2-size <value> adds version 2 tag, pad with extra <value> bytes\n"
            "    --genre-list    print alphabetically sorted ID3 genre list and exit\n"
            "    --ignore-tag-errors  ignore errors in values passed for tags\n" "\n");
    fprintf(fp,
            "    Note: A version 2 tag will NOT be added unless one of the input fields\n"
            "    won't fit in a version 1 tag (e.g. the title string is longer than 30\n"
            "    characters), or the '--add-id3v2' or '--id3v2-only' options are used,\n"
            "    or output is redirected to stdout.\n"
#if defined(WIN32)
            "\n\nMS-Windows-specific options:\n"
            "    --priority <type>  sets the process priority:\n"
            "                         0,1 = Low priority (IDLE_PRIORITY_CLASS)\n"
            "                         2 = normal priority (NORMAL_PRIORITY_CLASS, default)\n"
            "                         3,4 = High priority (HIGH_PRIORITY_CLASS))\n"
            "    Note: Calling '--priority' without a parameter will select priority 0.\n"
#endif
#if defined(__OS2__)
            "\n\nOS/2-specific options:\n"
            "    --priority <type>  sets the process priority:\n"
            "                         0 = Low priority (IDLE, delta = 0)\n"
            "                         1 = Medium priority (IDLE, delta = +31)\n"
            "                         2 = Regular priority (REGULAR, delta = -31)\n"
            "                         3 = High priority (REGULAR, delta = 0)\n"
            "                         4 = Maximum priority (REGULAR, delta = +31)\n"
            "    Note: Calling '--priority' without a parameter will select priority 0.\n"
#endif
            "\nMisc:\n    --license       print License information\n\n"
        );

#if defined(HAVE_NASM)
    wait_for(fp, lessmode);
    fprintf(fp,
            "  Platform specific:\n"
            "    --noasm <instructions> disable assembly optimizations for mmx/3dnow/sse\n");
    wait_for(fp, lessmode);
#endif

    display_bitrates(fp);

    return 0;
}

static void
display_bitrate(FILE * const fp, const char *const version, const int d, const int indx)
{
    int     i;
    int nBitrates = 14;
    if (d == 4)
        nBitrates = 8;


    fprintf(fp,
            "\nMPEG-%-3s layer III sample frequencies (kHz):  %2d  %2d  %g\n"
            "bitrates (kbps):", version, 32 / d, 48 / d, 44.1 / d);
    for (i = 1; i <= nBitrates; i++)
        fprintf(fp, " %2i", bitrate_table[indx][i]);
    fprintf(fp, "\n");
}

int
display_bitrates(FILE * const fp)
{
    display_bitrate(fp, "1", 1, 1);
    display_bitrate(fp, "2", 2, 0);
    display_bitrate(fp, "2.5", 4, 0);
    fprintf(fp, "\n");
    fflush(fp);
    return 0;
}


/*  note: for presets it would be better to externalize them in a file.
    suggestion:  lame --preset <file-name> ...
            or:  lame --preset my-setting  ... and my-setting is defined in lame.ini
 */

/*
Note from GB on 08/25/2002:
I am merging --presets and --alt-presets. Old presets are now aliases for
corresponding abr values from old alt-presets. This way we now have a
unified preset system, and I hope than more people will use the new tuned
presets instead of the old unmaintained ones.
*/



/************************************************************************
*
* usage
*
* PURPOSE:  Writes presetting info to #stdout#
*
************************************************************************/


static void
presets_longinfo_dm(FILE * msgfp)
{
    fprintf(msgfp,
            "\n"
            "The --preset switches are aliases over LAME settings.\n"
            "\n" "\n");
    fprintf(msgfp,
            "To activate these presets:\n"
            "\n" "   For VBR modes (generally highest quality):\n" "\n");
    fprintf(msgfp,
            "     \"--preset medium\" This preset should provide near transparency\n"
            "                             to most people on most music.\n"
            "\n"
            "     \"--preset standard\" This preset should generally be transparent\n"
            "                             to most people on most music and is already\n"
            "                             quite high in quality.\n" "\n");
    fprintf(msgfp,
            "     \"--preset extreme\" If you have extremely good hearing and similar\n"
            "                             equipment, this preset will generally provide\n"
            "                             slightly higher quality than the \"standard\"\n"
            "                             mode.\n" "\n");
    fprintf(msgfp,
            "   For CBR 320kbps (highest quality possible from the --preset switches):\n"
            "\n"
            "     \"--preset insane\"  This preset will usually be overkill for most\n"
            "                             people and most situations, but if you must\n"
            "                             have the absolute highest quality with no\n"
            "                             regard to filesize, this is the way to go.\n" "\n");
    fprintf(msgfp,
            "   For ABR modes (high quality per given bitrate but not as high as VBR):\n"
            "\n"
            "     \"--preset <kbps>\"  Using this preset will usually give you good\n"
            "                             quality at a specified bitrate. Depending on the\n"
            "                             bitrate entered, this preset will determine the\n");
    fprintf(msgfp,
            "                             optimal settings for that particular situation.\n"
            "                             While this approach works, it is not nearly as\n"
            "                             flexible as VBR, and usually will not attain the\n"
            "                             same level of quality as VBR at higher bitrates.\n" "\n");
    fprintf(msgfp,
            "The following options are also available for the corresponding profiles:\n"
            "\n"
            "   <fast>        standard\n"
            "   <fast>        extreme\n"
            "                 insane\n"
            "   <cbr> (ABR Mode) - The ABR Mode is implied. To use it,\n"
            "                      simply specify a bitrate. For example:\n"
            "                      \"--preset 185\" activates this\n"
            "                      preset and uses 185 as an average kbps.\n" "\n");
    fprintf(msgfp,
            "   \"fast\" - Enables the fast VBR mode for a particular profile.\n" "\n");
    fprintf(msgfp,
            "   \"cbr\"  - If you use the ABR mode (read above) with a significant\n"
            "            bitrate such as 80, 96, 112, 128, 160, 192, 224, 256, 320,\n"
            "            you can use the \"cbr\" option to force CBR mode encoding\n"
            "            instead of the standard abr mode. ABR does provide higher\n"
            "            quality but CBR may be useful in situations such as when\n"
            "            streaming an mp3 over the internet may be important.\n" "\n");
    fprintf(msgfp,
            "    For example:\n"
            "\n"
            "    \"--preset fast standard <input file> <output file>\"\n"
            " or \"--preset cbr 192 <input file> <output file>\"\n"
            " or \"--preset 172 <input file> <output file>\"\n"
            " or \"--preset extreme <input file> <output file>\"\n" "\n" "\n");
    fprintf(msgfp,
            "A few aliases are also available for ABR mode:\n"
            "phone => 16kbps/mono        phon+/lw/mw-eu/sw => 24kbps/mono\n"
            "mw-us => 40kbps/mono        voice => 56kbps/mono\n"
            "fm/radio/tape => 112kbps    hifi => 160kbps\n"
            "cd => 192kbps               studio => 256kbps\n");
}


extern void lame_set_msfix(lame_t gfp, double msfix);



static int
presets_set(lame_t gfp, int fast, int cbr, const char *preset_name, const char *ProgramName)
{
    int     mono = 0;

    if ((strcmp(preset_name, "help") == 0) && (fast < 1)
        && (cbr < 1)) {
        lame_version_print(stdout);
        presets_longinfo_dm(stdout);
        return -1;
    }



    /*aliases for compatibility with old presets */

    if (strcmp(preset_name, "phone") == 0) {
        preset_name = "16";
        mono = 1;
    }
    if ((strcmp(preset_name, "phon+") == 0) ||
        (strcmp(preset_name, "lw") == 0) ||
        (strcmp(preset_name, "mw-eu") == 0) || (strcmp(preset_name, "sw") == 0)) {
        preset_name = "24";
        mono = 1;
    }
    if (strcmp(preset_name, "mw-us") == 0) {
        preset_name = "40";
        mono = 1;
    }
    if (strcmp(preset_name, "voice") == 0) {
        preset_name = "56";
        mono = 1;
    }
    if (strcmp(preset_name, "fm") == 0) {
        preset_name = "112";
    }
    if ((strcmp(preset_name, "radio") == 0) || (strcmp(preset_name, "tape") == 0)) {
        preset_name = "112";
    }
    if (strcmp(preset_name, "hifi") == 0) {
        preset_name = "160";
    }
    if (strcmp(preset_name, "cd") == 0) {
        preset_name = "192";
    }
    if (strcmp(preset_name, "studio") == 0) {
        preset_name = "256";
    }

    if (strcmp(preset_name, "medium") == 0) {
        lame_set_VBR_q(gfp, 4);
        if (fast > 0) {
            lame_set_VBR(gfp, vbr_mtrh);
        }
        else {
            lame_set_VBR(gfp, vbr_rh);
        }
        return 0;
    }

    if (strcmp(preset_name, "standard") == 0) {
        lame_set_VBR_q(gfp, 2);
        if (fast > 0) {
            lame_set_VBR(gfp, vbr_mtrh);
        }
        else {
            lame_set_VBR(gfp, vbr_rh);
        }
        return 0;
    }

    else if (strcmp(preset_name, "extreme") == 0) {
        lame_set_VBR_q(gfp, 0);
        if (fast > 0) {
            lame_set_VBR(gfp, vbr_mtrh);
        }
        else {
            lame_set_VBR(gfp, vbr_rh);
        }
        return 0;
    }

    else if ((strcmp(preset_name, "insane") == 0) && (fast < 1)) {

        lame_set_preset(gfp, INSANE);

        return 0;
    }

    /* Generic ABR Preset */
    if (((atoi(preset_name)) > 0) && (fast < 1)) {
        if ((atoi(preset_name)) >= 8 && (atoi(preset_name)) <= 320) {
            lame_set_preset(gfp, atoi(preset_name));

            if (cbr == 1)
                lame_set_VBR(gfp, vbr_off);

            if (mono == 1) {
                lame_set_mode(gfp, MONO);
            }

            return 0;

        }
        else {
            lame_version_print(Console_IO.Error_fp);
            error_printf("Error: The bitrate specified is out of the valid range for this preset\n"
                         "\n"
                         "When using this mode you must enter a value between \"32\" and \"320\"\n"
                         "\n" "For further information try: \"%s --preset help\"\n", ProgramName);
            return -1;
        }
    }

    lame_version_print(Console_IO.Error_fp);
    error_printf("Error: You did not enter a valid profile and/or options with --preset\n"
                 "\n"
                 "Available profiles are:\n"
                 "\n"
                 "   <fast>        medium\n"
                 "   <fast>        standard\n"
                 "   <fast>        extreme\n"
                 "                 insane\n"
                 "          <cbr> (ABR Mode) - The ABR Mode is implied. To use it,\n"
                 "                             simply specify a bitrate. For example:\n"
                 "                             \"--preset 185\" activates this\n"
                 "                             preset and uses 185 as an average kbps.\n" "\n");
    error_printf("    Some examples:\n"
                 "\n"
                 " or \"%s --preset fast standard <input file> <output file>\"\n"
                 " or \"%s --preset cbr 192 <input file> <output file>\"\n"
                 " or \"%s --preset 172 <input file> <output file>\"\n"
                 " or \"%s --preset extreme <input file> <output file>\"\n"
                 "\n"
                 "For further information try: \"%s --preset help\"\n", ProgramName, ProgramName,
                 ProgramName, ProgramName, ProgramName);
    return -1;
}

static void
genre_list_handler(int num, const char *name, void *cookie)
{
    (void) cookie;
    console_printf("%3d %s\n", num, name);
}


/************************************************************************
*
* parse_args
*
* PURPOSE:  Sets encoding parameters to the specifications of the
* command line.  Default settings are used for parameters
* not specified in the command line.
*
* If the input file is in WAVE or AIFF format, the sampling frequency is read
* from the AIFF header.
*
* The input and output filenames are read into #inpath# and #outpath#.
*
************************************************************************/

/* would use real "strcasecmp" but it isn't portable */
static int
local_strcasecmp(const char *s1, const char *s2)
{
    unsigned char c1;
    unsigned char c2;

    do {
        c1 = tolower(*s1);
        c2 = tolower(*s2);
        if (!c1) {
            break;
        }
        ++s1;
        ++s2;
    } while (c1 == c2);
    return c1 - c2;
}



/* LAME is a simple frontend which just uses the file extension */
/* to determine the file type.  Trying to analyze the file */
/* contents is well beyond the scope of LAME and should not be added. */
static int
filename_to_type(const char *FileName)
{
    size_t  len = strlen(FileName);

    if (len < 4)
        return sf_unknown;

    FileName += len - 4;
    if (0 == local_strcasecmp(FileName, ".mpg"))
        return sf_mp123;
    if (0 == local_strcasecmp(FileName, ".mp1"))
        return sf_mp123;
    if (0 == local_strcasecmp(FileName, ".mp2"))
        return sf_mp123;
    if (0 == local_strcasecmp(FileName, ".mp3"))
        return sf_mp123;
    if (0 == local_strcasecmp(FileName, ".wav"))
        return sf_wave;
    if (0 == local_strcasecmp(FileName, ".aif"))
        return sf_aiff;
    if (0 == local_strcasecmp(FileName, ".raw"))
        return sf_raw;
    if (0 == local_strcasecmp(FileName, ".ogg"))
        return sf_ogg;
    return sf_unknown;
}

static int
resample_rate(double freq)
{
    if (freq >= 1.e3)
        freq *= 1.e-3;

    switch ((int) freq) {
    case 8:
        return 8000;
    case 11:
        return 11025;
    case 12:
        return 12000;
    case 16:
        return 16000;
    case 22:
        return 22050;
    case 24:
        return 24000;
    case 32:
        return 32000;
    case 44:
        return 44100;
    case 48:
        return 48000;
    default:
        error_printf("Illegal resample frequency: %.3f kHz\n", freq);
        return 0;
    }
}

enum ID3TAG_MODE 
{ ID3TAG_MODE_DEFAULT
, ID3TAG_MODE_V1_ONLY
, ID3TAG_MODE_V2_ONLY
};

/* Ugly, NOT final version */

#define T_IF(str)          if ( 0 == local_strcasecmp (token,str) ) {
#define T_ELIF(str)        } else if ( 0 == local_strcasecmp (token,str) ) {
#define T_ELIF_INTERNAL(str)        } else if (INTERNAL_OPTS && (0 == local_strcasecmp (token,str)) ) {
#define T_ELIF2(str1,str2) } else if ( 0 == local_strcasecmp (token,str1)  ||  0 == local_strcasecmp (token,str2) ) {
#define T_ELSE             } else {
#define T_END              }



/* end of parse.c */
