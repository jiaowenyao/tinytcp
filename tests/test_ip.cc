#include "src/net/net.h"
#include "src/net/pktbuf.h"
#include "src/config.h"
#include "test_net.h"
#include <list>

template class std::list<tinytcp::PktBlock*>;
static tinytcp::Logger::ptr g_logger = TINYTCP_LOG_ROOT();


int main() {

    YAML::Node root = YAML::LoadFile("/home/jwy/workspace/tinytcp/config/log.yaml");
    tinytcp::Config::load_from_yaml(root);

    tinytcp::ProtocolStack* p = tinytcp::ProtocolStackMgr::get_instance();
    p->init();
    auto pktmgr = tinytcp::PktMgr::get_instance();

    auto network = p->get_network();
    // auto loop = network->netif_open("loop");

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
    // auto eth0 = network->netif_open("eth0", &pcap_data);
    // eth0->set_netmask("255.255.240.0");
    auto eth0 = network->netif_open("eth0", &pcap_data);
    eth0->set_netmask("255.255.240.0");

    network->debug_print();

    tinytcp::PktBuffer::ptr buf = pktmgr->get_pktbuffer();
    buf->alloc(32);
    buf->fill(0x53, 32);
    // p->get_ipprotocol()->ipv4_out(1, "192.168.160.1", eth0->get_ipaddr(), buf);

    while (1) {
        sleep(1);
    }

    return 0;
}

