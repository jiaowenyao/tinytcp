#pragma once

#include <inttypes.h>

namespace tinytcp {


enum class net_err_t : int8_t {
    NET_ERR_OK           =  0,
    NET_ERR_SYS          = -1,  // 系统错误
    NET_ERR_MEM          = -2,  // 内存分配错误
    NET_ERR_FULL         = -3,  // 队列已满
    NET_ERR_TIMEOUT      = -4,  // 超时
};



} // namespace tinytcp


