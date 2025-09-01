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

    auto pktmgr = tinytcp::PktMgr::get_instance();

    tinytcp::ProtocolStack p;

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

    while (1) {
        sleep(1);
    }

    return 0;
}

