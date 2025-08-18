#include "pktbuf.h"
#include "src/config.h"
#include "src/macro.h"
#include <algorithm>

namespace tinytcp {

static tinytcp::Logger::ptr g_logger = TINYTCP_LOG_NAME("system");

static tinytcp::ConfigVar<uint32_t>::ptr g_pktbuf_blk_size =
    tinytcp::Config::look_up("tcp.pktbuf_blk_size", 1024U, "tcp pktbuf block size, 单个数据块的内存空间大小");
static tinytcp::ConfigVar<uint32_t>::ptr g_pktbuf_blk_cnt =
    tinytcp::Config::look_up("tcp.pktbuf_blk_cnt", 1024U, "tcp pktbuf block cnt, 协议栈中数据块的数量");
static tinytcp::ConfigVar<uint32_t>::ptr g_pktbuf_buf_cnt =
    tinytcp::Config::look_up("tcp.pktbuf_blk_cnt", 1024U, "tcp pktbuf buffer cnt, 协议栈中数据包的数量");


PktBlock::PktBlock() {
}

void PktBlock::init() {
    m_payload = new uint8_t[g_pktbuf_blk_size->value()];
    TINYTCP_ASSERT2(m_payload != nullptr, "pktbuf block payload new error");
}

void PktBlock::reset() {
    m_data = m_payload;
    m_size = 0;
}


PktBlock::~PktBlock() {
    delete[] m_payload;
}

uint64_t const PktBlock::get_last_size() const noexcept {
    uint64_t last_size = (uint64_t)(m_payload + g_pktbuf_blk_size->value() - (m_data + m_size));
    return last_size;
}

PktBuffer::PktBuffer()
    : m_capacity(0U)
    , m_ref(1) {

}

void PktBuffer::init() {
    m_capacity = 0U;
    m_ref = 1;
}

void PktBuffer::reset() {
    m_capacity = 0U;
    m_ref = 1;
}

uint8_t* PktBuffer::get_data() {
    if (m_blk_list.empty()) {
        return nullptr;
    }
    return m_blk_list.front()->get_data();
}

void PktBuffer::reset_access() {
    m_pos = 0;
    m_cur_blk = m_blk_list.end();
    m_blk_offset = nullptr;
    if (!m_blk_list.empty()) {
        m_cur_blk = m_blk_list.begin();
        m_blk_offset = (*m_cur_blk)->get_data();
    }
}

PktBuffer::~PktBuffer() {
    // 把资源交还给管理器
    auto pktmgr = PktMgr::get_instance();
    for (auto& blk : m_blk_list) {
        pktmgr->release_pktblock(blk);
    }
}

bool PktBuffer::alloc(uint32_t size, bool alloc_front, bool insert_front) {
    std::vector<PktBlock*> blk_list; // 先把分配出来的指针存起来，方便出问题后销毁
    uint32_t _size = size;
    m_capacity += _size;
    m_ref = 1;
    auto pktmgr = PktMgr::get_instance();
    PktBlock* first_block = nullptr;
    while (size) {
        PktBlock* blk = pktmgr->get_pktblock();
        if (blk == nullptr) {
            TINYTCP_LOG_WARN(g_logger) << "PktBuffer::alloc error, no free buf";
            for (auto& one_blk : blk_list) {
                pktmgr->release_pktblock(one_blk);
            }
            m_capacity -= _size;
            return false;
        }
        blk_list.push_back(blk);

        uint32_t cur_size = std::min(size, g_pktbuf_blk_size->value());
        blk->set_size(cur_size);
        if (alloc_front) {
            first_block = blk;
            // 头插法时的数据地址位置是不一样的
            blk->set_data(blk->get_payload() + g_pktbuf_blk_size->value() - cur_size);
        }
        else {
            if (first_block == nullptr) {
                first_block = blk;
            }
            blk->set_data(blk->get_payload());
        }
        size -= cur_size;
    }

    // 分配出来的链表，根据insert_front判断放到m_blk_list哪里
    if (insert_front) {
        if (alloc_front) {
            for (auto it = blk_list.begin(); it != blk_list.end(); ++it) {
                m_blk_list.push_front(*it);
            }
        }
        else {
            for (auto it = blk_list.rbegin(); it != blk_list.rend(); ++it) {
                m_blk_list.push_front(*it);
            }
        }
    }
    else {
        if (alloc_front) {
            for (auto it = blk_list.rbegin(); it != blk_list.rend(); ++it) {
                m_blk_list.push_back(*it);
            }
        }
        else {
            for (auto it = blk_list.begin(); it != blk_list.end(); ++it) {
                m_blk_list.push_back(*it);
            }
        }
    }

    reset_access();
    return true;
}

bool PktBuffer::free() {
    if ((--m_ref) == 0) {
        auto pktmgr = PktMgr::get_instance();
        while (!m_blk_list.empty()) {
            auto blk = m_blk_list.front();
            m_blk_list.pop_front();
            pktmgr->release_pktblock(blk);
        }
        pktmgr->release_pktbuffer(this);
        m_capacity = 0;
    }
    return true;
}

net_err_t PktBuffer::alloc_header(uint32_t size, bool is_cont) {
    if (m_blk_list.size() == 0) {
        TINYTCP_LOG_ERROR(g_logger) << "add header error, m_blk_list is empty";
        return net_err_t::NET_ERR_EMPTY;
    }

    auto first_block = m_blk_list.front();
    uint64_t pre_size  = (uint64_t)(first_block->get_data() - first_block->get_payload());
    // 剩下的空间足够分配
    if (pre_size >= size) {
        first_block->set_data(first_block->get_data() - size);
        first_block->set_size(first_block->get_size() + size);
        m_capacity += size;
    }
    else {
        auto pktmgr = PktMgr::get_instance();
        // 需要连续的空间
        if (is_cont) {
            if (size > g_pktbuf_blk_size->value()) {
                TINYTCP_LOG_ERROR(g_logger) << "header_size(" << size << ") > g_pktbuf_blk_size(" << g_pktbuf_blk_size->value() << ")";
                return net_err_t::NET_ERR_SIZE;
            }
            auto blk = pktmgr->get_pktblock();
            m_blk_list.push_front(blk);
            if (blk == nullptr) {
                TINYTCP_LOG_ERROR(g_logger) << "get_pktblock error, no free mem";
                return net_err_t::NET_ERR_NONE;
            }
            blk->set_data(blk->get_payload() + g_pktbuf_blk_size->value() - size);
            blk->set_size(size);
            m_capacity += size;
        }
        else {
            uint64_t pre_size = (uint64_t)(first_block->get_data() - first_block->get_payload());
            first_block->set_data(first_block->get_payload());
            first_block->set_size(first_block->get_size() + pre_size);
            m_capacity += pre_size;
            bool ok = alloc(size - pre_size, true, true);
            if (!ok) {
                TINYTCP_LOG_ERROR(g_logger) << "alloc error";
                return net_err_t::NET_ERR_NONE;
            }
        }
    }

    return net_err_t::NET_ERR_OK;
}

net_err_t PktBuffer::remove_header(uint32_t size) {
    TINYTCP_ASSERT2(m_blk_list.size() != 0U, "remove_header error: m_blk_list is empty");

    auto pktmgr = PktMgr::get_instance();
    while (size) {
        auto blk = m_blk_list.front();
        if (size < blk->get_size()) {
            auto data = blk->get_data();
            blk->set_data(data + size);
            blk->set_size(blk->get_size() - size);
            m_capacity -= size;
            break;
        }

        uint32_t cur_size = blk->get_size();
        m_capacity -= cur_size;
        size -= cur_size;
        m_blk_list.pop_front();
        pktmgr->release_pktblock(blk);
    }

    return net_err_t::NET_ERR_OK;
}

net_err_t PktBuffer::resize(uint32_t size) {
    if (size == m_capacity) {
        return net_err_t::NET_ERR_OK;
    }
    if (m_capacity == 0U) {
        bool ok = alloc(size, false, false);
        if (!ok) {
            TINYTCP_LOG_ERROR(g_logger) << "resize alloc error";
            return net_err_t::NET_ERR_MEM;
        }
        return net_err_t::NET_ERR_OK;
    }
    else if (size > m_capacity) { // 扩充
        auto tail_blk = m_blk_list.back();
        uint64_t last_size = tail_blk->get_last_size();
        if (last_size >= size) {
            tail_blk->set_size(tail_blk->get_size() + size);
            m_capacity += size;
        }
        else {
            tail_blk->set_size(tail_blk->get_size() + last_size);
            m_capacity += last_size;
            bool ok = alloc(size - m_capacity, false, false);
            if (!ok) {
                TINYTCP_LOG_ERROR(g_logger) << "resize alloc error";
                return net_err_t::NET_ERR_MEM;
            }
            return net_err_t::NET_ERR_OK;
        }
    }
    else {
        if (size == 0U) {
            free();
            return net_err_t::NET_ERR_OK;
        }
        auto pktmgr = PktMgr::get_instance();
        uint32_t total_size = 0U;
        auto it = m_blk_list.begin();
        for (; it != m_blk_list.end(); ++it) {
            total_size += (*it)->get_size();
            if (total_size >= size) {
                break;
            }
        }
        auto tail_blk = *it;
        // 删除后面的节点
        ++it;
        while (it != m_blk_list.end()) {
            m_capacity -= (*it)->get_size();
            it = m_blk_list.erase(it);
            pktmgr->release_pktblock(*it);
        }
        TINYTCP_ASSERT2(tail_blk->get_payload() == m_blk_list.back()->get_payload(), "tail != m_blk_list.back()");
        // 计算剩下的size
        uint32_t pre_list_size = m_capacity - tail_blk->get_size();
        tail_blk->set_size(size - pre_list_size);
        m_capacity = pre_list_size + tail_blk->get_size();
    }

    return net_err_t::NET_ERR_OK;
}

net_err_t PktBuffer::merge_buf(PktBuffer* buf) {
    auto pktmgr = PktMgr::get_instance();
    m_blk_list.splice(m_blk_list.end(), buf->m_blk_list);
    m_capacity += buf->m_capacity;
    pktmgr->release_pktbuffer(buf);
    return net_err_t::NET_ERR_OK;
}

net_err_t PktBuffer::set_cont_header(uint32_t size) {
    if (size > m_capacity) {
        TINYTCP_LOG_ERROR(g_logger) << "size(" << size << ") > m_capacity(" << m_capacity << ")";
        return net_err_t::NET_ERR_SIZE;
    }
    if (size > g_pktbuf_blk_size->value()) {
        TINYTCP_LOG_ERROR(g_logger) << "size(" << size << ") > g_pktbuf_blk_size(" << g_pktbuf_blk_size->value() << ")";
        return net_err_t::NET_ERR_SIZE;
    }
    PktBlock* first_blk = m_blk_list.front();
    uint32_t first_size = first_blk->get_size();
    if (size <= first_size) {
        return net_err_t::NET_ERR_OK;
    }
    auto pktmgr = PktMgr::get_instance();

    memmove(first_blk->get_payload(), first_blk->get_data(), first_size);
    first_blk->set_data(first_blk->get_payload());
    uint8_t* ptr = first_blk->get_payload() + first_size;
    uint32_t remain_size = size - first_size;
    auto it = m_blk_list.begin();
    ++it; // 把第一个块的数据移好位置之后,从第二个继续移动
    while (remain_size && it != m_blk_list.end()) {
        PktBlock* now_blk = *it;
        uint32_t cur_size = std::min(remain_size, now_blk->get_size());
        remain_size -= cur_size;
        memcpy(ptr, now_blk->get_data(), cur_size);
        ptr += cur_size;
        first_blk->set_size(first_blk->get_size() + cur_size);
        now_blk->set_data(now_blk->get_data() + cur_size);
        now_blk->set_size(now_blk->get_size() - cur_size);
        if (now_blk->get_size() == 0U) {
            it = m_blk_list.erase(it);
            pktmgr->release_pktblock(now_blk);
        }
    }
    return net_err_t::NET_ERR_OK;
}

net_err_t PktBuffer::seek(uint32_t offset) {
    if (m_pos == offset) {
        return net_err_t::NET_ERR_OK;
    }

    if (offset < 0 || offset > m_capacity) {
        TINYTCP_LOG_ERROR(g_logger) << "offset error";
        return net_err_t::NET_ERR_SIZE;
    }

    uint32_t move_bytes = 0;
    if (offset < m_pos) {
        m_cur_blk = m_blk_list.begin();
        m_blk_offset = (*m_cur_blk)->get_data();
        m_pos = 0;
        move_bytes = offset;
    }
    else {
        move_bytes = offset - m_pos;
    }

    while (move_bytes) {
        uint32_t remain_size = cur_blk_remain_size();
        uint32_t cur_move = std::min(remain_size, move_bytes);
        move_forward(cur_move);
        move_bytes -= cur_move;
    }

    return net_err_t::NET_ERR_OK;
}

net_err_t PktBuffer::copy(PktBuffer* src, uint32_t size) {
    if (total_blk_remain() < size || src->total_blk_remain() < size) {
        TINYTCP_LOG_ERROR(g_logger) << "size too big";
        return net_err_t::NET_ERR_SIZE;
    }

    while (size) {
        uint32_t dest_remain = cur_blk_remain_size();
        uint32_t src_remain = src->cur_blk_remain_size();
        uint32_t copy_size = std::min(dest_remain, src_remain);
        copy_size = std::min(size, copy_size);
        memcpy(m_blk_offset, src->get_blk_offset(), copy_size);
        move_forward(copy_size);
        src->move_forward(copy_size);
        size -= copy_size;
    }

    return net_err_t::NET_ERR_OK;
}

net_err_t PktBuffer::fill(uint8_t v, uint32_t size) {
    if (!size) {
        TINYTCP_LOG_ERROR(g_logger) << "fill error param";
        return net_err_t::NET_ERR_PARAM;
    }

    int remain_size = m_capacity - m_pos;
    if (remain_size < size) {
        TINYTCP_LOG_ERROR(g_logger) << "fill size too big, size=" << size << ", remain_size=" << remain_size << ", m_pos=" << m_pos;
        return net_err_t::NET_ERR_SIZE;
    }

    while (size) {
        uint32_t blk_size = cur_blk_remain_size();
        uint32_t fill_size = std::min(size, blk_size);
        memset(m_blk_offset, v, fill_size);
        move_forward(fill_size);
        size -= fill_size;
    }

    return net_err_t::NET_ERR_OK;
}

uint32_t PktBuffer::cur_blk_remain_size() {
    if (m_cur_blk == m_blk_list.end()) {
        return 0;
    }
    return (uint32_t)((*m_cur_blk)->get_data() + (*m_cur_blk)->get_size() - m_blk_offset);
}

void PktBuffer::move_forward(uint32_t size) {
    PktBlock* cur_blk = *m_cur_blk;
    // 不用担心m_pos和接下来的m_blk_offset对不上,在调用时size不会超过cur_blk
    m_pos += size;
    m_blk_offset += size;

    if (m_blk_offset >= cur_blk->get_data() + cur_blk->get_size()) {
        ++m_cur_blk;
        if (m_cur_blk != m_blk_list.end()) {
            m_blk_offset = (*m_cur_blk)->get_data();
        }
        else {
            m_blk_offset = nullptr;
        }
    }
}

net_err_t PktBuffer::write(const uint8_t* src, uint32_t size) {
    if (!src || !size) {
        TINYTCP_LOG_ERROR(g_logger) << "write error param";
        return net_err_t::NET_ERR_PARAM;
    }

    int remain_size = m_capacity - m_pos;
    if (remain_size < size) {
        TINYTCP_LOG_ERROR(g_logger) << "write size too big, size=" << size << ", remain_size=" << remain_size << ", m_pos=" << m_pos;
        return net_err_t::NET_ERR_SIZE;
    }

    while (size) {
        uint32_t blk_size = cur_blk_remain_size();
        uint32_t copy_size = std::min(size, blk_size);
        memcpy(m_blk_offset, src, copy_size);
        move_forward(copy_size);
        src += copy_size;
        size -= copy_size;
    }

    return net_err_t::NET_ERR_OK;
}

net_err_t PktBuffer::read(uint8_t* dest, uint32_t size) {
    if (!dest || !size) {
        TINYTCP_LOG_ERROR(g_logger) << "write error param";
        return net_err_t::NET_ERR_PARAM;
    }

    int remain_size = m_capacity - m_pos;
    if (remain_size < size) {
        TINYTCP_LOG_ERROR(g_logger) << "read size too big, size=" << size << ", remain_size=" << remain_size << ", m_pos=" << m_pos;
        return net_err_t::NET_ERR_SIZE;
    }

    while (size) {
        uint32_t blk_size = cur_blk_remain_size();
        uint32_t copy_size = std::min(size, blk_size);
        memcpy(dest, m_blk_offset, copy_size);
        move_forward(copy_size);
        dest += copy_size;
        size -= copy_size;
    }

    return net_err_t::NET_ERR_OK;
}

void PktBuffer::debug_print() {

    uint64_t total_size = 0;
    for (auto& blk : m_blk_list) {
        uint64_t pre_size  = (uint64_t)(blk->get_data() - blk->get_payload());
        uint64_t use_size  = blk->get_size();
        uint64_t last_size = (uint64_t)(blk->get_payload() + g_pktbuf_blk_size->value() - (blk->get_data() + use_size));
        TINYTCP_LOG_DEBUG(g_logger) << "pre_size=" << pre_size << ", use_size=" << use_size << ", last_size=" << last_size;
        total_size += use_size;
    }

    if (total_size != m_capacity) {
        TINYTCP_LOG_ERROR(g_logger) << "total_size != m_capacity, " << total_size << "!=" << m_capacity;
    }

    TINYTCP_LOG_DEBUG(g_logger) << "-------------------------------------------------------";

}

PktManager::PktManager() {
    m_pkt_blk = std::make_unique<MemBlock>(sizeof(PktBlock), g_pktbuf_blk_cnt->value());
    m_pkt_buf = std::make_unique<MemBlock>(sizeof(PktBuffer), g_pktbuf_buf_cnt->value());
    TINYTCP_LOG_INFO(g_logger) << "m_pkt_blk size=" << m_pkt_blk->size() << " m_pkt_buf size=" << m_pkt_buf->size();
    auto blk_cnt = g_pktbuf_blk_cnt->value();
    for (int i = 0; i < blk_cnt; ++i) {
        PktBlock* blk;
        bool ok = m_pkt_blk->alloc((void**)&blk, 0);
        TINYTCP_ASSERT2(ok, "m_pkt_blk->alloc error");
        // 内存池中的blk，还有一个m_payload缓存区需要提前分配, 用init函数初始化一下
        blk->init();
        ok = m_pkt_blk->free(blk);
        TINYTCP_ASSERT2(ok, "m_pkt_blk->free error, size=" + std::to_string(m_pkt_blk->size()));
    }
    auto buf_cnt = g_pktbuf_buf_cnt->value();
    for (int i = 0; i < buf_cnt; ++i) {
        PktBuffer* buf;
        bool ok = m_pkt_buf->alloc((void**)&buf, 0);
        TINYTCP_ASSERT2(ok, "m_pkt_buf->alloc error");
        new (buf) PktBuffer();
        ok = m_pkt_buf->free(buf);
        TINYTCP_ASSERT2(ok, "m_pkt_buf->free error, size=" + std::to_string(m_pkt_buf->size()));
    }
}

PktBlock* PktManager::get_pktblock() {
    PktBlock* ptr;
    if (!m_pkt_blk->alloc((void**)&ptr, 0)) {
        return nullptr;
    }
    ptr->reset();
    return ptr;
}

net_err_t PktManager::release_pktblock(PktBlock* ptr) {
    m_pkt_blk->free(ptr);
    return net_err_t::NET_ERR_OK;
}

PktBuffer* PktManager::get_pktbuffer() {
    PktBuffer* ptr;
    if (!m_pkt_buf->alloc((void**)&ptr, 0)) {
        return nullptr;
    }
    ptr->reset();
    return ptr;
}

net_err_t PktManager::release_pktbuffer(PktBuffer* ptr) {
    m_pkt_buf->free(ptr);
    return net_err_t::NET_ERR_OK;
}



} // namespace tinytcp

