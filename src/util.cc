#include "util.h"
#include "log.h"
#include <execinfo.h>
#include <sys/time.h>


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

} // namespace tinytcp




