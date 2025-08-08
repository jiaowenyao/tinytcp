
#include "net.h"
#include "exmsg.h"
#include "src/config.h"
#include "src/macro.h"
#include "src/log.h"


namespace tinytcp {

static tinytcp::Logger::ptr g_logger = TINYTCP_LOG_NAME("system");

static tinytcp::ConfigVar<uint32_t>::ptr g_tcp_msg_queue_size =
    tinytcp::Config::look_up("tcp.msg_queue_size", (uint32_t)1024, "tcp msg queue size");

ProtocolStack::ProtocolStack() {
    m_mem_block = std::make_unique<MemBlock>(sizeof(exmsg_t), g_tcp_msg_queue_size->value());
    TINYTCP_ASSERT2(m_mem_block == nullptr, "m_mem_block init error");
    m_msg_queue = std::make_unique<LockFreeRingQueue<exmsg_t*>>(g_tcp_msg_queue_size->value());
    TINYTCP_ASSERT2(m_msg_queue == nullptr, "m_msg_queue init error");
    m_network = std::make_unique<PcapNetWork>(this);
    TINYTCP_ASSERT2(m_network == nullptr, "m_network init error");
}

net_err_t ProtocolStack::init() {
    return net_err_t::NET_ERR_OK;
}

net_err_t ProtocolStack::start() {
    return net_err_t::NET_ERR_OK;
}




} // namespace tinytcp

