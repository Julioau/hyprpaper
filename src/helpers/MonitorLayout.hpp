#pragma once
#include <vector>
#include <string>

struct SMonitorInfo {
    std::string name;
    int         id = -1;
    int         x = 0, y = 0;
    int         w = 0, h = 0;
    double      scale     = 1.0;
    int         transform = 0;
    int         wMM = 0, hMM = 0;
};

namespace MonitorLayout {
    std::vector<SMonitorInfo> getMonitors();
};
