package spur.shouty

import android.view.View
import android.content.{Intent,DialogInterface}
import android.app.{Activity,AlertDialog}

object Log {
  def i(msg: String) { android.util.Log.i("shouty", msg) }
}

class ControllerActivity extends Activity with TypedViewHolder {

  override def onCreate(savedInstanceState: android.os.Bundle) {
    super.onCreate(savedInstanceState)

    startService(new Intent(this, classOf[ServerService]))

    setContentView(R.layout.recorder)

    ServerService.broadcastUrl(getBaseContext) match {
      case Some(location) =>
        getText(R.string.broadcast_content).toString.format(location)
        findView(TR.stream_url).setText(location)

        findView(TR.quit).setOnClickListener(
          new View.OnClickListener() {
            def onClick(v: View) {
              stopService(new Intent(ControllerActivity.this,
                                     classOf[ServerService]))
              ControllerActivity.this.finish()
            }
          }
        )
      case None =>
        new AlertDialog.Builder(this)
          .setTitle(getText(R.string.no_wifi_title))
          .setMessage(getText(R.string.no_wifi_message))
          .setNeutralButton(
            getText(R.string.no_wifi_button),
            new DialogInterface.OnClickListener {
              def onClick(dialog: DialogInterface,
                          which: Int) {
                ControllerActivity.this.finish()
              }
            }
          )
        .show()
    }
  }
}
