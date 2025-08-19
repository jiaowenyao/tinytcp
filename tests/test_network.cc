
#include <thread>
#include "src/net/net.h"
#include "src/net/pktbuf.h"
#include "src/config.h"
#include <list>
#include <string.h>

template class std::list<tinytcp::PktBlock*>;
static tinytcp::Logger::ptr g_logger = TINYTCP_LOG_ROOT();


void test_network_open() {

}


int main() {

    YAML::Node root = YAML::LoadFile("/home/jwy/workspace/tinytcp/config/log.yaml");
    tinytcp::Config::load_from_yaml(root);


    tinytcp::ProtocolStack p;

    // p.init();
    //
    // p.start();

    auto network = p.get_network();

    network->netif_open("loop");

    test_network_open();

    while (1) {
        std::this_thread::yield();
    }

    return 0;
}

