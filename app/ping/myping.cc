#include <time.h>
#include <iomanip>
#include <cstdlib>
#include <yaml-cpp/yaml.h>
#include "ping.h"
#include "src/config.h"
#include "src/macro.h"
#include "src/log.h"
#include "src/util.h"
#include "src/endiantool.h"
#include "src/api/net_api.h"
#include "src/net/net.h"

static tinytcp::Logger::ptr g_logger = TINYTCP_LOG_ROOT();

void ping_run(ping_t& ping, const char* dest, const PingOptions& options) {
    static uint16_t start_id = PING_DEFAULT_ID;
    int s = tinytcp::socket(AF_INET, SOCK_RAW, IPPROTO_ICMP);
    TINYTCP_ASSERT2(s >= 0, "s=" + std::to_string(s) + ", error=" + strerror(errno));

    struct tinytcp::sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = tinytcp::inet_addr(dest);
    addr.sin_port = 0;

    int fill_size = std::min(options.packet_size, (uint32_t)PING_BUFFER_SIZE);
    for (uint32_t i = 0; i < fill_size; ++i) {
        ping.req.buf[i] = i;
    }

    uint32_t total_size = sizeof(icmpv4_hdr_t) + fill_size;
    for (uint32_t i = 0, seq = 0; i < options.count; ++i, ++seq) {
        ping.req.echo_hdr.type = 8;
        ping.req.echo_hdr.code = 0;
        ping.req.echo_hdr.checksum = 0;
        ping.req.echo_hdr.id = tinytcp::host_to_net(start_id);
        ping.req.echo_hdr.seq = seq;

        ping.req.echo_hdr.checksum = tinytcp::checksum16(0, &ping.req, total_size, 0, 1);
        int send_size = tinytcp::sendto(s, (const char*)&ping.req, total_size, 0, (const struct tinytcp::sockaddr*)&addr, sizeof(addr));
        if (send_size < 0) {
            TINYTCP_LOG_ERROR(g_logger) << "sendto error:" << strerror(errno);
            break;
        }

        clock_t begin_time = clock();
        // 设置超时时间
        tinytcp::timeval timeout;
        timeout.tv_sec = options.timeout / 1000;
        timeout.tv_usec = (options.timeout % 1000) * 1000;
        tinytcp::setsockopt(s, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(tinytcp::timeval));
        // fd_set readfds;
        // FD_ZERO(&readfds);
        // FD_SET(s, &readfds);
        // int select_ret = select(s + 1, &readfds, NULL, NULL, &timeout);
        //
        // memset(&ping.reply, 0, sizeof(ping.reply));
        // int recv_size;
        // if (select_ret == 0) {
        //     // 超时
        //     std::cout << "Request timeout for icmp_seq " << seq << std::endl;
        //     usleep(options.interval * 1000);
        //     continue;
        // } else if (select_ret == -1) {
        //     // 错误
        //     TINYTCP_LOG_ERROR(g_logger) << "select error: " << strerror(errno);
        //     usleep(options.interval * 1000);
        //     continue;
        // }

        // 有数据可读
        int recv_size;
        memset(&ping.reply, 0, sizeof(ping.reply));
        struct tinytcp::sockaddr_in from_addr;
        socklen_t addr_len = sizeof(from_addr);
        recv_size = tinytcp::recvfrom(s, (char*)&ping.reply, sizeof(ping.reply), 0, 
                                (struct tinytcp::sockaddr*)&from_addr, (tinytcp::socklen_t*)&addr_len);

        if (recv_size <= 0) {
            TINYTCP_LOG_INFO(g_logger) << "recv timeout";
        }
        // TINYTCP_LOG_INFO(g_logger) << "recv ping";
        //
        if (recv_size > 0) {
            recv_size = recv_size - sizeof(tinytcp::ipv4_hdr_t) - sizeof(icmpv4_hdr_t);
            if (memcmp(ping.req.buf, ping.reply.buf, recv_size)) {
                TINYTCP_LOG_ERROR(g_logger) << "recv data error";
                continue;
            }
            fflush(stdout);

            tinytcp::ipv4_hdr_t* iphdr = &ping.reply.ip_hdr;
            int send_size = fill_size;
            if (recv_size == send_size) {
                std::cout << "recv from " << inet_ntoa(addr.sin_addr) << ": "
                          << "bytes=" << recv_size
                          << ", ";
            }
            else {
                std::cout << "recv from " << inet_ntoa(addr.sin_addr) << ": "
                          << "bytes=" << recv_size << "(send=" << send_size << ")"
                          << ", ";
            }

            double diff_ms = (double)(clock() - begin_time) / (CLOCKS_PER_SEC / 1000);
            if (diff_ms < 1) {
                std::cout << "time<1ms, TTL=" << uint32_t(iphdr->ttl);
            }
            else {
                std::cout << "time=" << std::fixed << std::setprecision(3) << diff_ms << ", TTL=" << uint32_t(iphdr->ttl);
            }

            std::cout << std::endl;
        }

        usleep(options.interval * 1000);
    }

    tinytcp::close(s);
}


// #include "tests/test_net.h"
int main(int argc, char** argv) {
    YAML::Node root = YAML::LoadFile("/home/jwy/workspace/tinytcp/config/log.yaml");
    tinytcp::Config::load_from_yaml(root);

    uint32_t ip;
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
    // eth0->set_netmask("255.255.240.0");
    // eth0->set_gateway("192.168.160.1");

    ping_t ping;
    PingOptions options;
    char opt;
    while ((opt = getopt(argc, argv, "n:l:t:h")) != -1) {
        switch (opt) {
            case 'n':
                options.count = std::stoi(optarg);
                if (options.count <= 0) {
                    std::cerr << "错误: Ping次数必须大于0\n";
                    exit(EXIT_FAILURE);
                }
                break;
            case 'l':
                options.packet_size = std::stoi(optarg);
                if (options.packet_size <= 0) {
                    std::cerr << "错误: 包大小必须大于0\n";
                    exit(EXIT_FAILURE);
                }
                break;
            case 't':
                options.timeout = std::stoi(optarg);
                if (options.timeout <= 0) {
                    std::cerr << "错误: 超时时间必须大于0\n";
                    exit(EXIT_FAILURE);
                }
                break;
            case 'h':
                // print_usage(argv[0]);
                exit(EXIT_SUCCESS);
            case '?':
                std::cerr << "未知选项或缺少参数: -" << char(optopt) << "\n";
                // print_usage(argv[0]);
                exit(EXIT_FAILURE);
            default:
                // print_usage(argv[0]);
                exit(EXIT_FAILURE);
        }
    }
    // 获取目标地址（非选项参数）
    if (optind < argc) {
        options.target = argv[optind];
    } else {
        std::cerr << "错误: 必须指定目标地址\n";
        // print_usage(argv[0]);
        exit(EXIT_FAILURE);
    }

    ping_run(ping, options.target.c_str(), options);

    while (1) {}
    return 0;
}

