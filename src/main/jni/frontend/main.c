/*
 *      Command line frontend program
 *
 *      Copyright (c) 1999 Mark Taylor
 *                    2000 Takehiro TOMINAGA
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

#include <jni.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define         MAX_U_32_NUM            0xFFFFFFFF

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#include <assert.h>
#include <stdio.h>

# ifndef HAVE_STRCHR
#  define strchr index
#  define strrchr rindex
# endif
char   *strchr(), *strrchr();
# ifndef HAVE_MEMCPY
#  define memcpy(d, s, n) bcopy ((s), (d), (n))
#  define memmove(d, s, n) bcopy ((s), (d), (n))
# endif

#ifdef HAVE_FCNTL_H
# include <fcntl.h>
#endif

#ifdef __sun__
/* woraround for SunOS 4.x, it has SEEK_* defined here */
#include <unistd.h>
#endif

#if defined(_WIN32)
# include <windows.h>
#endif

/*
 main.c is example code for how to use libmp3lame.a.  To use this library,
 you only need the library and lame.h.  All other .h files are private
 to the library.
*/

#include "brhist.h"
#include "parse.h"
#include "main.h"
#include "get_audio.h"
#include "portableio.h"
#include "timestatus.h"

/* PLL 14/04/2000 */
#if macintosh
#include <console.h>
#endif

#ifdef WITH_DMALLOC
#include <dmalloc.h>
#endif


#include <android/log.h> 
#define LOGV(...) __android_log_print(ANDROID_LOG_VERBOSE, "libnav", __VA_ARGS__) 

void
Java_org_freemp3droid_Converter_kill(JNIEnv*  env,jobject  thiz){
  userCancel=1;
}
static FILE *
init_files(lame_global_flags * gf, char *inPath, char *outPath, int *enc_delay, int *enc_padding)
{
    FILE   *outf;
    /* Mostly it is not useful to use the same input and output name.
       This test is very easy and buggy and don't recognize different names
       assigning the same file
     */
    if (0 != strcmp("-", outPath) && 0 == strcmp(inPath, outPath)) {
        error_printf("Input file and Output file are the same. Abort.\n");
        return NULL;
    }

    /* open the wav/aiff/raw pcm or mp3 input file.  This call will
     * open the file, try to parse the headers and
     * set gf.samplerate, gf.num_channels, gf.num_samples.
     * if you want to do your own file input, skip this call and set
     * samplerate, num_channels and num_samples yourself.
     */
    init_infile(gf, inPath, enc_delay, enc_padding);
    if ((outf = init_outfile(outPath, lame_get_decode_only(gf))) == NULL) {
        error_printf("Can't init outfile '%s'\n", outPath);
        return NULL;
    }

    return outf;
}

static void
print_lame_tag_leading_info(lame_global_flags * gf)
{
    if (lame_get_bWriteVbrTag(gf))
        console_printf("Writing LAME Tag...");
}

static void
print_trailing_info(lame_global_flags * gf)
{
    if (lame_get_bWriteVbrTag(gf))
        console_printf("done\n");

    if (lame_get_findReplayGain(gf)) {
        int     RadioGain = lame_get_RadioGain(gf);
        console_printf("ReplayGain: %s%.1fdB\n", RadioGain > 0 ? "+" : "",
                       ((float) RadioGain) / 10.0);
        if (RadioGain > 0x1FE || RadioGain < -0x1FE)
            error_printf
                    ("WARNING: ReplayGain exceeds the -51dB to +51dB range. Such a result is too\n"
                    "         high to be stored in the header.\n");
    }

    /* if (the user requested printing info about clipping) and (decoding
    on the fly has actually been performed) */
    if (print_clipping_info && lame_get_decode_on_the_fly(gf)) {
        float   noclipGainChange = (float) lame_get_noclipGainChange(gf) / 10.0f;
        float   noclipScale = lame_get_noclipScale(gf);

        if (noclipGainChange > 0.0) { /* clipping occurs */
            console_printf
                    ("WARNING: clipping occurs at the current gain. Set your decoder to decrease\n"
                    "         the  gain  by  at least %.1fdB or encode again ", noclipGainChange);

            /* advice the user on the scale factor */
            if (noclipScale > 0) {
                console_printf("using  --scale %.2f\n", noclipScale);
                console_printf("         or less (the value under --scale is approximate).\n");
            }
            else {
                /* the user specified his own scale factor. We could suggest
                * the scale factor of (32767.0/gfp->PeakSample)*(gfp->scale)
                * but it's usually very inaccurate. So we'd rather advice him to
                * disable scaling first and see our suggestion on the scale factor then. */
                console_printf("using --scale <arg>\n"
                        "         (For   a   suggestion  on  the  optimal  value  of  <arg>  encode\n"
                        "         with  --scale 1  first)\n");
            }

        }
        else {          /* no clipping */
            if (noclipGainChange > -0.1)
                console_printf
                        ("\nThe waveform does not clip and is less than 0.1dB away from full scale.\n");
            else
                console_printf
                        ("\nThe waveform does not clip and is at least %.1fdB away from full scale.\n",
                         -noclipGainChange);
        }
    }

}




static int
write_xing_frame(lame_global_flags * gf, FILE * outf)
{
    unsigned char mp3buffer[LAME_MAXMP3BUFFER];
    size_t  imp3, owrite;
    
    imp3 = lame_get_lametag_frame(gf, mp3buffer, sizeof(mp3buffer));
    if (imp3 > sizeof(mp3buffer)) {
        error_printf("Error writing LAME-tag frame: buffer too small: buffer size=%d  frame size=%d\n"
                    , sizeof(mp3buffer)
                    , imp3
                    );
        return -1;
    }
    if (imp3 <= 0) {
        return 0;
    }
    owrite = (int) fwrite(mp3buffer, 1, imp3, outf);
    if (owrite != imp3) {
        error_printf("Error writing LAME-tag \n");
        return -1;
    }
    if (flush_write == 1) {
        fflush(outf);
    }
    return imp3;
}



static int
lame_encoder(lame_global_flags * gf, FILE * outf, int nogap, char *inPath, char *outPath)
{
    unsigned char mp3buffer[LAME_MAXMP3BUFFER];
    int     Buffer[2][1152];
    int     iread, imp3, owrite, id3v2_size;

    imp3 = lame_get_id3v2_tag(gf, mp3buffer, sizeof(mp3buffer));
    if ((size_t)imp3 > sizeof(mp3buffer)) {
        error_printf("Error writing ID3v2 tag: buffer too small: buffer size=%d  ID3v2 size=%d\n"
                , sizeof(mp3buffer)
                , imp3
                    );
        return 1;
    }
    owrite = (int) fwrite(mp3buffer, 1, imp3, outf);
    if (owrite != imp3) {
        error_printf("Error writing ID3v2 tag \n");
        return 1;
    }
    if (flush_write == 1) {
        fflush(outf);
    }    
    id3v2_size = imp3;
    /* encode until we hit eof */
    do {
        /* read in 'iread' samples. 
        The value of iread is 1152  (2010/07/11) on my nexus one.
        */
        iread = get_audio(gf, Buffer);
        //needed to add a multiplier of 2 to achieve bytes on my G1. 2010/01/09 
        totBytes += iread *2;
        rewind(progFile);
        fprintf(progFile,"%d",totBytes);

        if (iread >= 0) {
            //char buf[50];
            //sprintf(buf,"%i",iread);
            //LOGV(buf); 
            /* encode */
            imp3 = lame_encode_buffer_int(gf, Buffer[0], Buffer[1], iread,
                                          mp3buffer, sizeof(mp3buffer));
 
            /* was our output buffer big enough? */
            if (imp3 < 0) {
                if (imp3 == -1)
                    error_printf("mp3 buffer is not big enough... \n");
                else
                    error_printf("mp3 X internal error:  error code=%i\n", imp3);
                return 1;
            }
            owrite = (int) fwrite(mp3buffer, 1, imp3, outf);
            if (owrite != imp3) {
                error_printf("Error writing mp3 output \n");
                return 1;
            }
        }
        if (flush_write == 1) {
            fflush(outf);
        }
    } while (iread > 0 && userCancel != 1);

    if (nogap)
        imp3 = lame_encode_flush_nogap(gf, mp3buffer, sizeof(mp3buffer)); /* may return one more mp3 frame */
    else
        imp3 = lame_encode_flush(gf, mp3buffer, sizeof(mp3buffer)); /* may return one more mp3 frame */

    if (imp3 < 0) {
        if (imp3 == -1)
            error_printf("mp3 buffer is not big enough... \n");
        else
            error_printf("mp3 internal error:  error code=%i\n", imp3);
        return 1;

    }

    
    owrite = (int) fwrite(mp3buffer, 1, imp3, outf);
    if (owrite != imp3) {
        error_printf("Error writing mp3 output \n");
        return 1;
    }
    if (flush_write == 1) {
        fflush(outf);
    }

    
    imp3 = lame_get_id3v1_tag(gf, mp3buffer, sizeof(mp3buffer));
    if ((size_t)imp3 > sizeof(mp3buffer)) {
        error_printf("Error writing ID3v1 tag: buffer too small: buffer size=%d  ID3v1 size=%d\n"
                , sizeof(mp3buffer)
                , imp3
                    );
    }
    else {
        if (imp3 > 0) {
            owrite = (int) fwrite(mp3buffer, 1, imp3, outf);
            if (owrite != imp3) {
                error_printf("Error writing ID3v1 tag \n");
                return 1;
            }
            if (flush_write == 1) {
                fflush(outf);
            }
        }
    }
    
    if (silent <= 0) {
        print_lame_tag_leading_info(gf);
    }
    if (fseek(outf, id3v2_size, SEEK_SET) != 0) {
        error_printf("fatal error: can't update LAME-tag frame!\n");                    
    }
    else {
        write_xing_frame(gf, outf);
    }

    if (silent <= 0) {
        print_trailing_info(gf);
    }    
    return 0;
}

/*this method expects a block at a time instead of the whole file, which lame_encoder does */
static int
lame_realtime_encoder(lame_global_flags * gf, FILE * outf, int nogap, short *inInt,int sizeInInt )
{
    unsigned char mp3buffer[LAME_MAXMP3BUFFER];
    //Buffer is read by the get_audio method. it has 2 arrays because of stereo aka numchannels.
    //only the first buffer is used for mono.
    
    int     Buffer[2][1152];
    int     iread, imp3, owrite, id3v2_size;

    imp3 = lame_get_id3v2_tag(gf, mp3buffer, sizeof(mp3buffer));
    if ((size_t)imp3 > sizeof(mp3buffer)) {
        error_printf("Error writing ID3v2 tag: buffer too small: buffer size=%d  ID3v2 size=%d\n"
                , sizeof(mp3buffer)
                , imp3
                    );
        return 1;
    }
     
    /* encode */
    //char outp[150];
   // sprintf(outp,"inInt: %i",sizeInInt);
    //LOGV(outp);
    short blank[sizeInInt];
    imp3 = lame_encode_buffer(gf, inInt, blank, sizeInInt, mp3buffer, sizeof(mp3buffer));
    /* was our output buffer big enough? */
    if (imp3 < 0) {
      if (imp3 == -1)
        LOGV("mp3 buffer is not big enough... \n");
      else { 
      char outp[150];
  
        sprintf(outp,"mp3 X internal error:  error code=%i\n", imp3);
	    LOGV(outp);      
      }
      return 1;
    }
    LOGV(mp3buffer);
	char outp[150];
  
        sprintf(outp,"mp3 X internal error:  error code=%i\n", imp3);
	    LOGV(outp);      
    
    owrite = (int) fwrite(mp3buffer, 1, imp3, outf);
    if (owrite != imp3) {
      LOGV("Error writing mp3 output \n");
      return 1;
    }
    
    
    fflush(outf);
    
}


JNIEXPORT jint JNICALL
JNI_OnLoad(JavaVM *vm, void *reserved)
{

    return JNI_VERSION_1_2;
}

//this is an attempt to convert raw data frame by frame, if anyone can find a solution, send a patch.
void
Java_org_freemp3droid_Converter_convertMP3Realtime(JNIEnv*  env,jobject  thiz,jstring aaa,jstring outF, jshortArray inBytes,
          int sampleRate, int bitRate){

  userCancel=0;
  lame_global_flags *gf = lame_init();
  lame_set_num_channels(gf, 1);
  lame_set_in_samplerate(gf,sampleRate);
  lame_set_brate(gf,bitRate);
  lame_set_bWriteVbrTag(gf,0);
  int ipr = lame_init_params(gf);

  const char* outfs;
  outfs = (*env)->GetStringUTFChars(env, outF, NULL);

  int     enc_delay = -1;
  int     enc_padding = -1;

  FILE * outf;
  int outFopen=0;
  if(outFopen == 0) {
    outf = fopen(outfs,"a+b");
    unsigned char mp3buffer[LAME_MAXMP3BUFFER];
    int imp3 = lame_get_id3v2_tag(gf, mp3buffer, sizeof(mp3buffer));
    fwrite(mp3buffer, 1, imp3, outf);
    outFopen =1;
  }  
  
  jshort* jInBytes = (*env)->GetShortArrayElements(env,inBytes,NULL);
  jsize sizeIn = (*env)->GetArrayLength(env,inBytes); 
  short inInts[sizeIn];
  int totRead=0; 
  int amountRead; 
  
   while(totRead < sizeIn) {
  	 if(sizeIn - totRead < 1152) {
  	   amountRead = sizeIn -totRead;
     } else {
       amountRead = 1152;
     }
     int i;
     for(i=0;i< amountRead;i++) {
       inInts[i] = jInBytes[i];
   	   totRead++;
  	 }
     lame_realtime_encoder(gf, outf,0 ,inInts,amountRead);
  }
  
  //failing on JNI: unpinPrimitiveArray() failed to find entry (valid=0)
  //(*env)->ReleaseByteArrayElements(env,jInBytes,inBytes,0);
}

void
Java_org_freemp3droid_Converter_convertMP3(JNIEnv*  env,jobject  thiz,jstring inFile ,jstring outFile, 
          jstring progF,  int sampleRate, int bitRate){

  userCancel=0;
  totBytes=0;
  lame_global_flags *gf = lame_init();
  lame_set_num_channels(gf, 1);
  lame_set_in_samplerate(gf,sampleRate);
  lame_set_brate(gf,bitRate);
  lame_set_bWriteVbrTag(gf,0);
  int ipr = lame_init_params(gf);

  const char* outfs;
  outfs = (*env)->GetStringUTFChars(env, outFile, NULL);

  const char* infs;
  infs = (*env)->GetStringUTFChars(env,inFile, NULL);
  int     enc_delay = -1;
  int     enc_padding = -1;

  const char* progfs;
  progfs = (*env)->GetStringUTFChars(env, progF, NULL);
  //set the global progress file variable to be accessed in lame_encoder()
  progFile = fopen(progfs,"w");
  FILE *outf = init_files(gf,infs,outfs,&enc_delay,&enc_padding);
  lame_encoder(gf, outf,0 ,infs ,outfs );
}

