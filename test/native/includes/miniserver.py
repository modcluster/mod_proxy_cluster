#!/usr/bin/python3
# mini server to test mod_proxy_cluster.
#
import socket

class MiniServer():
  def __init__(self):
    self.HOST = "127.0.0.1"  # Standard loopback interface address (localhost)
    self.PORT = 8080  # Port to listen on (non-privileged ports are > 1023)
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


# main logic
server = MiniServer()
s = server.listen()
while True:
  conn, addr = s.accept()
  server.processcon(conn, addr)
