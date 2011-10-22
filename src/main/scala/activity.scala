package spur.shouty

import android.view.View
import android.content.Intent
import android.app.Activity

class ControllerActivity extends Activity with TypedViewHolder {
  lazy val server = unfiltered.netty.Http(8080).plan(Stream)

  override def onCreate(savedInstanceState: android.os.Bundle) {
    super.onCreate(savedInstanceState)

    startService(new Intent(this, classOf[ServerService]))

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
        ControllerActivity.this.finish()
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
    Thread.sleep(5000)
    System.exit(0)
  }
}
