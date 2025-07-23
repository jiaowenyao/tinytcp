#pragma once
#include <string>
#include <inttypes.h>
#include <memory>
#include <list>
#include <iostream>
#include <sstream>
#include <fstream>
#include <vector>
#include <map>
#include <functional>
#include "singleton.h"
#include "util.h"
#include "thread.h"


// 定义日志输出的宏
#define TINYTCP_LOG_LEVEL(logger_, level_)     \
    if ((logger_)->level() <= level_)  \
        tinytcp::LogEventWrap(           \
            tinytcp::LogEvent::ptr(      \
                new tinytcp::LogEvent(logger_, level_, __FILE__, __LINE__, 0, tinytcp::get_thread_id(), tinytcp::Thread::get_name(), time(0)))).ss()


#define TINYTCP_LOG_DEBUG(logger) TINYTCP_LOG_LEVEL(logger, tinytcp::LogLevel::DEBUG)
#define TINYTCP_LOG_INFO(logger) TINYTCP_LOG_LEVEL(logger, tinytcp::LogLevel::INFO)
#define TINYTCP_LOG_WARN(logger) TINYTCP_LOG_LEVEL(logger, tinytcp::LogLevel::WARN)
#define TINYTCP_LOG_ERROR(logger) TINYTCP_LOG_LEVEL(logger, tinytcp::LogLevel::ERROR)
#define TINYTCP_LOG_FATAL(logger) TINYTCP_LOG_LEVEL(logger, tinytcp::LogLevel::FATAL)

#define TINYTCP_LOG_ROOT() tinytcp::LoggerMgr::get_instance()->get_root()

#define TINYTCP_LOG_NAME(name) tinytcp::LoggerMgr::get_instance()->get_logger(name)

namespace tinytcp {

class Logger;
class LoggerManager;

// 日志级别
class LogLevel {
public:
    enum Level {
        UNKNOW = 0,
        DEBUG  = 1,
        INFO   = 2,
        WARN   = 3,
        ERROR  = 4,
        FATAL  = 5,
        NEVER  = 100 // 逻辑上删除logger，永不打印
    };

    static const char* to_string(LogLevel::Level Level);
    static LogLevel::Level from_string(const std::string& str);
};


// 存放日志用到的属性
class LogEvent {
public:
    using ptr = std::shared_ptr<LogEvent>;
    LogEvent(std::shared_ptr<Logger> logger, LogLevel::Level level,
             const char* file, uint32_t line, uint32_t elapse,
             uint32_t thread_id, const std::string& thread_name, uint64_t time);

    const char* file() const { return m_file; }
    uint32_t line() const { return m_line; }
    uint32_t elapse() const { return m_elapse; }
    uint32_t thread_id() const { return m_thread_id; }
    const std::string& thread_name() const { return m_thread_name; }
    time_t time() const { return m_time; }
    std::string content() const { return m_ss.str(); }
    std::shared_ptr<Logger> logger() const { return m_logger; }
    LogLevel::Level level() const { return m_level; }

    std::stringstream& ss() { return m_ss; }

private:
    const char* m_file   = nullptr;  // 文件名
    uint32_t m_line      = 0;        // 行号
    uint32_t m_elapse    = 0;        // 程序从启动开始到现在的毫秒级耗时
    uint32_t m_thread_id = 0;        // 线程号
    std::string m_thread_name;       // 线程名称
    time_t m_time;                   // 时间戳
    std::stringstream m_ss;          // 日志内容

    std::shared_ptr<Logger> m_logger; // 打印的时候需要知道是哪个logger调用的
    LogLevel::Level m_level;
};

class LogEventWrap {
public:
    LogEventWrap(LogEvent::ptr e);
    ~LogEventWrap();
    std::stringstream& ss() { return m_event->ss(); }

private:
    LogEvent::ptr m_event;
};
    

// 日志格式器
class LogFormatter {
public:
    using ptr = std::shared_ptr<LogFormatter>;
    LogFormatter(const std::string& pattern);
    std::string format(std::shared_ptr<Logger> logger, LogLevel::Level level, LogEvent::ptr event);

public:
    // 格式化的基类
    class FormatItem {
    public:
        using ptr = std::shared_ptr<FormatItem>;
        virtual ~FormatItem() {}
        // 把结果放在出参os中，可能会有多个组合在一起
        virtual void format(std::ostream& os, std::shared_ptr<Logger> logger, LogLevel::Level level, LogEvent::ptr event) = 0;
    };
    void init(); // 用init来做pattern的解析
    bool is_error() { return m_error; }
    std::string pattern() const { return m_pattern; }
private:
    // 通过pattern解析出item的信息
    std::string m_pattern;
    std::vector<FormatItem::ptr> m_items;
    bool m_error = false; // 解析是否有问题
};


// 日志输出目的地
class LogAppender {
public:
    using ptr = std::shared_ptr<LogAppender>;
    virtual ~LogAppender() {}

    // 把logger也传递下来，拿到logger name
    virtual void log(std::shared_ptr<Logger> logger, LogLevel::Level level, LogEvent::ptr event) = 0;

    virtual std::string to_yaml_string() = 0;

    void set_formatter(LogFormatter::ptr formatter);

    void set_level(LogLevel::Level level) { m_level = level; }

    LogFormatter::ptr formatter();

protected:
    // 子类也可能用到的日志等级
    LogLevel::Level m_level = LogLevel::Level::DEBUG;
    /**
     * 最终是用appender的formatter，就把logger自己的formatter给这个appender
     * 所以如果appender没有定义formatter，就会默认设置为Logger的formatter
     * */ 
    LogFormatter::ptr m_formatter;

    Mutex m_mutex;
};

// 日志输出器
class Logger : public std::enable_shared_from_this<Logger> {
public:

    using ptr = std::shared_ptr<Logger>;

    Logger(const std::string& name = "root");

    void log(LogLevel::Level level, LogEvent::ptr event);

    // 不同级别的输出
    void debug(LogEvent::ptr event);
    void info(LogEvent::ptr event);
    void warn(LogEvent::ptr event);
    void error(LogEvent::ptr event);
    void fatal(LogEvent::ptr event);

    // 操作appender
    void add_appender(LogAppender::ptr appender);
    void del_appender(LogAppender::ptr appender);
    void clear_appenders();

    void set_level(LogLevel::Level level) { m_level = level; }
    LogLevel::Level level() const { return m_level; }

    const std::string& name() const { return m_name; }
    
    void set_formatter(LogFormatter::ptr val);
    void set_formatter(const std::string& val);
    LogFormatter::ptr formatter();

    std::string to_yaml_string();

    void set_root(const Logger::ptr& root) { m_root = root; }
private:
    std::string m_name;                      // 日志名称
    LogLevel::Level m_level;                 // 日志等级
    std::list<LogAppender::ptr> m_appender;  // Appender集合
    LogFormatter::ptr m_formatter;
    Logger::ptr m_root;
    Mutex m_mutex;
};


// 输出到控制台的appender
class StdoutLogAppender : public LogAppender {
public:
    using ptr = std::shared_ptr<StdoutLogAppender>;

    void log(Logger::ptr logger, LogLevel::Level level, LogEvent::ptr event) override;

    virtual std::string to_yaml_string() override;
private:
};


// 输出到文件的appender
class FileLogAppender : public LogAppender {
public:
    FileLogAppender(const std::string& filename);

    using ptr = std::shared_ptr<FileLogAppender>;

    void log(Logger::ptr logger, LogLevel::Level level, LogEvent::ptr event) override;

    // 重新打开文件
    bool reopen();

    virtual std::string to_yaml_string() override;
private:

    std::string m_filename;

    std::ofstream m_filestream;
};

class LoggerManager {
public:
    LoggerManager();
    Logger::ptr get_logger(const std::string& name);

    void init();
    Logger::ptr get_root() const { return m_root; }

    std::string to_yaml_string();
private:
    Logger::ptr m_root;
    std::map<std::string, Logger::ptr> m_loggers;
    Mutex m_mutex;
};


using LoggerMgr = tinytcp::Singleton<LoggerManager>;
using LoggerMgrPtr = tinytcp::SingletonPtr<LoggerManager>;

} // namespace tinytcp


