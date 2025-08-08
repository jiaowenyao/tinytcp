#include "protocol_stack.h"

namespace tinytcp {

exmsg_t* IProtocolStack::get_msg_block() {
    exmsg_t* ptr = nullptr;
    // 采用非阻塞获取数据，丢失数据不是协议栈关心的事情
    if (!m_mem_block->alloc(ptr, 0)) {
        return nullptr;
    }
    return ptr;
}

net_err_t IProtocolStack::release_msg_block(exmsg_t* msg) {
    m_mem_block->free(msg);
    return net_err_t::NET_ERR_OK;
}

net_err_t IProtocolStack::push_msg(exmsg_t* msg, uint32_t timeout_ms) {
    m_msg_queue->push(msg, timeout_ms);
    return net_err_t::NET_ERR_OK;
}

net_err_t IProtocolStack::pop_msg() {

    return net_err_t::NET_ERR_OK;
}

}
