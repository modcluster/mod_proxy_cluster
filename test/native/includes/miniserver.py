#!/usr/bin/python3
# mini server to test mod_proxy_cluster.
#
import socket
import time
import sys

import threading

import random
import string

class MiniServer():
  def __init__(self, port):
    self.HOST = "127.0.0.1"  # Standard loopback interface address (localhost)
    self.PORT = port  # Port to listen on (non-privileged ports are > 1023)
    letters = string.ascii_lowercase
    self.JVMROUTE = ''.join(random.choice(letters) for i in range(10))
    self.page = ""
    self.page = self.page + "HTTP/1.1 200 OK\r\n"
    self.page = self.page + "Content-Length: 10\r\n"
    self.page = self.page + "Content-Type: text/plain; charset=UTF-8\r\n"
    self.page = self.page + "\r\n"
    self.page = self.page + "1234567890"

  def listen(self):  
    s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    s.bind((self.HOST, self.PORT))
    s.listen()
    return s

  def processcon(self, conn, addr):
    print(f"Connected by {addr}")
    while True:
       data = conn.recv(1024)
       if not data:
         break
       print("sending: " + self.page)
       conn.sendall(bytes(self.page, 'utf-8'))
    conn.close()

  def processtimeout(self, conn, addr):
    print(f"Connected by {addr}")
    while True:
       data = conn.recv(1024)
       if not data:
         break
       print("Timing out...")
       time.sleep(60)
    conn.close()

  def processtimeoutacc(self, s):
    print("Timing out...")
    time.sleep(400)
    conn, addr = s.accept()
    print(f"Connected by {addr}")
    conn.close()

  def sendmess(self, s , method, mess):
    met = method + " / HTTP/1.1\r\n"
    s.send(bytes(met, 'utf-8'))
    s.send(b"Host: localhost:6666\r\n")
    contentlength = "Content-Length: " + str(len(mess))
    s.send(bytes(contentlength, 'utf-8'))
    s.send(b"\r\n")
    s.send(b"\r\n")
    s.send(bytes(mess, 'utf-8'))
    while True:
      response = s.recv(512)
      sresp = response.decode('utf-8')
      if sresp == "":
        raise Exception("Not 200 but empty!")
      print(sresp)
      if sresp.startswith("HTTP/1.1 "):
        if not sresp.startswith("HTTP/1.1 200 "):
          raise Exception("Not 200!")
        break

  def sendconfig(self, s):
    mess = ""
    mess = mess + "JVMRoute=" + self.JVMROUTE
    mess = mess + "&Host=" + self.HOST
    mess = mess + "&Maxattempts=1&Port=" + str(self.PORT)
    mess = mess + "&StickySessionForce=No&Timeout=20&Type=http&ping=20"
    self.sendmess(s, "CONFIG" , mess)
    
  def sendstatus(self, s):
    mess = ""
    mess = mess + "JVMRoute=" + self.JVMROUTE + "&Load=100"
    self.sendmess(s, "STATUS" , mess)

  # send enable-app for the app (no / in the app string!)
  def sendenable(self, s, app):
    mess = ""
    mess = mess + "JVMRoute=" + self.JVMROUTE + "&Alias=default-host%2Clocalhost&Context=%2F" + app
    self.sendmess(s, "ENABLE-APP" , mess)

  # send status message every 10 seconds
  def loopsendstatus(self, s):
    while True:
      self.sendstatus(s)
      time.sleep(5)

# main logic
def main():
  args = sys.argv[1:]
  port = 8080
  timeout = False
  if len(args) == 1:
    port = int(sys.argv[1])

  # start the listen on the server.
  server = MiniServer(port)
  s = server.listen()

  # connect MCMP client part.
  ai = socket.getaddrinfo("localhost", 6666)
  addr = ai[0][-1]
  client = socket.socket()
  client.connect(addr)
  server.sendconfig(client)
  server.sendenable(client, "myapp")
  t1 = threading.Thread(target=server.loopsendstatus, args=(client,))
  t1.start()
  while True:
    if timeout:
      server.processtimeoutacc(s)
    else:
      conn, addr = s.accept()
      server.processcon(conn, addr)

if __name__ == '__main__':
  main()
