import queue
import unittest

import isotp

import node
import node_mac


class TestNodeRegistry(unittest.TestCase):
    def fake_send_func(self, msg: isotp.CanMessage) -> None:
        pass

    def test_add_find(self) -> None:
        reg = node.NodeRegistry(self.fake_send_func, queue.Queue())
        mac1 = node_mac.NodeMac(b'\x01\x02\x03\x04\x05\x06')
        mac2 = node_mac.NodeMac(b'\xFF\x02\x03\x04\x05\x06')
        reg.add_node(mac1)
        found_node = reg.find_node_by_mac(mac1)
        self.assertIsInstance(found_node, node.Node)
        assert found_node is not None
        self.assertEqual(found_node.mac, mac1)
        self.assertIsNone(reg.find_node_by_mac(mac2))
        found_node2 = reg.find_node_by_addr(found_node.addr)
        assert found_node2 is not None
        self.assertEqual(found_node.mac, found_node2.mac)
        self.assertIsNone(reg.find_node_by_addr(42))

    def test_add_duplicate(self) -> None:
        reg = node.NodeRegistry(self.fake_send_func, queue.Queue())
        mac1 = node_mac.NodeMac(b'\x01\x02\x03\x04\x05\x06')
        reg.add_node(mac1)
        with self.assertRaises(RuntimeError):
            reg.add_node(mac1)


if __name__ == '__main__':
    unittest.main()
