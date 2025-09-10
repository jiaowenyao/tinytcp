#pragma once
#include "sock.h"


namespace tinytcp {

#pragma pack(1)
struct udp_from_t {
    uint32_t from;
    int16_t port;
};
struct udp_hdr_t {
    uint16_t src_port;
    uint16_t dest_port;
    uint16_t total_len;
    uint16_t checksum;
};
struct udp_pkt_t {
    udp_hdr_t hdr;
    uint8_t data[1];
};
#pragma pack()


class UDPSock : public Sock {
public:
    UDPSock(int family, int protocol);
    ~UDPSock();
    void init() override;
    net_err_t sendto(const void* buf, size_t len, int flags,
                             const struct sockaddr* dest, socklen_t dest_len,
                             ssize_t* result_len) override;
    net_err_t recvfrom(const void* buf, size_t len, int flags,
                             struct sockaddr* src, socklen_t src_len,
                             ssize_t* result_len) override;

    net_err_t udp_out(const ipaddr_t& dest, uint16_t port, PktBuffer::ptr);
};


net_err_t udp_in(PktBuffer::ptr buf, const ipaddr_t& src_ip, const ipaddr_t& dest_ip);
std::ostream& operator<<(std::ostream& os, const udp_hdr_t& hdr);


} // namespace tinytcp




