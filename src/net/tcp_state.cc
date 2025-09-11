#include "tcp_state.h"
#include "tcp.h"
#include "src/log.h"
#include "src/magic_enum.h"


namespace tinytcp {

static Logger::ptr g_logger = TINYTCP_LOG_NAME("system");

#define ENUM_NAME(e) \
    magic_enum::enum_name((e))

net_err_t tcp_closed_in(TCPSock* tcp, tcp_seg_t* seg) {

    return net_err_t::NET_ERR_OK;
}

net_err_t tcp_listen_in(TCPSock* tcp, tcp_seg_t* seg) {

    return net_err_t::NET_ERR_OK;
}

net_err_t tcp_syn_sent_in(TCPSock* tcp, tcp_seg_t* seg) {
    INIT_TCP_HDR;
    if (tcp_hdr->f_ack) {
        if (ack <= tcp->m_send.iss || ack > tcp->m_send.nxt) {
            TINYTCP_LOG_WARN(g_logger) << "ack error, state=" << ENUM_NAME(tcp->get_state());
            return tcp_send_reset(*seg);
        }
    }

    if (tcp_hdr->f_rst) {
        if (!tcp_hdr->f_ack) {
            return net_err_t::NET_ERR_OK;
        }
        return tcp_abort(tcp, net_err_t::NET_ERR_RESET);
    }

    if (tcp_hdr->f_syn) {
        tcp->m_recv.iss = seq;
        tcp->m_recv.nxt = seq + 1;
        tcp->flags.irs_valid = 1;
        if (tcp_hdr->f_ack) {
            tcp_ack_process(tcp, seg);
        }
    }

    return net_err_t::NET_ERR_OK;
}

net_err_t tcp_syn_recvd_in(TCPSock* tcp, tcp_seg_t* seg) {

    return net_err_t::NET_ERR_OK;
}

net_err_t tcp_established_in(TCPSock* tcp, tcp_seg_t* seg) {

    return net_err_t::NET_ERR_OK;
}

net_err_t tcp_fin_wait_1_in(TCPSock* tcp, tcp_seg_t* seg) {

    return net_err_t::NET_ERR_OK;
}

net_err_t tcp_fin_wait_2_in(TCPSock* tcp, tcp_seg_t* seg) {

    return net_err_t::NET_ERR_OK;
}

net_err_t tcp_closing_in(TCPSock* tcp, tcp_seg_t* seg) {

    return net_err_t::NET_ERR_OK;
}

net_err_t tcp_time_wait_in(TCPSock* tcp, tcp_seg_t* seg) {

    return net_err_t::NET_ERR_OK;
}

net_err_t tcp_close_wait_in(TCPSock* tcp, tcp_seg_t* seg) {

    return net_err_t::NET_ERR_OK;
}

net_err_t tcp_last_ack_in(TCPSock* tcp, tcp_seg_t* seg) {

    return net_err_t::NET_ERR_OK;
}



} // namespace tinytcp



