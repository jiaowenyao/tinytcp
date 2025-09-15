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

    route_entry_t* rt = ProtocolStackMgr::get_instance()->get_ipprotocol()->route_find(m_remote_ip);
    if (rt->netif->get_mtu()) {
        m_mss = TCP_DEFAULT_MSS;
    }
    else if(!(rt->next_hop == ipaddr_t(0U))) {
        m_mss = TCP_DEFAULT_MSS;
    }
    else {
        m_mss = rt->netif->get_mtu() - sizeof(ipv4_hdr_t) - sizeof(tcp_hdr_t);
    }
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

int TCPSock::tcp_write_send_buf(const char* buf, int len) {
    int free_cnt = m_send.buf.get_free_size();
    if (free_cnt <= 0) {
        return 0;
    }

    int write_len = std::min(len, free_cnt);
    m_send.buf.tcp_buf_write_send(buf, write_len);
    return write_len;
}

net_err_t TCPSock::send(const void* buf, size_t len, int flags, ssize_t* result_len) {
    switch ((int)m_state) {
        case TCP_STATE_CLOSED: {
            TINYTCP_LOG_ERROR(g_logger) << "tcp closed.";
            return net_err_t::NET_ERR_CLOSE;
        }
        case TCP_STATE_FIN_WAIT_1:
        case TCP_STATE_FIN_WAIT_2:
        case TCP_STATE_TIME_WAIT:
        case TCP_STATE_LAST_ACK:
        case TCP_STATE_CLOSING: {
            TINYTCP_LOG_ERROR(g_logger) << "tcp closed.";
            return net_err_t::NET_ERR_CLOSE;
        }
        case TCP_STATE_CLOSE_WAIT:
        case TCP_STATE_ESTABLISHED: {
            break;
        }
        case TCP_STATE_LISTEN:
        case TCP_STATE_SYN_RECVD:
        case TCP_STATE_SYN_SENT:
        default: {
            TINYTCP_LOG_ERROR(g_logger) << "tcp state error.";
            return net_err_t::NET_ERR_STATE;
        }
    }

    int size = tcp_write_send_buf((const char*)buf, len);
    if (size <= 0) {
        *result_len = 0;
        return net_err_t::NET_ERR_NEED_WAIT;
    }
    else {
        *result_len = size;
        transmit();
    }

    return net_err_t::NET_ERR_OK;
}

net_err_t TCPSock::recvfrom(void* buf, size_t len, int flags,
                            struct sockaddr* src, socklen_t src_len,
                            ssize_t* result_len) {

    return net_err_t::NET_ERR_OK;
}

net_err_t TCPSock::recv(void* buf, size_t len, int flags, ssize_t* result_len) {
    bool need_wait = true;
    switch (m_state) {
        case TCP_STATE_LAST_ACK:
        case TCP_STATE_CLOSED: {
            TINYTCP_LOG_ERROR(g_logger) << "tcp closed";
            return net_err_t::NET_ERR_CLOSE;
        }
        case TCP_STATE_CLOSE_WAIT:
        case TCP_STATE_CLOSING: {
            need_wait = false;
            break;
        }
        case TCP_STATE_FIN_WAIT_1:
        case TCP_STATE_FIN_WAIT_2:
        case TCP_STATE_ESTABLISHED: {
            break;
        }
        case TCP_STATE_LISTEN:
        case TCP_STATE_SYN_SENT:
        case TCP_STATE_SYN_RECVD:
        case TCP_STATE_TIME_WAIT:
        default: {
            TINYTCP_LOG_ERROR(g_logger) << "tcp state error";
            return net_err_t::NET_ERR_STATE;
        }
    }

    *result_len = 0;
    int cnt = m_recv.buf.tcp_buf_read_recv((char*)buf, len);
    if (cnt > 0) {
        *result_len = cnt;
        return net_err_t::NET_ERR_OK;
    }
    if (need_wait) {
        return net_err_t::NET_ERR_NEED_WAIT;
    }
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

void TCPSock::tcp_read_option(tcp_hdr_t* hdr) {
    uint8_t* opt_start = (uint8_t*)hdr + sizeof(tcp_hdr_t);
    uint8_t* opt_end = opt_start + hdr->get_header_size() - sizeof(tcp_hdr_t);
    if (opt_end <= opt_start) {
        return;
    }

    while (opt_start < opt_end) {
        switch (opt_start[0]) {
            case TCP_OPT_MSS: {
                tcp_opt_mss_t* opt = (tcp_opt_mss_t*)opt_start;
                if (opt->length == 4) {
                    uint16_t mss = net_to_host(opt->mss);
                    if (mss < m_mss) {
                        m_mss = mss;
                    }
                }
                opt_start += opt->length;
                break;
            }
            case TCP_OPT_NOP: {
                ++opt_start;
                break;
            }
            case TCP_OPT_END: {
                return;
            }
            default: {
                ++opt_start;
                break;
            }
        }
    }
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

bool TCPSock::tcp_seq_acceptable(tcp_seg_t* seg) {
    uint32_t recv_win = get_tcp_recv_window();
    if (seg->seq_len == 0) {
        if (recv_win == 0) {
            return seg->seq == m_recv.nxt;
        }
        else {
            bool v = TCP_SEQ_LE(m_recv.nxt, seg->seq) && TCP_SEQ_LT(seg->seq, m_recv.nxt + recv_win);
            return v;
        }
    }
    else {
        if (recv_win == 0) {
            return false;
        }
        else {
            bool v = TCP_SEQ_LE(m_recv.nxt, seg->seq) && TCP_SEQ_LT(seg->seq, m_recv.nxt + recv_win);
            uint32_t slast = seg->seq + seg->seq_len - 1;
            v |= (TCP_SEQ_LE(m_recv.nxt, slast) && TCP_SEQ_LT(slast, m_recv.nxt + recv_win));
            return v;
        }
    }
    return false;
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
    net_err_t err = buf->set_cont_header(sizeof(tcp_hdr_t));
    CHECK_NET_ERROR(err, "set_cont_header error");
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
        // tcp_send_reset(seg);
        tcp_closed_in(nullptr, &seg);
        return net_err_t::NET_ERR_OK;
    }

    if ((tcp->m_state != TCP_STATE_CLOSED)
        && (tcp->m_state != TCP_STATE_SYN_SENT)
        && (tcp->m_state != TCP_STATE_LISTEN)) {
        if (!tcp->tcp_seq_acceptable(&seg)) {
            TINYTCP_LOG_ERROR(g_logger) << "seq error";
            return net_err_t::NET_ERR_OK;
        }
    }

    auto it = s_tcp_state_proc.find(tcp->get_state());
    if (it != s_tcp_state_proc.end()) {
        buf->reset_access();
        net_err_t err = buf->seek(tcp_hdr->get_header_size());
        CHECK_NET_ERROR(err, "seek failed");
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

void TCPSock::get_send_info(int& data_offset, int& data_len) {
    data_offset = m_send.nxt - m_send.una;
    data_len = m_send.buf.get_size() - data_offset;
    data_len = std::min(data_len, m_mss);
}

int TCPSock::copy_send_data(PktBuffer::ptr pktbuf, int data_offset, int data_len) {
    if (data_len == 0) {
        return 0;
    }

    net_err_t err = pktbuf->resize(pktbuf->get_capacity() + data_len);
    if ((int8_t)err < 0) {
        TINYTCP_LOG_ERROR(g_logger) << "copy_send_data resize error";
        return -1;
    }

    uint32_t hdr_size = ((tcp_hdr_t*)pktbuf->get_data())->get_header_size();
    pktbuf->reset_access();
    pktbuf->seek(hdr_size);
    m_send.buf.tcp_buf_read_send(pktbuf, data_offset, data_len);
    return data_len;
}

void TCPSock::write_sync_option(PktBuffer::ptr buf) {
    int opt_len = sizeof(tcp_opt_mss_t);
    net_err_t err = buf->resize(buf->get_capacity() + opt_len);
    if ((int8_t)err < 0) {
        TINYTCP_LOG_ERROR(g_logger) << "resize error";
        return;
    }
    tcp_opt_mss_t mss;
    mss.kind = TCP_OPT_MSS;
    mss.length = sizeof(tcp_opt_mss_t);
    mss.mss = net_to_host((uint16_t)m_mss);

    buf->reset_access();
    buf->seek(sizeof(tcp_hdr_t));
    err = buf->write((uint8_t*)&mss, sizeof(mss));
    if ((int8_t)err < 0) {
        TINYTCP_LOG_ERROR(g_logger) << "write error";
        return;
    }
}

net_err_t TCPSock::transmit() {
    int data_offset = 0;
    int data_len = 0;
    get_send_info(data_offset, data_len);
    if (data_len < 0) {
        return net_err_t::NET_ERR_OK;
    }

    int seq_len = data_len;
    if (flags.syn_out) {
        ++seq_len;
    }
    if (flags.fin_out) {
        ++seq_len;
    }
    if (seq_len == 0) {
        return net_err_t::NET_ERR_OK;
    }

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
    hdr->f_fin = 0;
    if (m_send.buf.get_size() == 0) {
        hdr->f_fin = flags.fin_out;
    }
    hdr->win = host_to_net(get_tcp_recv_window());
    hdr->urgptr = 0;
    if (hdr->f_syn) {
        write_sync_option(pktbuf);
    }
    hdr->set_header_size(pktbuf->get_capacity());

    copy_send_data(pktbuf, data_offset, data_len);

    m_send.nxt += hdr->f_syn + hdr->f_fin + data_len;


    return hdr->send_out(pktbuf, m_remote_ip, m_local_ip);
}

net_err_t tcp_abort(TCPSock* tcp, net_err_t err) {
    tcp->set_state(TCP_STATE_CLOSED);
    tcp->wakeup(SOCK_WAIT_ALL, err);
    return net_err_t::NET_ERR_OK;
}

net_err_t tcp_ack_process(TCPSock* tcp, tcp_seg_t* seg) {
    INIT_TCP_HDR;

    // m_send.una < ack <= m_send.nxt
    if (TCP_SEQ_LE(ack, tcp->m_send.una)) {
        return net_err_t::NET_ERR_OK;
    }
    else if (TCP_SEQ_LT(tcp->m_send.nxt, ack)) {
        return net_err_t::NET_ERR_UNREACH;
    }

    // 响应报文的处理，una(未响应地址)+1
    if (tcp->flags.syn_out) {
        tcp->m_send.una++;
        tcp->flags.syn_out = 0;
    }

    int acked_cnt = tcp_hdr->ack - tcp->m_send.una;
    int unacked = tcp->m_send.nxt - tcp->m_send.una;
    int curr_acked = std::min(acked_cnt, unacked);
    if (curr_acked > 0) {
        // 唤醒等待写入的线程
        tcp->wakeup(SOCK_WAIT_WRITE, net_err_t::NET_ERR_OK);
        // 未确认的指针往前移
        tcp->m_send.una += curr_acked;
        // 移除对端已经确认的数据
        curr_acked -= tcp->m_send.buf.tcp_buf_remove(curr_acked);

        // 如果发过来的ack大于我的un ack，则说明对端已经收到了我的fin, 清空标志位，给后续处理
        // if (tcp->flags.fin_out && tcp_hdr->ack > tcp->m_send.una) {
        // 如果curr_acked为1，说明对端接收到了fin
        if (tcp->flags.fin_out && curr_acked) {
            tcp->flags.fin_out = 0;
        }
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
    hdr->win = host_to_net(tcp->get_tcp_recv_window());
    hdr->urgptr = 0;
    hdr->set_header_size(sizeof(tcp_hdr_t));

    return hdr->send_out(pktbuf, tcp->get_remote_ip(), tcp->get_local_ip());
}

int TCPSock::copy_data_to_recvbuf(tcp_seg_t* seg) {
    int data_offset = seg->seq - m_recv.nxt;
    if (seg->data_len && data_offset == 0) {
        return m_recv.buf.tcp_buf_write_recv(seg->buf, data_offset, seg->data_len);
    }
    return 0;
}

net_err_t tcp_data_in(TCPSock* tcp, tcp_seg_t* seg) {
    INIT_TCP_HDR;

    int size = tcp->copy_data_to_recvbuf(seg);
    if (size < 0) {
        TINYTCP_LOG_ERROR(g_logger) << "copy data to recv buf error";
        return net_err_t::NET_ERR_SIZE;
    }

    int wakeup = 0;
    if (size > 0) {
        tcp->m_recv.nxt += size;
        ++wakeup;
    }

    // fin为1，并且通过seq和nxt判断数据确实已经接收完成
    if (tcp_hdr->f_fin && (tcp->m_recv.nxt == seg->seq)) {
        tcp->flags.fin_in = 1;
        tcp->m_recv.nxt++;
        ++wakeup;
    }

    if (wakeup) {
        if (tcp->flags.fin_in) {
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

uint16_t TCPSock::get_tcp_recv_window() {
    uint16_t window = m_recv.buf.get_free_size();
    return window;
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

