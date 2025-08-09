#include <thread>
#include "src/net/net.h"
#include "src/config.h"


int main() {

    YAML::Node root = YAML::LoadFile("/home/jwy/workspace/tinytcp/config/log.yaml");
    tinytcp::Config::load_from_yaml(root);


    tinytcp::ProtocolStack p;

    p.init();

    p.start();

    while (1) {
        std::this_thread::yield();
    }

    return 0;
}

