import logging
import queue
import threading

import can
import isotp

import msg
import msg as h42msg
import node
from packet import RecvPacket, SendPacket


class IsotpCanServer:
    def __init__(self, bus: can.BusABC, logger: logging.Logger) -> None:
        self.__bus = bus
        self.__logger = logger
        self.__packet_recv_queue: queue.Queue[RecvPacket] = queue.Queue()
        self.__node_registry = node.NodeRegistry(self.__my_txfn, self.__packet_recv_queue)
        self.__recv_worker_thread = threading.Thread(target=self.__recv_worker, daemon=True)
        self.__recv_worker_thread.start()

    def send_packet(self, packet: SendPacket) -> None:
        node = self.__node_registry.find_node_by_addr(packet.dst_addr)
        if node is None:
            raise ValueError(f"Node not found: {packet.dst_addr}")
        node.send_packet(packet)

    def recv_packet(self) -> RecvPacket:
        return self.__packet_recv_queue.get(block=True)

    def __recv_worker(self) -> None:
        while True:
            try:
                bus_msg = self.__bus.recv(1.0)
            except Exception as exc:
                self.__logger.warning(f"Error receiving CAN message: {exc}")
                continue
            # Validate the received CAN message
            if bus_msg is None or bus_msg.is_remote_frame:
                continue
            if bus_msg.is_error_frame:
                self.__logger.warning("Error frame received")
                continue
            if not bus_msg.is_extended_id:
                self.__logger.warning("Unexpected standard ID CAN message")
                continue
            # Validate the received OverCAN message
            h42_msg = h42msg.Msg(bus_msg)
            if h42_msg.type == h42msg.MsgType.ADDRESS_REQUEST:
                self.__handle_address_request(h42_msg)
                continue
            if h42_msg.type != h42msg.MsgType.ISOTP:
                self.__logger.warning(f"Unexpected packet type: {h42_msg.type}")
                continue
            if h42_msg.dst_addr != h42msg.ADDRESS_MASTER:
                # Nodes should only send messages to the master
                self.__logger.warning(f"Unexpected destination address: {h42_msg.dst_addr}")
                continue
            src_node = self.__node_registry.find_node_by_addr(h42_msg.src_addr)
            if src_node is None:
                self.__handle_unknown_node(node_addr=h42_msg.src_addr)
                continue
            isotp_msg = isotp.CanMessage(arbitration_id=bus_msg.arbitration_id,
                                         data=bus_msg.data,
                                         dlc=bus_msg.dlc,
                                         extended_id=bus_msg.is_extended_id)

            # Clear out randomness and suppress the source address to match the mask in the isotp layer
            isotp_msg.arbitration_id &= ~0x1FE0FF00

            src_node.on_received_can_msg(isotp_msg)

    def __handle_address_request(self, m: h42msg.Msg) -> None:
        addr_req = m.as_address_request
        try:
            self.__logger.info(f"Address request from {addr_req.node_mac}")
            new_node = self.__node_registry.find_node_by_mac(addr_req.node_mac)
            if new_node is None:
                new_node = self.__node_registry.add_node(addr_req.node_mac)
                self.__logger.info(f"New node added: {new_node.addr}")
            else:
                self.__logger.info(f"Node already exists: {new_node.addr}")
            assert new_node is not None
            resp = h42msg.make_address_response(0, new_node.addr, addr_req.node_mac)
        except RuntimeError as e:
            self.__logger.error(e)
            resp = h42msg.make_address_response(1, 0, addr_req.node_mac)
        self.__bus.send(resp.can_msg)

    def __handle_unknown_node(self, node_addr: int) -> None:
        self.__logger.warning(f"Message from unknown node: {node_addr}. Requesting it to get a new address.")
        m = msg.make_address_request_request(node_address=node_addr)
        self.__bus.send(m.can_msg)

    def __my_txfn(self, msg: isotp.CanMessage) -> None:
        m = can.Message(
            arbitration_id=msg.arbitration_id,
            data=msg.data,
            dlc=msg.dlc,
            is_extended_id=msg.is_extended_id)
        self.__bus.send(m)
