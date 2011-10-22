package spur.shouty

import android.content.{Intent,Context}
import android.app.{
  Service,Notification,NotificationManager,PendingIntent}
import android.util.Log

class ServerService extends Service {
  def onBind(intent: Intent) = null
  lazy val notes = getSystemService(
    Context.NOTIFICATION_SERVICE
  ).asInstanceOf[NotificationManager]

  override def onStartCommand(intent: Intent,
                              flags: Int,
                              startId: Int) = {
    notifyStarted()
    Service.START_STICKY
  }

  def notifyStarted() {
    val text = getText(R.string.started_server)
    val note = new Notification(
      android.R.drawable.stat_sys_download,
      text, 
      System.currentTimeMillis())

    val contentIntent = PendingIntent.getActivity(
      this, 0, new Intent(this, classOf[ControllerActivity]), 0)
                                                  
    note.setLatestEventInfo(getApplicationContext,
                            text, text, contentIntent)
    notes.notify(R.string.started_server, note)
  }
}
