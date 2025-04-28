import logging
import socket
import threading
from typing import Dict

from can_server import CanServer
from packet import SendPacket

# Define maximum send size as a constant. ISOTP max is 4095
MAX_SEND_SIZE = 2048


class CanTcpBridge:
    def __init__(self,
                 can_srv: CanServer,
                 tcp_server_host: str,
                 tcp_server_port: int,
                 logger: logging.Logger):
        self.can_server = can_srv
        self.tcp_server_host = tcp_server_host
        self.tcp_server_port = tcp_server_port
        self.logger = logger
        self.connections: Dict[int, socket.socket] = {}
        self.lock = threading.Lock()

    def run(self):
        receiver_thread = threading.Thread(target=self._receiver_loop)
        receiver_thread.start()
        receiver_thread.join()

    def _receiver_loop(self):
        while True:
            packet = self.can_server.recv_packet()
            with self.lock:
                if packet.src_addr not in self.connections:
                    self.logger.info(f"First CAN packet from node {packet.src_addr}. Opening new TCP connection")
                    sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
                    sock.connect((self.tcp_server_host, self.tcp_server_port))
                    self.connections[packet.src_addr] = sock
                    handler_thread = threading.Thread(target=self._handle_connection, args=(packet.src_addr, sock))
                    handler_thread.start()
                else:
                    sock = self.connections[packet.src_addr]
            try:
                sock.sendall(packet.data)
            except Exception as e:
                self.logger.error(
                    f"Error sending to TCP server for node {packet.src_addr}: {e}. Closing TCP connection.")
                with self.lock:
                    if packet.src_addr in self.connections and self.connections[packet.src_addr] == sock:
                        del self.connections[packet.src_addr]
                sock.close()

    def _handle_connection(self, node_id: int, sock: socket.socket):
        while True:
            try:
                data = sock.recv(MAX_SEND_SIZE)
                if not data:
                    break
                # Split data into chunks of MAX_SEND_SIZE
                for i in range(0, len(data), MAX_SEND_SIZE):
                    chunk = data[i:i + MAX_SEND_SIZE]
                    self.can_server.send_packet(SendPacket(dst_addr=node_id, data=chunk))
            except Exception as e:
                self.logger.error(f"Error receiving from TCP server for node {node_id}: {e}. Closing connection")
                break
        with self.lock:
            if node_id in self.connections and self.connections[node_id] == sock:
                del self.connections[node_id]
        sock.close()
