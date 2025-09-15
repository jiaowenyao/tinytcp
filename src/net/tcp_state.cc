#include "tcp_state.h"
#include "tcp.h"
#include "src/log.h"
#include "src/magic_enum.h"


namespace tinytcp {

static Logger::ptr g_logger = TINYTCP_LOG_NAME("system");

#define ENUM_NAME(e) \
    magic_enum::enum_name((e))

net_err_t tcp_closed_in(TCPSock* tcp, tcp_seg_t* seg) {
    if (!seg->hdr->f_rst) {
        tcp_send_reset(*seg);
    }
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

        // 建立连接时处理对端发送过来的连接选项
        tcp->tcp_read_option(tcp_hdr);

        if (tcp_hdr->f_ack) {
            tcp_ack_process(tcp, seg);
        }

        if (tcp_hdr->f_ack) {
            tcp_send_ack(tcp, seg);
            tcp->set_state(TCP_STATE_ESTABLISHED);
            tcp->wakeup(SOCK_WAIT_CONN, net_err_t::NET_ERR_OK);
        }
        else { // 处理可能出现的同时发起请求，4次握手
            tcp->set_state(TCP_STATE_SYN_RECVD);
            tcp->tcp_send_syn();
        }
    }

    return net_err_t::NET_ERR_OK;
}

net_err_t tcp_syn_recvd_in(TCPSock* tcp, tcp_seg_t* seg) {

    return net_err_t::NET_ERR_OK;
}

net_err_t tcp_established_in(TCPSock* tcp, tcp_seg_t* seg) {
    INIT_TCP_HDR;

    if (tcp_hdr->f_rst) {
        TINYTCP_LOG_WARN(g_logger) << "recv a rst";
        return tcp_abort(tcp, net_err_t::NET_ERR_RESET);
    }

    if (tcp_hdr->f_syn) {
        TINYTCP_LOG_WARN(g_logger) << "recv a syn";
        tcp_send_reset(*seg);
        return tcp_abort(tcp, net_err_t::NET_ERR_RESET);
    }

    net_err_t err = tcp_ack_process(tcp, seg);
    if ((int8_t)err < 0) {
        TINYTCP_LOG_WARN(g_logger) << "ack process failed";
        return net_err_t::NET_ERR_UNREACH;
    }

    tcp_data_in(tcp, seg);

    // 收到数据后，看有没有数据需要传输
    tcp->transmit();

    if (tcp_hdr->f_fin) {
        tcp->set_state(TCP_STATE_CLOSE_WAIT);
    }
    return net_err_t::NET_ERR_OK;
}

net_err_t tcp_fin_wait_1_in(TCPSock* tcp, tcp_seg_t* seg) {
    INIT_TCP_HDR;

    if (tcp_hdr->f_rst) {
        TINYTCP_LOG_WARN(g_logger) << "recv a rst";
        return tcp_abort(tcp, net_err_t::NET_ERR_RESET);
    }

    if (tcp_hdr->f_syn) {
        TINYTCP_LOG_WARN(g_logger) << "recv a syn";
        tcp_send_reset(*seg);
        return tcp_abort(tcp, net_err_t::NET_ERR_RESET);
    }

    net_err_t err = tcp_ack_process(tcp, seg);
    if ((int8_t)err < 0) {
        TINYTCP_LOG_WARN(g_logger) << "ack process failed";
        return net_err_t::NET_ERR_UNREACH;
    }

    tcp_data_in(tcp, seg);

    tcp->transmit();

    if (tcp->flags.fin_out == 0) {
        if (tcp_hdr->f_fin) {
            tcp_time_wait(tcp, seg);
        }
        else {
            tcp->set_state(TCP_STATE_FIN_WAIT_2);
        }
    }
    else if (tcp_hdr->f_fin) { // 否则就是触发了两端同时发送关闭请求的过程，进入CLOSING状态
        tcp->set_state(TCP_STATE_CLOSING);
    }

    return net_err_t::NET_ERR_OK;
}

net_err_t tcp_fin_wait_2_in(TCPSock* tcp, tcp_seg_t* seg) {
    INIT_TCP_HDR;

    if (tcp_hdr->f_rst) {
        TINYTCP_LOG_WARN(g_logger) << "recv a rst";
        return tcp_abort(tcp, net_err_t::NET_ERR_RESET);
    }

    if (tcp_hdr->f_syn) {
        TINYTCP_LOG_WARN(g_logger) << "recv a syn";
        tcp_send_reset(*seg);
        return tcp_abort(tcp, net_err_t::NET_ERR_RESET);
    }

    net_err_t err = tcp_ack_process(tcp, seg);
    if ((int8_t)err < 0) {
        TINYTCP_LOG_WARN(g_logger) << "ack process failed";
        return net_err_t::NET_ERR_UNREACH;
    }

    tcp_data_in(tcp, seg);

    if (tcp_hdr->f_fin) {
        tcp_time_wait(tcp, seg);
    }

    return net_err_t::NET_ERR_OK;
}

net_err_t tcp_closing_in(TCPSock* tcp, tcp_seg_t* seg) {
    INIT_TCP_HDR;

    if (tcp_hdr->f_rst) {
        TINYTCP_LOG_WARN(g_logger) << "recv a rst";
        return tcp_abort(tcp, net_err_t::NET_ERR_RESET);
    }

    if (tcp_hdr->f_syn) {
        TINYTCP_LOG_WARN(g_logger) << "recv a syn";
        tcp_send_reset(*seg);
        return tcp_abort(tcp, net_err_t::NET_ERR_RESET);
    }

    net_err_t err = tcp_ack_process(tcp, seg);
    if ((int8_t)err < 0) {
        TINYTCP_LOG_WARN(g_logger) << "ack process failed";
        return net_err_t::NET_ERR_UNREACH;
    }

    tcp->transmit();

    if (tcp->flags.fin_out == 0) {
        tcp_time_wait(tcp, seg);
    }

    return net_err_t::NET_ERR_OK;
}

net_err_t tcp_time_wait_in(TCPSock* tcp, tcp_seg_t* seg) {
    INIT_TCP_HDR;

    if (tcp_hdr->f_rst) {
        TINYTCP_LOG_WARN(g_logger) << "recv a rst";
        return tcp_abort(tcp, net_err_t::NET_ERR_RESET);
    }

    if (tcp_hdr->f_syn) {
        TINYTCP_LOG_WARN(g_logger) << "recv a syn";
        tcp_send_reset(*seg);
        return tcp_abort(tcp, net_err_t::NET_ERR_RESET);
    }

    net_err_t err = tcp_ack_process(tcp, seg);
    if ((int8_t)err < 0) {
        TINYTCP_LOG_WARN(g_logger) << "ack process failed";
        return net_err_t::NET_ERR_UNREACH;
    }

    if (tcp_hdr->f_fin) {
        tcp_send_ack(tcp, seg);
        tcp_time_wait(tcp, seg);
    }


    return net_err_t::NET_ERR_OK;
}

net_err_t tcp_close_wait_in(TCPSock* tcp, tcp_seg_t* seg) {
    INIT_TCP_HDR;

    if (tcp_hdr->f_rst) {
        TINYTCP_LOG_WARN(g_logger) << "recv a rst";
        return tcp_abort(tcp, net_err_t::NET_ERR_RESET);
    }

    if (tcp_hdr->f_syn) {
        TINYTCP_LOG_WARN(g_logger) << "recv a syn";
        tcp_send_reset(*seg);
        return tcp_abort(tcp, net_err_t::NET_ERR_RESET);
    }

    net_err_t err = tcp_ack_process(tcp, seg);
    if ((int8_t)err < 0) {
        TINYTCP_LOG_WARN(g_logger) << "ack process failed";
        return net_err_t::NET_ERR_UNREACH;
    }

    tcp->transmit();

    return net_err_t::NET_ERR_OK;
}

net_err_t tcp_last_ack_in(TCPSock* tcp, tcp_seg_t* seg) {
    INIT_TCP_HDR;

    if (tcp_hdr->f_rst) {
        TINYTCP_LOG_WARN(g_logger) << "recv a rst";
        return tcp_abort(tcp, net_err_t::NET_ERR_RESET);
    }

    if (tcp_hdr->f_syn) {
        TINYTCP_LOG_WARN(g_logger) << "recv a syn";
        tcp_send_reset(*seg);
        return tcp_abort(tcp, net_err_t::NET_ERR_RESET);
    }

    net_err_t err = tcp_ack_process(tcp, seg);
    if ((int8_t)err < 0) {
        TINYTCP_LOG_WARN(g_logger) << "ack process failed";
        return net_err_t::NET_ERR_UNREACH;
    }

    tcp->transmit();

    // 收到对端的回应后才调用tcp_abort关闭连接
    if (tcp->flags.fin_out == 0) {
        return tcp_abort(tcp, net_err_t::NET_ERR_CLOSE);
    }

    return net_err_t::NET_ERR_OK;
}



} // namespace tinytcp



