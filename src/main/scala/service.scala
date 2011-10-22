package spur.shouty

import android.content.{Intent,Context}
import android.app.{
  Service,Notification,NotificationManager,PendingIntent}

class ServerService extends Service {
  def onBind(intent: Intent) = null
  lazy val notes = getSystemService(
    Context.NOTIFICATION_SERVICE
  ).asInstanceOf[NotificationManager]
  lazy val server = unfiltered.netty.Http(8080).plan(Stream)

  override def onStartCommand(intent: Intent,
                              flags: Int,
                              startId: Int) = Service.START_STICKY
  val encode = Encode(Mic)

  override def onCreate() {
    server.start()
    Mic.start(encode(Stream.write))

    notify(broadcastUrl)
  }

  override def onDestroy() {
    notes.cancel(R.string.broadcast_title)
    Mic.stop()
    Stream.stop()
    server.stop()
  }

  def broadcastUrl = {
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

    "http://%s:%d/".format(ip, server.port)
  }

  def notify(url: String) {
    val note = new Notification(
      android.R.drawable.stat_sys_upload,
      null,
      System.currentTimeMillis())

    val contentIntent = PendingIntent.getActivity(
      this, 0, new Intent(this, classOf[ControllerActivity]), 0)
    val content = getText(R.string.broadcast_content).toString.format(url)

    note.setLatestEventInfo(getApplicationContext,
                            getText(R.string.broadcast_title),
                            content,
                            contentIntent)

    note.flags = Notification.FLAG_NO_CLEAR |
                 Notification.FLAG_ONGOING_EVENT

    notes.notify(R.string.broadcast_title, note)
  }
}
