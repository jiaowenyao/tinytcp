
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

#include "src/api/net_api.h"

int tcp_echo_server(int port) {
    // 打开套接字
    int s = tinytcp::socket(AF_INET, SOCK_STREAM, 0);
    if (s < 0) {
        printf("open socket error\n");
        goto end;
    }

    struct tinytcp::sockaddr_in local_addr;
    local_addr.sin_family = AF_INET;
    local_addr.sin_port = tinytcp::htons(port);
    local_addr.sin_addr.s_addr = 0;
    if (tinytcp::bind(s, (struct tinytcp::sockaddr *)&local_addr, sizeof(local_addr)) < 0) {
        printf("bind error\n");
        goto end;
    }

    while (1) {}

    while (1) {
        struct tinytcp::sockaddr_in client_addr;
        char buf[256];

        // 接受来自客户端的数据包
        socklen_t addr_len = sizeof(client_addr);
        ssize_t size = tinytcp::recvfrom(s, buf, sizeof(buf), 0, (struct tinytcp::sockaddr *)&client_addr, (tinytcp::socklen_t*)&addr_len);
        if (size < 0) {
            printf("recv from error\n");
            goto end;
        }

        // 加上下面的打印之后，由于比较费时，会导致UDP包来不及接收被被丢弃
        printf("udp echo server:connect ip: %s, port: %d\n", tinytcp::inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port));

        // 发回去
        size = tinytcp::sendto(s, buf, size, 0, (struct tinytcp::sockaddr *)&client_addr, addr_len);
        if (size < 0) {
            printf("sendto error\n");
            goto end;
        }
    }
end:
    if (s >= 0) {
        tinytcp::close(s);
    }

    return 0;
}


int main() {

    YAML::Node root = YAML::LoadFile("/home/jwy/workspace/tinytcp/config/log.yaml");
    tinytcp::Config::load_from_yaml(root);

    uint8_t hwaddr[6] = {0x00, 0x15, 0x5d, 0xc6, 0x5d, 0xc0};
    tinytcp::ipaddr_t _ip;
    tinytcp::pcap_data_t pcap_data {
        .ip = "192.168.166.24",
        .hwaddr = hwaddr,
        .netmask = "255.255.240.0",
        .gateway = "192.168.160.1"
    };
    auto p = tinytcp::ProtocolStackMgr::get_instance();
    auto eth0 = p->get_network()->netif_open("eth0", &pcap_data);

    tcp_echo_server(1001);

    return 0;
}


