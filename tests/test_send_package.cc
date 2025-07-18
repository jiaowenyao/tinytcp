#include "plat/sys_plat.h"
#include <iostream>


int main (int argc, char *argv[]) {
    const char netdev0_ip[] = "172.21.167.249";
    const uint8_t netdev0_hwaddr[] = {0x00, 0x15, 0x5d, 0xfa, 0x9f, 0x65};
    pcap_t* pcap = pcap_device_open(netdev0_ip, netdev0_hwaddr);

    std::string buffer;
    buffer.resize(1024);
    uint32_t counter = 0;
    while (pcap) {
        plat_printf("begin test: %d\n", counter++);
        for (int i = 0; i < buffer.size(); ++i) {
            buffer[i] = i;
        }


        if (pcap_inject(pcap, buffer.c_str(), buffer.size()) == PCAP_ERROR) {
            plat_printf("pcap send: send packet failed %s\n", pcap_geterr(pcap));
            break;
        }

        usleep(500000);
    }


    return 0;
}

