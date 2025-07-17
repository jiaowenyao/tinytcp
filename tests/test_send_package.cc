#include "plat/sys_plat.h"
#include <iostream>


int main (int argc, char *argv[]) {
    const char netdev0_ip[] = "172.29.130.120";
    const uint8_t netdev0_hwaddr[] = {0x00, 0x15, 0x5d, 0x3e, 0x8f, 0x1e};
    pcap_t* pcap = pcap_device_open(netdev0_ip, netdev0_hwaddr);

    int cnt = 0;
    std::string buffer;
    while (pcap) {
        // std::cout << "begin test: " << cnt++ << std::endl;
        buffer = "begin test: " + std::to_string(cnt++);

        std::cout << buffer << std::endl;

        if (pcap_inject(pcap, buffer.c_str(), buffer.size()) == PCAP_ERROR) {
            std::cout << "pcap_inject error: " << pcap_geterr(pcap) << std::endl;
            break;
        }

        usleep(100);
    }


    return 0;
}

