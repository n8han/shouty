package spur.shouty

case class Encode(source: { 
  def sampleRate: Int
  def bufferSize: Int
}) {
  System.loadLibrary("Lame")
  private val mp3 = new Array[Byte](source.bufferSize / 4)

  def apply(out: (Array[Byte], Int) => Unit)
           (pcm: Array[Short], len: Int) {
    val written = NativeEncode.pcm2mp3(pcm, len, source.sampleRate, mp3)
    out(mp3, written)
  }
}
