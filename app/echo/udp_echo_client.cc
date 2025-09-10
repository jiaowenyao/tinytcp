#include "udp_echo_client.h"


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

/**
 * @brief udp回显客户端程序
 */
int udp_echo_client_start(const char* ip, int port) {
    printf("udp echo client, ip: %s, port: %d\n", ip, port);
    printf("Enter quit to exit\n");

    // 创建套接字，使用流式传输，即tcp
    int s = tinytcp::socket(AF_INET, SOCK_DGRAM, 0);
    if (s < 0) {
        printf("open socket error");
        goto end;
    }

    // 连接的服务地址和端口
    struct tinytcp::sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = tinytcp::inet_addr(ip);
    server_addr.sin_port = tinytcp::htons(port);

    // connect(s, (const struct sockaddr*)&server_addr, sizeof(server_addr));

    printf(">>");
    char buf[128];
    while (fgets(buf, sizeof(buf), stdin) != NULL) {
        if (strncmp(buf, "quit", 4) == 0) {
            break;
        }
 
        // 将数据写到服务器中，不含结束符
        // 在第一次发送前会自动绑定到本地的某个端口和地址中
        size_t total_len = strlen(buf);
        // ssize_t size = send(s, buf, total_len, 0);
        ssize_t size = tinytcp::sendto(s, buf, total_len, 0, (struct tinytcp::sockaddr *)&server_addr, sizeof(server_addr));
        if (size < 0) {
            printf("send error");
            goto end;
        }

        // continue;

        memset(buf, 0, sizeof(buf));
        // size = recv(s, buf, sizeof(buf), 0);
        struct tinytcp::sockaddr_in remote_addr;
        tinytcp::socklen_t addr_len;
        size = tinytcp::recvfrom(s, buf, sizeof(buf), 0, (struct tinytcp::sockaddr*)&remote_addr, &addr_len);
        if (size < 0) {
            printf("recv error");
            goto end;
        }
        buf[sizeof(buf) - 1] = '\0';

        // 在屏幕上显示出来
        printf("%s", buf);
        printf(">>");
    }

end:
    if (s >= 0) {
        close(s);
    }
    return -1;
}


int main() {

    YAML::Node root = YAML::LoadFile("/home/jwy/workspace/tinytcp/config/log.yaml");
    tinytcp::Config::load_from_yaml(root);

    uint8_t hwaddr[6] = {0x00, 0x15, 0x5d, 0xc6, 0x57, 0x27};
    tinytcp::ipaddr_t _ip;
    tinytcp::pcap_data_t pcap_data {
        .ip = "192.168.163.110",
        .hwaddr = hwaddr,
        .netmask = "255.255.240.0",
        .gateway = "192.168.160.1"
    };
    auto p = tinytcp::ProtocolStackMgr::get_instance();
    auto eth0 = p->get_network()->netif_open("eth0", &pcap_data);

    udp_echo_client_start("192.168.1.14", 1000);

    return 0;
}


