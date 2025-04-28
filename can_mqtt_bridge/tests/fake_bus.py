import queue
from typing import Optional, Tuple

import can
from can import BusABC


class FakeBus(BusABC):
    def __init__(self) -> None:
        self._recv_queue: queue.Queue[can.Message] = queue.Queue()
        self._sent_queue: queue.Queue[can.Message] = queue.Queue()
        super().__init__(channel="fake")

    def send(self, msg: can.Message, timeout: Optional[float] = None) -> None:
        self._sent_queue.put(msg)
        # print("daemon sent - ", hex(msg.arbitration_id))

    def _recv_internal(self, timeout: Optional[float]) -> Tuple[Optional[can.Message], bool]:
        # print("daemon receiving ...")
        try:
            m = self._recv_queue.get(block=True, timeout=timeout)
            # print("daemon recv - ", hex(m.arbitration_id))
            return m, False
        except queue.Empty:
            return None, False

    def node_send(self, msg: can.Message) -> None:
        self._recv_queue.put(msg)
        # print("node sent - ", hex(msg.arbitration_id))

    def node_recv(self, timeout: float | None = None) -> can.Message:
        # print("node receiving ...")
        m = self._sent_queue.get(block=True, timeout=timeout)
        # print("node recv - ", hex(m.arbitration_id))
        return m
