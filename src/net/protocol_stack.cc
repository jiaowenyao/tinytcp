#include "protocol_stack.h"

namespace tinytcp {

exmsg_t::ptr IProtocolStack::get_msg_block() {
    exmsg_t* msg;
    // 采用非阻塞获取数据，丢失数据不是协议栈关心的事情
    if (!m_mem_block->alloc((void**)&msg, 0)) {
        return nullptr;
    }
    new (msg) exmsg_t();
    exmsg_t::ptr ptr(msg, [this](const exmsg_t* msg) {
        m_mem_block->free(msg);
    });
    return ptr;
}

// net_err_t IProtocolStack::release_msg_block(exmsg_t* msg) {
//     m_mem_block->free(msg);
//     return net_err_t::NET_ERR_OK;
// }

net_err_t IProtocolStack::push_msg(exmsg_t::ptr msg, uint32_t timeout_ms) {
    bool ok = m_msg_queue->push(msg, timeout_ms);
    if (!ok) {
        return net_err_t::NET_ERR_MEM;
    }
    return net_err_t::NET_ERR_OK;
}

net_err_t IProtocolStack::pop_msg() {

    return net_err_t::NET_ERR_OK;
}

}
