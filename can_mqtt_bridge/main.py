import logging
import sys
from io import TextIOWrapper

import can
import os

import can_tcp_bridge
import isotp_can_server
import mqttdbg
from can_server import CanServer
from packet import SendPacket, RecvPacket


def make_logger() -> logging.Logger:
    logger = logging.getLogger(__name__)
    logger.setLevel(logging.DEBUG)

    stdout_handler = logging.StreamHandler(sys.stdout)
    stdout_handler.setLevel(logging.DEBUG)
    assert isinstance(stdout_handler.stream, TextIOWrapper)
    stdout_handler.stream.reconfigure(line_buffering=True)
    logger.addHandler(stdout_handler)

    return logger


def check_slcan_dongle() -> can.BusABC:
    # Attempt to initialize the SLCAN interface
    # Adjust 'slcan0' to match your system's interface name (e.g., 'COM3' on Windows)
    interface = os.getenv('CAN_INTERFACE', 'slcan')
    channel = os.getenv('CAN_CHANNEL', '/dev/ttyACM0')
    bitrate = int(os.getenv('CAN_BITRATE', 20000))
    bus = can.interface.Bus(interface=interface, channel=channel, bitrate=bitrate)
    print("CAN dongle detected and initialized successfully!")
    return bus


class DgbShim:
    def __init__(self, srv: CanServer):
        self.srv = srv

    def send_packet(self, packet: SendPacket):
        print("-----------------")
        print(f"Server -> Node {packet.dst_addr}")
        mqttdbg.print_mqtt_message(packet.data)
        self.srv.send_packet(packet)

    def recv_packet(self) -> RecvPacket:
        p = self.srv.recv_packet()
        print("-----------------")
        print(f"Node {p.src_addr} -> Server")
        mqttdbg.print_mqtt_message(p.data)
        return p


def app_main(bus: can.BusABC) -> None:
    log = make_logger()
    can_srv = isotp_can_server.IsotpCanServer(bus, log)
    can_srv_shimmed = DgbShim(can_srv)

    host = os.getenv('MQTT_HOST', '192.168.0.62');
    port = int(os.getenv('MQTT_PORT', '1833'));
    bridge = can_tcp_bridge.CanTcpBridge(can_srv_shimmed, host, port, log)
    bridge.run()


if __name__ == "__main__":
    bus = check_slcan_dongle()
    if bus:
        app_main(bus)
        bus.shutdown()
