#pragma once


#include "net_err.h"
#include "exmsg.h"
#include "network.h"
#include "protocol_stack.h"
#include "memblock.h"
#include "src/thread.h"
#include "src/lock_free_ring_queue.h"


namespace tinytcp {

class ProtocolStack : public IProtocolStack {

public:
    ProtocolStack();
    ~ProtocolStack() = default;
    net_err_t init()  override;
    net_err_t start() override;

private:
    INetWork::uptr m_network;
    Thread::ptr m_work_thread;
};


} // namespace tinytcp


