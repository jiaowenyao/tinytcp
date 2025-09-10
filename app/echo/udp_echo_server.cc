
#include "udp_echo_server.h"


#include <string.h>
#include <stdio.h>
#include <yaml-cpp/yaml.h>
#include "src/config.h"
#include "src/net/ipaddr.h"
#include "src/net/net.h"

#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>

int udp_echo_server(int port) {
    // 打开套接字
    int s = socket(AF_INET, SOCK_DGRAM, 0);
    if (s < 0) {
        printf("open socket error\n");
        goto end;
    }

    struct sockaddr_in local_addr;
    local_addr.sin_family = AF_INET;
    local_addr.sin_port = htons(port);
    local_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    if (bind(s, (struct sockaddr *)&local_addr, sizeof(local_addr)) < 0) {
        printf("bind error\n");
        goto end;
    }

    // 绑定本地端口
    while (1) {
        struct sockaddr_in client_addr;
        char buf[256];

        // 接受来自客户端的数据包
        socklen_t addr_len = sizeof(client_addr);
        ssize_t size = recvfrom(s, buf, sizeof(buf), 0, (struct sockaddr *)&client_addr, &addr_len);
        if (size < 0) {
            printf("recv from error\n");
            goto end;
        }

        // 加上下面的打印之后，由于比较费时，会导致UDP包来不及接收被被丢弃
        printf("udp echo server:connect ip: %s, port: %d\n", inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port));

        // 发回去
        size = sendto(s, buf, size, 0, (struct sockaddr *)&client_addr, addr_len);
        if (size < 0) {
            printf("sendto error\n");
            goto end;
        }
    }
end:
    if (s >= 0) {
        close(s);
    }

    return 0;
}


int main() {

    YAML::Node root = YAML::LoadFile("/home/jwy/workspace/tinytcp/config/log.yaml");
    tinytcp::Config::load_from_yaml(root);

    // uint8_t hwaddr[6] = {0x00, 0x15, 0x5d, 0xc6, 0x57, 0x27};
    // tinytcp::ipaddr_t _ip;
    // tinytcp::pcap_data_t pcap_data {
    //     .ip = "192.168.163.110",
    //     .hwaddr = hwaddr,
    //     .netmask = "255.255.240.0",
    //     .gateway = "192.168.160.1"
    // };
    // auto p = tinytcp::ProtocolStackMgr::get_instance();
    // auto eth0 = p->get_network()->netif_open("eth0", &pcap_data);

    udp_echo_server(1001);

    return 0;
}


