#include "plat/sys_plat.h"
#include <iostream>


int main (int argc, char *argv[]) {
    const char netdev0_ip[] = "172.21.167.249";
    const uint8_t netdev0_hwaddr[] = {0x00, 0x15, 0x5d, 0xfa, 0x9f, 0x65};
    pcap_t* pcap = pcap_device_open(netdev0_ip, netdev0_hwaddr);

    std::string buffer;
    buffer.resize(1024);
    uint32_t counter = 0;

    char errbuf[1024] = {0};
    if (pcap_getnonblock(pcap, errbuf) == -1) {
        std::cerr << "Non-block mode error: " << errbuf << std::endl;
    }

    while (pcap) {
        for (int i = 0; i < buffer.size(); ++i) {
            buffer[i] = i;
        }

        struct pcap_pkthdr* pkthdr;
        const uint8_t* pkt_data;

        plat_printf("begin test: %d\n", counter++);
        if (pcap_next_ex(pcap, &pkthdr, &pkt_data) != 1) {
            std::cout << "---" << std::endl;
            continue;
        }

        int len = std::min(pkthdr->len, (uint32_t)buffer.size());
        memcpy(&buffer[0], pkt_data, len);
        buffer[0] = 9;
        buffer[1] = 9;

        if (pcap_inject(pcap, buffer.c_str(), len) == PCAP_ERROR) {
            plat_printf("pcap send: send packet failed %s\n", pcap_geterr(pcap));
            break;
        }

        usleep(500000);
    }


    return 0;
}

