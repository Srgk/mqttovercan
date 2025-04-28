class NodeMac:
    def __init__(self, mac: bytes) -> None:
        assert len(mac) == 6
        assert all(0 <= b <= 0xFF for b in mac)
        self.mac = bytes(mac)

    @property
    def bytes(self) -> bytes:
        return self.mac

    def __eq__(self, other: object) -> bool:
        assert isinstance(other, NodeMac)
        return self.mac == other.mac

    def __str__(self) -> str:
        return ':'.join(f'{b:02X}' for b in self.mac)
