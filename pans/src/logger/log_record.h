#ifndef PANS_SRC_PANS_LOGGER_LOG_RECORD_H
#define PANS_SRC_PANS_LOGGER_LOG_RECORD_H

#include <chrono>
#include <cstdint>
#include <string_view>

#include <logger/log_level.h>
#include <pans/macros.h>

namespace pans::detail{
    // 非拥有视图：所有 string_view 引用的数据必须活到格式化完成。
    // 异步入队前需另行保存字符串，不能仅复制此结构来延长数据生命周期。
    struct LogRecordView{
        LogLevel::Level m_level = LogLevel::Level::LOG_LV_DEBUG;
        std::string_view m_logger_name = "";    // 日志器的名字
        std::string_view m_message = "";        // 日志内容
        std::chrono::system_clock::time_point m_timestamp;  // 日志时间
        std::chrono::steady_clock::duration m_elapsed;      // 日志记录耗时
        u64 m_thread_id = 0;                               // 线程ID
        u64 m_fiber_id = 0;                                // 协程ID
        std::string_view m_thread_name = "";                // 线程名字
        std::string_view m_file_name = "";                  // 文件名字
        u32 m_line = 0;                                     // 行号
    };
}
#endif
