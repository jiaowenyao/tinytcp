#pragma once
#include "sock.h"
#include "src/endiantool.h"


namespace tinytcp {

#pragma pack(1)
struct tcp_hdr_t {
    uint16_t sport;
    uint16_t dport;
    uint32_t seq;
    uint32_t ack;
    union {
        uint16_t flag;
#if BYTE_ORDER == LITTLE_ENDIAN
        struct {
            uint16_t resv   : 4;
            uint16_t shdr   : 4;
            uint16_t f_fin  : 1;
            uint16_t f_syn  : 1;
            uint16_t f_rst  : 1;
            uint16_t f_psh  : 1;
            uint16_t f_ack  : 1;
            uint16_t f_urg  : 1;
            uint16_t f_ece  : 1;
            uint16_t f_cwr  : 1;
        };
#else
        struct {
            uint16_t shdr   : 4;
            uint16_t resv   : 4;
            uint16_t f_cwr  : 1;
            uint16_t f_ece  : 1;
            uint16_t f_urg  : 1;
            uint16_t f_ack  : 1;
            uint16_t f_psh  : 1;
            uint16_t f_rst  : 1;
            uint16_t f_syn  : 1;
            uint16_t f_fin  : 1;
        };
#endif
    };
    uint16_t win;  // 窗口大小
    uint16_t checksum;
    uint16_t urgptr;

    uint32_t get_header_size() noexcept { return shdr * 4; }
    void set_header_size(int size) noexcept { shdr = size / 4; }
    net_err_t send_out(PktBuffer::ptr buf, const ipaddr_t& remote_ip, const ipaddr_t& local_ip);
};

struct tcp_pkt_t {
    tcp_hdr_t hdr;
    uint8_t data[1];
};
#pragma pack()

struct tcp_seg_t {
    ipaddr_t local_ip;
    ipaddr_t remote_ip;
    tcp_hdr_t* hdr;
    PktBuffer::ptr buf;
    uint32_t data_len;
    uint32_t seq;
    uint32_t seq_len;

    tcp_seg_t(PktBuffer::ptr _buf, const ipaddr_t& local, const ipaddr_t& remote)
        : local_ip(local)
        , remote_ip(remote)
        , hdr((tcp_hdr_t*)_buf->get_data())
        , buf(_buf) {
        data_len = buf->get_capacity() - hdr->get_header_size();
        seq = hdr->seq;
        seq_len = data_len + hdr->f_syn + hdr->f_fin;
    }
};

enum tcp_state_t {
    TCP_STATE_CLOSED,
    TCP_STATE_LISTEN,
    TCP_STATE_SYN_SENT,
    TCP_STATE_SYN_RECVD,
    TCP_STATE_ESTABLISHED,
    TCP_STATE_FIN_WAIT_1,
    TCP_STATE_FIN_WAIT_2,
    TCP_STATE_CLOSING,
    TCP_STATE_TIME_WAIT,
    TCP_STATE_CLOSE_WAIT,
    TCP_STATE_LAST_ACK,

    TCP_STATE_MAX,
};

#define INIT_TCP_HDR                          \
    tcp_hdr_t* tcp_hdr = seg->hdr;            \
    uint32_t ack = net_to_host(tcp_hdr->ack); \
    uint32_t seq = net_to_host(tcp_hdr->seq);


class TCPSock : public Sock {
public:
    TCPSock(int family, int protocol);
    ~TCPSock();
    void init() override;

    net_err_t sendto(const void* buf, size_t len, int flags,
                             const struct sockaddr* dest, socklen_t dest_len,
                             ssize_t* result_len) override;
    net_err_t recvfrom(const void* buf, size_t len, int flags,
                             struct sockaddr* src, socklen_t src_len,
                             ssize_t* result_len) override;
    net_err_t connect(sockaddr* addr, socklen_t addr_len) override;
    net_err_t close() override;

    net_err_t transmit(); // 发送处理函数

    net_err_t tcp_init_connect();
    net_err_t tcp_send_syn();

    tcp_state_t get_state() const noexcept { return m_state; }
    void set_state(tcp_state_t state) noexcept { m_state = state; }
    tcp_state_t m_state;
public:
    struct {
        uint32_t una; // 没有确认的数据
        uint32_t nxt; // 下一个需要发送的数据
        uint32_t iss; // 起始序列号
        sock_wait_t wait;
    } m_send;

    struct {
        uint32_t nxt; // 下一个期望接收的数据
        uint32_t iss; // 初始序号
        sock_wait_t wait;
    } m_recv;

    struct {
        uint32_t syn_out   : 1; // 为1的话,syn已经发送
        uint32_t irs_valid : 1; // 收到了对端的syn
    } flags;
};

net_err_t tcp_in(PktBuffer::ptr buf, const ipaddr_t& src_ip, const ipaddr_t& dest_ip);
net_err_t tcp_send_reset(tcp_seg_t& seg);
// 终止当前tcp的通信
net_err_t tcp_abort(TCPSock* tcp, net_err_t err);
net_err_t tcp_ack_process(TCPSock* tcp, tcp_seg_t* seg);


std::ostream& operator<<(std::ostream& os, const tcp_hdr_t& hdr);
std::ostream& operator<<(std::ostream& os, const tcp_seg_t& seg);




} // namespace tinytcp

