#pragma once
#include "sock.h"
#include "tcp_buf.h"
#include "src/endiantool.h"


namespace tinytcp {

#pragma pack(1)
struct tcp_opt_mss_t {
    uint8_t kind;
    uint8_t length;
    uint16_t mss;
};
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
        seq = net_to_host(hdr->seq);
        seq_len = data_len + hdr->f_syn + hdr->f_fin;
    }
};

enum tcp_state_t {
    TCP_STATE_FREE = 0,
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

#define TCP_DEFAULT_MSS         536
#define TCP_OPT_END             0
#define TCP_OPT_NOP             1
#define TCP_OPT_MSS             2

class TCPSock : public Sock {
public:
    TCPSock(int family, int protocol);
    ~TCPSock();
    void init() override;

    net_err_t sendto(const void* buf, size_t len, int flags,
                             const struct sockaddr* dest, socklen_t dest_len,
                             ssize_t* result_len) override;
    net_err_t send(const void* buf, size_t len, int flags, ssize_t* result_len) override;
    net_err_t recvfrom(void* buf, size_t len, int flags,
                             struct sockaddr* src, socklen_t src_len,
                             ssize_t* result_len) override;
    net_err_t recv(void* buf, size_t len, int flags, ssize_t* result_len) override;
    net_err_t connect(sockaddr* addr, socklen_t addr_len) override;
    net_err_t close() override;
    net_err_t free();

    net_err_t transmit(); // 发送处理函数

    net_err_t tcp_init_connect();
    net_err_t tcp_send_syn();
    net_err_t tcp_send_fin();
    void tcp_read_option(tcp_hdr_t* hdr);

    tcp_state_t get_state() const noexcept { return m_state; }
    void set_state(tcp_state_t state) noexcept { m_state = state; }

    int tcp_write_send_buf(const char* buf, int len);
    void get_send_info(int& data_offset, int& data_len);
    int copy_send_data(PktBuffer::ptr pktbuf, int data_offset, int data_len);
    int copy_data_to_recvbuf(tcp_seg_t* seg);
    void write_sync_option(PktBuffer::ptr buf);
    uint16_t get_tcp_recv_window();
    bool tcp_seq_acceptable(tcp_seg_t* seg);

public:
    tcp_state_t m_state;
    int m_mss;
    struct {
        TCPBuffer buf;
        uint32_t una; // 没有确认的数据
        uint32_t nxt; // 下一个需要发送的数据
        uint32_t iss; // 起始序列号
        sock_wait_t wait;
    } m_send;

    struct {
        TCPBuffer buf;
        uint32_t nxt; // 下一个期望接收的数据
        uint32_t iss; // 初始序号
        sock_wait_t wait;
    } m_recv;

    struct {
        uint32_t syn_out   : 1; // 为1的话,syn已经发送
        uint32_t fin_in    : 1; // 接收是否真的处于完毕的状态
        uint32_t fin_out   : 1; // 要发送fin标志位
        uint32_t irs_valid : 1; // 收到了对端的syn
    } flags;
};

net_err_t tcp_in(PktBuffer::ptr buf, const ipaddr_t& src_ip, const ipaddr_t& dest_ip);
net_err_t tcp_send_reset(tcp_seg_t& seg);
// 终止当前tcp的通信
net_err_t tcp_abort(TCPSock* tcp, net_err_t err);
net_err_t tcp_ack_process(TCPSock* tcp, tcp_seg_t* seg);
net_err_t tcp_send_ack(TCPSock* tcp, tcp_seg_t* seg);
net_err_t tcp_data_in(TCPSock* tcp, tcp_seg_t* seg); // data检查, 也能给fin报文发送ack
net_err_t tcp_time_wait(TCPSock* tcp, tcp_seg_t* seg);

std::ostream& operator<<(std::ostream& os, const tcp_hdr_t& hdr);
std::ostream& operator<<(std::ostream& os, const tcp_seg_t& seg);

// a <= b
#define TCP_SEQ_LE(a, b)    (((int32_t)(a) - (int32_t)(b)) <= 0)
#define TCP_SEQ_LT(a, b)    (((int32_t)(a) - (int32_t)(b)) < 0)



} // namespace tinytcp

