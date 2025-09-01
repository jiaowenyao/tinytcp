#pragma once

#include <thread>
#include <sys/syscall.h>
#include <sys/types.h>
#include <unistd.h>
#include <vector>

namespace tinytcp {
    
pid_t get_thread_id();

void back_trace(std::vector<std::string>& bt, int size, int skip = 1); // skip: 越过前面多少层打印堆栈信息
std::string back_trace_to_string(int size = 64, int skip = 2, const std::string& prefix = "");


// 时间相关
uint64_t get_current_ms();
uint64_t get_current_us();

std::string string_to_hex(const std::string& str);
std::ostream& operator<<(std::ostream& os, const std::pair<const std::string&, bool>& p);

// 16位校验和
uint16_t checksum16(void* buf, uint16_t len, uint32_t pre_sum, int complement);

} // namespace tinytcp

