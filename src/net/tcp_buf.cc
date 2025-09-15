#include "tcp_buf.h"
#include "src/log.h"
#include "src/config.h"
#include "src/macro.h"


namespace tinytcp {

static Logger::ptr g_logger = TINYTCP_LOG_NAME("system");
static ConfigVar<uint32_t>::ptr g_tcp_send_buffer_size =
    Config::look_up<uint32_t>("tcp.tcp_send_buffer_size", 1024U, "tcp发送队列大小");


TCPBuffer::TCPBuffer()
    : m_data(new uint8_t[g_tcp_send_buffer_size->value()])
    , m_size(0)
    , m_capacity(g_tcp_send_buffer_size->value())
    , m_in(0)
    , m_out(0) {
}

TCPBuffer::~TCPBuffer() {
    if (m_data) {
        delete[] m_data;
    }
}


void TCPBuffer::tcp_buf_write_send(const char* buffer, uint32_t len) {
    while (len > 0) {
        m_data[m_in++] = *buffer++;
        if (m_in >= m_capacity) {
            m_in = 0;
        }
        --len;
        ++m_size;
    }
}

void TCPBuffer::tcp_buf_read_send(PktBuffer::ptr pktbuf, int data_offset, int data_len) {
    // 超过要求的数据量，进行调整
    int free_for_us = m_size - data_offset;      // 跳过offset之前的数据
    if (data_len > free_for_us) {
        TINYTCP_LOG_WARN(g_logger) << "resize for send: " << data_len << " -> " << free_for_us;
        data_len = free_for_us;
    }

    // 复制过程中要考虑buf中的数据回绕的问题
    int start = m_out + data_offset;     // 注意拷贝的偏移
    if (start >= m_capacity) {
        start -= m_capacity;
    }

    while (data_len > 0) {
        // 当前超过末端，则只拷贝到末端的区域
        int end = start + data_len;
        if (end >= m_capacity) {
            end = m_capacity;
        }
        int copy_size = (int)(end - start);

        // 写入数据
        net_err_t err = pktbuf->write(m_data + start, copy_size);
        TINYTCP_ASSERT2((int8_t)err >= 0, "write buffer failed.");

        // 更新start，处理回绕的问题
        start += copy_size;
        if (start >= m_capacity) {
            start -= m_capacity;
        }
        data_len -= copy_size;
    }
}

int TCPBuffer::tcp_buf_remove(int cnt) {
    if (cnt > m_size) {
        cnt = m_size;
    }
    m_out += cnt;
    if (m_out >= m_capacity) {
        m_out -= m_capacity;
    }
    m_size -= cnt;
    return cnt;
}

int TCPBuffer::tcp_buf_write_recv(PktBuffer::ptr pktbuf, int data_offset, int total) {
    int start = m_in + data_offset;
    if (start >= m_capacity) {
        start = start - m_capacity;
    }

    // 计算实际可写的数据量
    int free_size = get_free_size() - data_offset;
    total = (total > free_size) ? free_size : total;

    int size = total;
    while (size > 0) {
        // 从start到缓存末端的单元数量，可能其中有数据也可能没有
        int free_to_end = m_capacity - start;

        // 大小超过到尾部的空闲数据量，只拷贝一部分
        int curr_copy = size > free_to_end ? free_to_end : size;
        pktbuf->read(m_data + start, curr_copy);

        start += curr_copy;
        if (start >= m_capacity) {
            start = start - m_capacity;
        }

        // 增加已写入的数据量
        m_size += curr_copy;
        size -= curr_copy;
    }

    m_in = start;
    return total;
}

int TCPBuffer::tcp_buf_read_recv(char* buf, int data_len) {
    int total = std::min(m_size, data_len);

    int curr_size = 0;
    while (curr_size < total) {
        *buf++ = m_data[m_out++];
        if (m_out >= m_capacity) {
            m_out = 0;
        }
        --m_size;
        ++curr_size;
    }
    return total;
}




} // namespace tinytcp

