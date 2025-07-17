import socket
try:
    s = socket.socket(socket.AF_PACKET, socket.SOCK_RAW)
    s.bind(("eth0", 0))
    s.send(b"\x00" * 64)  # 发送64字节的空数据包
    print("Raw socket send succeeded")
except Exception as e:
    print(f"Raw socket error: {e}")
