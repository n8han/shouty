package spur.shouty

import _root_.android.app.Activity
import _root_.android.os.Bundle
import _root_.android.view.View
import java.io.{File,FileInputStream,BufferedInputStream}

class MainActivity extends Activity with TypedViewHolder {
  lazy val server = unfiltered.netty.Http(8080).plan(Stream)

  override def onCreate(savedInstanceState: Bundle) {
    super.onCreate(savedInstanceState)

    setContentView(R.layout.recorder)

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

    findView(TR.stream_url).setText(
      "http://%s:%d/ ".format(ip, server.port))

    findView(TR.quit).setOnClickListener(new View.OnClickListener() {
      def onClick(v: View) {
        MainActivity.this.finish()
      }
    })

    server.start()
    val encode = Encode(Mic)
    Mic.start(encode(Stream.write))
  }
  override def onDestroy() {
    super.onDestroy()
    Mic.stop()
    Stream.stop()
    server.stop()
    System.exit(0) // sorry! make this into a service, somebody
  }
}
