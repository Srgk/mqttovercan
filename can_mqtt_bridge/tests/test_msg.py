import unittest

import can

import msg
from node_mac import NodeMac


class TestMsg(unittest.TestCase):
    def test_msg_access(self) -> None:
        m = msg.Msg(can.Message(
            arbitration_id=msg.MsgType.ADDRESS_REQUEST.value << 16 | msg.ADDRESS_BROADCAST,
            data=[1, 2, 3, 4, 5, 6],
            is_extended_id=True))
        m_addr_req = m.as_address_request
        self.assertEqual(m_addr_req.type, msg.MsgType.ADDRESS_REQUEST)
        self.assertEqual(m_addr_req.dst_addr, msg.ADDRESS_BROADCAST)
        self.assertEqual(m_addr_req.node_mac, NodeMac(b'\x01\x02\x03\x04\x05\x06'))

    def test_msg_access_fail(self) -> None:
        m = msg.Msg(can.Message(
            arbitration_id=msg.MsgType.ISOTP.value << 8 | msg.ADDRESS_BROADCAST,
            data=[1, 2, 3, 4, 5],
            is_extended_id=True))
        with self.assertRaises(Exception):
            _ = m.as_address_request


if __name__ == '__main__':
    unittest.main()
