package spur.shouty

import java.io.File
import android.media.MediaRecorder

object Mic {
  val recorder = new MediaRecorder
  def start(file: File) {
    recorder.setAudioSource(recorder.AudioSource.MIC)
    recorder.setOutputFormat(recorder.OutputFormat.MPEG_4)
    recorder.setAudioEncoder(recorder.AudioEncoder.AAC)
    recorder.setOutputFile(file.getAbsolutePath)
    recorder.prepare()
    recorder.start()
  }
  def stop() {
    recorder.stop()
  }
}
