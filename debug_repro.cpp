#include <iostream>
#include <vector>
#include <string>
#include <algorithm>
#include <cmath>
#include <limits>
#include <optional>

// Mock structures
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

struct Vector2D { double x, y; };
struct SViewport {
    Vector2D globalSize;
    Vector2D offset; // negative
};

// Mock data
std::vector<SMonitorInfo> getMonitors() {
    std::vector<SMonitorInfo> monitors;
    
    // Monitor eDP-1 (ID 0)
    SMonitorInfo m1;
    m1.name = "eDP-1";
    m1.description = "LG Display 0x06E2";
    m1.id = 0;
    m1.x = 0; m1.y = 0;
    m1.w = 1920; m1.h = 1080;
    m1.scale = 1.0;
    m1.transform = 0;
    m1.wMM = 340; m1.hMM = 190;
    monitors.push_back(m1);

    // Monitor DP-2 (ID 1)
    SMonitorInfo m2;
    m2.name = "DP-2";
    m2.description = "Dell Inc. DELL P2725HE DWWKG34";
    m2.id = 1;
    m2.x = 0; m2.y = -1080;
    m2.w = 1920; m2.h = 1080;
    m2.scale = 1.0;
    m2.transform = 0;
    m2.wMM = 600; m2.hMM = 340;
    monitors.push_back(m2);

    // Monitor DP-3 (ID 2)
    SMonitorInfo m3;
    m3.name = "DP-3";
    m3.description = "Dell Inc. DELL P2319H 37FW923";
    m3.id = 2;
    m3.x = 1920; m3.y = -1080;
    m3.w = 1920; m3.h = 1080; // BEFORE swap
    m3.scale = 1.0;
    m3.transform = 3;
    m3.wMM = 510; m3.hMM = 290; // BEFORE swap
    monitors.push_back(m3);

    // Apply transforms
    for (auto& m : monitors) {
        if (m.transform % 2 == 1) {
            std::swap(m.w, m.h);
            std::swap(m.wMM, m.hMM);
        }
    }
    return monitors;
}

// Logic
std::optional<SViewport> recalculateSpan(const std::string& monName) {
    auto monitors = getMonitors();
    if (monitors.empty()) return std::nullopt;

    // Calculate PPI for each monitor
    double maxPPI = 1.0;
    /*
    for (const auto& m : monitors) {
        if (m.wMM <= 0 || m.hMM <= 0) continue;
        double ppi = (double)m.w / m.wMM;
        if (ppi > maxPPI) maxPPI = ppi;
    }
    */

    if (maxPPI <= 0) return std::nullopt;

    struct SMonitorPhysInfo {
        SMonitorInfo* info = nullptr;
        double physX = 0, physY = 0;
        double physW = 0, physH = 0;
    };

    std::vector<SMonitorPhysInfo> physMonitors;
    physMonitors.reserve(monitors.size());
    for (auto& m : monitors) {
        physMonitors.push_back({&m, 0, 0, 0, 0});
    }

    // Sort by Logical X then Y
    std::sort(physMonitors.begin(), physMonitors.end(), [](const SMonitorPhysInfo& a, const SMonitorPhysInfo& b) {
        if (a.info->x != b.info->x) return a.info->x < b.info->x;
        return a.info->y < b.info->y;
    });

    // Calculate Physical Dimensions and Positions
    for (auto& pm : physMonitors) {
        if (pm.info->wMM > 0) pm.physW = pm.info->wMM * maxPPI;
        else pm.physW = pm.info->w;

        if (pm.info->hMM > 0) pm.physH = pm.info->hMM * maxPPI;
        else pm.physH = pm.info->h;

        bool placedX = false;
        for (const auto& neighbor : physMonitors) {
            if (&neighbor == &pm) continue;
            if (std::abs((neighbor.info->x + neighbor.info->w) - pm.info->x) < 5) {
                int yOverlap = std::min(neighbor.info->y + neighbor.info->h, pm.info->y + pm.info->h) - std::max(neighbor.info->y, pm.info->y);
                if (yOverlap > 0) {
                     pm.physX = neighbor.physX + neighbor.physW;
                     placedX = true;
                     break; 
                }
            }
        }
        
        if (!placedX) {
            if (pm.info->x == 0) pm.physX = 0;
            else pm.physX = pm.info->x;
        }
    }
    
    // Sort by Y then X
    std::sort(physMonitors.begin(), physMonitors.end(), [](const SMonitorPhysInfo& a, const SMonitorPhysInfo& b) {
        if (a.info->y != b.info->y) return a.info->y < b.info->y;
        return a.info->x < b.info->x;
    });

    for (auto& pm : physMonitors) {
         bool placedY = false;
         for (const auto& neighbor : physMonitors) {
             if (&neighbor == &pm) continue;
             if (std::abs((neighbor.info->y + neighbor.info->h) - pm.info->y) < 5) {
                 int xOverlap = std::min(neighbor.info->x + neighbor.info->w, pm.info->x + pm.info->w) - std::max(neighbor.info->x, pm.info->x);
                 if (xOverlap > 0) {
                     pm.physY = neighbor.physY + neighbor.physH;
                     placedY = true;
                     break;
                 }
             }
         }
         
         if (!placedY) {
             if (pm.info->y == 0) pm.physY = 0;
             else pm.physY = pm.info->y;
         }
    }

    // Log physical positions
    for (auto& pm : physMonitors) {
        std::cout << "Monitor " << pm.info->name << ": PhysX=" << pm.physX << ", PhysY=" << pm.physY 
                  << ", PhysW=" << pm.physW << ", PhysH=" << pm.physH << std::endl;
    }

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

    double globalW = maxPX - minPX;
    double globalH = maxPY - minPY;

    double offsetX = -(myPhysInfo->physX - minPX);
    double offsetY = -(myPhysInfo->physY - minPY);

    double scaleX = 1.0;
    double scaleY = 1.0;
    
    if (myPhysInfo->physW > 0) scaleX = (double)myPhysInfo->info->w / myPhysInfo->physW;
    if (myPhysInfo->physH > 0) scaleY = (double)myPhysInfo->info->h / myPhysInfo->physH;
    
    SViewport vp;
    vp.globalSize = { globalW * scaleX, globalH * scaleY };
    vp.offset = { offsetX * scaleX, offsetY * scaleY };
    
    return vp;
}

int main() {
    std::cout << "Debugging eDP-1:" << std::endl;
    auto vp = recalculateSpan("eDP-1");
    if (vp) {
        std::cout << "Viewport eDP-1: Size=" << vp->globalSize.x << "x" << vp->globalSize.y 
                  << " Offset=" << vp->offset.x << "," << vp->offset.y << std::endl;
    } else {
        std::cout << "No viewport for eDP-1" << std::endl;
    }

    std::cout << "\nDebugging DP-2:" << std::endl;
    vp = recalculateSpan("DP-2");
    if (vp) {
        std::cout << "Viewport DP-2: Size=" << vp->globalSize.x << "x" << vp->globalSize.y 
                  << " Offset=" << vp->offset.x << "," << vp->offset.y << std::endl;
    }
    return 0;
}
