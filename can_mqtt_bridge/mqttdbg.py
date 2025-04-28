from typing import Tuple


def decode_remaining_length(buffer: bytes, pos: int) -> Tuple[int, int]:
    """Decode the variable-length 'remaining length' field from the MQTT fixed header.

    Args:
        buffer (bytes): The MQTT message buffer.
        pos (int): The current position in the buffer.

    Returns:
        Tuple[int, int]: The remaining length and the new position in the buffer.
    """
    multiplier = 1
    value = 0
    while True:
        if pos >= len(buffer):
            raise ValueError("Malformed remaining length")
        byte = buffer[pos]
        pos += 1
        value += (byte & 127) * multiplier
        if byte & 128 == 0:
            break
        multiplier *= 128
        if multiplier > 128 * 128 * 128:
            raise ValueError("Malformed remaining length")
    return value, pos


def read_string(buffer: bytes, pos: int) -> Tuple[str, int]:
    """Read a UTF-8 encoded string prefixed with a 2-byte length from the buffer.

    Args:
        buffer (bytes): The MQTT message buffer.
        pos (int): The current position in the buffer.

    Returns:
        Tuple[str, int]: The string and the new position in the buffer.
    """
    length = int.from_bytes(buffer[pos:pos + 2], 'big')
    pos += 2
    string = buffer[pos:pos + length].decode('utf-8')
    pos += length
    return string, pos


def read_uint16(buffer: bytes, pos: int) -> Tuple[int, int]:
    """Read a 2-byte unsigned integer (big-endian) from the buffer.

    Args:
        buffer (bytes): The MQTT message buffer.
        pos (int): The current position in the buffer.

    Returns:
        Tuple[int, int]: The integer value and the new position in the buffer.
    """
    value = int.from_bytes(buffer[pos:pos + 2], 'big')
    pos += 2
    return value, pos


def print_mqtt_message(buffer: bytes) -> None:
    """Parse an MQTT message from a bytes buffer and print it in human-readable form.

    Args:
        buffer (bytes): The MQTT message buffer.
    """
    # Define MQTT message type names
    message_types = {
        1: "CONNECT",
        2: "CONNACK",
        3: "PUBLISH",
        4: "PUBACK",
        5: "PUBREC",
        6: "PUBREL",
        7: "PUBCOMP",
        8: "SUBSCRIBE",
        9: "SUBACK",
        10: "UNSUBSCRIBE",
        11: "UNSUBACK",
        12: "PINGREQ",
        13: "PINGRESP",
        14: "DISCONNECT",
    }

    # Start parsing the fixed header
    pos = 0
    first_byte = buffer[pos]
    pos += 1
    message_type = first_byte >> 4  # Upper 4 bits are the message type
    flags = first_byte & 0x0F  # Lower 4 bits are flags
    remaining_length, pos = decode_remaining_length(buffer, pos)

    # Print the message type
    print(f"Message Type: {message_types.get(message_type, 'Unknown')}")

    # Parse and print based on message type
    if message_type == 1:  # CONNECT
        # Variable header
        protocol_name, pos = read_string(buffer, pos)
        protocol_level = buffer[pos]
        pos += 1
        connect_flags = buffer[pos]
        pos += 1
        keep_alive, pos = read_uint16(buffer, pos)

        # Payload fields
        client_id, pos = read_string(buffer, pos)
        will_topic = None
        will_message = None
        if connect_flags & 0x04:  # Will Flag
            will_topic, pos = read_string(buffer, pos)
            will_message, pos = read_string(buffer, pos)
        username = None
        if connect_flags & 0x80:  # User Name Flag
            username, pos = read_string(buffer, pos)
        password = None
        if connect_flags & 0x40:  # Password Flag
            password, pos = read_string(buffer, pos)

        # Print CONNECT fields
        print(f"Protocol Name: {protocol_name}")
        print(f"Protocol Level: {protocol_level}")
        print(f"Connect Flags: {bin(connect_flags)}")
        print(f"Keep Alive: {keep_alive}")
        print(f"Client ID: {client_id}")
        if will_topic:
            print(f"Will Topic: {will_topic}")
            print(f"Will Message: {will_message}")
        if username:
            print(f"Username: {username}")
        if password:
            print(f"Password: {password}")

    elif message_type == 3:  # PUBLISH
        # Extract flags
        dup = (flags & 0x08) >> 3
        qos = (flags & 0x06) >> 1
        retain = flags & 0x01

        # Variable header
        topic_name, pos = read_string(buffer, pos)
        packet_id = None
        if qos > 0:
            packet_id, pos = read_uint16(buffer, pos)

        # Payload
        payload = buffer[pos:]  # Assuming buffer contains exactly one message

        # Print PUBLISH fields
        print(f"DUP: {dup}")
        print(f"QoS: {qos}")
        print(f"RETAIN: {retain}")
        print(f"Topic Name: {topic_name}")
        if packet_id:
            print(f"Packet Identifier: {packet_id}")
        print(f"PayloadB: [{len(payload)}] {payload.hex()}")
        print(f"PayloadS: {payload.decode('utf-8', errors='replace')}")
    elif message_type == 8:  # SUBSCRIBE
        # Variable header
        packet_id, pos = read_uint16(buffer, pos)
        print(f"Packet Identifier: {packet_id}")

        # Payload - list of topic filters and requested QoS
        print("Subscriptions:")
        while pos < len(buffer):
            topic, pos = read_string(buffer, pos)
            qos = buffer[pos] & 0x03  # QoS is the lower 2 bits
            pos += 1
            print(f"  Topic: {topic}, Requested QoS: {qos}")
    elif message_type == 12:  # PINGREQ
        if remaining_length != 0:
            print("Invalid PINGREQ. Remaining length should be 0.")
