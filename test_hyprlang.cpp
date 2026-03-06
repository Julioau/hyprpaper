#include <hyprlang.hpp>
#include <iostream>
#include <any>

int main() {
    Hyprlang::CConfig config("test_vars.conf", Hyprlang::SConfigOptions{});
    config.addConfigValue("path", Hyprlang::STRING{""});
    config.commence();
    auto result = config.parse();
    
    if (result.error) {
        std::cerr << "Error: " << result.getError() << std::endl;
        return 1;
    }
    
    try {
        auto val = std::any_cast<Hyprlang::STRING>(config.getConfigValue("path"));
        std::cout << "Value: " << val << std::endl;
    } catch (const std::bad_any_cast& e) {
        std::cerr << "Bad cast: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
}
