import functools
import queue
import threading
from typing import Optional

import isotp
from typing_extensions import Callable

from node_mac import NodeMac
from packet import Packet, RecvPacket, SendPacket

MIN_NODE_ADDR = 1
MAX_NODE_ADDR = 254


class Node:
    def __init__(self,
                 node_mac: NodeMac,
                 node_addr: int,
                 send_func: Callable[[isotp.CanMessage], None],
                 recv_packet_queue: queue.Queue[RecvPacket]) -> None:
        self.__mac = node_mac
        self.__addr = node_addr
        self.__send_func = send_func
        self.__recv_msg_queue: queue.Queue[isotp.CanMessage] = queue.Queue()
        self.__recv_packet_queue = recv_packet_queue
        isotp_addr = isotp.Address(isotp.AddressingMode.Normal_29bits, rxid=0x0, txid=node_addr)
        partial_rxfn = functools.partial(Node.__my_rxfn, self)
        params = {
            'blocking_send': True,
            'stmin': 2,
            'rx_flowcontrol_timeout': 2000
        }
        self.__isotp = isotp.TransportLayer(rxfn=partial_rxfn, txfn=send_func, address=isotp_addr, params=params)
        self.__isotp.start()
        self.__recv_worker_thread = threading.Thread(target=self.__recv_worker, daemon=True)
        self.__recv_worker_thread.start()

    @property
    def mac(self) -> NodeMac:
        return self.__mac

    @property
    def addr(self) -> int:
        return self.__addr

    def on_received_can_msg(self, isotp_msg: isotp.CanMessage) -> None:
        self.__recv_msg_queue.put(isotp_msg)

    def recv_packet(self) -> Packet:
        return self.__recv_packet_queue.get(block=True)

    def send_packet(self, packet: SendPacket) -> None:
        if packet.dst_addr != self.__addr:
            raise ValueError(f"Packet destination address {packet.dst_addr} does not match node address {self.__addr}")
        self.__isotp.send(packet.data)

    def __recv_worker(self) -> None:
        while True:
            isotp_msg = self.__isotp.recv(block=True, timeout=1.0)
            if isotp_msg is None:
                continue
            self.__recv_packet_queue.put(RecvPacket(self.__addr, isotp_msg))

    def __my_rxfn(self, timeout: float) -> Optional[isotp.CanMessage]:
        try:
            return self.__recv_msg_queue.get(block=True, timeout=timeout)
        except queue.Empty:
            return None


class NodeRegistry:
    MAX_NODES = 254

    def __init__(
            self,
            send_func: Callable[[isotp.CanMessage], None],
            recv_packet_queue: queue.Queue[RecvPacket]) -> None:
        self.__nodes: list[Node] = []
        self.__send_func = send_func
        self.__recv_packet_queue = recv_packet_queue

    def add_node(self, node_mac: NodeMac) -> Node:
        for node in self.__nodes:
            if node.mac == node_mac:
                raise RuntimeError(f"Node with MAC {node_mac} already exists.")
        if len(self.__nodes) >= self.MAX_NODES:
            raise RuntimeError("No more addresses. Overwriting old nodes is not implemented yet.")
        n = Node(node_mac, self.__get_next_node_addr(), self.__send_func, self.__recv_packet_queue)
        self.__nodes.append(n)
        return n

    def find_node_by_mac(self, node_mac: NodeMac) -> Node | None:
        for node in self.__nodes:
            if node.mac == node_mac:
                return node
        return None

    def find_node_by_addr(self, node_addr: int) -> Node | None:
        assert MIN_NODE_ADDR <= node_addr <= MAX_NODE_ADDR
        if node_addr > len(self.__nodes):
            return None
        return self.__nodes[node_addr - MIN_NODE_ADDR]

    def __get_next_node_addr(self) -> int:
        return len(self.__nodes) + MIN_NODE_ADDR
