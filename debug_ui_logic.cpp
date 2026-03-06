#include "src/config/ConfigManager.hpp"
#include "src/config/WallpaperMatcher.hpp"
#include "src/helpers/Logger.hpp"
#include <hyprutils/string/String.hpp> 
#include <iostream>
#include <vector>
#include <algorithm>
#include <cmath>
#include <limits>
#include <optional>

// Mock structures
struct Vector2D { double x, y; };
struct SViewport {
    Vector2D globalSize;
    Vector2D offset; // negative
};

struct SMonitorInfo {
    std::string name;
    std::string description;
    int         id = -1;
    int         x = 0, y = 0;
    int         w = 0, h = 0;
    double      scale     = 1.0;
    int         transform = 0;
    int         wMM = 0, hMM = 0;
};

// Use inline globals from headers (g_config, g_matcher, g_logger)

static std::string_view pruneDesc(const std::string_view& sv) {
    if (sv.contains('('))
        return Hyprutils::String::trim(sv.substr(0, sv.find_last_of('(')));
    return sv;
}

// Mock getMonitors based on your YAML
std::vector<SMonitorInfo> getMockMonitors() {
    std::vector<SMonitorInfo> monitors;
    
    // eDP-1
    SMonitorInfo m1;
    m1.name = "eDP-1";
    m1.description = "LG Display 0x06E2";
    m1.x = 0; m1.y = 0; // Logical position (irrelevant if manual overrides work)
    m1.w = 1920; m1.h = 1080;
    monitors.push_back(m1);

    // DP-2
    SMonitorInfo m2;
    m2.name = "DP-2";
    m2.description = "Dell Inc. DELL P2725HE DWWKG34";
    m2.x = 1920; m2.y = 0;
    m2.w = 1920; m2.h = 1080;
    monitors.push_back(m2);

    // DP-3
    SMonitorInfo m3;
    m3.name = "DP-3";
    m3.description = "Dell Inc. DELL P2319H 37FW923";
    m3.x = 3840; m3.y = 0;
    m3.w = 1080; m3.h = 1920;
    monitors.push_back(m3);

    return monitors;
}

// The Logic being tested
std::optional<SViewport> recalculateSpan(const std::string& monName) {
    auto monitors = getMockMonitors();

    // Calculate PPI (Dummy calculation, should be overridden by manual regions)
    double maxPPI = 1.0;

    struct SMonitorPhysInfo {
        SMonitorInfo* info = nullptr;
        double physX = 0, physY = 0;
        double physW = 0, physH = 0;
    };

    std::vector<SMonitorPhysInfo> physMonitors;
    physMonitors.reserve(monitors.size());
    for (auto& m : monitors) {
        auto& pm = physMonitors.emplace_back();
        pm.info = &m;

        // Check for overrides
        const auto SETTING = g_matcher->getSetting(m.name, pruneDesc(m.description));

        // Physical Size 
        if (SETTING && SETTING->get().manualW.has_value()) {
            pm.physW = *SETTING->get().manualW;
            std::cout << "DEBUG: " << m.name << " ManualW: " << pm.physW << std::endl;
        } else {
            pm.physW = m.w; // Fallback
             std::cout << "DEBUG: " << m.name << " FallbackW: " << pm.physW << std::endl;
        }

        if (SETTING && SETTING->get().manualH.has_value()) {
            pm.physH = *SETTING->get().manualH;
             std::cout << "DEBUG: " << m.name << " ManualH: " << pm.physH << std::endl;
        } else {
            pm.physH = m.h; // Fallback
             std::cout << "DEBUG: " << m.name << " FallbackH: " << pm.physH << std::endl;
        }
    }

    // Sort logic (skipped for brevity as we expect manual overrides for ALL)
    
    // X Loop
    for (auto& pm : physMonitors) {
        const auto SETTING = g_matcher->getSetting(pm.info->name, pruneDesc(pm.info->description));
        if (SETTING && SETTING->get().manualX.has_value()) {
            pm.physX = *SETTING->get().manualX;
             std::cout << "DEBUG: " << pm.info->name << " ManualX: " << pm.physX << std::endl;
            continue;
        }
         std::cout << "DEBUG: " << pm.info->name << " NO ManualX!" << std::endl;
    }
    
    // Y Loop
    for (auto& pm : physMonitors) {
         const auto SETTING = g_matcher->getSetting(pm.info->name, pruneDesc(pm.info->description));
         if (SETTING && SETTING->get().manualY.has_value()) {
             pm.physY = *SETTING->get().manualY;
             std::cout << "DEBUG: " << pm.info->name << " ManualY: " << pm.physY << std::endl;
             continue;
         }
          std::cout << "DEBUG: " << pm.info->name << " NO ManualY!" << std::endl;
    }

    // Bounding Box
    double minPX = std::numeric_limits<double>::max();
    double minPY = std::numeric_limits<double>::max();
    double maxPX = std::numeric_limits<double>::min();
    double maxPY = std::numeric_limits<double>::min();

    SMonitorPhysInfo* myPhysInfo = nullptr;

    for (auto& pm : physMonitors) {
        if (pm.info->name == monName)
            myPhysInfo = &pm;
        
        if (pm.physX < minPX) minPX = pm.physX;
        if (pm.physY < minPY) minPY = pm.physY;
        if (pm.physX + pm.physW > maxPX) maxPX = pm.physX + pm.physW;
        if (pm.physY + pm.physH > maxPY) maxPY = pm.physY + pm.physH;
    }

    if (!myPhysInfo) return std::nullopt;

    std::cout << "DEBUG: Bounding Box: Min(" << minPX << "," << minPY << ") Max(" << maxPX << "," << maxPY << ")" << std::endl;

    double globalW = maxPX - minPX;
    double globalH = maxPY - minPY;

    double offsetX = -(myPhysInfo->physX - minPX);
    double offsetY = -(myPhysInfo->physY - minPY);
    
    std::cout << "DEBUG: " << monName << " Raw Offset: " << offsetX << ", " << offsetY << std::endl;

    double scaleX = 1.0;
    double scaleY = 1.0;
    
    if (myPhysInfo->physW > 0) scaleX = (double)myPhysInfo->info->w / myPhysInfo->physW;
    if (myPhysInfo->physH > 0) scaleY = (double)myPhysInfo->info->h / myPhysInfo->physH;
    
    std::cout << "DEBUG: " << monName << " Scale: " << scaleX << ", " << scaleY << std::endl;

    SViewport vp;
    vp.globalSize = { globalW * scaleX, globalH * scaleY };
    vp.offset = { offsetX * scaleX, offsetY * scaleY };
    
    return vp;
}

int main() {
    // Only init g_config, others are auto-init by headers
    g_config = makeUnique<CConfigManager>("hyprpaper_nxt.conf");
    
    if (!g_config->init()) {
         std::cerr << "Config Init Failed" << std::endl;
         return 1;
    }

    // Register outputs to simulate CUI::run / CUI::registerOutput
    // This populates m_monitorStates in matcher so getSetting works
    g_matcher->registerOutput("eDP-1", "LG Display 0x06E2");
    g_matcher->registerOutput("DP-2", "Dell Inc. DELL P2725HE DWWKG34");
    g_matcher->registerOutput("DP-3", "Dell Inc. DELL P2319H 37FW923");

    std::vector<std::string> monitors = {"eDP-1", "DP-2", "DP-3"};

    for (const auto& m : monitors) {
        std::cout << "\nTesting " << m << "..." << std::endl;
        auto vp = recalculateSpan(m);
        if (vp) {
            std::cout << "Result " << m << ":" << std::endl;
            std::cout << "  Viewport Size: " << vp->globalSize.x << " x " << vp->globalSize.y << std::endl;
            std::cout << "  Viewport Offset: " << vp->offset.x << " , " << vp->offset.y << std::endl;
        } else {
            std::cout << "Failed to calculate viewport for " << m << std::endl;
        }
    }

    return 0;
}