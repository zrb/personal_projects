#!/usr/bin/env python3
from socketserver import TCPServer, BaseRequestHandler
import random
import time

class RandomGeneratorHandler(BaseRequestHandler):
    def handle(self):
        time.sleep(1)
        n = random.randint(1, 100)
        self.request.sendall(n.to_bytes(4, byteorder='little'))

class RandomGeneratorServer(TCPServer):
    allow_reuse_address = True

if __name__ == "__main__":
    HOST, PORT = "localhost", 56789
    with RandomGeneratorServer((HOST, PORT), RandomGeneratorHandler) as server:
        print(f"Starting random number generator server on {HOST}:{PORT}")
        server.serve_forever()
