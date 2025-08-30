
#include <thread>
#include "src/net/net.h"
#include "src/net/pktbuf.h"
#include "src/config.h"
#include "test_net.h"
#include <list>
#include <string.h>

template class std::list<tinytcp::PktBlock*>;
static tinytcp::Logger::ptr g_logger = TINYTCP_LOG_ROOT();

void print1() {
    TINYTCP_LOG_INFO(g_logger) << __FUNCTION__;
}

void print2() {
    TINYTCP_LOG_INFO(g_logger) << __FUNCTION__;
}

int main() {

    YAML::Node root = YAML::LoadFile("/home/jwy/workspace/tinytcp/config/log.yaml");
    tinytcp::Config::load_from_yaml(root);

    auto pktmgr = tinytcp::PktMgr::get_instance();


    tinytcp::ProtocolStack p;

    // p.init();
    //
    // p.start();

    auto network = p.get_network();
    auto loop = network->netif_open("loop");


    uint32_t ip;
    uint8_t hwaddr[6] = {0};
    tinytcp::ipaddr_t _ip;
    get_network_info_by_pcap("eth0", &ip, hwaddr);
    _ip.q_addr = ip;
    std::string ip_str = _ip.to_string();
    tinytcp::pcap_data_t pcap_data {
        .ip = ip_str.c_str(),
        .hwaddr = hwaddr
    };
    auto eth0 = network->netif_open("eth0", &pcap_data);
    eth0->set_netmask("255.255.240.0");

    network->debug_print();

    // eth0->netif_out(tinytcp::ipaddr_t(), buf);
    // netif->netif_out(tinytcp::ipaddr_t(), buf);

    // p.add_timer(1000, print1, true);
    // p.add_timer(3000, print2, true);

    while (1) {
        tinytcp::PktBuffer::ptr buf = pktmgr->get_pktbuffer();
        buf->alloc(32);
        buf->fill(0x53, 32);
        // eth0->netif_out("192.168.160.255", buf);
        eth0->netif_out("192.168.175.255", buf);
        // eth0->netif_out("172.20.32.132", buf);
        ((tinytcp::EtherNet*)eth0)->make_arp_request("192.168.160.1");
        // return 0;
        sleep(1);
        // std::this_thread::yield();
        // sleep(10);
    }

    return 0;
}

