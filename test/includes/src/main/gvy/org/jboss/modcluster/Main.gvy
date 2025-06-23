package org.jboss.modcluster

static void main(String[] args) {
  if ( args[0] == "WebSocketsTest" ) { 
    t = new WebSocketsTest()
    t.performWsTunnelTest("TEST", "ws://localhost:6666/websocket-hello-0.0.1/websocket/helloName", "Hello")
  } else {
    t = new WebHTTPTest()
    t.performHTTPTest()
  }
}
