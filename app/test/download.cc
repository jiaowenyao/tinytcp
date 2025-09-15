#include <stdio.h>
#include <string.h>
#include "yaml-cpp/yaml.h"
#include "src/net/net.h"
#include "src/config.h"
#include "src/api/net_api.h"

/**
 * @brief 简单的从对方客户机进行下载，以便测试TCP接收
 */
void download_test(const char * filename, int port, const char* host_ip="192.168.1.14") {
    // printf("try to download %s from %s: %d\n", filename, friend0_ip, port);

    // 创建服务器套接字，使用IPv4，数据流传输
	int sockfd = tinytcp::socket(AF_INET, SOCK_STREAM, 0);
	if (sockfd < 0) {
		printf("create socket error\n");
        exit(1);
	}

    FILE * file = fopen(filename, "wb");
    if (file == (FILE *)0) {
        printf("open file failed.\n");
        return;
    }

    // 绑定本地地址
    struct tinytcp::sockaddr_in addr;
	memset(&addr, 0, sizeof(addr));
	addr.sin_family = AF_INET;
	addr.sin_port = tinytcp::htons(port);
	addr.sin_addr.s_addr = tinytcp::inet_addr(host_ip);
    if (tinytcp::connect(sockfd, (const struct tinytcp::sockaddr *)&addr, sizeof(addr)) < 0) {
		printf("connect error\n");
        exit(1);
    }

    char buf[8192];
    ssize_t total_size = 0;
    int rcv_size;
    while ((rcv_size = tinytcp::recv(sockfd, buf, sizeof(buf), 0)) > 0) {
        fwrite(buf, 1, rcv_size, file);
        fflush(file);
        printf(".");
        total_size += rcv_size;
    }
    if (rcv_size < 0) {
        // 接收完毕
        printf("rcv file size: %d\n", (int)total_size);
        goto failed;
    }
    printf("rcv file size: %d\n", (int)total_size);
    printf("rcv file ok\n");
    tinytcp::close(sockfd);
    fclose(file);
    return;

failed:
    printf("rcv file error\n");
    // close(sockfd);
    tinytcp::close(sockfd);
    if (file) {
        fclose(file);
    }
    return;
}


int main() {
    YAML::Node root = YAML::LoadFile("/home/jwy/workspace/tinytcp/config/log.yaml");
    tinytcp::Config::load_from_yaml(root);


    uint8_t hwaddr[6] = {0x00, 0x15, 0x5d, 0xc6, 0x5e, 0x7e};
    tinytcp::ipaddr_t _ip;
    tinytcp::pcap_data_t pcap_data {
        .ip = "192.168.168.207",
        .hwaddr = hwaddr,
        .netmask = "255.255.240.0",
        .gateway = "192.168.160.1"
    };
    auto p = tinytcp::ProtocolStackMgr::get_instance();
    auto eth0 = p->get_network()->netif_open("eth0", &pcap_data);

    download_test("test.txt", 10010);

    return 0;
}




