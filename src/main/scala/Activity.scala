package spur.shouty

import _root_.android.app.Activity
import _root_.android.os.Bundle
import _root_.android.widget.TextView
import org.jboss.netty.channel.{Channel,ChannelFuture,ChannelFutureListener}
import org.jboss.netty.handler.codec.http.{HttpHeaders, DefaultHttpChunk,
                                           DefaultHttpChunkTrailer}
import org.jboss.netty.channel.group.{DefaultChannelGroup, ChannelGroupFuture}
class MainActivity extends Activity {
  override def onCreate(savedInstanceState: Bundle) {
    super.onCreate(savedInstanceState)

    import android.net.wifi.WifiManager
    import android.content.Context.WIFI_SERVICE
    val wifiManager = getBaseContext().getSystemService(
      WIFI_SERVICE).asInstanceOf[WifiManager]
    val wifiInfo = wifiManager.getConnectionInfo()
    val ipAddress = wifiInfo.getIpAddress()
    val ip = "%d.%d.%d.%d".format(
      (ipAddress & 0xff),
      (ipAddress >> 8 & 0xff),
      (ipAddress >> 16 & 0xff),
      (ipAddress >> 24 & 0xff))

    setContentView(new TextView(this) {
      setText("hello, world " + ip)
    })

    import unfiltered.request._
    import unfiltered.response._
    import unfiltered.netty._
    object Hello extends async.Plan with ServerErrorResponse {
      val ChunkedMp3 =
        unfiltered.response.Connection(HttpHeaders.Values.CLOSE) ~>
        TransferEncoding(HttpHeaders.Values.CHUNKED) ~>
        ContentType("audio/mpeg3")
      val listeners = new DefaultChannelGroup
      def intent = {
        case req =>
          val initial = req.underlying.defaultResponse(ChunkedMp3)
          val ch = req.underlying.event.getChannel
          ch.write(initial).addListener { () =>
            listeners.add(ch)
          }

      }
      def tick(payload: Array[Byte], len: Int) {
        import org.jboss.netty.buffer.ChannelBuffers
        println("tick")
        val chunk = new DefaultHttpChunk(
          ChannelBuffers.copiedBuffer(payload, 0, len)
        )
        listeners.write(chunk)
      }
    }
    unfiltered.netty.Http(8080).plan(Hello).start()

    import android.os.Handler
    val handler = new Handler
    handler.post(new Runnable{
      def run {
        import java.io.{File,FileInputStream,BufferedInputStream}
        val file = new File("/mnt/sdcard/Music/test.mp3");
        val is = new BufferedInputStream(new FileInputStream(file))
        val buf = new Array[Byte](1024*16)

        @annotation.tailrec def read() {
          val len = is.read(buf)
          if (len > -1) {
            Hello.tick(buf, len)
            Thread.sleep(500)
            read()
          }
        }
        read()
      }
    })
  }
  implicit def block2listener[T](block: () => T): ChannelFutureListener =
    new ChannelFutureListener {
      def operationComplete(future: ChannelFuture) { block() }
    }
}
