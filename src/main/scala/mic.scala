package spur.shouty

import android.media.{AudioRecord,AudioFormat,MediaRecorder}

object Mic {
  val frequency = 11025
  val channelConfiguration = AudioFormat.CHANNEL_CONFIGURATION_MONO
  val audioEncoding = AudioFormat.ENCODING_PCM_16BIT
  val bufferSize = AudioRecord.getMinBufferSize(frequency,
                                                channelConfiguration,
                                                audioEncoding)
  def start(pcm: (Array[Byte], Int) => Unit) {
    val source = (new MediaRecorder).AudioSource.MIC
    val audioRecord = new AudioRecord(source,
                                      frequency,
                                      channelConfiguration,
                                      audioEncoding,
                                      bufferSize)

    audioRecord.startRecording()
    new Thread {
      override def run {
        while(true) {
          val buffer = new Array[Byte](bufferSize)
          val len = audioRecord.read(buffer, 0, bufferSize)
          pcm(buffer, len)
        }
      }
    }.start()
  }
}
