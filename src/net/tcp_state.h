#pragma once
#include "net_err.h"
#include <functional>


namespace tinytcp {


class TCPSock;
class tcp_seg_t;

using tcp_proc_ptr = net_err_t(*)(TCPSock*, tcp_seg_t*);
using tcp_proc_func = std::function<net_err_t(TCPSock*, tcp_seg_t*)>;

net_err_t tcp_closed_in(TCPSock* tcp, tcp_seg_t* seg);

net_err_t tcp_listen_in(TCPSock* tcp, tcp_seg_t* seg);

net_err_t tcp_syn_sent_in(TCPSock* tcp, tcp_seg_t* seg);

net_err_t tcp_syn_recvd_in(TCPSock* tcp, tcp_seg_t* seg);

net_err_t tcp_established_in(TCPSock* tcp, tcp_seg_t* seg);

net_err_t tcp_fin_wait_1_in(TCPSock* tcp, tcp_seg_t* seg);

net_err_t tcp_fin_wait_2_in(TCPSock* tcp, tcp_seg_t* seg);

net_err_t tcp_closing_in(TCPSock* tcp, tcp_seg_t* seg);

net_err_t tcp_time_wait_in(TCPSock* tcp, tcp_seg_t* seg);

net_err_t tcp_close_wait_in(TCPSock* tcp, tcp_seg_t* seg);

net_err_t tcp_last_ack_in(TCPSock* tcp, tcp_seg_t* seg);

} // namespace tinytcp
