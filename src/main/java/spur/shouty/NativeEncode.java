package spur.shouty;

public class NativeEncode {
  public static native int pcm2mp3(short[] pcm,
                                   int pcmlen,
                                   int sampleRate,
                                   byte[] mp3);
}
