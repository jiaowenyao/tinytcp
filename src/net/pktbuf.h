// 数据包, 数据包由数据块组成

#pragma once
#include <list>
#include <inttypes.h>
#include "src/singleton.h"
#include "src/net/memblock.h"
#include "src/net/net_err.h"

namespace tinytcp {

// 数据块
class PktBlock {
public:
    PktBlock();
    ~PktBlock();

    void init();
    void reset();

    uint32_t const get_size() const noexcept { return m_size; }
    uint8_t* const get_data() const noexcept { return m_data; }
    uint8_t* const get_payload() const noexcept { return m_payload; }
    uint64_t const get_last_size() const noexcept;


    void set_size(uint32_t size) noexcept { m_size = size; }
    void set_data(uint8_t* ptr) noexcept { m_data = ptr; }

private:
    uint32_t m_size;    // 数据块里的数据大小
    uint8_t* m_data = nullptr;    // 数据在内存空间中的起始位置
    uint8_t* m_payload = nullptr; // 数据块中的内存空间
};


// 数据包
class PktBuffer {
public:
    PktBuffer();
    ~PktBuffer();
    void init();
    void reset();

    void reset_access();

    // 分配多少字节的空间
    bool alloc(uint32_t size, bool alloc_front = true, bool insert_front = false);
    bool free();

    const std::list<PktBlock*>& get_list() const noexcept { return m_blk_list; }
    const uint8_t* get_blk_offset() const noexcept { return m_blk_offset; }
    uint32_t get_capacity() const noexcept { return m_capacity; }
    uint32_t total_blk_remain() const noexcept { return m_capacity - m_pos; }
    uint32_t get_pos() const noexcept { return m_pos; }
    std::list<PktBlock*>::iterator get_cur_blk() const noexcept { return m_cur_blk; }
    uint8_t* get_data();
    void add_ref() noexcept { ++m_ref; }

    // 增加报头, is_cont:包头空间是否需要连续
    net_err_t alloc_header(uint32_t size, bool is_cont = true);
    net_err_t remove_header(uint32_t size);
    net_err_t resize(uint32_t size);
    net_err_t merge_buf(PktBuffer* buf);
    // 调整包头，调成连续的
    net_err_t set_cont_header(uint32_t size);

    // 读写
    net_err_t write(const uint8_t* src, uint32_t size);
    net_err_t read(uint8_t* dest, uint32_t size);
    net_err_t seek(uint32_t offset);
    // 从另一个PktBuffer中拷贝数据进来
    net_err_t copy(PktBuffer* src, uint32_t size);
    // 填充数据包
    net_err_t fill(uint8_t v, uint32_t size);

    // 当前指向块的剩余大小
    uint32_t cur_blk_remain_size();

    // 移动包到下一个位置,如果跨越包,指向开头
    void move_forward(uint32_t size);


    // 调试打印
    void debug_print();

private:
    uint32_t m_capacity; // 数据包总的容量大小

    // 头插法插入数据块，方便后续加入包头
    std::list<PktBlock*> m_blk_list;

    // 读写相关
    int32_t m_pos;
    std::list<PktBlock*>::iterator m_cur_blk;
    uint8_t* m_blk_offset;

    std::atomic_int m_ref;

    // PktBuffer* prev = nullptr;
    // PktBuffer* next = nullptr;
};


class PktManager {

public:
    PktManager();
    ~PktManager() = default;

    PktBlock* get_pktblock();
    net_err_t release_pktblock(PktBlock* ptr);

    PktBuffer* get_pktbuffer();
    net_err_t release_pktbuffer(PktBuffer* ptr);

    uint32_t get_blk_list_size() const { return m_pkt_blk->size(); }
    uint32_t get_buf_list_size() const { return m_pkt_buf->size(); }

private:
    MemBlock::uptr m_pkt_blk;
    MemBlock::uptr m_pkt_buf;

};


using PktMgr = tinytcp::Singleton<PktManager>;


} // namespace tinytcp

