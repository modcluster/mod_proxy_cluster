package org.jboss.modcluster

import com.gargoylesoftware.htmlunit.WebClient
import com.gargoylesoftware.htmlunit.WebRequest 
import com.gargoylesoftware.htmlunit.WebResponse

import groovy.util.logging.Slf4j

import static org.junit.Assert.assertEquals
import static org.junit.Assert.assertTrue
import static org.junit.Assert.fail

/**
 * The test checks that testapp1 on tomcat8080 and testapp2 on tomcat8081 are accessible from mod_cluster.
 */
@Slf4j
class WebHTTPTest {
  private void performHTTPTest() {
    Collection<String> confirms = []
    WebClient webClient = new WebClient()
    webClient.getOptions().setThrowExceptionOnFailingStatusCode(false)
    WebRequest request1 = new WebRequest(new URL('http://localhost:8000/testapp1/test.jsp'))
    // request1.setCharset(StandardCharsets.UTF_8)
    WebResponse response1 = webClient.loadWebResponse(request1)
    assertTrue("Can't find test1!", response1.getStatusCode() == 200)
    WebRequest request2 = new WebRequest(new URL('http://localhost:8000/testapp2/test.jsp'))
    // request2.setCharset(StandardCharsets.UTF_8)
    WebResponse response2 = webClient.loadWebResponse(request2)
    assertTrue("Can't find test2!", response2.getStatusCode() == 200)
  }
}
