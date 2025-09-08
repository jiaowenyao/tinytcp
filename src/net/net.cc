
#include "net.h"
#include "exmsg.h"
#include "src/config.h"
#include "src/macro.h"
#include "src/log.h"
#include "magic_enum.h"
#include "network.h"
#include "ip.h"
#include "icmp.h"
#include <fcntl.h>


namespace tinytcp {

static tinytcp::Logger::ptr g_logger = TINYTCP_LOG_NAME("system");

static tinytcp::ConfigVar<uint32_t>::ptr g_tcp_msg_queue_size =
    tinytcp::Config::look_up("tcp.msg_queue_size", (uint32_t)1024, "tcp msg queue size");
static tinytcp::ConfigVar<uint32_t>::ptr g_tcp_func_exmsg_queue_size =
    tinytcp::Config::look_up("tcp.func_exmsg_queue_size", (uint32_t)256, "tcp timer msg queue size");

ProtocolStack::ProtocolStack() {
    TINYTCP_LOG_DEBUG(g_logger) << "g_tcp_msg_queue_size=" << g_tcp_msg_queue_size->value();
    m_mem_block = std::make_unique<MemBlock>(sizeof(exmsg_t), g_tcp_msg_queue_size->value());
    TINYTCP_ASSERT2(m_mem_block != nullptr, "m_mem_block init error");
    m_msg_queue = std::make_unique<LockFreeRingQueue<exmsg_t::ptr>>(g_tcp_msg_queue_size->value());
    TINYTCP_ASSERT2(m_msg_queue != nullptr, "m_msg_queue init error");
    m_network = std::make_unique<PcapNetWork>();
    TINYTCP_ASSERT2(m_network != nullptr, "m_network init error");
    m_ipprotocol = std::make_unique<IPProtocol>();
    TINYTCP_ASSERT2(m_ipprotocol != nullptr, "m_ipprotocol init error");
    m_icmpprotocol = std::make_unique<ICMPProtocol>();
    TINYTCP_ASSERT2(m_icmpprotocol != nullptr, "m_icmpprotocol init error");

    // 启动工作线程
    m_work_thread = std::make_unique<Thread>(std::bind(&ProtocolStack::work_thread_func, this), "work_thread");
    // 启动定时器线程
    timer_thread_init();
}

net_err_t ProtocolStack::init() {
    m_ipprotocol->init();
    return net_err_t::NET_ERR_OK;
}

net_err_t ProtocolStack::start() {
    return net_err_t::NET_ERR_OK;
}


void ProtocolStack::work_thread_func() {
    TINYTCP_LOG_INFO(g_logger) << "work thread begin";

    while (true) {
        // 阻塞,取出消息
        exmsg_t::ptr msg = nullptr;
        if (!m_msg_queue->pop(&msg)) {
            continue;
        }

        // TINYTCP_LOG_DEBUG(g_logger)
        //     << "work thread recv msg=" << (uint64_t)msg
        //     << " msg_type=" << magic_enum::enum_name(msg->type);

        switch (msg->type) {
            case exmsg_t::NET_EXMSG_NETIF_IN: {
                TINYTCP_LOG_DEBUG(g_logger) << "do netif in";
                do_netif_in(msg);
                break;
            }
            case exmsg_t::NET_EXMSG_TIMER_FUNC: {
                std::function<net_err_t()> func;
                func.swap(msg->func.func);
                if (func) {
                    try {
                        func();
                    } catch (const std::exception& e) {
                        TINYTCP_LOG_ERROR(g_logger) << "Timer callback error: " << e.what();
                    }
                }
                break;
            }
            case exmsg_t::NET_EXMSG_EXEC_FUNC: {
                std::function<net_err_t()> func;
                func.swap(msg->func.func);
                if (func) {
                    msg->func.err = func();
                }
                msg->func.sem.notify();
                break;
            }
            default:
                break;
        }

        // 工作线程消费完之后把内存块放回去
        // if (msg->type == exmsg_t::NET_EXMSG_TIMER_FUN) {
        //     release_timer_msg_block(msg);
        // }
        // else if (msg->type == exmsg_t::NET_EXMSG_NETIF_IN) {
        //     release_msg_block(msg);
        // }
    }
}

net_err_t ProtocolStack::do_netif_in(exmsg_t::ptr msg) {
    INetIF* netif = msg->netif.netif;

    while (netif->get_in_queue_size() != 0) {
        PktBuffer::ptr buf = netif->get_buf_from_in_queue(0);
        if (buf == nullptr) {
            TINYTCP_LOG_ERROR(g_logger) << "do_netif_in get buf error!!!";
            return net_err_t::NET_ERR_MEM;
        }

        net_err_t err = netif->link_in(buf);
        if ((int8_t)err < 0) {
            TINYTCP_LOG_WARN(g_logger) << "netif link in error:" << magic_enum::enum_name(err);
            // buf->free();
        }
        // TINYTCP_LOG_INFO(g_logger) << "recv a packet";
        // buf->fill(0x11, 6);
        // err = netif->netif_out(ipaddr_t(), buf);
        // if ((int8_t)err < 0) {
        //     buf->free();
        // }

    }

    return net_err_t::NET_ERR_OK;
}

void ProtocolStack::on_timer_inserted_at_front() {
    tickle_event();
}

void ProtocolStack::tickle_event() {
    eventfd_t value = 0xFF;
    eventfd_write(m_event_fd, value);
}

void ProtocolStack::timer_thread_func() {
    epoll_event* events = new epoll_event[64]();
    std::shared_ptr<epoll_event> shared_events(events, [](epoll_event* ptr) {
        delete[] ptr;
    });

    while (true) {
        uint64_t next_timeout = get_next_time();
        int rt = 0;
        do {
            static const uint64_t MAX_TIMEOUT = 3000;
            if (next_timeout != ~0ULL) {
                next_timeout = std::min(next_timeout, MAX_TIMEOUT);
            }
            else {
                next_timeout = MAX_TIMEOUT;
            }
            rt = epoll_wait(m_event_fd, events, 64, int(next_timeout));
            if (rt < 0 && rt == EINTR) {
            }
            else {
                break;
            }

        } while (true);

        std::vector<std::function<net_err_t()>> cbs;
        list_expired_cb(cbs);
        for (auto& cb : cbs) {
            // cb();
            exmsg_t::ptr timer_exmsg = get_func_exmsg_block();
            if (timer_exmsg == nullptr) {
                TINYTCP_LOG_ERROR(g_logger) << "get timer msg block error";
                continue;
            }
            timer_exmsg->type = exmsg_t::NET_EXMSG_TIMER_FUNC;
            timer_exmsg->func.func = cb;
            if ((int8_t)push_msg(timer_exmsg, -1) < 0) {
                TINYTCP_LOG_ERROR(g_logger) << "push timer message failed";
                // release_timer_msg_block(timer_exmsg);
            }
        }
    }
}

exmsg_t::ptr ProtocolStack::get_func_exmsg_block() {
    exmsg_t* msg;
    if (!m_func_exmsg_mem_block->alloc((void**)&msg, 0)) {
        return nullptr;
    }
    new (msg) exmsg_t();
    exmsg_t::ptr ptr(msg, [this](const exmsg_t* msg) {
        m_func_exmsg_mem_block->free(msg);
    });
    ptr->type = exmsg_t::NET_EXMSG_TIMER_FUNC;
    return ptr;
}

// net_err_t ProtocolStack::release_timer_msg_block(exmsg_t* msg) {
//     if (msg) {
//         msg->~exmsg_t();
//         bool ok = m_timer_mem_block->free(msg);
//     }
//     return net_err_t::NET_ERR_OK;
// }


void ProtocolStack::timer_thread_init() {
    m_epoll_fd = epoll_create1(0);
    TINYTCP_ASSERT2(m_epoll_fd != -1, "epoll_create1 error");
    m_event_fd = eventfd(0, EFD_NONBLOCK);
    TINYTCP_ASSERT2(m_event_fd != -1, "eventfd create error");
    m_func_exmsg_mem_block = std::make_unique<MemBlock>(sizeof(exmsg_t), g_tcp_func_exmsg_queue_size->value());
    TINYTCP_ASSERT2(m_func_exmsg_mem_block != nullptr, "m_timer_mem_block init error");
    m_func_exmsg_msg_queue = std::make_unique<LockFreeRingQueue<exmsg_t::ptr>>(g_tcp_func_exmsg_queue_size->value());
    TINYTCP_ASSERT2(m_func_exmsg_msg_queue != nullptr, "m_timer_msg_queue init error");

    epoll_event event;
    event.events = EPOLLET | EPOLLIN;
    int rt = fcntl(m_event_fd, F_SETFL, O_NONBLOCK);
    TINYTCP_ASSERT(!rt);
    rt = epoll_ctl(m_epoll_fd, EPOLL_CTL_ADD, m_event_fd, &event);
    TINYTCP_ASSERT(!rt);

    m_timer_thread = std::make_unique<Thread>(std::bind(&ProtocolStack::timer_thread_func, this), "timer_thread");
    TINYTCP_ASSERT2(m_timer_thread != nullptr, "m_timer_thread create error");
}

net_err_t ProtocolStack::exmsg_func_exec(std::function<net_err_t()> func) {
    exmsg_t::ptr msg = get_func_exmsg_block();
    msg->func.func = func;
    msg->type = exmsg_t::NET_EXMSG_EXEC_FUNC;

    net_err_t err = net_err_t::NET_ERR_MEM;

    while (err != net_err_t::NET_ERR_OK) {
        err = push_msg(msg, 0);
    }

    msg->func.sem.wait();

    return msg->func.err;
}

} // namespace tinytcp

