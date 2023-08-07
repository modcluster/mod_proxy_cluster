package org.jboss.modcluster

import com.ning.http.client.AsyncHttpClient
import com.ning.http.client.ListenableFuture
import com.ning.http.client.ws.WebSocket
import com.ning.http.client.ws.WebSocketListener
import com.ning.http.client.ws.WebSocketTextListener
import com.ning.http.client.ws.WebSocketUpgradeHandler

import groovy.util.logging.Slf4j

import java.util.concurrent.CountDownLatch
import java.util.concurrent.LinkedBlockingDeque
import java.util.concurrent.TimeUnit
import java.util.concurrent.atomic.AtomicInteger

import static org.junit.Assert.assertEquals
import static org.junit.Assert.assertTrue
import static org.junit.Assert.fail

/**
 * The test checks that websocket-hello-0.0.1/ is working.
 * https://github.com/jfclere/httpd_websocket for the webapp
 */
@Slf4j
class WebSocketsTest {

  private void performWsTunnelTest(String httpdServerId, String wsUrl, String TEST_MESSAGE) {
    final CountDownLatch latch = new CountDownLatch(1)
    final LinkedBlockingDeque<String> received = new LinkedBlockingDeque<String>()
    final AtomicInteger controlListSize = new AtomicInteger();
    final AsyncHttpClient httpClient = new AsyncHttpClient()
    final List<WebSocket> websockets = []
    try {
      final WebSocketUpgradeHandler webSocketUpgradeHandler = new WebSocketUpgradeHandler.Builder().addWebSocketListener(new WebSocketTextListener() {
        @Override
        public void onMessage(String message) {
          received.add(message)
          log.info("[$httpdServerId]: received message: $message")
          latch.countDown()
        }

        @Override
        public void onOpen(WebSocket websocket) {
          log.info("[$httpdServerId]: WebSocket opened.")
        }

        @Override
        public void onClose(WebSocket websocket) {
          log.info("[$httpdServerId]: WebSocket closed.")
        }

        @Override
        public void onError(Throwable t) {
          fail("[$httpdServerId]:(JWS-63) Url:[${wsUrl}] WebSocket error: " + t.getMessage())
          t.printStackTrace()
        }
      } as WebSocketListener).build()

      (0..50).each { iter ->
        final AsyncHttpClient.BoundRequestBuilder builder = httpClient.prepareGet(wsUrl)
	builder.setHeader("sec-websocket-key", "c9khTUa/GnMLiHiJhMgqNA==");
        builder.setHeader("sec-websocket-version", "13");
        builder.setHeader("sec-websocket-extensions", "x-webkit-deflate-frame; toto=titi");
        // builder.setHeader("sec-websocket-extensions", "toto");
        // builder.setHeader("toto", "titi")
        final ListenableFuture future = builder.execute(webSocketUpgradeHandler)
        final WebSocket websocket = future.get()
        websockets.add(websocket)
        (0..2).each { it ->
          websocket.sendMessage("${TEST_MESSAGE} ${iter} ${it}")
          controlListSize.getAndIncrement()
        }
      }

      final int TIMEOUT = 10000
      long start = System.currentTimeMillis()
      while (controlListSize.toInteger() != received.findAll({ it -> it.contains(TEST_MESSAGE) }).size() && System.currentTimeMillis() - start < TIMEOUT) {
        Library.letsSleep(100)
      }

      assertTrue("WebSocketTomcatTest.doWebSocketTest(): Message was not received till 10 seconds.", latch.await(10, TimeUnit.SECONDS))
      assertTrue("WebSocketTomcatTest.doWebSocketTest(): Unexpected message from server: ${received.getFirst()}.", received.getFirst().contains("from websocket endpoint"))
      assertEquals("Sent and received messages don't match.", controlListSize.toInteger(), received.findAll({ it -> it.contains(TEST_MESSAGE) }).size())
    } finally {
      websockets?.each { it?.close() }
      httpClient?.close()
    }
  }
}
