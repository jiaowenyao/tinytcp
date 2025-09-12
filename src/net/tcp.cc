#include "tcp.h"
#include "tcp_state.h"
#include "src/log.h"
#include "src/config.h"
#include "src/net/net.h"
#include "src/api/net_api.h"
#include "src/endiantool.h"
#include "src/net/protocol.h"

#include <unordered_map>


namespace tinytcp {


static Logger::ptr g_logger = TINYTCP_LOG_NAME("system");

static std::list<TCPSock*> g_tcp_list;

static ConfigVar<uint32_t>::ptr g_net_port_dyn_start =
    Config::look_up("tcp.net_port_dyn_start", 1024U, "动态分配端口的起始");
static ConfigVar<uint32_t>::ptr g_net_port_dyn_end =
    Config::look_up("tcp.net_port_dyn_start", 65535U, "动态分配端口的结束");
 
static int tcp_alloc_port() {
    static int search_index = g_net_port_dyn_start->value();
    for (int i = g_net_port_dyn_start->value(); i <= g_net_port_dyn_end->value(); ++i) {
        int port = search_index++;
        if (search_index > g_net_port_dyn_end->value()) {
            search_index = g_net_port_dyn_start->value();
        }
        bool has_used = false;
        for (auto& tcp : g_tcp_list) {
            if (tcp->get_local_port() == port) {
                has_used = true;
                break;
            }
        }
        if (!has_used) {
            return port;
        }
    }
    return 0;
}

net_err_t tcp_hdr_t::send_out(PktBuffer::ptr buf, const ipaddr_t& dest, const ipaddr_t& src) {
    auto ipproto = ProtocolStackMgr::get_instance()->get_ipprotocol();
    checksum = 0;
    buf->reset_access();
    checksum = checksum_peso(buf, dest, src, NET_PROTOCOL_TCP);

    net_err_t err = ipproto->ipv4_out(NET_PROTOCOL_TCP, dest, src, buf);
    CHECK_NET_ERROR(err, "tcp hdr out error");
    return net_err_t::NET_ERR_OK;
}

TCPSock::TCPSock(int family, int protocol)
    : Sock(family, protocol) {
    if (protocol == 0) {
        m_protocol = IPPROTO_TCP;
    }
    m_recv_wait = new sock_wait_t;
    m_conn_wait = new sock_wait_t;
    m_send_wait = new sock_wait_t;
    m_state = TCP_STATE_CLOSED;
}

TCPSock::~TCPSock() {
    for (auto it = g_tcp_list.begin(); it != g_tcp_list.end(); ++it) {
        if (*it == this) {
            it = g_tcp_list.erase(it);
            break;
        }
    }
}


void TCPSock::init() {
    g_tcp_list.push_back(this);
}

net_err_t TCPSock::sendto(const void* buf, size_t len, int flags,
                            const struct sockaddr* dest, socklen_t dest_len,
                            ssize_t* result_len) {

    return net_err_t::NET_ERR_OK;
}

net_err_t TCPSock::recvfrom(const void* buf, size_t len, int flags,
                            struct sockaddr* src, socklen_t src_len,
                            ssize_t* result_len) {

    return net_err_t::NET_ERR_OK;
}

static uint32_t tcp_get_iss() {
    static uint32_t seq = 0;
    return seq++;
}

net_err_t TCPSock::tcp_init_connect() {
    m_send.iss = tcp_get_iss();
    m_send.una = m_send.iss;
    m_send.nxt = m_send.iss;

    m_recv.nxt = 0;
    return net_err_t::NET_ERR_OK;
}

net_err_t TCPSock::tcp_send_syn() {
    flags.syn_out = 1;
    transmit();
    return net_err_t::NET_ERR_OK;
}

net_err_t TCPSock::tcp_send_fin() {
    flags.fin_out = 1;
    transmit();
    return net_err_t::NET_ERR_OK;
}

net_err_t TCPSock::connect(sockaddr* addr, socklen_t addr_len) {
    sockaddr_in* addr_in = (sockaddr_in*)addr;

    if (m_state != TCP_STATE_CLOSED) {
        TINYTCP_LOG_ERROR(g_logger) << "tcp is not closed";
        return net_err_t::NET_ERR_STATE;
    }

    m_remote_ip = ipaddr_t(addr_in->sin_addr.s_addr);
    m_remote_port = net_to_host(addr_in->sin_port);
    if (m_local_port == 0) {
        int port = tcp_alloc_port();
        if (port == 0) {
            TINYTCP_LOG_ERROR(g_logger) << "alloc port failed";
            return net_err_t::NET_ERR_NONE;
        }
        m_local_port = port;
    }

    if (m_local_ip == ipaddr_t(0U)) {
        auto ipproto = ProtocolStackMgr::get_instance()->get_ipprotocol();
        route_entry_t* rt = ipproto->route_find(m_remote_ip);
        if (rt == nullptr) {
            TINYTCP_LOG_ERROR(g_logger) << "no route to host";
            return net_err_t::NET_ERR_UNREACH;
        }
        m_local_ip = rt->netif->get_ipaddr();
    }


    net_err_t err = tcp_init_connect();
    CHECK_NET_ERROR(err, "tcp_init_connect error");
    err = tcp_send_syn();
    CHECK_NET_ERROR(err, "tcp_send_syn error");

    m_state = TCP_STATE_SYN_SENT;

    return net_err_t::NET_ERR_NEED_WAIT;
}

net_err_t TCPSock::free() {
    for (auto it = g_tcp_list.begin(); it != g_tcp_list.end(); ++it) {
        if (*it == this) {
            it = g_tcp_list.erase(it);
            break;
        }
    }
    return net_err_t::NET_ERR_OK;
}

net_err_t TCPSock::close() {

    switch ((int)m_state) {
        case TCP_STATE_CLOSED: {
            TINYTCP_LOG_INFO(g_logger) << "tcp already closed";
            free();
            return net_err_t::NET_ERR_OK;
        }
        case TCP_STATE_SYN_SENT:
        case TCP_STATE_SYN_RECVD: {
            tcp_abort(this, net_err_t::NET_ERR_CLOSE);
            free();
            return net_err_t::NET_ERR_OK;
        }
        case TCP_STATE_CLOSE_WAIT: {
            tcp_send_fin();
            m_state = TCP_STATE_LAST_ACK;
            return net_err_t::NET_ERR_NEED_WAIT;
        }
        case TCP_STATE_ESTABLISHED: {
            tcp_send_fin();
            m_state = TCP_STATE_FIN_WAIT_1;
            return net_err_t::NET_ERR_NEED_WAIT;
        }

        default: {
            TINYTCP_LOG_ERROR(g_logger) << "tcp state error";
            return net_err_t::NET_ERR_STATE;
        }
    }

    return net_err_t::NET_ERR_OK;
}








static TCPSock* tcp_find(const ipaddr_t& local_ip, uint16_t local_port, const ipaddr_t& remote_ip, uint16_t remote_port) {

    for (auto& tcp : g_tcp_list) {
        if (tcp->get_local_port() == local_port
            && tcp->get_remote_ip() == remote_ip
            && tcp->get_remote_port() == remote_port) {
            if (tcp->get_local_ip() == ipaddr_t(0U)) {
                return tcp;
            }
            else if (tcp->get_local_ip() == local_ip) {
                return tcp;
            }
            else {}
        }
    }

    return nullptr;
}


net_err_t tcp_in(PktBuffer::ptr buf, const ipaddr_t& src_ip, const ipaddr_t& dest_ip) {
    const static std::unordered_map<tcp_state_t, tcp_proc_ptr> s_tcp_state_proc = {
        {TCP_STATE_CLOSED,      tcp_closed_in},
        {TCP_STATE_LISTEN,      tcp_listen_in},
        {TCP_STATE_SYN_SENT,    tcp_syn_sent_in},
        {TCP_STATE_SYN_RECVD,   tcp_syn_recvd_in},
        {TCP_STATE_ESTABLISHED, tcp_established_in},
        {TCP_STATE_FIN_WAIT_1,  tcp_fin_wait_1_in},
        {TCP_STATE_FIN_WAIT_2,  tcp_fin_wait_2_in},
        {TCP_STATE_CLOSING,     tcp_closing_in},
        {TCP_STATE_TIME_WAIT,   tcp_time_wait_in},
        {TCP_STATE_CLOSE_WAIT,  tcp_close_wait_in},
        {TCP_STATE_LAST_ACK,    tcp_last_ack_in},
    };

    buf->reset_access();
    tcp_hdr_t* tcp_hdr = (tcp_hdr_t*)buf->get_data();
    if (tcp_hdr->checksum) {
        if (checksum_peso(buf, dest_ip, src_ip, NET_PROTOCOL_TCP) != 0) {
            TINYTCP_LOG_ERROR(g_logger) << "tcp check sum failed";
            return net_err_t::NET_ERR_BROKEN;
        }
    }
    // 包检查
    if (buf->get_capacity() < sizeof(tcp_hdr_t) || buf->get_capacity() < tcp_hdr->get_header_size()) {
        TINYTCP_LOG_WARN(g_logger) << "tcp pkt size error";
        return net_err_t::NET_ERR_SIZE;
    }
    if (!tcp_hdr->sport || !tcp_hdr->dport) {
        TINYTCP_LOG_WARN(g_logger) << "tcp pkt port error";
        return net_err_t::NET_ERR_BROKEN;
    }
    if (tcp_hdr->flag == 0) {
        TINYTCP_LOG_WARN(g_logger) << "tcp flag error";
        return net_err_t::NET_ERR_BROKEN;
    }

    uint16_t sport = net_to_host(tcp_hdr->sport);
    uint16_t dport = net_to_host(tcp_hdr->dport);
    uint32_t seq = net_to_host(tcp_hdr->seq);
    uint32_t ack = net_to_host(tcp_hdr->ack);
    uint16_t win = net_to_host(tcp_hdr->win);
    uint16_t urgptr = net_to_host(tcp_hdr->urgptr);

    tcp_seg_t seg(buf, dest_ip, src_ip);

    TCPSock* tcp = tcp_find(dest_ip, dport, src_ip, sport);
    if (!tcp) {
        TINYTCP_LOG_INFO(g_logger) << "no tcp found";
        tcp_send_reset(seg);
        return net_err_t::NET_ERR_OK;
    }

    auto it = s_tcp_state_proc.find(tcp->get_state());
    if (it != s_tcp_state_proc.end()) {
        it->second(tcp, &seg);
    }
    else {
        TINYTCP_LOG_ERROR(g_logger) << "no state proc!!!";
        return net_err_t::NET_ERR_PARAM;
    }

    return net_err_t::NET_ERR_OK;
}

net_err_t tcp_send_reset(tcp_seg_t& seg) {
    PktBuffer::ptr pktbuf = PktMgr::get_instance()->get_pktbuffer();
    if (!pktbuf) {
        return net_err_t::NET_ERR_MEM;
    }
    bool ok = pktbuf->alloc(sizeof(tcp_hdr_t));
    CHECK_NET_ERROR(ok ? net_err_t::NET_ERR_OK : net_err_t::NET_ERR_MEM, "alloc error");

    tcp_hdr_t* in = seg.hdr;
    tcp_hdr_t* out = (tcp_hdr_t*)pktbuf->get_data();
    out->sport = in->dport;
    out->dport = in->sport;
    out->flag = 0;
    out->f_rst = 1;
    out->set_header_size(sizeof(tcp_hdr_t));

    if (in->f_ack) {
        out->seq = in->ack;
        out->ack = 0;
        out->f_ack = 0;
    }
    else {
        out->ack = host_to_net(net_to_host(in->seq) + seg.seq_len);
        out->f_ack = 1;
    }

    out->win = out->urgptr = 0;
    return out->send_out(pktbuf, seg.remote_ip, seg.local_ip);
}

net_err_t TCPSock::transmit() {
    PktBuffer::ptr pktbuf = PktMgr::get_instance()->get_pktbuffer();
    if (!pktbuf) {
        TINYTCP_LOG_WARN(g_logger) << "transmit get buf error";
        return net_err_t::NET_ERR_MEM;
    }
    bool ok = pktbuf->alloc(sizeof(tcp_hdr_t));
    CHECK_NET_ERROR(ok ? net_err_t::NET_ERR_OK : net_err_t::NET_ERR_MEM, "alloc error");
    pktbuf->reset_access();

    tcp_hdr_t* hdr = (tcp_hdr_t*)pktbuf->get_data();
    memset(hdr, 0, sizeof(tcp_hdr_t));
    hdr->sport = host_to_net(m_local_port);
    hdr->dport = host_to_net(m_remote_port);
    hdr->seq = host_to_net(m_send.nxt);
    hdr->ack = host_to_net(m_recv.nxt);
    hdr->flag = 0;
    hdr->f_syn = flags.syn_out;
    hdr->f_ack = flags.irs_valid;
    hdr->f_fin = flags.fin_out;
    hdr->win = host_to_net((uint16_t)1024);
    hdr->urgptr = 0;
    hdr->set_header_size(sizeof(tcp_hdr_t));

    m_send.nxt += hdr->f_syn + hdr->f_fin;


    return hdr->send_out(pktbuf, m_remote_ip, m_local_ip);
}

net_err_t tcp_abort(TCPSock* tcp, net_err_t err) {
    tcp->set_state(TCP_STATE_CLOSED);
    tcp->wakeup(SOCK_WAIT_ALL, err);
    return net_err_t::NET_ERR_OK;
}

net_err_t tcp_ack_process(TCPSock* tcp, tcp_seg_t* seg) {
    INIT_TCP_HDR;

    // 响应报文的处理，una(未响应地址)+1
    if (tcp->flags.syn_out) {
        tcp->m_send.una++;
        tcp->flags.syn_out = 0;
    }

    return net_err_t::NET_ERR_OK;
}

net_err_t tcp_send_ack(TCPSock* tcp, tcp_seg_t* seg) {
    ALLOC_PKTBUF(sizeof(tcp_hdr_t));
    tcp_hdr_t* hdr = (tcp_hdr_t*)pktbuf->get_data();
    memset(hdr, 0, sizeof(tcp_hdr_t));
    hdr->sport = host_to_net(tcp->get_local_port());
    hdr->dport = host_to_net(tcp->get_remote_port());
    hdr->seq = host_to_net(tcp->m_send.nxt);
    hdr->ack = host_to_net(tcp->m_recv.nxt);
    hdr->flag = 0;
    hdr->f_syn = 0;
    hdr->f_ack = 1;
    hdr->win = host_to_net((uint16_t)1024);
    hdr->urgptr = 0;
    hdr->set_header_size(sizeof(tcp_hdr_t));

    return hdr->send_out(pktbuf, tcp->get_remote_ip(), tcp->get_local_ip());
}

net_err_t tcp_data_in(TCPSock* tcp, tcp_seg_t* seg) {
    INIT_TCP_HDR;

    int wakeup = 0;

    if (tcp_hdr->f_fin) {
        tcp->m_recv.nxt++;
        ++wakeup;
    }

    if (wakeup) {
        if (tcp_hdr->f_fin) {
            tcp->wakeup(SOCK_WAIT_ALL, net_err_t::NET_ERR_CLOSE);
        }
        else {
            tcp->wakeup(SOCK_WAIT_READ, net_err_t::NET_ERR_OK);
        }
        tcp_send_ack(tcp, seg);
    }

    return net_err_t::NET_ERR_OK;
}

net_err_t tcp_time_wait(TCPSock* tcp, tcp_seg_t* seg) {
    tcp->set_state(TCP_STATE_TIME_WAIT);
    return net_err_t::NET_ERR_OK;
}

std::ostream& operator<<(std::ostream& os, const tcp_hdr_t& hdr) {
    os  << "sport=" << net_to_host(hdr.sport)
        << "\ndport=" << net_to_host(hdr.dport)
        << "\nseq=" << net_to_host(hdr.ack)
        << "\nack=" << net_to_host(hdr.ack)
        << "\nresv=" << hdr.resv
        << "\nshdr" << hdr.shdr
        << "\nf_fin=" << hdr.f_fin
        << "\nf_syn=" << hdr.f_syn
        << "\nf_rst=" << hdr.f_rst
        << "\nf_psh=" << hdr.f_psh
        << "\nf_ack=" << hdr.f_ack
        << "\nf_urg=" << hdr.f_urg
        << "\nf_ece=" << hdr.f_ece
        << "\nf_cwr=" << hdr.f_cwr
        << "\nwin=" << hdr.win
        << "\nchecksum=" << hdr.checksum
        << "\nurgptr=" << hdr.urgptr;

    return os;
}


std::ostream& operator<<(std::ostream& os, const tcp_seg_t& seg) {
    os  << "local_ip=" << seg.local_ip
        << "\nremote_ip=" << seg.remote_ip
        << "\nhdr=\n" << *seg.hdr
        << "\ndata_len=" << seg.data_len
        << "\nseq=" << seg.seq
        << "\nseq_len=" << seg.seq_len;
    return os;
}


} // namespace tinytcp

