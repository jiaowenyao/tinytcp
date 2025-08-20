
#include "network.h"
#include "src/log.h"

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
    msg->id = id++;

    net_err_t err = msg_send(msg, 0);
    if ((int)err < 0) {
        TINYTCP_LOG_WARN(g_logger) << "msg_send error, msg_queue is full";
        m_protocal_stack->release_msg_block(msg);
        return err;
    }
    return net_err_t::NET_ERR_OK;
}

net_err_t INetWork::exmsg_netif_out() {

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

    netif->set_state(INetIF::NETIF_ACTIVE);
    return net_err_t::NET_ERR_OK;
}

net_err_t INetWork::set_deactive(INetIF* netif) {
    if (netif->get_state() != INetIF::NETIF_ACTIVE) {
        TINYTCP_LOG_ERROR(g_logger) << "netif is not actived";
        return net_err_t::NET_ERR_STATE;
    }

    netif->clear_in_queue();
    netif->clear_out_queue();

    if (netif == m_default_netif) {
        m_default_netif = nullptr;
    }

    netif->set_state(INetIF::NETIF_OPENED);
    return net_err_t::NET_ERR_OK;
}

PktBuffer* INetWork::get_buf_from_in_queue(NetListIt netif_it, int timeout_ms) {
    PktBuffer* buf = (*netif_it)->get_buf_from_in_queue(timeout_ms);
    return buf;
}

net_err_t INetWork::put_buf_to_in_queue(NetListIt netif_it, PktBuffer* buf, int timeout_ms) {
    (*netif_it)->put_buf_to_in_queue(buf, timeout_ms);
    return net_err_t::NET_ERR_OK;
}

PktBuffer* INetWork::get_buf_from_out_queue(NetListIt netif_it, int timeout_ms) {
    PktBuffer* buf = (*netif_it)->get_buf_from_out_queue(timeout_ms);
    return buf;
}

net_err_t INetWork::put_buf_to_out_queue(NetListIt netif_it, PktBuffer* buf, int timeout_ms) {
    (*netif_it)->put_buf_to_out_queue(buf, timeout_ms);
    return net_err_t::NET_ERR_OK;
}

PcapNetWork::PcapNetWork(IProtocolStack* protocal_stack)
    : INetWork(protocal_stack) {

    m_send_thread = std::make_unique<Thread>(std::bind(&PcapNetWork::send_func, this), "network_send_thread");
    m_recv_thread = std::make_unique<Thread>(std::bind(&PcapNetWork::recv_func, this), "network_recv_thread");
}

net_err_t PcapNetWork::init()  {

    return net_err_t::NET_ERR_OK;
}

net_err_t PcapNetWork::start() {

    return net_err_t::NET_ERR_OK;
}

void PcapNetWork::recv_func() {
    TINYTCP_LOG_INFO(g_logger) << "PcapNetWork recv begin";
    while (true) {
        sleep(1);
        // exmsg_netif_in();
    }
}

void PcapNetWork::send_func() {

}

namespace {

bool _loop_net_registered = INetWork::register_netif_factory("loop",
    [](INetWork* network, const char* name, void* ops_data) -> std::unique_ptr<INetIF> {
        return std::make_unique<LoopNet>(network, name, ops_data);
    });

};



} // namespace tinytcp

