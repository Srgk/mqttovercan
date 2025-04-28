from enum import Enum

import can

from node_mac import NodeMac

ADDRESS_BROADCAST = 0xFF
ADDRESS_MASTER = 0x00


class MsgType(Enum):
    ISOTP = 0
    ADDRESS_REQUEST = 5
    ADDRESS_RESPONSE = 6
    UNKNOWN = 99


def make_can_id(t: MsgType, src: int, dst: int) -> int:
    return t.value << 16 | src << 8 | dst


class _MsgBase:
    def __init__(self, can_msg: can.Message) -> None:
        assert can_msg.is_extended_id
        assert not can_msg.is_error_frame
        assert not can_msg.is_remote_frame
        self.can_msg = can_msg

    @property
    def type(self) -> MsgType:
        i = (self.can_msg.arbitration_id >> 16) & 0x07
        return MsgType(i) if i in MsgType else MsgType.UNKNOWN

    @property
    def src_addr(self) -> int:
        return (self.can_msg.arbitration_id >> 8) & 0xFF

    @property
    def dst_addr(self) -> int:
        return self.can_msg.arbitration_id & 0xFF


class _MsgAddressRequest(_MsgBase):
    def __init__(self, can_msg: can.Message) -> None:
        super().__init__(can_msg)
        assert self.type == MsgType.ADDRESS_REQUEST
        assert can_msg.dlc == 6

    @property
    def node_mac(self) -> NodeMac:
        return NodeMac(self.can_msg.data[:6])


class Msg(_MsgBase):
    def __init__(self, can_msg: can.Message) -> None:
        super().__init__(can_msg)

    @property
    def as_address_request(self) -> _MsgAddressRequest:
        return _MsgAddressRequest(self.can_msg)


def make_address_response(status_code: int, new_address: int, node_mac: NodeMac) -> Msg:
    assert 0 <= status_code <= 0xFF
    assert ADDRESS_MASTER < new_address < ADDRESS_BROADCAST
    return Msg(can.Message(
        arbitration_id=make_can_id(MsgType.ADDRESS_RESPONSE, ADDRESS_MASTER, ADDRESS_BROADCAST),
        is_extended_id=True,
        dlc=8,
        data=node_mac.bytes + bytes([status_code]) + bytes([new_address])
    ))


def make_address_request_request(node_address: int) -> Msg:
    assert ADDRESS_MASTER < node_address < ADDRESS_BROADCAST
    return Msg(can.Message(
        arbitration_id=make_can_id(MsgType.ADDRESS_REQUEST, ADDRESS_MASTER, node_address),
        is_extended_id=True,
        dlc=0)
    )
