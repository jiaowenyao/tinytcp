#!/usr/bin/env python3
import sys
sys.path.extend(['', '/home/jwy/anaconda3/lib/python312.zip', '/home/jwy/anaconda3/lib/python3.12', '/home/jwy/anaconda3/lib/python3.12/lib-dynload', '/home/jwy/anaconda3/lib/python3.12/site-packages'])

#!/usr/bin/env python3
from scapy.all import *
import subprocess
import re

def get_interface_mac(interface):
    """获取接口的MAC地址"""
    try:
        result = subprocess.run(['ip', 'link', 'show', interface], 
                              capture_output=True, text=True, check=True)
        mac_match = re.search(r'link/ether ([0-9a-f:]+)', result.stdout, re.IGNORECASE)
        if mac_match:
            return mac_match.group(1).lower()
        else:
            raise ValueError(f"无法找到 {interface} 的MAC地址")
    except subprocess.CalledProcessError:
        raise ValueError(f"无法获取接口 {interface} 的信息")

def send_arp_reply_to_eth0():
    """向eth0发送ARP回应，告知veth0的MAC地址"""
    # 获取两个接口的MAC地址
    eth0_mac = get_interface_mac("eth0")
    veth0_mac = get_interface_mac("veth0")
    
    print(f"eth0 MAC地址: {eth0_mac}")
    print(f"veth0 MAC地址: {veth0_mac}")
    
    # 假设veth0的IP地址是172.20.32.132（根据你的设置调整）
    veth0_ip = "192.168.200.1"
    eth0_ip = "172.20.32.132"
    
    # 构造ARP回应包
    # 这个包的意思是："veth0的IP(172.20.32.132)对应的MAC是veth0_mac"
    packet = Ether(dst=veth0_mac, src=eth0_mac) / ARP(
        op=2,
        psrc=eth0_ip,
        hwsrc=eth0_mac,
        pdst=veth0_ip,
        hwdst=veth0_mac
    )
    
    # print(f"发送ARP回应到eth0: {veth0_ip} is at {veth0_mac}")
    
    # 发送包（从eth0接口发送）
    sendp(packet, iface="eth0", verbose=True, count=3, inter=0.5)
    print("发送完成!")

if __name__ == "__main__":
    send_arp_reply_to_eth0()
