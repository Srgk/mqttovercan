class Packet:
    def __init__(self, data: bytes) -> None:
        if len(data) > 4095:
            raise ValueError("Packet longer than 4095 bytes")
        self.data = data

    @property
    def len(self) -> int:
        return len(self.data)


class RecvPacket(Packet):
    def __init__(self, src_addr: int, data: bytes) -> None:
        super().__init__(data)
        self.src_addr = src_addr


class SendPacket(Packet):
    def __init__(self, dst_addr: int, data: bytes) -> None:
        super().__init__(data)
        self.dst_addr = dst_addr
