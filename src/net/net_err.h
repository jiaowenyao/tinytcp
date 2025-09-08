#pragma once

#include <inttypes.h>

namespace tinytcp {


enum class net_err_t : int8_t {
    NET_ERR_SYS = -128,   // 系统错误
    NET_ERR_MEM,          // 内存分配错误
    NET_ERR_FULL,         // 队列已满
    NET_ERR_TIMEOUT,      // 超时
    NET_ERR_SIZE,         // 大小错误
    NET_ERR_EMPTY,        // 队列是空
    NET_ERR_NONE,
    NET_ERR_PARAM,
    NET_ERR_STATE,
    NET_ERR_IO,
    NET_ERR_NOT_SUPPORT,
    NET_ERR_UNREACH,
    NET_ERR_EXIST,
    ////
    NET_ERR_OK = 0,
    NET_ERR_NEED_WAIT = 1,
};



} // namespace tinytcp


