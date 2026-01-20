#include "MonitorLayout.hpp"
#include "../ipc/HyprlandSocket.hpp"
#include "Logger.hpp"
#include <hyprutils/string/VarList.hpp>
#include <hyprutils/string/String.hpp>
#include <algorithm>

using namespace Hyprutils::String;

std::vector<SMonitorInfo> MonitorLayout::getMonitors() {
    const auto REPLY = HyprlandSocket::getFromSocket("monitors all");

    if (!REPLY) {
        g_logger->log(LOG_ERR, "Failed to get monitors from Hyprland: {}", REPLY.error());
        return {};
    }

    std::vector<SMonitorInfo> monitors;
    CVarList                  lines(REPLY.value(), 0, '\n', true); // split by newline

    SMonitorInfo*             currentMonitor = nullptr;

    for (const auto& line : lines) {
        const auto TRIMMED = trim(line);

        if (TRIMMED.starts_with("Monitor")) {
            // New monitor
            // Format: Monitor NAME (ID X):
            monitors.emplace_back();
            currentMonitor = &monitors.back();

            CVarList parts(TRIMMED, 0, ' ');
            if (parts.size() >= 2)
                currentMonitor->name = parts[1];
            
            // ID extraction
            auto idStart = TRIMMED.find("(ID ");
            if (idStart != std::string::npos) {
                auto idEnd = TRIMMED.find(")", idStart);
                if (idEnd != std::string::npos) {
                    try {
                        currentMonitor->id = std::stoi(TRIMMED.substr(idStart + 4, idEnd - (idStart + 4)));
                    } catch (...) {}
                }
            }
            continue;
        }

        if (!currentMonitor)
            continue;

        if (TRIMMED.starts_with("description:")) {
            currentMonitor->description = TRIMMED.substr(12);
            if (currentMonitor->description.starts_with(' '))
                currentMonitor->description = currentMonitor->description.substr(1);
        }
        else if (TRIMMED.contains("x") && TRIMMED.contains("@") && TRIMMED.contains("at")) {
            // Resolution line: "1920x1080@60.00000 at 0x0"
            // We need to parse this carefully.
            try {
                auto atPos = TRIMMED.find(" at ");
                if (atPos != std::string::npos) {
                    // Parse Position
                    std::string posStr = TRIMMED.substr(atPos + 4);
                    auto xPos = posStr.find('x');
                    if (xPos != std::string::npos) {
                        currentMonitor->x = std::stoi(posStr.substr(0, xPos));
                        currentMonitor->y = std::stoi(posStr.substr(xPos + 1));
                    }

                    // Parse Resolution
                    std::string resStr = TRIMMED.substr(0, TRIMMED.find('@'));
                    auto xRes = resStr.find('x');
                    if (xRes != std::string::npos) {
                        currentMonitor->w = std::stoi(resStr.substr(0, xRes));
                        currentMonitor->h = std::stoi(resStr.substr(xRes + 1));
                    }
                }
            } catch (...) {
                g_logger->log(LOG_ERR, "Failed to parse resolution line: {}", TRIMMED);
            }
        }
        else if (TRIMMED.starts_with("scale:")) {
            try {
                currentMonitor->scale = std::stod(TRIMMED.substr(7));
            } catch (...) {}
        }
        else if (TRIMMED.starts_with("transform:")) {
            try {
                currentMonitor->transform = std::stoi(TRIMMED.substr(11));
            } catch (...) {}
        }
        else if (TRIMMED.starts_with("physical size (mm):")) {
             try {
                std::string sizeStr = TRIMMED.substr(20); // len of "physical size (mm): "
                auto xPos = sizeStr.find('x');
                if (xPos != std::string::npos) {
                    currentMonitor->wMM = std::stoi(sizeStr.substr(0, xPos));
                    currentMonitor->hMM = std::stoi(sizeStr.substr(xPos + 1));
                }
             } catch (...) {}
        }
    }

    // Apply transforms
    for (auto& m : monitors) {
        if (m.transform % 2 == 1) {
            std::swap(m.w, m.h);
            std::swap(m.wMM, m.hMM);
        }
    }

    return monitors;
}
