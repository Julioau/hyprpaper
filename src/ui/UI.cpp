#include "UI.hpp"
#include "../defines.hpp"
#include "../helpers/Logger.hpp"
#include "../helpers/GlobalState.hpp"
#include "../helpers/MonitorLayout.hpp"
#include "../ipc/HyprlandSocket.hpp"
#include "../ipc/IPC.hpp"
#include "../config/WallpaperMatcher.hpp"

#include <hyprtoolkit/core/Output.hpp>

#include <hyprutils/string/String.hpp>
#include <algorithm>
#include <cmath>
#include <iostream>
#include <limits>

CUI::CUI() = default;

CUI::~CUI() {
    m_targets.clear();
}

static std::string_view pruneDesc(const std::string_view& sv) {
    if (sv.contains('('))
        return Hyprutils::String::trim(sv.substr(0, sv.find_last_of('(')));
    return sv;
}

class CWallpaperTarget::CImagesData {
  public:
    CImagesData(Hyprtoolkit::eImageFitMode fitMode, const std::vector<std::string>& images, const int timeout = 0, const std::optional<SViewport>& viewport = std::nullopt) :
        fitMode(fitMode), images(images), timeout(timeout > 0 ? timeout : 30), viewport(viewport) {}

    const Hyprtoolkit::eImageFitMode fitMode;
    const std::vector<std::string>   images;
    const int                        timeout;
    const std::optional<SViewport>   viewport;

    std::string                      nextImage() {
        current = (current + 1) % images.size();
        return images[current];
    }

  private:
    size_t current = 0;
};

CWallpaperTarget::CWallpaperTarget(SP<Hyprtoolkit::IBackend> backend, SP<Hyprtoolkit::IOutput> output, const std::vector<std::string>& path, Hyprtoolkit::eImageFitMode fitMode,
                                   const int timeout, const std::optional<SViewport>& viewport) : m_monitorName(output->port()), m_backend(backend), m_viewport(viewport) {
    static const auto SPLASH_REPLY = HyprlandSocket::getFromSocket("/splash");

    static const auto PENABLESPLASH = Hyprlang::CSimpleConfigValue<Hyprlang::INT>(g_config->hyprlang(), "splash");
    static const auto PSPLASHOFFSET = Hyprlang::CSimpleConfigValue<Hyprlang::INT>(g_config->hyprlang(), "splash_offset");
    static const auto PSPLASHALPHA  = Hyprlang::CSimpleConfigValue<Hyprlang::FLOAT>(g_config->hyprlang(), "splash_opacity");

    ASSERT(path.size() > 0);

    m_window = Hyprtoolkit::CWindowBuilder::begin()
                   ->type(Hyprtoolkit::HT_WINDOW_LAYER)
                   ->prefferedOutput(output)
                   ->anchor(0xF)
                   ->layer(0)
                   ->preferredSize({0, 0})
                   ->exclusiveZone(-1)
                   ->appClass("hyprpaper")
                   ->commence();

    m_bg = Hyprtoolkit::CRectangleBuilder::begin()
               ->size({Hyprtoolkit::CDynamicSize::HT_SIZE_PERCENT, Hyprtoolkit::CDynamicSize::HT_SIZE_PERCENT, {1, 1}})
               ->color([] { return Hyprtoolkit::CHyprColor{0xFF000000}; })
               ->commence();
    m_null = Hyprtoolkit::CNullBuilder::begin()->size({Hyprtoolkit::CDynamicSize::HT_SIZE_PERCENT, Hyprtoolkit::CDynamicSize::HT_SIZE_PERCENT, {1, 1}})->commence();

    auto imageBuilder = Hyprtoolkit::CImageBuilder::begin()
                  ->path(std::string{path.front()})
                  ->sync(true)
                  ->fitMode(fitMode);

    if (m_viewport) {
        imageBuilder->size({Hyprtoolkit::CDynamicSize::HT_SIZE_ABSOLUTE, Hyprtoolkit::CDynamicSize::HT_SIZE_ABSOLUTE, { (float)m_viewport->globalSize.x, (float)m_viewport->globalSize.y }});
    } else {
        imageBuilder->size({Hyprtoolkit::CDynamicSize::HT_SIZE_PERCENT, Hyprtoolkit::CDynamicSize::HT_SIZE_PERCENT, {1.F, 1.F}});
    }
    
    m_image = imageBuilder->commence();

    if (m_viewport) {
        m_image->setPositionMode(Hyprtoolkit::IElement::HT_POSITION_ABSOLUTE);
        m_image->setAbsolutePosition({ (float)m_viewport->offset.x, (float)m_viewport->offset.y });
    } else {
        m_image->setPositionMode(Hyprtoolkit::IElement::HT_POSITION_ABSOLUTE);
        m_image->setPositionFlag(Hyprtoolkit::IElement::HT_POSITION_FLAG_CENTER, true);
    }

    if (path.size() > 1) {
        m_imagesData = makeUnique<CImagesData>(fitMode, path, timeout, m_viewport);
        m_timer =
            m_backend->addTimer(std::chrono::milliseconds(std::chrono::seconds(m_imagesData->timeout)), [this](ASP<Hyprtoolkit::CTimer> self, void*) { onRepeatTimer(); }, nullptr);
    }

    m_window->m_rootElement->addChild(m_bg);
    m_window->m_rootElement->addChild(m_null);
    m_null->addChild(m_image);

    if (!SPLASH_REPLY)
        g_logger->log(LOG_ERR, "Can't get splash: {}", SPLASH_REPLY.error());

    if (SPLASH_REPLY && *PENABLESPLASH) {
        m_splash = Hyprtoolkit::CTextBuilder::begin()
                       ->text(std::string{SPLASH_REPLY.value()})
                       ->fontSize({Hyprtoolkit::CFontSize::HT_FONT_TEXT, 1.15F})
                       ->color([] { return g_ui->backend()->getPalette()->m_colors.text; })
                       ->a(*PSPLASHALPHA)
                       ->commence();
        m_splash->setPositionMode(Hyprtoolkit::IElement::HT_POSITION_ABSOLUTE);
        m_splash->setPositionFlag(Hyprtoolkit::IElement::HT_POSITION_FLAG_HCENTER, true);
        m_splash->setPositionFlag(Hyprtoolkit::IElement::HT_POSITION_FLAG_BOTTOM, true);
        m_splash->setAbsolutePosition({0.F, sc<float>(-*PSPLASHOFFSET)});
        m_null->addChild(m_splash);
    }

    m_window->open();
}

CWallpaperTarget::~CWallpaperTarget() {
    if (m_timer && !m_timer->passed())
        m_timer->cancel();
}

void CWallpaperTarget::onRepeatTimer() {

    ASSERT(m_imagesData);

    auto BUILDER = m_image->rebuild();
    
    BUILDER->path(m_imagesData->nextImage())
           ->sync(true)
           ->fitMode(m_imagesData->fitMode);
    
    if (m_imagesData->viewport) {
        BUILDER->size({Hyprtoolkit::CDynamicSize::HT_SIZE_ABSOLUTE, Hyprtoolkit::CDynamicSize::HT_SIZE_ABSOLUTE, { (float)m_imagesData->viewport->globalSize.x, (float)m_imagesData->viewport->globalSize.y }});
        // Position update needs to be done on the element if rebuild doesn't support it directly in this chain context easily, 
        // but typically rebuild() preserves properties unless overwritten.
        // Size is overwritten here. Position should be preserved if not touched?
        // Let's assume yes. If not, we can re-apply absolute position after commence() if needed, but builder usually returns element.
    } else {
        BUILDER->size({Hyprtoolkit::CDynamicSize::HT_SIZE_PERCENT, Hyprtoolkit::CDynamicSize::HT_SIZE_PERCENT, {1.F, 1.F}});
    }

    BUILDER->commence();
    
    // Ensure position is correct if viewport is active (rebuild might reset layout flags?)
    if (m_imagesData->viewport) {
         m_image->setPositionMode(Hyprtoolkit::IElement::HT_POSITION_ABSOLUTE);
         m_image->setAbsolutePosition({ (float)m_imagesData->viewport->offset.x, (float)m_imagesData->viewport->offset.y });
    }

    m_timer =
        m_backend->addTimer(std::chrono::milliseconds(std::chrono::seconds(m_imagesData->timeout)), [this](ASP<Hyprtoolkit::CTimer> self, void*) { onRepeatTimer(); }, nullptr);
}

void CUI::registerOutput(const SP<Hyprtoolkit::IOutput>& mon) {
    g_matcher->registerOutput(mon->port(), pruneDesc(mon->desc()));
    if (IPC::g_IPCSocket)
        IPC::g_IPCSocket->onNewDisplay(mon->port());
    mon->m_events.removed.listenStatic([this, m = WP<Hyprtoolkit::IOutput>{mon}] {
        g_matcher->unregisterOutput(m->port());
        if (IPC::g_IPCSocket)
            IPC::g_IPCSocket->onRemovedDisplay(m->port());
        std::erase_if(m_targets, [&m](const auto& e) { return e->m_monitorName == m->port(); });
    });
}

bool CUI::run() {
    std::cerr << "DEBUG: CUI::run called" << std::endl;
    static const auto PENABLEIPC = Hyprlang::CSimpleConfigValue<Hyprlang::INT>(g_config->hyprlang(), "ipc");

    //
    Hyprtoolkit::IBackend::SBackendCreationData data;
    data.pLogConnection = makeShared<Hyprutils::CLI::CLoggerConnection>(*g_logger);
    data.pLogConnection->setName("hyprtoolkit");
    data.pLogConnection->setLogLevel(g_state->verbose ? LOG_TRACE : LOG_ERR);

    m_backend = Hyprtoolkit::IBackend::createWithData(data);

    if (!m_backend)
        return false;

    if (*PENABLEIPC)
        IPC::g_IPCSocket = makeUnique<IPC::CSocket>();

    const auto MONITORS = m_backend->getOutputs();

    for (const auto& m : MONITORS) {
        registerOutput(m);
    }

    m_listeners.newMon = m_backend->m_events.outputAdded.listen([this](SP<Hyprtoolkit::IOutput> mon) { registerOutput(mon); });

    g_logger->log(LOG_DEBUG, "Found {} output(s)", MONITORS.size());

    // load the config now, then bind
    for (const auto& m : MONITORS) {
        targetChanged(m);
    }

    m_listeners.targetChanged = g_matcher->m_events.monitorConfigChanged.listen([this](const std::string_view& m) { targetChanged(m); });

    m_backend->enterLoop();

    return true;
}

SP<Hyprtoolkit::IBackend> CUI::backend() {
    return m_backend;
}

static Hyprtoolkit::eImageFitMode toFitMode(const std::string_view& sv) {
    if (sv.starts_with("contain"))
        return Hyprtoolkit::IMAGE_FIT_MODE_CONTAIN;
    if (sv.starts_with("cover"))
        return Hyprtoolkit::IMAGE_FIT_MODE_COVER;
    if (sv.starts_with("tile"))
        return Hyprtoolkit::IMAGE_FIT_MODE_TILE;
    if (sv.starts_with("fill"))
        return Hyprtoolkit::IMAGE_FIT_MODE_STRETCH;
    // span falls back to cover here, handled in targetChanged
    return Hyprtoolkit::IMAGE_FIT_MODE_COVER;
}

std::optional<SViewport> CUI::recalculateSpan(const std::string& monName) {
    auto monitors = MonitorLayout::getMonitors();
    if (monitors.empty())
        return std::nullopt;

    // Calculate PPI for each monitor
    double maxPPI = 0.0;
    for (const auto& m : monitors) {
        if (m.wMM <= 0 || m.hMM <= 0) continue;
        double ppi = (double)m.w / m.wMM;
        if (ppi > maxPPI) maxPPI = ppi;
    }

    if (maxPPI <= 0) {
        g_logger->log(LOG_ERR, "recalculateSpan: Could not calculate valid PPI (maxPPI <= 0)");
        return std::nullopt;
    }

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
        // Physical Size in MaxPPI pixels
        if (pm.info->wMM > 0)
            pm.physW = pm.info->wMM * maxPPI;
        else
            pm.physW = pm.info->w; // Fallback

        if (pm.info->hMM > 0)
            pm.physH = pm.info->hMM * maxPPI;
        else
            pm.physH = pm.info->h; // Fallback

        // Determine Position
        // Heuristic: Find a "Left Neighbor"
        bool placedX = false;
        for (const auto& neighbor : physMonitors) {
            if (&neighbor == &pm) continue; // It's us (or not processed yet fully? No, vector is stable)
            // Note: Since we iterate in sorted order, we can check previous elements.
            // But we are in the loop. We need to check if 'neighbor' has been assigned a valid position?
            // Since we sort by X, neighbors to the left are processed.
            
            // Check for Left Adjacency: Neighbor.LogicalRight approx equals My.LogicalLeft
            if (std::abs((neighbor.info->x + neighbor.info->w) - pm.info->x) < 5) {
                // Check vertical overlap
                int yOverlap = std::min(neighbor.info->y + neighbor.info->h, pm.info->y + pm.info->h) - std::max(neighbor.info->y, pm.info->y);
                if (yOverlap > 0) {
                     pm.physX = neighbor.physX + neighbor.physW;
                     placedX = true;
                     break; 
                }
            }
        }
        
        if (!placedX) {
            // Disjoint or first monitor?
            // If it's the very first (min X), physX is 0.
            // If it's disjoint, we estimate position based on logical gap?
            // Let's assume logical coordinates roughly map to physical if gaps exist.
            // Scale logical X by (MaxPPI / StandardDPI) ? Or MaxPPI / 96?
            // Or simpler: Just keep relative offset to 0.
            // But we don't know the PPI of the "gap".
            // Let's assume the gap has the same PPI as THIS monitor (as if this monitor extended left).
            // physX = logicalX * (CurrentPPI / Scale?) -> No.
            
            // Fallback: logicalX.
            // NOTE: This might be huge if logicalX is large.
            // But if all monitors are handled by adjacency, this only triggers for the root.
            if (pm.info->x == 0) pm.physX = 0;
            else pm.physX = pm.info->x; // Potentially wrong but safe fallback
        }
    }
    
    // Do the same for Y (Vertical Stacking)
    // Sort by Y then X
    std::sort(physMonitors.begin(), physMonitors.end(), [](const SMonitorPhysInfo& a, const SMonitorPhysInfo& b) {
        if (a.info->y != b.info->y) return a.info->y < b.info->y;
        return a.info->x < b.info->x;
    });

    for (auto& pm : physMonitors) {
         bool placedY = false;
         for (const auto& neighbor : physMonitors) {
             if (&neighbor == &pm) continue;
             
             // Top Adjacency
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

    // Find bounding box of physical layout
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

    if (!myPhysInfo) {
        g_logger->log(LOG_ERR, "recalculateSpan: Monitor {} not found in layout", monName);
        return std::nullopt;
    }

    // Calculate Viewport
    // Global Size is the size of the bounding box
    double globalW = maxPX - minPX;
    double globalH = maxPY - minPY;

    // Offset of this monitor within the global box
    double offsetX = -(myPhysInfo->physX - minPX);
    double offsetY = -(myPhysInfo->physY - minPY);

    // SCALE CORRECTION
    // The CWallpaperTarget will render an Image Element.
    // We want the slice of the image corresponding to this monitor to match the monitor's resolution.
    // The slice width is `myPhysInfo->physW`.
    // The monitor resolution width is `myPhysInfo->info->w`.
    // So we must scale the entire image by `Scale = info->w / physW`.

    double scaleX = 1.0;
    double scaleY = 1.0;
    
    if (myPhysInfo->physW > 0) scaleX = (double)myPhysInfo->info->w / myPhysInfo->physW;
    if (myPhysInfo->physH > 0) scaleY = (double)myPhysInfo->info->h / myPhysInfo->physH;
    
    // Apply scale to global metrics
    // Note: We use the same scale for X and Y if we want to preserve aspect ratio?
    // Wallcrop logic: `resize((m["width"], m['height']))`. It resizes to fit resolution exactly.
    // So we apply independent scales.

    SViewport vp;
    vp.globalSize = { globalW * scaleX, globalH * scaleY };
    vp.offset = { offsetX * scaleX, offsetY * scaleY };
    
    return vp;
}

void CUI::targetChanged(const std::string_view& monName) {
    const auto               MONITORS = m_backend->getOutputs();
    SP<Hyprtoolkit::IOutput> monitor;

    for (const auto& m : MONITORS) {
        if (m->port() != monName)
            continue;

        monitor = m;
    }

    if (!monitor) {
        g_logger->log(LOG_ERR, "targetChanged but {} has no output?", monName);
        return;
    }

    targetChanged(monitor);
}

void CUI::targetChanged(const SP<Hyprtoolkit::IOutput>& mon) {
    const auto TARGET = g_matcher->getSetting(mon->port(), pruneDesc(mon->desc()));

    if (!TARGET) {
        g_logger->log(LOG_DEBUG, "Monitor {} has no target: no wp will be created", mon->port());
        return;
    }

    std::erase_if(m_targets, [&mon](const auto& e) { return e->m_monitorName == mon->port(); });

    std::optional<SViewport> viewport = std::nullopt;
    if (TARGET->get().fitMode.starts_with("span")) {
         viewport = recalculateSpan(mon->port());
    }

    m_targets.emplace_back(makeShared<CWallpaperTarget>(m_backend, mon, TARGET->get().paths, toFitMode(TARGET->get().fitMode), TARGET->get().timeout, viewport));
}
