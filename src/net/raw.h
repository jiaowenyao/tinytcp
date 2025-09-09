#pragma once
#include "sock.h"
#include "pktbuf.h"
#include "lock_free_ring_queue.h"


namespace tinytcp {


class RAWSock : public Sock {
public:
    RAWSock(int family, int protocol);
    ~RAWSock();
    net_err_t sendto(const void* buf, size_t len, int flags,
                             const struct sockaddr* dest, socklen_t dest_len,
                             ssize_t* result_len) override;
    net_err_t recvfrom(const void* buf, size_t len, int flags,
                             const struct sockaddr* src, socklen_t src_len,
                             ssize_t* result_len) override;
    net_err_t setopt(int level, int optname,
                             const char* optval, int optlen) override;
};


net_err_t raw_in(PktBuffer::ptr buf);


} // namespace tinytcp

