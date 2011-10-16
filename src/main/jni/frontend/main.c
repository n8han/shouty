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

JNIEXPORT jint JNICALL
Java_spur_shouty_NativeEncode_pcm2mp3(JNIEnv* env,
                                      jobject thiz,
                                      jshortArray _pcm,
                                      jint pcmlen,
                                      jint sampleRate,
                                      jbyteArray _mp3) {
  static lame_global_flags *gf;

  if (!gf) {
    LOGV("initializing LAME");
    gf = lame_init();
    lame_set_num_channels(gf, 1);
    lame_set_in_samplerate(gf, sampleRate);
    lame_init_params(gf);
  }

  jshort *pcm = (*env)->GetShortArrayElements(env, _pcm, NULL);
  jbyte *mp3 = (*env)->GetByteArrayElements(env, _mp3, NULL);

  jsize mp3len = (*env)->GetArrayLength(env, _mp3);

  int ret = lame_encode_buffer(gf, pcm, NULL, pcmlen, mp3, mp3len);
  char outp[150];
  sprintf(outp,"pcmlen %d mp3len %d ret %d", pcmlen, mp3len, ret);
  LOGV(outp);

  (*env)->ReleaseShortArrayElements(env, _pcm, pcm, 0);
  (*env)->ReleaseByteArrayElements(env, _mp3, mp3, 0);
  return ret;
}
