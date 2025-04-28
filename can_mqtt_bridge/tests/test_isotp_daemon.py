import functools
import logging
import queue
import sys
import unittest
from typing import Tuple, Optional

import can
import isotp

from fake_bus import FakeBus
from msg import MsgType
from packet import SendPacket


def make_logger() -> logging.Logger:
    logger = logging.getLogger(__name__)
    logger.setLevel(logging.DEBUG)

    stdout_handler = logging.StreamHandler(sys.stdout)
    stdout_handler.setLevel(logging.DEBUG)
    logger.addHandler(stdout_handler)

    return logger


def make_can_msg(t: MsgType, src: int, dst: int, data: bytes) -> can.Message:
    return can.Message(
        arbitration_id=t.value << 16 | src << 8 | dst,
        data=data,
        is_extended_id=True)


def parse_can_id(can_id: int) -> Tuple[MsgType, int, int]:
    return MsgType((can_id >> 16) & 0x07), (can_id >> 8) & 0xFF, can_id & 0xFF


def __rxfn(bus: FakeBus, timeout: float) -> Optional[isotp.CanMessage]:
    try:
        m = bus.node_recv(timeout)
        return isotp.CanMessage(arbitration_id=m.arbitration_id, data=m.data, dlc=m.dlc, extended_id=m.is_extended_id)
    except queue.Empty:
        return None


def __txfn(bus: FakeBus, msg: isotp.CanMessage) -> None:
    bus.node_send(
        can.Message(arbitration_id=msg.arbitration_id, data=msg.data, dlc=msg.dlc, is_extended_id=msg.is_extended_id))


def make_node_isotp_transport(node_addr: int, bus: FakeBus) -> isotp.TransportLayer:
    isotp_addr = isotp.Address(isotp.AddressingMode.Normal_29bits, rxid=node_addr, txid=(node_addr << 8))
    params = {
        'blocking_send': True
    }
    transport = isotp.TransportLayer(
        rxfn=functools.partial(__rxfn, bus),
        txfn=functools.partial(__txfn, bus),
        address=isotp_addr,
        params=params)
    transport.start()
    return transport


class TestIsotpDaemon(unittest.TestCase):
    def test_isotp_daemon_address_request(self) -> None:
        can_bus = FakeBus()
        daemon = isotp_daemon.IsotpCanServer(bus=can_bus, logger=make_logger())
        # Send address request
        can_bus.node_send(make_can_msg(MsgType.ADDRESS_REQUEST, 0xFF, 0x00, b'\x01\x02\x03\x04\x05\x06'))
        # Receive address response
        m = can_bus.node_recv()
        t, src, dst = parse_can_id(m.arbitration_id)
        self.assertEqual(MsgType.ADDRESS_RESPONSE, t)
        self.assertEqual(src, 0x00)
        self.assertEqual(dst, 0xFF)
        self.assertEqual(m.data[7], 0x01)  # The node new address

    def test_isotp_send_receive(self) -> None:
        can_bus = FakeBus()
        daemon = isotp_daemon.IsotpCanServer(bus=can_bus, logger=make_logger())
        can_bus.node_send(make_can_msg(MsgType.ADDRESS_REQUEST, 0xFF, 0x00, b'\x01\x02\x03\x04\x05\x06'))
        m = can_bus.node_recv()
        node_addr = m.data[7]
        node_transport = make_node_isotp_transport(node_addr, can_bus)
        data = b'X' * 2048
        # Daemon sends data to node
        daemon.send_packet(SendPacket(dst_addr=node_addr, data=data))
        recv_data = node_transport.recv(block=True)
        self.assertEqual(data, recv_data)
        # Node sends data to daemon
        node_transport.send(data)
        recv_packet = daemon.recv_packet()
        self.assertEqual(data, recv_packet.data)
