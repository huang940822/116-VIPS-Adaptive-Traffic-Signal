import socket
import struct
import time

# 設定目標 IP 和端口
target_ip = '192.168.100.2'
target_port = 20204

# 創建 UDP socket
sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)

# 初始化封包內容
cmd = 2   
status = 0  
imgid2 = 234
imgid3 = 235
imgid4 = 236

try:
    while True:
        # 封包的內容: packet_length, cmd, 2, cmsid, status, imgid
        packet = struct.pack('BBBBBB', 6, cmd, 2, 2, status, imgid2)
        sock.sendto(packet, (target_ip, target_port))
        print(f"Packet sent: cmd={cmd}, 2, cmsid={2}, status={status}, imgid={imgid2}")

        packet = struct.pack('BBBBBB', 6, cmd, 2, 3, status, imgid3)
        sock.sendto(packet, (target_ip, target_port))
        print(f"Packet sent: cmd={cmd}, 2, cmsid={3}, status={status}, imgid={imgid3}")

        packet = struct.pack('BBBBBB', 6, cmd, 2, 4, status, imgid4)
        sock.sendto(packet, (target_ip, target_port))
        print(f"Packet sent: cmd={cmd}, 2, cmsid={4}, status={status}, imgid={imgid4}")
        # 每秒傳送一次
        time.sleep(1)

        # 更新封包中的某些數值（根據需要）
        # imgid = (imgid + 1) % 256

finally:
    # 關閉 socket
    sock.close()
