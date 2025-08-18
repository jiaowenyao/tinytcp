#include <thread>
#include "src/net/net.h"
#include "src/net/pktbuf.h"
#include "src/config.h"
#include <list>
#include <string.h>

template class std::list<tinytcp::PktBlock*>;
static tinytcp::Logger::ptr g_logger = TINYTCP_LOG_ROOT();

void pktbuf_test() {
    auto pktmgr = tinytcp::PktMgr::get_instance();

    TINYTCP_LOG_INFO(g_logger) << pktmgr->get_blk_list_size() << std::endl;
    tinytcp::PktBuffer pktbuf;
    pktbuf.alloc(2000);

    // auto it = pktbuf.get_list().begin();
    // std::cout << (*it)->get_size()
    //     << ' ' << (uint64_t)((*it)->get_data())
    //     << ' ' << (uint64_t)((*it)->get_payload()) << std::endl;
    // it++;
    // std::cout << (*it)->get_size()
    //     << ' ' << (uint64_t)((*it)->get_data())
    //     << ' ' << (uint64_t)((*it)->get_payload()) << std::endl;
    //
    pktbuf.debug_print();
    for (int i = 0; i < 10; ++i) {
        pktbuf.alloc_header(200, false);
        pktbuf.debug_print();
    }

    std::cout << "===================" << std::endl;

    // pktbuf.debug_print();
    for (int i = 0; i < 10; ++i) {
        pktbuf.remove_header(200);
        pktbuf.debug_print();
    }

    TINYTCP_LOG_INFO(g_logger) << pktmgr->get_blk_list_size() << std::endl;
    std::cout << "+++++++++++++++++++" << std::endl;

    pktbuf.resize(200);
    pktbuf.debug_print();
    pktbuf.resize(2000);
    pktbuf.debug_print();
    pktbuf.resize(4000);
    pktbuf.debug_print();
    pktbuf.resize(1000);
    pktbuf.debug_print();
    pktbuf.resize(100);
    pktbuf.debug_print();
    pktbuf.resize(0);
    pktbuf.debug_print();
    pktbuf.resize(3000);
    pktbuf.debug_print();
    pktbuf.resize(7000);
    pktbuf.debug_print();

    TINYTCP_LOG_INFO(g_logger) << pktmgr->get_blk_list_size() << std::endl;
    pktbuf.resize(0);
    TINYTCP_LOG_INFO(g_logger) << pktmgr->get_blk_list_size() << std::endl;

}

void pktbuf_header_test() {
    auto pktmgr = tinytcp::PktMgr::get_instance();
    auto pktbuf_1 = pktmgr->get_pktbuffer();
    auto pktbuf_2 = pktmgr->get_pktbuffer();

    pktbuf_1->alloc(3000);
    pktbuf_1->debug_print();
    pktbuf_2->alloc(5000);
    pktbuf_2->debug_print();
    pktbuf_1->merge_buf(pktbuf_2);
    pktbuf_1->debug_print();

    std::cout << "-----" << std::endl;
    pktbuf_1->set_cont_header(800);
    pktbuf_1->debug_print();
    pktbuf_1->set_cont_header(1000);
    pktbuf_1->debug_print();

    std::cout << "-----" << std::endl;
    pktbuf_1->resize(100);
    pktbuf_1->alloc(120);
    pktbuf_1->alloc(450);
    pktbuf_1->alloc(200);
    pktbuf_1->alloc(300);
    pktbuf_1->debug_print();
    pktbuf_1->set_cont_header(1000);
    pktbuf_1->debug_print();


    std::cout << pktmgr->get_buf_list_size() << ' ' << pktmgr->get_blk_list_size() << std::endl;
}

void pktbuf_file_test() {
    auto pktmgr = tinytcp::PktMgr::get_instance();
    auto pktbuf = pktmgr->get_pktbuffer();
    pktbuf->alloc(64);
    pktbuf->alloc(64);
    pktbuf->alloc(64);
    pktbuf->alloc(64);
    pktbuf->alloc(64);
    uint8_t write_tmp[256] = {0};
    for (int i = 0; i < 256; ++i) {
        write_tmp[i] = i;
    }
    pktbuf->write(write_tmp, 256);
    pktbuf->debug_print();

    uint8_t read_tmp[256] = {0};
    pktbuf->reset_access();
    // pktbuf->seek(0);
    // pktbuf->read(read_tmp, 256);

    auto pktbuf_2 = pktmgr->get_pktbuffer();
    pktbuf_2->alloc(256);
    pktbuf_2->seek(40);
    pktbuf_2->copy(pktbuf, 100);
    pktbuf_2->fill(233, pktbuf_2->total_blk_remain());
    pktbuf_2->seek(0);
    pktbuf_2->read(read_tmp, 256);

    pktbuf->debug_print();
    std::cout << "++ " << strcmp((const char*)write_tmp, (const char*)read_tmp) << std::endl;
    for (int i = 0; i < 256; ++i) {
        std::cout << (uint32_t)read_tmp[i] << ' ';
        fflush(stdout);
    }
    std::cout << std::endl;
    std::cout << pktmgr->get_buf_list_size() << ' ' << pktmgr->get_blk_list_size() << std::endl;
    pktbuf->free();
    pktbuf_2->free();
    std::cout << pktmgr->get_buf_list_size() << ' ' << pktmgr->get_blk_list_size() << std::endl;
}

int main() {

    YAML::Node root = YAML::LoadFile("/home/jwy/workspace/tinytcp/config/log.yaml");
    tinytcp::Config::load_from_yaml(root);


    tinytcp::ProtocolStack p;

    // p.init();
    //
    // p.start();

    // pktbuf_test();
    // pktbuf_header_test();
    pktbuf_file_test();

    while (1) {
        std::this_thread::yield();
    }

    return 0;
}

