import unittest

import node_mac


class TestNodeMac(unittest.TestCase):

    def test_add(self) -> None:
        n1 = node_mac.NodeMac(b'\x01\x02\x03\x04\x05\x06')
        n2 = node_mac.NodeMac(b'\x01\x02\x03\x04\x05\x06')
        n3 = node_mac.NodeMac(b'\x01\x02\x03\x04\x05\x07')
        self.assertEqual(n1, n2)
        self.assertNotEqual(n1, n3)

    def test_to_str(self) -> None:
        n = node_mac.NodeMac(b'\xFF\x02\x03\x04\x05\x06')
        self.assertEqual(str(n), 'FF:02:03:04:05:06')


if __name__ == '__main__':
    unittest.main()
