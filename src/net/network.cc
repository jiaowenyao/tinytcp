
#include "network.h"
#include "src/log.h"
#include "macro.h"
#include "plat/sys_plat.h"

namespace tinytcp {

std::map<std::string, INetWork::NetIFFactoryFunc> INetWork::s_netif_factory_registry;

static tinytcp::Logger::ptr g_logger = TINYTCP_LOG_NAME("system");

INetWork::INetWork(IProtocolStack* protocal_stack)
    : m_protocal_stack(protocal_stack) {

}

INetWork::~INetWork() {
    for (auto& it : m_netif_list) {
        delete it;
    }
}

#if defined(DEBUG_INETIF)
void INetWork::debug_print() {
    for (auto& it : m_netif_list) {
        it->debug_print();
    }
}
#else
void INetWork::debug_print() {}
#endif // DEBUG_INETIF

std::list<INetIF*>::iterator INetWork::find_netif_by_name(const std::string& name) {
    // for (auto& it = m_netif_list.begin(); it != m_netif_list.end(); ++it) {
    //     if ((*it)->)
    // }
    for (auto it = m_netif_list.begin(); it != m_netif_list.end(); ++it) {
        if (strcmp((*it)->get_name(), name.c_str())) {
            return it;
        }
    }
    return m_netif_list.end();
}

net_err_t INetWork::exmsg_netif_in(INetIF* netif) {
    TINYTCP_LOG_DEBUG(g_logger) << "exmsg netif in";
    exmsg_t* msg = m_protocal_stack->get_msg_block();
    if (msg == nullptr) {
        TINYTCP_LOG_WARN(g_logger) << "no free msg block";
        return net_err_t::NET_ERR_MEM;
    }
    static uint32_t id = 0U;
    msg->type = exmsg_t::EXMSGTYPE::NET_EXMSG_NETIF_IN;
    msg->netif = msg_netif_t(netif);
    // msg->data.emplace<msg_netif_t>(netif);
    // msg->data = msg_netif_t(netif);

    net_err_t err = msg_send(msg, 0);
    if ((int)err < 0) {
        TINYTCP_LOG_WARN(g_logger) << "msg_send error, msg_queue is full";
        m_protocal_stack->release_msg_block(msg);
        return err;
    }
    return net_err_t::NET_ERR_OK;
}



net_err_t INetWork::netif_close(std::list<INetIF*>::iterator& netif_it) {
    auto netif = *netif_it;
    if (netif->get_state() == INetIF::NETIF_ACTIVE) {
        TINYTCP_LOG_ERROR(g_logger) << "netif is actived, need use deactive";
        return net_err_t::NET_ERR_STATE;
    }

    netif->close();
    netif->set_state(INetIF::NETIF_CLOSED);
    m_netif_list.erase(netif_it);

    return net_err_t::NET_ERR_OK;
}

bool INetWork::register_netif_factory(const std::string& type_name, NetIFFactoryFunc factory_func) {
    if (s_netif_factory_registry.find(type_name) != s_netif_factory_registry.end()) {
        return false;
    }
    s_netif_factory_registry[type_name] = factory_func;
    return true;
}

INetIF* INetWork::netif_open(const char* dev_name, void* ops_data) {
    std::string name_str = std::string(dev_name);
    auto it = s_netif_factory_registry.find(name_str);
    if (it == s_netif_factory_registry.end()) {
        TINYTCP_LOG_ERROR(g_logger) << "no dev_name";
        return nullptr;
    }

    std::unique_ptr<INetIF> inetif = it->second(this, dev_name, ops_data);
    if (!inetif) {
        TINYTCP_LOG_ERROR(g_logger) << "inet create error";
        return nullptr;
    }

    net_err_t err = inetif->open();
    if ((int8_t)err < 0) {
        TINYTCP_LOG_ERROR(g_logger) << "inetif open error, err=" << (int8_t)err;
        inetif->close();
        return nullptr;
    }
    set_active(inetif.get());

    m_netif_list.push_back(inetif.get());
    INetIF* inetif_ptr = inetif.release();

    TINYTCP_LOG_INFO(g_logger) << "netif_open: " << dev_name << " success";

    return inetif_ptr;
}

net_err_t INetWork::msg_send(exmsg_t* msg, int32_t timeout_ms) {
    m_protocal_stack->push_msg(msg, timeout_ms);

    return net_err_t::NET_ERR_OK;
}


net_err_t INetWork::set_active(INetIF* netif) {
    if (netif->get_state() != INetIF::NETIF_OPENED) {
        TINYTCP_LOG_ERROR(g_logger) << "netif is not opened";
        return net_err_t::NET_ERR_STATE;
    }

    if (m_default_netif == nullptr && netif->get_type() != NETIF_TYPE_LOOP) {
        m_default_netif = netif;
    }

    net_err_t err = netif->link_open();
    if ((int8_t)err < 0) {
        TINYTCP_LOG_ERROR(g_logger) << "netif link open error";
        return err;
    }

    netif->set_state(INetIF::NETIF_ACTIVE);
    return net_err_t::NET_ERR_OK;
}

net_err_t INetWork::set_deactive(INetIF* netif) {
    if (netif->get_state() != INetIF::NETIF_ACTIVE) {
        TINYTCP_LOG_ERROR(g_logger) << "netif is not actived";
        return net_err_t::NET_ERR_STATE;
    }

    netif->link_close();

    netif->clear_in_queue();
    netif->clear_out_queue();

    if (netif == m_default_netif) {
        m_default_netif = nullptr;
    }

    netif->set_state(INetIF::NETIF_OPENED);
    return net_err_t::NET_ERR_OK;
}

PktBuffer::ptr INetWork::get_buf_from_in_queue(NetListIt netif_it, int timeout_ms) {
    PktBuffer::ptr buf = (*netif_it)->get_buf_from_in_queue(timeout_ms);
    return buf;
}

net_err_t INetWork::put_buf_to_in_queue(NetListIt netif_it, PktBuffer::ptr buf, int timeout_ms) {
    (*netif_it)->put_buf_to_in_queue(buf, timeout_ms);
    return net_err_t::NET_ERR_OK;
}

PktBuffer::ptr INetWork::get_buf_from_out_queue(NetListIt netif_it, int timeout_ms) {
    PktBuffer::ptr buf = (*netif_it)->get_buf_from_out_queue(timeout_ms);
    return buf;
}

net_err_t INetWork::put_buf_to_out_queue(NetListIt netif_it, PktBuffer::ptr buf, int timeout_ms) {
    (*netif_it)->put_buf_to_out_queue(buf, timeout_ms);
    return net_err_t::NET_ERR_OK;
}

PcapNetWork::PcapNetWork(IProtocolStack* protocal_stack)
    : INetWork(protocal_stack) {

}

net_err_t PcapNetWork::init()  {

    return net_err_t::NET_ERR_OK;
}

net_err_t PcapNetWork::start() {

    return net_err_t::NET_ERR_OK;
}

void PcapNetWork::recv_func(void* arg) {
    TINYTCP_LOG_INFO(g_logger) << "PcapNetWork recv begin";
    INetIF* netif = static_cast<INetIF*>(arg);
    pcap_t* pcap = (pcap_t*)netif->get_ops_data();
    auto pktmgr = PktMgr::get_instance();

    // test: 在wsl中，用bpf过滤一下arp
    // struct bpf_program fp;
    // bpf_u_int32 net, mask;
    // char errbuf[PCAP_ERRBUF_SIZE];
    // TINYTCP_ASSERT2(pcap_lookupnet(netif->get_name(), &net, &mask, errbuf) != -1, "lookupnet error");
    // ipaddr_t _mask;
    // _mask.q_addr = mask;
    // TINYTCP_LOG_INFO(g_logger) << "mask=" << _mask;
    // char filter_exp[] = "not arp and not ip and not icmp";
    // int err = pcap_compile(pcap, &fp, filter_exp, 0, mask);
    // if (err != 0) {
    //     TINYTCP_LOG_ERROR(g_logger) << pcap_geterr(pcap);
    //     TINYTCP_ASSERT2(0, "bpf compile error");
    // }
    // pcap_setfilter(pcap, &fp);

    while (true) {
        struct pcap_pkthdr* pkthdr;
        const uint8_t* pkt_data;
        if (pcap_next_ex(pcap, &pkthdr, &pkt_data) != 1) {
            continue;
        }
        TINYTCP_LOG_DEBUG(g_logger) << "pkt_data len=" << pkthdr->len;
        PktBuffer::ptr buf = pktmgr->get_pktbuffer();
        if (buf == nullptr) {
            TINYTCP_LOG_WARN(g_logger) << "get_pktbuffer == nullptr";
            continue;
        }
        bool ok = buf->alloc(pkthdr->len);
        if (!ok) {
            TINYTCP_LOG_WARN(g_logger) << "buf alloc error";
            continue;
        }
        buf->reset_access();
        buf->write(pkt_data, pkthdr->len);

        if ((int8_t)netif->put_buf_to_in_queue(buf) < 0) {
            TINYTCP_LOG_WARN(g_logger) << "put buf error";
            // buf->free();
            continue;
        }
    }

    TINYTCP_LOG_ERROR(g_logger) << "PcapNetWork recv end";
}

void PcapNetWork::send_func(void* arg) {
    TINYTCP_LOG_INFO(g_logger) << "PcapNetWork send begin";
    INetIF* netif = static_cast<INetIF*>(arg);
    pcap_t* pcap = (pcap_t*)netif->get_ops_data();
    auto pktmgr = PktMgr::get_instance();

    // 以太网协议, 数据缓冲区:1500, 目的地址:6, 源地址:6, 类型:2 
    static uint8_t rw_buffer[1500 + 6 + 6 + 2]; // 最后还有4个字节的校验位，网卡会自动填充, 代码中不用管
    while (true) {
        PktBuffer::ptr buf = netif->get_buf_from_out_queue(0);
        if (buf == nullptr) {
            continue;
        }

        uint32_t total_size = buf->get_capacity();
        buf->reset_access();
        memset(rw_buffer, 0, sizeof(rw_buffer));
        buf->read(rw_buffer, total_size);
        // buf->free();

        if (pcap_inject(pcap, rw_buffer, total_size) == -1) {
            TINYTCP_LOG_ERROR(g_logger) << "pcap_inject error: " << pcap_geterr(pcap) << ", send size=" << total_size;
        }
        else {
            TINYTCP_LOG_DEBUG(g_logger) << "pcap_inject send successfully, data=" << std::make_pair(std::string((const char*)rw_buffer, total_size), true);
        }
    }
}

net_err_t PcapNetWork::exmsg_netif_out(INetIF* netif) {

    return net_err_t::NET_ERR_OK;
}

namespace {

bool _loop_net_registered = INetWork::register_netif_factory("loop",
    [](INetWork* network, const char* name, void* ops_data) -> std::unique_ptr<INetIF> {
        return std::make_unique<LoopNet>(network, name, ops_data);
    });

bool _eth0_net_registered = INetWork::register_netif_factory("eth0",
    [](INetWork* network, const char* name, void* ops_data) -> std::unique_ptr<INetIF> {
        return std::make_unique<EtherNet>(network, name, ops_data);
    });


};



} // namespace tinytcp

