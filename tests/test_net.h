#include <stdio.h>
#include <pcap/pcap.h>
#include <arpa/inet.h>
#include <string.h>
#include <linux/if_packet.h>


// 通过网卡名称获取IP和MAC地址
static int get_network_info_by_pcap(const char *ifname, uint32_t *ip, uint8_t *mac) {
    pcap_if_t *alldevs;
    pcap_if_t *device;
    char errbuf[PCAP_ERRBUF_SIZE];
    int found = 0;

    // 获取所有网卡设备
    if (pcap_findalldevs(&alldevs, errbuf) == -1) {
        fprintf(stderr, "获取网卡列表失败: %s\n", errbuf);
        return -1;
    }

    // 遍历查找指定网卡
    for (device = alldevs; device != NULL; device = device->next) {
        if (strcmp(device->name, ifname) == 0) {
            found = 1;
            break;
        }
    }

    if (!found) {
        fprintf(stderr, "未找到网卡: %s\n", ifname);
        pcap_freealldevs(alldevs);
        return -1;
    }

    // 获取MAC地址
    if (device->addresses != NULL) {
        // 第一个地址通常是MAC地址
        struct pcap_addr *addr = device->addresses;
        if (addr->addr != NULL && addr->addr->sa_family == AF_PACKET) {
            struct sockaddr_ll *sll = (struct sockaddr_ll *)addr->addr;
            if (mac != NULL && sll->sll_halen == 6) {
                memcpy(mac, sll->sll_addr, 6);
            }
        }
    }

    // 获取IP地址
    for (pcap_addr_t *addr = device->addresses; addr != NULL; addr = addr->next) {
        if (addr->addr != NULL && addr->addr->sa_family == AF_INET) {
            struct sockaddr_in *sin = (struct sockaddr_in *)addr->addr;
            if (ip != NULL) {
                *ip = sin->sin_addr.s_addr;
            }
            break; // 只取第一个IPv4地址
        }
    }

    pcap_freealldevs(alldevs);
    return 0;
}

