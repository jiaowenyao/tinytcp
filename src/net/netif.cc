#include "netif.h"
#include "log.h"
#include "config.h"
#include "macro.h"


namespace tinytcp {

static tinytcp::Logger::ptr g_logger = TINYTCP_LOG_NAME("system");

static tinytcp::ConfigVar<uint32_t>::ptr g_netif_in_queue_size =
    tinytcp::Config::look_up("tcp.netif_in_queue_size", 1024U, "netif in queue size, 网卡输入队列的大小");
static tinytcp::ConfigVar<uint32_t>::ptr g_netif_out_queue_size =
    tinytcp::Config::look_up("tcp.netif_out_queue_size", 1024U, "netif out queue size, 网卡输出队列的大小");

INetIF::INetIF(const char* name, void* ops_data)
    : m_state(NETIF_OPENED)
    , m_ops_data(ops_data) {
    set_name(name);
    m_in_q = std::make_unique<LockFreeRingQueue<void*>>(g_netif_in_queue_size->value());
    TINYTCP_ASSERT2(m_in_q != nullptr, "m_in_q init error");
    m_out_q = std::make_unique<LockFreeRingQueue<void*>>(g_netif_out_queue_size->value());
    TINYTCP_ASSERT2(m_out_q != nullptr, "m_out_q init error");
}

INetIF::~INetIF() {

}

void INetIF::set_name(const char* name) {
    memcpy(m_name, name, strlen(name));
}
LoopNet::LoopNet(const char* name, void* ops_data)
    : INetIF(name, ops_data) {

}
LoopNet::~LoopNet() {

}

net_err_t LoopNet::open() {
    m_type = NETIF_TYPE_LOOP;
    return net_err_t::NET_ERR_OK;
}

void LoopNet::close() {

}

net_err_t LoopNet::send() {

    return net_err_t::NET_ERR_OK;
}

} // namespace tinytcp


