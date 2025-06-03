#include <iostream>
#include <string>
#include <unordered_map>
#include <functional>
#include <memory>
#include <vector>
#include <sstream>
#include <algorithm>

enum class HttpMethod { GET, POST, PUT, DELETE };

// URL解码函数
std::string urlDecode(const std::string& str) {
    std::string result;
    result.reserve(str.size());
    
    for (size_t i = 0; i < str.size(); ++i) {
        if (str[i] == '%') {
            if (i + 2 < str.size()) {
                int value;
                std::istringstream iss(str.substr(i + 1, 2));
                if (iss >> std::hex >> value) {
                    result += static_cast<char>(value);
                    i += 2;
                } else {
                    result += str[i];
                }
            } else {
                result += str[i];
            }
        } else if (str[i] == '+') {
            result += ' ';
        } else {
            result += str[i];
        }
    }
    
    return result;
}

// 解析查询字符串
std::unordered_map<std::string, std::string> parseQueryString(const std::string& query) {
    std::unordered_map<std::string, std::string> params;
    if (query.empty()) return params;
    
    size_t start = 0;
    while (start < query.size()) {
        size_t end = query.find('&', start);
        if (end == std::string::npos) end = query.size();
        
        size_t eq_pos = query.find('=', start);
        if (eq_pos != std::string::npos && eq_pos < end) {
            std::string key = urlDecode(query.substr(start, eq_pos - start));
            std::string value = urlDecode(query.substr(eq_pos + 1, end - eq_pos - 1));
            params[key] = value;
        }
        
        start = end + 1;
    }
    
    return params;
}

// 路径标准化
std::string normalizePath(const std::string& path) {
    if (path.empty()) return "/";
    
    std::string result;
    result.reserve(path.size() + 1);
    
    // 确保以/开头
    if (path[0] != '/') {
        result += '/';
    }
    
    // 移除重复的/
    bool lastWasSlash = false;
    for (char c : path) {
        if (c == '/') {
            if (!lastWasSlash) {
                result += '/';
                lastWasSlash = true;
            }
        } else {
            result += c;
            lastWasSlash = false;
        }
    }
    
    // 移除末尾的/(除非是根路径)
    if (result.size() > 1 && result.back() == '/') {
        result.pop_back();
    }
    
    return result;
}

struct HttpRequest {
    std::string path;
    HttpMethod method;
    std::unordered_map<std::string, std::string> params;  // 路径参数
    std::unordered_map<std::string, std::string> query;   // 查询参数
    
    const std::string& getParam(const std::string& name, 
                               const std::string& defaultValue = "") const {
        auto it = params.find(name);
        return it != params.end() ? it->second : defaultValue;
    }
    
    const std::string& getQuery(const std::string& name,
                               const std::string& defaultValue = "") const {
        auto it = query.find(name);
        return it != query.end() ? it->second : defaultValue;
    }
};

using HttpHandler = std::function<void(const HttpRequest&)>;

struct RadixNode {
    std::string path;
    std::unordered_map<HttpMethod, HttpHandler> handlers;
    std::unordered_map<char, std::shared_ptr<RadixNode>> children;
    bool is_wildcard = false;
    std::string wildcard_name;
    
    explicit RadixNode(std::string p = "") : path(std::move(p)) {}
};

class HttpRouter {
private:
    std::shared_ptr<RadixNode> root;

    void insertPath(HttpMethod method, const std::string& raw_path, HttpHandler handler) {
        // 分离路径和查询参数
        size_t query_start = raw_path.find('?');
        std::string path_only = query_start == std::string::npos 
            ? raw_path 
            : raw_path.substr(0, query_start);
        
        std::string normalized = normalizePath(path_only);
        auto node = root;
        size_t pos = 1; // 跳过开头的/
        
        while (pos < normalized.size()) {
            size_t next_slash = normalized.find('/', pos);
            if (next_slash == std::string::npos) {
                next_slash = normalized.size();
            }
            
            std::string segment = normalized.substr(pos, next_slash - pos);
            bool is_wildcard = (!segment.empty() && segment[0] == ':');
            
            if (is_wildcard) {
                // 处理通配符段
                std::string param_name = segment.substr(1);
                if (param_name.empty()) {
                    throw std::invalid_argument("Empty parameter name in path: " + raw_path);
                }
                
                if (node->children.count(':') == 0) {
                    auto wildcard_node = std::make_shared<RadixNode>(segment);
                    wildcard_node->is_wildcard = true;
                    wildcard_node->wildcard_name = param_name;
                    node->children[':'] = wildcard_node;
                }
                node = node->children.at(':');
            } else {
                // 处理静态路径段
                bool found = false;
                for (auto& pair : node->children) {
                    if (pair.first == ':') continue;
                    
                    auto& child = pair.second;
                    size_t i = 0;
                    size_t min_len = std::min(child->path.size(), segment.size());
                    while (i < min_len && child->path[i] == segment[i]) {
                        i++;
                    }
                    
                    if (i > 0) {
                        if (i < child->path.size()) {
                            // 分裂节点
                            auto split_node = std::make_shared<RadixNode>(child->path.substr(0, i));
                            child->path = child->path.substr(i);
                            split_node->children[child->path[0]] = child;
                            node->children[pair.first] = split_node;
                            node = split_node;
                        } else {
                            node = child;
                        }
                        
                        if (i < segment.size()) {
                            // 剩余部分创建新节点
                            auto new_node = std::make_shared<RadixNode>(segment.substr(i));
                            node->children[segment[i]] = new_node;
                            node = new_node;
                        }
                        found = true;
                        break;
                    }
                }
                
                if (!found) {
                    // 全新节点
                    auto new_node = std::make_shared<RadixNode>(segment);
                    node->children[segment[0]] = new_node;
                    node = new_node;
                }
            }
            
            pos = next_slash + 1;
        }
        
        node->handlers[method] = handler;
    }
    
    std::pair<HttpHandler, HttpRequest> 
    searchPath(HttpMethod method, const std::string& raw_path) const {
        HttpRequest req;
        req.method = method;
        
        // 分离路径和查询参数
        size_t query_start = raw_path.find('?');
        std::string path_only = query_start == std::string::npos 
            ? raw_path 
            : raw_path.substr(0, query_start);
        
        req.path = normalizePath(path_only);
        
        // 解析查询参数
        if (query_start != std::string::npos) {
            std::string query_str = raw_path.substr(query_start + 1);
            req.query = parseQueryString(query_str);
        }
        
        auto node = root;
        size_t pos = 1; // 跳过开头的/
        
        while (pos < req.path.size()) {
            size_t next_slash = req.path.find('/', pos);
            if (next_slash == std::string::npos) {
                next_slash = req.path.size();
            }
            
            std::string segment = req.path.substr(pos, next_slash - pos);
            bool found = false;
            
            // 先尝试静态匹配
            for (auto& pair : node->children) {
                if (pair.first == ':') continue;
                
                auto& child = pair.second;
                if (segment == child->path) {
                    node = child;
                    found = true;
                    break;
                }
            }
            
            // 尝试通配符匹配
            if (!found && node->children.count(':')) {
                auto wildcard_node = node->children.at(':');
                req.params[wildcard_node->wildcard_name] = segment;
                node = wildcard_node;
                found = true;
            }
            
            if (!found) {
                return {nullptr, req};
            }
            
            pos = next_slash + 1;
        }
        
        if (node->handlers.count(method)) {
            return {node->handlers.at(method), req};
        }
        
        return {nullptr, req};
    }

public:
    HttpRouter() : root(std::make_shared<RadixNode>()) {}
    
    void addRoute(HttpMethod method, const std::string& path, HttpHandler handler) {
        if (path.empty() || path[0] != '/') {
            throw std::invalid_argument("Path must start with '/': " + path);
        }
        insertPath(method, path, handler);
    }
    
    void handleRequest(HttpMethod method, const std::string& path) const {
        auto [handler, req] = searchPath(method, path);
        if (handler) {
            try {
                handler(req);
            } catch (const std::exception& e) {
                std::cerr << "Handler error: " << e.what() << std::endl;
            }
        } else {
            std::cout << "404 Not Found: " << path << std::endl;
        }
    }
};

