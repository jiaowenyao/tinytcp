#pragma once
#include <inttypes.h>
#include "pktbuf.h"

namespace tinytcp {

class TCPBuffer {
friend class TCPSock;
public:
    TCPBuffer();
    ~TCPBuffer();

    int32_t get_size() const noexcept { return m_size; }
    int32_t get_capacity() const noexcept { return m_capacity; }
    int32_t get_free_size() const noexcept { return m_capacity - m_size; }

    void tcp_buf_write_send(const char* buffer, uint32_t len);
    void tcp_buf_read_send(PktBuffer::ptr pktbuf, int data_offset, int data_len);
    int tcp_buf_remove(int cnt);
    int tcp_buf_write_recv(PktBuffer::ptr pktbuf, int data_offset, int total);
    int tcp_buf_read_recv(char* buf, int data_len);

private:
    uint8_t* m_data;
    int32_t m_size;
    int32_t m_capacity;
    int32_t m_in;
    int32_t m_out;
};


} // namespace tinytcp



