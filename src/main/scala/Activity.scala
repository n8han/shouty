package spur.shouty

import _root_.android.app.Activity
import _root_.android.os.Bundle
import _root_.android.widget.TextView
import java.io.{File,FileInputStream,BufferedInputStream}

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
      setText("Tune in at http://%s:8080/ ".format(ip))
    })

    unfiltered.netty.Http(8080).plan(Hello).start()
    val filebuf = new File(getBaseContext.getCacheDir(),
                           "buf.aac")
    println(filebuf.getAbsolutePath)
    
    Mic.start(filebuf)

    new Thread {
      override def run {
        Thread.sleep(1000)
        val is = new BufferedInputStream(new FileInputStream(filebuf))
        val buf = new Array[Byte](1024*16)

        while (true) {
          if (is.available() > 0) {
            val len = is.read(buf)
            Hello.tick(buf, len)
          } else {
            println("none avail")
          }
          Thread.sleep(500)
        }
      }
    }.start()
    new Thread {
      override def run {
        Thread.sleep(60000)
        Mic.stop()
        println("STOPPED")
      }
    }.start()
  }
}
