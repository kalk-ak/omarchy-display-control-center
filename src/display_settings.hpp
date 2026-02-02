#ifndef DISPLAY_SETTINGS_HPP
#define DISPLAY_SETTINGS_HPP

#include <string>
#include <vector>

/** A single display mode: resolution and refresh rate. */
struct DisplayMode
{
    int width = 0;
    int height = 0;
    int refreshRate = 0;

    std::string resolutionStr() const;
    std::string refreshStr() const;
};

/** Info for one monitor from hyprctl monitors -j. */
struct MonitorInfo
{
    std::string name;
    int x = 0;
    int y = 0;
    double scale = 1.0;
    std::vector<DisplayMode> modes;
    /** Current mode (from width/height/refreshRate in JSON). */
    DisplayMode current;
};

/**
 * Query and apply Hyprland monitor resolution/refresh rate.
 * Uses hyprctl monitors -j and hyprctl keyword monitor.
 */
namespace DisplaySettings
{
/** Run hyprctl monitors -j and parse JSON. Returns empty on error. */
std::vector<MonitorInfo> getMonitors();

/** Apply monitor config: hyprctl keyword monitor <name>,<res>@<hz>,<pos>,<scale> */
bool applyMonitor(const std::string &name, int width, int height, int refreshRate,
                 int posX, int posY, double scale, std::string *error = nullptr);
} // namespace DisplaySettings

#endif // DISPLAY_SETTINGS_HPP
