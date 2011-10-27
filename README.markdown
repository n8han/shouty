![Shouty](https://github.com/n8han/shouty/raw/master/src/main/res/drawable-xhdpi/icon.png)

Shouty
======

Shouty is an Android app for **ad hoc broadcasting**. Using any local
wifi network, connected or not to the internet, Shouty serves an mp3
stream from your microphone to anyone who wants to connect.

This could be useful in a setting where you want to carry a message
using phones and computers you already have, without depending on
external networks and systems. For best results,
[bring your own wifi](http://wiki.daviddarts.com/PirateBox).

Download
--------

[From the downloads page](https://github.com/n8han/shouty/downloads).

Tested Players
--------------

To listen to Shouty to it you need an app to play streaming mp3. We've
confirmed support on the following players, by entering in the
Shouty broadcast URL as described.

* Ubuntu, Banshee - Media -> Open Location...
* Mac OS 10.7, Safari - Location bar
* Mac OS 10.7, iTunes - Advanced -> Open Stream...
* Android, ServeStream - Location bar

Shouty and the player must generally be on the same wifi network.

Protocol
--------

The network protocol is just chunked HTTP. We thought about using
Shoutcast/Icecast, but we don't need to interleave track names
(although, that might be interesting) and plain HTTP works fine in the
players tested.

Needs
-----

What to help this project? See the [open issues][issues].

[issues]: https://github.com/n8han/shouty/issues

Building and Contributing
-------------------------

Just install the Android SDK and NDK, as well as the Scala builder
[sbt][sbt], and also `publish-local` the current master of
[android-plugin](https://github.com/jberkel/android-plugin/). And
you're done!

[sbt]: https://github.com/harrah/xsbt/wiki
