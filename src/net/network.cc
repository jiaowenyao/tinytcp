
#include "network.h"
#include "src/log.h"

namespace tinytcp {

static tinytcp::Logger::ptr g_logger = TINYTCP_LOG_NAME("system");

INetWork::INetWork(IProtocolStack* protocal_stack)
    : m_protocal_stack(protocal_stack) {

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
        netif_in();
    }
}

void PcapNetWork::send_func() {

}

net_err_t PcapNetWork::netif_in() {
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

net_err_t PcapNetWork::netif_out() {

    return net_err_t::NET_ERR_OK;
}

net_err_t PcapNetWork::msg_send(exmsg_t* msg, int32_t timeout_ms) {
    m_protocal_stack->push_msg(msg, timeout_ms);

    return net_err_t::NET_ERR_OK;
}

} // namespace tinytcp

