#include "config.h"

namespace tinytcp {

static tinytcp::Logger::ptr g_logger = TINYTCP_LOG_NAME("system");

ConfigVarBase::ptr Config::look_up_base(const std::string& name) {
    RWMutex::ReadLock lock(get_mutex());
    auto it = get_datas().find(name);
    return it == get_datas().end() ? nullptr : it->second;
}


/**
 * 用于拼接配置
 * 比如一个配置：A.B=1
 * 在yaml中的存在形式是：
 * A:
 *  B: 1
 * 为了方便使用，需要把A和B拼接起来
*/
static void list_all_member(const std::string& prefix,
                          const YAML::Node& node,
                          std::list<std::pair<std::string, const YAML::Node> >& output) {

    if (prefix.find_first_not_of("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ._0123456789") != std::string::npos) {
        TINYTCP_LOG_ERROR(TINYTCP_LOG_ROOT()) << "Config invalid name: " << prefix << " : " << node;
    }
    output.push_back(std::make_pair(prefix, node));
    if (node.IsMap()) {
        for (auto it = node.begin(); it != node.end(); ++it) {
            list_all_member(prefix.empty() ? it->first.Scalar() : prefix + "." + it->first.Scalar(),
                          it->second,
                          output);
        }
    }
}

void Config::load_from_yaml(const YAML::Node& root) {
    std::list<std::pair<std::string, const YAML::Node> > all_nodes;
    list_all_member("", root, all_nodes);

    for (auto& i : all_nodes) {
        std::string key = i.first;
        if (key.empty()) {
            continue;
        }
        ConfigVarBase::ptr var = look_up_base(key);
        if (var != nullptr) {
            if (i.second.IsScalar()) {
                var->from_string(i.second.Scalar());
            }
            else {
                std::stringstream ss;
                ss << i.second;
                var->from_string(ss.str());
            }
        }
    }

}

void Config::visit(std::function<void(ConfigVarBase::ptr)> cb) {
    RWMutex::ReadLock lock(get_mutex());
    ConfigVarMap& m = get_datas();
    for (auto it = m.begin(); it != m.end(); ++it) {
        cb(it->second);
    }
}

} // namespace tinytcp

