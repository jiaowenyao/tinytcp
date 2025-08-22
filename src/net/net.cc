
#include "net.h"
#include "exmsg.h"
#include "src/config.h"
#include "src/macro.h"
#include "src/log.h"
#include "magic_enum.h"


namespace tinytcp {

static tinytcp::Logger::ptr g_logger = TINYTCP_LOG_NAME("system");

static tinytcp::ConfigVar<uint32_t>::ptr g_tcp_msg_queue_size =
    tinytcp::Config::look_up("tcp.msg_queue_size", (uint32_t)1024, "tcp msg queue size");

ProtocolStack::ProtocolStack() {
    TINYTCP_LOG_DEBUG(g_logger) << "g_tcp_msg_queue_size=" << g_tcp_msg_queue_size->value();
    m_mem_block = std::make_unique<MemBlock>(sizeof(exmsg_t), g_tcp_msg_queue_size->value());
    TINYTCP_ASSERT2(m_mem_block != nullptr, "m_mem_block init error");
    m_msg_queue = std::make_unique<LockFreeRingQueue<exmsg_t*>>(g_tcp_msg_queue_size->value());
    TINYTCP_ASSERT2(m_msg_queue != nullptr, "m_msg_queue init error");
    m_network = std::make_unique<PcapNetWork>(this);
    TINYTCP_ASSERT2(m_network != nullptr, "m_network init error");

    // 启动工作线程
    m_work_thread = std::make_unique<Thread>(std::bind(&ProtocolStack::work_thread_func, this), "work_thread");
}

net_err_t ProtocolStack::init() {
    return net_err_t::NET_ERR_OK;
}

net_err_t ProtocolStack::start() {
    return net_err_t::NET_ERR_OK;
}


void ProtocolStack::work_thread_func() {
    TINYTCP_LOG_INFO(g_logger) << "work thread begin";

    while (true) {
        // 阻塞,取出消息
        exmsg_t* msg = nullptr;
        if (!m_msg_queue->pop(&msg)) {
            continue;
        }

        TINYTCP_LOG_DEBUG(g_logger)
            << "work thread recv msg=" << (uint64_t)msg
            << " msg_type=" << magic_enum::enum_name(msg->type);

        switch (msg->type) {
            case exmsg_t::NET_EXMSG_NETIF_IN: {
                TINYTCP_LOG_DEBUG(g_logger) << "do netif in";
                do_netif_in(msg);
                break;
            }
            default:
                break;
        }

        // 工作线程消费完之后把内存块放回去
        release_msg_block(msg);
    }
}

net_err_t ProtocolStack::do_netif_in(exmsg_t* msg) {
    INetIF* netif = msg->netif.netif;

    while (true) {
        PktBuffer* buf = netif->get_buf_from_in_queue(0);
        if (buf == nullptr) {
            TINYTCP_LOG_ERROR(g_logger) << "do_netif_in get buf error!!!";
            return net_err_t::NET_ERR_MEM;
        }
        // TINYTCP_LOG_INFO(g_logger) << "recv a packet";
        buf->free();
    }

    return net_err_t::NET_ERR_OK;
}



} // namespace tinytcp

