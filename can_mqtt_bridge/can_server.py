from typing import Protocol

from packet import RecvPacket, SendPacket


class CanServer(Protocol):
    def recv_packet(self) -> RecvPacket:
        pass

    def send_packet(self, p: SendPacket) -> None:
        pass
