#include "src/config/ConfigManager.hpp"
#include "src/config/WallpaperMatcher.hpp"
#include "src/helpers/Logger.hpp"
#include <iostream>

// Globals are already inline in headers, just use them.

int main() {
    // Re-initialize if needed, or just use them.
    // The inline globals are unique_ptrs, so they are initialized to nullptr or default values (if makeUnique is called in header).
    
    // In WallpaperMatcher.hpp: inline UP<CWallpaperMatcher> g_matcher = makeUnique<CWallpaperMatcher>();
    // In Logger.hpp: inline UP<Hyprutils::CLI::CLogger> g_logger = makeUnique<Hyprutils::CLI::CLogger>();
    // In ConfigManager.hpp: inline UP<CConfigManager> g_config; (initialized to nullptr)
    
    // So we only need to init g_config.
    g_config = makeUnique<CConfigManager>("hyprpaper_nxt.conf");

    std::cout << "Parsing hyprpaper_nxt.conf..." << std::endl;

    if (!g_config->init()) {
        std::cerr << "Failed to initialize config!" << std::endl;
        return 1;
    }

    auto settings = g_config->getSettings();
    std::cout << "Found " << settings.size() << " wallpaper settings." << std::endl;

    for (const auto& s : settings) {
        std::cout << "------------------------------------------------" << std::endl;
        std::cout << "Monitor: " << s.monitor << std::endl;
        std::cout << "Path: " << (s.paths.empty() ? "None" : s.paths[0]) << std::endl;
        std::cout << "Fit Mode: " << s.fitMode << std::endl;
        
        std::cout << "Manual Regions:" << std::endl;
        std::cout << "  X: " << (s.manualX.has_value() ? std::to_string(*s.manualX) : "Default") << std::endl;
        std::cout << "  Y: " << (s.manualY.has_value() ? std::to_string(*s.manualY) : "Default") << std::endl;
        std::cout << "  W: " << (s.manualW.has_value() ? std::to_string(*s.manualW) : "Default") << std::endl;
        std::cout << "  H: " << (s.manualH.has_value() ? std::to_string(*s.manualH) : "Default") << std::endl;
    }
    std::cout << "------------------------------------------------" << std::endl;

    return 0;
}