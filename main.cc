#include <iostream>
#include "router.h"

int main() {
    HttpRouter router;
    
    // 测试路由
    router.addRoute(HttpMethod::GET, "/", [](const HttpRequest& req) {
        std::cout << "Home page" << std::endl;
        if (!req.query.empty()) {
            std::cout << "Query parameters:" << std::endl;
            for (const auto& [k, v] : req.query) {
                std::cout << "  " << k << " = " << v << std::endl;
            }
        }
    });
    
    router.addRoute(HttpMethod::GET, "/search", [](const HttpRequest& req) {
        std::cout << "Search page" << std::endl;
        std::string query = req.getQuery("q", "");
        std::string page = req.getQuery("page", "1");
        std::cout << "Search query: " << query << ", Page: " << page << std::endl;
    });
    
    router.addRoute(HttpMethod::GET, "/users/:id", [](const HttpRequest& req) {
        std::cout << "User profile: ID = " << req.getParam("id") << std::endl;
        if (!req.query.empty()) {
            std::cout << "Query parameters:" << std::endl;
            for (const auto& [k, v] : req.query) {
                std::cout << "  " << k << " = " << v << std::endl;
            }
        }
    });

    router.addRoute(HttpMethod::POST, "/post/:id", [](const HttpRequest& req) {
        std::cout << "POST: ID = " << req.getParam("id") << std::endl;
        if (!req.query.empty()) {
            std::cout << "Query parameters:" << std::endl;
            for (const auto& [k, v] : req.query) {
                std::cout << "  " << k << " = " << v << std::endl;
            }
        }
    });
    
    router.addRoute(HttpMethod::GET, "/users/:id/post/:post_id", [](const HttpRequest& req) {
        std::cout << "User profile: ID = " << req.getParam("id") << std::endl;
	std::cout << "Post Id = " << req.getParam("post_id") << std::endl;
        if (!req.query.empty()) {
            std::cout << "Query parameters:" << std::endl;
            for (const auto& [k, v] : req.query) {
                std::cout << "  " << k << " = " << v << std::endl;
            }
        }
    });
    
    // 测试用例
    std::cout << "=== 测试开始 ===" << std::endl;
    
    struct TestCase {
        std::string path;
        std::string description;
    };
    
    std::vector<TestCase> tests = {
        {"/", "根路径"},
        {"/?debug=true", "带查询参数的根路径"},
        {"/search?q=radix+tree&page=2", "搜索页面"},
	{"/users","不带params"},
        {"/users/123", "用户页面"},
        {"/users/456?details=true&tab=posts", "带查询参数的用户页面"},
        {"/invalid/path", "无效路径"},
	{"/users/123/post/456?user_id=1&user_id=2&uu=8?asd6666&","重复query参数"},
	{"/users/1/post","只带一个params"},
	{"/post/2","POST"}
    };
    
    for (const auto& test : tests) {
        std::cout << "\nGET测试: " << test.description << " (" << test.path << ")" << std::endl;
        router.handleRequest(HttpMethod::GET, test.path);
    }

    for (const auto& test : tests) {
        std::cout << "\nPOST测试: " << test.description << " (" << test.path << ")" << std::endl;
        router.handleRequest(HttpMethod::POST, test.path);
    }
   
    return 0;
}
