#include "log.h"
#include "config.h"

namespace tinytcp {

class MessageFormatterItem : public LogFormatter::FormatItem {
public:
    MessageFormatterItem(const std::string& fmt = "") {} 
    void format(std::ostream& os, Logger::ptr logger, LogLevel::Level level, LogEvent::ptr event) override {
        os << event->content();
    }
};

class LevelFormatterItem : public LogFormatter::FormatItem {
public:
    LevelFormatterItem(const std::string& fmt = "") {} 
    void format(std::ostream& os, Logger::ptr logger, LogLevel::Level level, LogEvent::ptr event) override {
        os << LogLevel::to_string(level);
    }
};

class ElapseFormatterItem : public LogFormatter::FormatItem {
public:
    ElapseFormatterItem(const std::string& fmt = "") {} 
    void format(std::ostream& os, Logger::ptr logger, LogLevel::Level level, LogEvent::ptr event) override {
        os << event->elapse();
    }
};

class NameFormatterItem : public LogFormatter::FormatItem {
public:
    NameFormatterItem(const std::string& fmt = "") {} 
    void format(std::ostream& os, Logger::ptr logger, LogLevel::Level level, LogEvent::ptr event) override {
        /**
         * 为什么不直接用logger呢？
         * 因为在Manager get_logger的时候，可能这个logger是不存在的，于是默认构造一个
         * 那么appender就使用root的，这个函数调用时的logger就会是root，我们需要获取真正的logger名字
         */
        os << event->logger()->name();
    }
};

class ThreadIdFormatterItem : public LogFormatter::FormatItem {
public:
    ThreadIdFormatterItem(const std::string& fmt = "") {} 
    void format(std::ostream& os, Logger::ptr logger, LogLevel::Level level, LogEvent::ptr event) override {
        os << event->thread_id();
    }
};


class ThreadNameFormatterItem : public LogFormatter::FormatItem {
public:
    ThreadNameFormatterItem(const std::string& fmt = "") {} 
    void format(std::ostream& os, Logger::ptr logger, LogLevel::Level level, LogEvent::ptr event) override {
        os << event->thread_name();
    }
};

class DateTimeFormatterItem : public LogFormatter::FormatItem {
public:
    DateTimeFormatterItem(const std::string format = "%Y-%m-%d %H:%M:%S")
        : m_format(format) {
        if (m_format.empty()) {
            m_format = "%Y-%m-%d %H:%M:%S";
        }
    }

    void format(std::ostream& os, Logger::ptr logger, LogLevel::Level level, LogEvent::ptr event) override {
        struct tm tm;
        time_t time = event->time();
        localtime_r(&time, &tm);
        char buf[64] = {0};
        strftime(buf, sizeof(buf), m_format.c_str(), &tm);
        os << buf;
    }
private:
    std::string m_format;
};

class FilenameFormatterItem : public LogFormatter::FormatItem {
public:
    FilenameFormatterItem(const std::string& fmt = "") {} 
    void format(std::ostream& os, Logger::ptr logger, LogLevel::Level level, LogEvent::ptr event) override {
        os << event->file();
    }
};

class LineFormatterItem : public LogFormatter::FormatItem {
public:
    LineFormatterItem(const std::string& fmt = "") {} 
    void format(std::ostream& os, Logger::ptr logger, LogLevel::Level level, LogEvent::ptr event) override {
        os << event->line();
    }
};

class NewLineFormatterItem : public LogFormatter::FormatItem {
public:
    NewLineFormatterItem(const std::string& fmt = "") {} 
    void format(std::ostream& os, Logger::ptr logger, LogLevel::Level level, LogEvent::ptr event) override {
        os << std::endl;
    }
};

class StringFormatterItem : public LogFormatter::FormatItem {
public:
    StringFormatterItem(const std::string& str)
        : m_string(str) {}
    void format(std::ostream& os, Logger::ptr logger, LogLevel::Level level, LogEvent::ptr event) override {
        os << m_string;
    }
private:
    std::string m_string;
};  

class TabFormatterItem : public LogFormatter::FormatItem {
public:
    TabFormatterItem(const std::string& fmt = "") {}
    void format(std::ostream& os, Logger::ptr logger, LogLevel::Level level, LogEvent::ptr event) override {
        os << "\t";
    }
};

LogEvent::LogEvent(std::shared_ptr<Logger> logger, LogLevel::Level level,
             const char* file, uint32_t line, uint32_t elapse,
             uint32_t thread_id, const std::string& thread_name, uint64_t time)
    : m_file(file)
    , m_line(line)
    , m_elapse(elapse)
    , m_thread_id(thread_id)
    , m_thread_name(thread_name)
    , m_time(time)
    , m_logger(logger)
    , m_level(level) {
}
LogEventWrap::LogEventWrap(LogEvent::ptr e)
    : m_event(e) {

}
LogEventWrap::~LogEventWrap() {
    m_event->logger()->log(m_event->level(), m_event);
}


Logger::Logger(const std::string& name)
    : m_name(name)
    , m_level(LogLevel::Level::DEBUG)
    , m_root(nullptr) {

    m_formatter.reset(new LogFormatter("%d{%Y-%m-%d %H:%M:%S}%T[%t %N]%T%T[%p]%T[%c]%T%f:%l%T%m%T%n"));
}

void Logger::log(LogLevel::Level level, LogEvent::ptr event) {
    if (level >= m_level) {
        Mutex::Lock lock(m_mutex);
        for (auto& i : m_appender) {
            if (!m_appender.empty()) {
                i->log(shared_from_this(), level, event);
            }
            else if (m_root != nullptr) {
                m_root->log(level, event);
            }
        }
    }
}

void Logger::debug(LogEvent::ptr event) {
    log(LogLevel::Level::DEBUG, event);
}

void Logger::info(LogEvent::ptr event) {
    log(LogLevel::Level::INFO, event);
}

void Logger::warn(LogEvent::ptr event) {
    log(LogLevel::Level::WARN, event);
}

void Logger::error(LogEvent::ptr event) {
    log(LogLevel::Level::ERROR, event);
}

void Logger::fatal(LogEvent::ptr event) {
    log(LogLevel::Level::FATAL, event);
}

void Logger::add_appender(LogAppender::ptr appender) {
    Mutex::Lock lock(m_mutex);
    // 如果这个appender没有formatter，就把logger自己的formatter给这个appender
    if (!appender->formatter()) {
        appender->set_formatter(m_formatter);
    }
    m_appender.push_back(appender);
}

void Logger::del_appender(LogAppender::ptr appender) {
    Mutex::Lock lock(m_mutex);
    for (auto it = m_appender.begin(); it != m_appender.end(); ++it) {
        if (*it == appender) {
            m_appender.erase(it);
            break;
        }
    }
}

void Logger::clear_appenders() {
    Mutex::Lock lock(m_mutex);
    m_appender.clear();
}

void Logger::set_formatter(LogFormatter::ptr val) {
    Mutex::Lock lock(m_mutex);
    m_formatter = val;
}

void Logger::set_formatter(const std::string& val) {
    Mutex::Lock lock(m_mutex);
    tinytcp::LogFormatter::ptr new_val(new tinytcp::LogFormatter(val));
    // 如果formatter解析失败，就不设置给logger，沿用logger初始化时默认的formatter
    if (new_val->is_error()) {
        std::cout << "Logger set_formatter name=" << m_name
                  << " value=" << val << " invalid formatter"
                  << std::endl;
        return;
    }
    m_formatter = new_val;
}


std::string Logger::to_yaml_string() {
    Mutex::Lock lock(m_mutex);
    YAML::Node node;
    node["name"] = m_name;
    if (m_level != LogLevel::UNKNOW) {
        node["level"] = LogLevel::to_string(m_level);
    }
    if (m_formatter != nullptr) {
        node["formatter"] = m_formatter->pattern();
    }
    for (auto& i : m_appender) {
        node["appenders"].push_back(YAML::Load(i->to_yaml_string()));
    }
    std::stringstream ss;
    ss << node;
    return ss.str();
}

LogFormatter::ptr Logger::formatter() {
    Mutex::Lock lock(m_mutex);
    return m_formatter;
}

const char* LogLevel::to_string(LogLevel::Level level) {
    switch (level)
    {
#define XX(name)                  \
    case LogLevel::Level::name:   \
        return #name;             \
        break;

    XX(DEBUG);
    XX(INFO);
    XX(WARN);
    XX(ERROR);
    XX(FATAL);

#undef XX    
    default:
        return "UNKNOW";
        break;
    }
    return "UNKNOW";
}

LogLevel::Level LogLevel::from_string(const std::string& str) {
#define XX(name)\
    if (str == #name) {\
        return LogLevel::name;\
    }

    XX(DEBUG);
    XX(INFO);
    XX(WARN);
    XX(ERROR);
    XX(FATAL);

    return LogLevel::UNKNOW;
#undef XX
}

void LogAppender::set_formatter(LogFormatter::ptr formatter) {
    Mutex::Lock lock(m_mutex);
    m_formatter = formatter;
}

LogFormatter::ptr LogAppender::formatter() {
    Mutex::Lock lock(m_mutex);
    return m_formatter;
}

FileLogAppender::FileLogAppender(const std::string& filename) : m_filename(filename) {
    reopen();
}

void FileLogAppender::log(Logger::ptr logger, LogLevel::Level level, LogEvent::ptr event) {
    if (level >= m_level) {
        Mutex::Lock lock(m_mutex);
        m_filestream << m_formatter->format(logger, level, event);
    }
}

bool FileLogAppender::reopen() {
    Mutex::Lock lock(m_mutex);
    if (m_filestream) {
        m_filestream.close();
    }
    m_filestream.open(m_filename);
    return !!m_filestream;
}


void StdoutLogAppender::log(Logger::ptr logger, LogLevel::Level level, LogEvent::ptr event) {
    if (level >= m_level) {
        Mutex::Lock lock(m_mutex);
        std::cout << m_formatter->format(logger, level, event);
    }
}


std::string StdoutLogAppender::to_yaml_string() {
    Mutex::Lock lock(m_mutex);
    YAML::Node node;
    node["type"] = "StdoutLogAppender";
    if (m_level != LogLevel::UNKNOW) {
        node["level"] = LogLevel::to_string(m_level);
    }
    if (m_formatter != nullptr) {
        node["formatter"] = m_formatter->pattern();
    }
    std::stringstream ss;
    ss << node;
    return ss.str();
} 

std::string FileLogAppender::to_yaml_string() {
    Mutex::Lock lock(m_mutex);
    YAML::Node node;
    node["type"] = "FileLogAppender";
    node["file"] = m_filename;
    if (m_level != LogLevel::UNKNOW) {
        node["level"] = LogLevel::to_string(m_level);
    }
    if (m_formatter != nullptr) {
        node["formatter"] = m_formatter->pattern();
    }
    std::stringstream ss;
    ss << node;
    return ss.str();
} 


LogFormatter::LogFormatter(const std::string& pattern)
    : m_pattern(pattern) {
    init();
}

void LogFormatter::init() {
    // tuple(str, format, type) 存放这种三元组的格式
    std::vector<std::tuple<std::string, std::string, int> > vec;
    std::string nstr;
    for (size_t i = 0; i < m_pattern.size(); ++i) {
        if (m_pattern[i] != '%') {
            nstr.append(1, m_pattern[i]);
            continue;
        }

        if ((i + 1) < m_pattern.size() && m_pattern[i + 1] == '%') {
            nstr.append(1, '%');
            continue;
        }

        size_t n = i + 1;
        int fmt_status = 0;
        size_t fmt_begine = 0;

        std::string str;
        std::string fmt;
        while (n < m_pattern.size()) {
            if (!fmt_status && 
                (!isalpha(m_pattern[n]) && m_pattern[n] != '{' && m_pattern[n] != '}')) {
                str = m_pattern.substr(i + 1, n - i - 1);
                break;
            }
            if (fmt_status == 0) {
                if (m_pattern[n] == '{') {
                    str = m_pattern.substr(i + 1, n - i - 1);
                    fmt_status = 1; // 解析具体格式
                    fmt_begine = n;
                    ++n;
                    continue;
                }
            }
            if (fmt_status == 1) {
                if (m_pattern[n] == '}') {
                    fmt = m_pattern.substr(fmt_begine + 1, n - fmt_begine - 1);
                    fmt_status = 0;
                    ++n;
                    break;
                }
            }
            ++n;
            if (n == m_pattern.size()) {
                if (str.empty()) {
                    str = m_pattern.substr(i + 1);
                }
            }
        }

        if (fmt_status == 0) {
            if (!nstr.empty()) {
                vec.push_back(std::make_tuple(nstr, "", 0));
                nstr.clear();
            }
            vec.push_back(std::make_tuple(str, fmt, 1));
            i = n - 1;
        }
        else if (fmt_status == 1) {
            std::cout << "pattern parse error: " << m_pattern << " - " << m_pattern.substr(i) << std::endl;
            m_error = true;
            vec.push_back(std::make_tuple("<<pattern-error>>", fmt, 0));
        }
    }

    if (!nstr.empty()) {
        vec.push_back(std::make_tuple(nstr, "", 0));
    }

    static std::map<std::string, std::function<FormatItem::ptr(const std::string& str)> > s_format_items = {
#define XX(str, C) \
        {#str, [](const std::string& fmt) { return FormatItem::ptr(new C(fmt)); } }

        XX(m, MessageFormatterItem),
        XX(p, LevelFormatterItem),
        XX(r, ElapseFormatterItem),
        XX(c, NameFormatterItem),
        XX(t, ThreadIdFormatterItem),
        XX(n, NewLineFormatterItem),
        XX(d, DateTimeFormatterItem),
        XX(f, FilenameFormatterItem),
        XX(l, LineFormatterItem),
        XX(T, TabFormatterItem),
        XX(N, ThreadNameFormatterItem)
#undef XX
    };

    for (auto& i : vec) {
        if (std::get<2>(i) == 0) {
            m_items.push_back(FormatItem::ptr(new StringFormatterItem(std::get<0>(i))));
        }
        else {
            auto it = s_format_items.find(std::get<0>(i));
            if (it == s_format_items.end()) {
                m_items.push_back(FormatItem::ptr(new StringFormatterItem("<<error_format %" + std::get<0>(i) + ">>")));
                m_error = true;
            }
            else {
                m_items.push_back(it->second(std::get<1>(i)));
            }
        }

        // std::cout << "{" << std::get<0>(i) << "} - {" << std::get<1>(i) << "} - {" << std::get<2>(i) << "}" << std::endl;
    }

}

std::string LogFormatter::format(Logger::ptr logger, LogLevel::Level level, LogEvent::ptr event) {
    std::stringstream ss;
    for (auto& i : m_items) {
        i->format(ss, logger, level, event);
    }
    return ss.str();
}


LoggerManager::LoggerManager()
    : m_root(new Logger()) {
    // 默认的root，默认输出到控制台
    m_root->add_appender(StdoutLogAppender::ptr(new StdoutLogAppender));
    m_loggers[m_root->name()] = m_root;
    init();
}

Logger::ptr LoggerManager::get_logger(const std::string& name) {
    Mutex::Lock lock(m_mutex);
    auto it = m_loggers.find(name);
    if (it != m_loggers.end()) {
        return it->second;
    }
    // 如果get_logger的时候没有找到，那就默认构造一个logger
    // 此时logger->m_root赋值为manager的m_root，这样子就能使用m_root的appender了
    Logger::ptr logger(new Logger(name));
    logger->set_root(m_root);
    m_loggers[name] = logger;
    return logger;
}

std::string LoggerManager::to_yaml_string() {
    Mutex m_mutex;
    YAML::Node node;
    for (auto& i : m_loggers) {
        node.push_back(YAML::Load(i.second->to_yaml_string()));
    }
    std::stringstream ss;
    ss << node;
    return ss.str();
}

// yaml配置中的日志格式类
struct LogAppenderDefine {
    int32_t type = 0; // 1: File, 2: Stdout
    LogLevel::Level level = LogLevel::UNKNOW;
    std::string formatter;
    std::string file;

    bool operator==(const LogAppenderDefine& other) const {
        return type == other.type
            && level == other.level
            && formatter == other.formatter
            && file == other.file;
    }
};


struct LogDefine {
    std::string name;
    LogLevel::Level level = LogLevel::UNKNOW;
    std::string formatter;
    std::vector<LogAppenderDefine> appenders;

    bool operator==(const LogDefine& other) const {
        return name == other.name
            && level == other.level
            && formatter == other.formatter
            && appenders == other.appenders;
    }
    bool operator<(const LogDefine& other) const {
        return name < other.name;
    }
};

template<>
class LexicalCast<std::string, LogDefine> {
public:
    LogDefine operator()(const std::string& v) {
        YAML::Node n = YAML::Load(v);
        LogDefine ld;
        if(!n["name"].IsDefined()) {
            std::cout << "log config error: name is null, " << n
                      << std::endl;
            throw std::logic_error("log config name is null");
        }
        ld.name = n["name"].as<std::string>();
        ld.level = LogLevel::from_string(n["level"].IsDefined() ? n["level"].as<std::string>() : "");
        if(n["formatter"].IsDefined()) {
            ld.formatter = n["formatter"].as<std::string>();
        }

        if(n["appenders"].IsDefined()) {
            //std::cout << "==" << ld.name << " = " << n["appenders"].size() << std::endl;
            for(size_t x = 0; x < n["appenders"].size(); ++x) {
                auto a = n["appenders"][x];
                if(!a["type"].IsDefined()) {
                    std::cout << "log config error: appender type is null, " << a
                              << std::endl;
                    continue;
                }
                std::string type = a["type"].as<std::string>();
                LogAppenderDefine lad;
                if(type == "FileLogAppender") {
                    lad.type = 1;
                    if(!a["file"].IsDefined()) {
                        std::cout << "log config error: fileappender file is null, " << a
                              << std::endl;
                        continue;
                    }
                    lad.file = a["file"].as<std::string>();
                    if(a["formatter"].IsDefined()) {
                        lad.formatter = a["formatter"].as<std::string>();
                    }
                } else if(type == "StdoutLogAppender") {
                    lad.type = 2;
                    if(a["formatter"].IsDefined()) {
                        lad.formatter = a["formatter"].as<std::string>();
                    }
                } else {
                    std::cout << "log config error: appender type is invalid, " << a
                              << std::endl;
                    continue;
                }

                ld.appenders.push_back(lad);
            }
        }
        return ld;
    }
};

template<>
class LexicalCast<LogDefine, std::string> {
public:
    std::string operator()(const LogDefine& i) {
        YAML::Node n;
        n["name"] = i.name;
        if(i.level != LogLevel::UNKNOW) {
            n["level"] = LogLevel::to_string(i.level);
        }
        if(!i.formatter.empty()) {
            n["formatter"] = i.formatter;
        }

        for(auto& a : i.appenders) {
            YAML::Node na;
            if(a.type == 1) {
                na["type"] = "FileLogAppender";
                na["file"] = a.file;
            } else if(a.type == 2) {
                na["type"] = "StdoutLogAppender";
            }
            if(a.level != LogLevel::UNKNOW) {
                na["level"] = LogLevel::to_string(a.level);
            }

            if(!a.formatter.empty()) {
                na["formatter"] = a.formatter;
            }

            n["appenders"].push_back(na);
        }
        std::stringstream ss;
        ss << n;
        return ss.str();
    }
};



tinytcp::ConfigVar<std::set<LogDefine> >::ptr g_log_defines = 
    tinytcp::Config::look_up("logs", std::set<LogDefine>(), "logs config");

struct LogIniter {
    LogIniter() {
        // LogDefine:配置文件中log配置，所以对于log应该有三种事件: 新增、修改、删除
        g_log_defines->add_listener([](const std::set<LogDefine>& old_value, const std::set<LogDefine>& new_value){
            TINYTCP_LOG_INFO(TINYTCP_LOG_ROOT()) << "on_logger_conf_changed";
            // 新增
            for (auto& i : new_value) {
                auto it = old_value.find(i);
                tinytcp::Logger::ptr logger;
                if (it == old_value.end()) {
                    // 新增Logger, 采用TINYTCP_LOG_NAME方式生成新的log可以把结果放到m_loggers中去, get_logger的时候就可以获取到了
                    logger = TINYTCP_LOG_NAME(i.name);
                }
                else {
                    if (!(i == *it)) {
                        // 修改
                        logger = TINYTCP_LOG_NAME(i.name);
                    }
                }
                logger->set_level(i.level);
                if (!i.formatter.empty()) {
                    logger->set_formatter(i.formatter);
                }

                logger->clear_appenders();
                for (auto& a : i.appenders) {
                    tinytcp::LogAppender::ptr ap;
                    if (a.type == 1) {
                        ap.reset(new FileLogAppender(a.file));
                    }
                    else if (a.type == 2) {
                        ap.reset(new StdoutLogAppender);
                    }
                    ap->set_level(a.level);
                    logger->add_appender(ap);
                }
            }

            // 删除: 老的里面有，新的里面没有
            for (auto &i : old_value) {
                auto it = new_value.find(i);
                if (it == new_value.end()) {
                    // 删除logger，防止静态资源出现问题，使用逻辑上删除
                    auto logger = TINYTCP_LOG_NAME(i.name);
                    logger->set_level(LogLevel::Level::NEVER);
                    logger->clear_appenders();
                }
            }
                
        });
    }
};

// 在main函数执行之前就被构造
struct LogIniter __log_init;


void LoggerManager::init() {

}
    
} // namespace tinytcp


