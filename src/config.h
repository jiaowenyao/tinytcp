#pragma once

#include <memory>
#include <string>
#include <sstream>
#include <boost/lexical_cast.hpp>
#include <yaml-cpp/yaml.h>
#include <map>
#include <set>
#include <unordered_set>
#include <unordered_map>
#include <list>
#include "log.h"
#include "mutex.h"

namespace tinytcp {

// 配置的基类
class ConfigVarBase {
public:
    using ptr = std::shared_ptr<ConfigVarBase>;

    ConfigVarBase(const std::string& name, const std::string& description = "")
        : m_name(name)
        , m_description(description) {
    }
    virtual ~ConfigVarBase() {}

    const std::string& name() const { return m_name; }
    const std::string& description() const { return m_description; }

    // 明文转换
    virtual std::string to_string() = 0;
    // 解析
    virtual bool from_string(const std::string& val) = 0;
    // 获取类型名
    virtual std::string type_name() const = 0;

protected:
    std::string m_name;
    std::string m_description;
};


// 类型转换，From type -> To type
template<class F, class T>
class LexicalCast {
public:
    T operator()(const F& v) {
        return boost::lexical_cast<T>(v);
    }
};
//////////////////// 类型转换 vector 偏特化支持
template<class T>
class LexicalCast<std::string, std::vector<T> > {
public:
    std::vector<T> operator()(const std::string& v) {
        YAML::Node node = YAML::Load(v);
        std::vector<T> vec;
        std::stringstream ss;
        for (size_t i = 0; i < node.size(); ++i) {
            ss.str("");
            ss << node[i];
            vec.push_back(LexicalCast<std::string, T>()(ss.str()));
        }
        return vec;
    }
};

template<class T>
class LexicalCast<std::vector<T>, std::string> {
public:
    std::string operator()(const std::vector<T>& v) {
        YAML::Node node;
        for (auto& i : v) {
            node.push_back(YAML::Load(LexicalCast<T, std::string>()(i)));
        }
        std::stringstream ss;
        ss << node;
        return ss.str();
    }
};

//////////////////// 类型转换 list 偏特化支持
template<class T>
class LexicalCast<std::string, std::list<T> > {
public:
    std::list<T> operator()(const std::string& v) {
        YAML::Node node = YAML::Load(v);
        std::list<T> vec;
        std::stringstream ss;
        for (size_t i = 0; i < node.size(); ++i) {
            ss.str("");
            ss << node[i];
            vec.push_back(LexicalCast<std::string, T>()(ss.str()));
        }
        return vec;
    }
};

template<class T>
class LexicalCast<std::list<T>, std::string> {
public:
    std::string operator()(const std::list<T>& v) {
        YAML::Node node;
        for (auto& i : v) {
            node.push_back(YAML::Load(LexicalCast<T, std::string>()(i)));
        }
        std::stringstream ss;
        ss << node;
        return ss.str();
    }
};

//////////////////// 类型转换 set 偏特化支持
template<class T>
class LexicalCast<std::string, std::set<T> > {
public:
    std::set<T> operator()(const std::string& v) {
        YAML::Node node = YAML::Load(v);
        std::set<T> vec;
        std::stringstream ss;
        for (size_t i = 0; i < node.size(); ++i) {
            ss.str("");
            ss << node[i];
            vec.insert(LexicalCast<std::string, T>()(ss.str()));
        }
        return vec;
    }
};

template<class T>
class LexicalCast<std::set<T>, std::string> {
public:
    std::string operator()(const std::set<T>& v) {
        YAML::Node node;
        for (auto& i : v) {
            node.push_back(YAML::Load(LexicalCast<T, std::string>()(i)));
        }
        std::stringstream ss;
        ss << node;
        return ss.str();
    }
};

//////////////////// 类型转换 unordered_set 偏特化支持
template<class T>
class LexicalCast<std::string, std::unordered_set<T> > {
public:
    std::unordered_set<T> operator()(const std::string& v) {
        YAML::Node node = YAML::Load(v);
        std::unordered_set<T> vec;
        std::stringstream ss;
        for (size_t i = 0; i < node.size(); ++i) {
            ss.str("");
            ss << node[i];
            vec.insert(LexicalCast<std::string, T>()(ss.str()));
        }
        return vec;
    }
};

template<class T>
class LexicalCast<std::unordered_set<T>, std::string> {
public:
    std::string operator()(const std::unordered_set<T>& v) {
        YAML::Node node;
        for (auto& i : v) {
            node.push_back(YAML::Load(LexicalCast<T, std::string>()(i)));
        }
        std::stringstream ss;
        ss << node;
        return ss.str();
    }
};

//////////////////// 类型转换 map 偏特化支持
template<class T>
class LexicalCast<std::string, std::map<std::string, T> > {
public:
    std::map<std::string, T> operator()(const std::string& v) {
        YAML::Node node = YAML::Load(v);
        std::map<std::string, T> vec;
        std::stringstream ss;
        for (auto it = node.begin(); it != node.end(); ++it) {
            ss.str("");
            ss << it->second;
            vec.insert(std::make_pair(it->first.Scalar(), LexicalCast<std::string, T>()(ss.str())));
        }
        return vec;
    }
};

template<class T>
class LexicalCast<std::map<std::string, T>, std::string> {
public:
    std::string operator()(const std::map<std::string, T>& v) {
        YAML::Node node;
        for (auto& i : v) {
            node[i.first] = YAML::Load(LexicalCast<T, std::string>()(i.second));
        }
        std::stringstream ss;
        ss << node;
        return ss.str();
    }
};

//////////////////// 类型转换 unordered_map 偏特化支持
template<class T>
class LexicalCast<std::string, std::unordered_map<std::string, T> > {
public:
    std::unordered_map<std::string, T> operator()(const std::string& v) {
        YAML::Node node = YAML::Load(v);
        std::unordered_map<std::string, T> vec;
        std::stringstream ss;
        for (auto it = node.begin(); it != node.end(); ++it) {
            ss.str("");
            ss << it->second;
            vec.insert(std::make_pair(it->first.Scalar(), LexicalCast<std::string, T>()(ss.str())));
        }
        return vec;
    }
};

template<class T>
class LexicalCast<std::unordered_map<std::string, T>, std::string> {
public:
    std::string operator()(const std::unordered_map<std::string, T>& v) {
        YAML::Node node;
        for (auto& i : v) {
            node[i.first] = YAML::Load(LexicalCast<T, std::string>()(i.second));
        }
        std::stringstream ss;
        ss << node;
        return ss.str();
    }
};



/**
 * 配置可能会有很多种，用模板
 * 这里还有FromStr和ToStr，是因为可能会有复杂的数据类型
 * 这两个类提供to_string和from_string的方法
 * 用仿函数实现：
 * FromStr T operator()(const std::string&)
 * ToStr std::string operator()(const T&)
 */
template<class T, class FromStr = LexicalCast<std::string, T>,
                  class ToStr   = LexicalCast<T, std::string> >
class ConfigVar : public ConfigVarBase {
public:
    using ptr = std::shared_ptr<ConfigVar>;
    using on_change_cb = std::function<void(const T& old_value, const T& new_value)>;

    ConfigVar(const std::string& name, const T& default_value, const std::string& description = "")
        : ConfigVarBase(name, description)
        , m_val(default_value) {

    }

    std::string to_string() override {
        try {
            RWMutex::ReadLock lock(m_mutex);
            return ToStr()(m_val);
        }
        catch (std::exception& e) {
            TINYTCP_LOG_ERROR(TINYTCP_LOG_ROOT()) << "ConfigVar::to_string exception: "
                << e.what() << " convert: " << typeid(m_val).name() << "to_string";
        }
        return "";
    }

    bool from_string(const std::string& val) override {
        try {
            set_value(FromStr()(val));
            return true;
        }
        catch (std::exception& e) {
            TINYTCP_LOG_ERROR(TINYTCP_LOG_ROOT()) << "ConfigVar::from_string exception: "
                << e.what() << " convert string to: " << typeid(m_val).name();
        }
        return false;
    }

    const T value() {
        RWMutex::ReadLock lock(m_mutex);
        return m_val;
    }
    void set_value(const T& v) {
        {
            RWMutex::ReadLock lock(m_mutex);
            if (m_val == v) {
                return;
            }
            for (auto& i : m_cbs) {
                i.second(m_val, v);
            }
        }
        RWMutex::WriteLock lock(m_mutex);
        m_val = v;
    }
    std::string type_name() const override { return typeid(T).name(); }

    uint64_t add_listener(on_change_cb cb) {
        static uint64_t s_func_id = 0;
        RWMutex::WriteLock lock(m_mutex);
        ++s_func_id;
        m_cbs[s_func_id] = cb;
        return s_func_id;
    }

    void del_listener(uint64_t key) {
        RWMutex::WriteLock lock(m_mutex);
        m_cbs.erase(key);
    }

    on_change_cb get_listener(uint64_t key) {
        RWMutex::ReadLock lock(m_mutex);
        auto it = m_cbs.find(key);
        return it == m_cbs.end() ? nullptr : it->second;
    }

    void clear_listener() {
        RWMutex::WriteLock lock(m_mutex);
        m_cbs.clear();
    }

private:
    RWMutex m_mutex; // 配置是读多写少，用读写锁
    T m_val;
    // 数值变更回调函数
    std::map<uint64_t, on_change_cb> m_cbs;
};

class Config {
public:
    using ConfigVarMap = std::map<std::string, ConfigVarBase::ptr>;

    template<class T>
    static typename ConfigVar<T>::ptr look_up(
            const std::string& name,
            const T& default_value,
            const std::string& description = "") {
        RWMutex::WriteLock lock(get_mutex());
        auto it = get_datas().find(name);
        if (it != get_datas().end()) {
            auto tmp = std::dynamic_pointer_cast<ConfigVar<T> >(it->second);
            if (tmp != nullptr) {
                TINYTCP_LOG_INFO(TINYTCP_LOG_ROOT()) << "lookup name=" << name << " exits";
                return tmp;
            }
            else {
                TINYTCP_LOG_ERROR(TINYTCP_LOG_ROOT()) << "lookup name=" << name << " exits but type not "
                    << typeid(T).name() << ", real type=" << it->second->type_name()
                    << " " << it->second->to_string();
                return nullptr;
            }
        }
        if (name.find_first_not_of("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ._0123456789") != std::string::npos) {
            TINYTCP_LOG_ERROR(TINYTCP_LOG_ROOT()) << "lookup name invalid: " << name;
            throw std::invalid_argument(name);
        }
        typename ConfigVar<T>::ptr v(new ConfigVar<T>(name, default_value, description));
        get_datas()[name] = v;
        return v;
    }

    template<class T>
    static typename ConfigVar<T>::ptr look_up(const std::string& name) {
        RWMutex::ReadLock lock(get_mutex());
        auto it = get_datas().find(name);
        if (it == get_datas().end()) {
            return nullptr;
        }
        return std::dynamic_pointer_cast<ConfigVar<T> >(it->second);
    }

    static void load_from_yaml(const YAML::Node& root);
    
    static ConfigVarBase::ptr look_up_base(const std::string& name);

    static void visit(std::function<void(ConfigVarBase::ptr)> cb);

private:
    static ConfigVarMap& get_datas() {
        static ConfigVarMap s_datas;
        return s_datas;
    }

    static RWMutex& get_mutex() {
        static RWMutex s_mutex;
        return s_mutex;
    }
};
    
} // namespace tinytcp

