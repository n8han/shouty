package spur.shouty

import android.view.View
import android.content.Intent
import android.app.Activity

object Log {
  def i(msg: String) { android.util.Log.i("shouty", msg) }
}

class ControllerActivity extends Activity with TypedViewHolder {

  override def onCreate(savedInstanceState: android.os.Bundle) {
    super.onCreate(savedInstanceState)

    startService(new Intent(this, classOf[ServerService]))

    setContentView(R.layout.recorder)

    findView(TR.stream_url).setText("fixme")

    findView(TR.quit).setOnClickListener(new View.OnClickListener() {
      def onClick(v: View) {
        stopService(new Intent(ControllerActivity.this,
                               classOf[ServerService]))
        ControllerActivity.this.finish()
      }
    })
  }
}
