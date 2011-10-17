Shouty
======

Shouty is an Android app for **ad hoc broadcasting**. Using any local
wifi network, connected or not to the internet, Shouty serves an mp3
stream from your microphone to anyone who wants to connect.

Download
--------

[From the downloads page](https://github.com/n8han/shouty/downloads).

Playback
--------

You just have to find your player's buried menu for this, "Open
Location" or "Connect to Stream" or something along those lines. Then,
enter the URL that appears on your phone's screen when you start the
app. Also, be on the same wifi network as the phone.

Protocol
--------

The network protocol is just chunked HTTP. We thought about using
Shoutcast/Icecast, but we don't need to interleave track names
(although, that might be interesting) and plain HTTP works fine in the
players tested.

Interface
---------

The interface is really terrible. Can you (yes you!) please fork and
make it better?

Building and Contributing
-------------------------

Just install the Android SDK and NDK, as well as the Scala builder
[sbt][sbt], and also `publish-local` the current master of
[android-plugin](https://github.com/jberkel/android-plugin/). And
you're done!

[sbt]: https://github.com/harrah/xsbt/wiki
