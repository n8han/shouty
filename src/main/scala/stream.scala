package spur.shouty

import org.jboss.netty.handler.codec.http.{HttpHeaders, DefaultHttpChunk,
                                           DefaultHttpChunkTrailer}
import org.jboss.netty.channel.group.{DefaultChannelGroup, ChannelGroupFuture}
import org.jboss.netty.channel.{Channel,ChannelFuture,ChannelFutureListener}

import unfiltered.request._
import unfiltered.response._
import unfiltered.netty._

object Stream extends async.Plan with ServerErrorResponse {
  @volatile private var stopping = false

  val ChunkedMp3 =
    unfiltered.response.Connection(HttpHeaders.Values.CLOSE) ~>
    TransferEncoding(HttpHeaders.Values.CHUNKED) ~>
    ContentType("audio/mp3")

  val listeners = new DefaultChannelGroup
  def intent = {
    case req =>
      val initial = req.underlying.defaultResponse(ChunkedMp3)
      val ch = req.underlying.event.getChannel
      ch.write(initial).addListener { () =>
        listeners.add(ch)
      }
  }
  def chunk(payload: Array[Byte], len: Int) = {
      import org.jboss.netty.buffer.ChannelBuffers
      new DefaultHttpChunk(
        ChannelBuffers.copiedBuffer(payload, 0, len)
      )
  }
  def write(payload: Array[Byte], len: Int) {
    if (!stopping) {
      listeners.write(chunk(payload, len))
    }
  }
  def stop(last: Array[Byte], len: Int) {
    stopping = true
    if (len > 0)
      listeners.write(chunk(last, len)).await()
    listeners.write(new DefaultHttpChunkTrailer).await()
    listeners.close()
    stopping = false
  }
  implicit def block2listener[T](block: () => T): ChannelFutureListener =
    new ChannelFutureListener {
      def operationComplete(future: ChannelFuture) { block() }
    }
}
