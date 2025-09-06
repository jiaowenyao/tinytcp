#include "util.h"
#include "log.h"
#include <execinfo.h>
#include <sys/time.h>
#include <iomanip>


namespace tinytcp {

tinytcp::Logger::ptr g_logger = TINYTCP_LOG_NAME("system");

pid_t get_thread_id() {
    return syscall(SYS_gettid);
}

void back_trace(std::vector<std::string>& bt, int size, int skip) {
    void** array = (void**)malloc(sizeof(void*) * size);
    size_t s = ::backtrace(array, size);
    char** strings = backtrace_symbols(array, s);
    if (strings == NULL) {
        TINYTCP_LOG_ERROR(g_logger) << "backtrace_symbols error";
        free(array);
        return;
    }
    for (size_t i = skip; i < s; ++i) {
        bt.push_back(strings[i]);
    }
    free(strings);
    free(array);
}

std::string back_trace_to_string(int size, int skip, const std::string& prefix) {
    std::vector<std::string> bt;
    back_trace(bt, size, skip);
    std::stringstream ss;
    for (size_t i = 0; i < bt.size(); ++i) {
        ss << prefix << bt[i] << std::endl;
    }
    return ss.str();
}

uint64_t get_current_ms() {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return tv.tv_sec * 1000UL + tv.tv_usec / 1000;
}

uint64_t get_current_us() {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return tv.tv_sec * 1000 * 1000UL + tv.tv_usec;
}


std::string string_to_hex(const std::string& str) {
    std::stringstream ss;
    ss << std::hex << std::setfill('0');

    for (size_t i = 0; i < str.size(); ++i) {
        ss << std::setw(2) << static_cast<uint32_t>(static_cast<uint8_t>(str[i])) << " ";
    }

    return ss.str();
}

std::ostream& operator<<(std::ostream& os, const std::pair<const std::string&, bool>& p) {
    // 如果第二个参数为 true，则输出十六进制
    if (p.second) {
        os << string_to_hex(p.first);
    } else {
        os << p.first;
    }
    return os;
}

uint16_t checksum16(uint32_t offset, void* buf, uint16_t len, uint32_t pre_sum, int complement) {
    uint16_t* curr_buf = (uint16_t*)buf;
    uint32_t checksum = pre_sum;

    if (offset & 1) {
        uint8_t* buf = (uint8_t*)curr_buf;
        checksum += (*buf++) << 8;
        curr_buf = (uint16_t*)buf;
        --len;
    }

    while (len > 1) {
        checksum += *curr_buf++;
        len -= 2;
    }
    if (len > 0) {
        checksum += *(uint8_t*)curr_buf;
    }
    uint16_t high;
    while ((high = checksum >> 16) != 0) {
        checksum = high + (checksum & 0xFFFF);
    }
    if (complement) {
        return (uint16_t)~checksum;
    }
    return (uint16_t)checksum;
}

} // namespace tinytcp




