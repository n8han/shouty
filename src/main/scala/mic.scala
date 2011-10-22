package spur.shouty

import android.media.{AudioRecord,AudioFormat,MediaRecorder}

object Mic {
  val sampleRate = 11025
  val channelConfiguration = AudioFormat.CHANNEL_IN_MONO
  val audioEncoding = AudioFormat.ENCODING_PCM_16BIT
  val bufferSize = AudioRecord.getMinBufferSize(sampleRate,
                                                channelConfiguration,
                                                audioEncoding)*8
  @volatile private var stopping = false

  def start(pcm: (Array[Short], Int) => Unit) {
    val source = (new MediaRecorder).AudioSource.MIC
    val audioRecord = new AudioRecord(source,
                                      sampleRate,
                                      channelConfiguration,
                                      audioEncoding,
                                      bufferSize)

    audioRecord.startRecording()
    new Thread {
      override def run {
        while(!stopping) {
          val buffer = new Array[Short](bufferSize)
          val len = audioRecord.read(buffer, 0, bufferSize)
          pcm(buffer, len)
        }
        audioRecord.stop()
        stopping = false
      }
    }.start()
  }
  def stop() {
    stopping = true
  }
}
