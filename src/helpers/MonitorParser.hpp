#pragma once
#include <string>
#include <vector>

struct SMonitorInfo {
    std::string name;
    int         id        = -1;
    int         x         = 0;
    int         y         = 0;
    int         width     = 0;
    int         height    = 0;
    double      scale     = 1.0;
    int         transform = 0;
    int         widthMM   = 0;
    int         heightMM  = 0;
};

class CMonitorParser {
  public:
    static std::vector<SMonitorInfo> parse(const std::string& json);
};
