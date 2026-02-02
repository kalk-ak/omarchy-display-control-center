#include "display_settings.hpp"
#include <glibmm.h>
#include <sstream>
#include <algorithm>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

std::string DisplayMode::resolutionStr() const
{
    std::ostringstream o;
    o << width << "x" << height;
    return o.str();
}

std::string DisplayMode::refreshStr() const
{
    return std::to_string(refreshRate) + " Hz";
}

namespace
{
bool parseMonitor(const json &j, MonitorInfo &out)
{
    if (!j.contains("name") || !j["name"].is_string())
        return false;
    out.name = j["name"].get<std::string>();
    out.x = j.value("x", 0);
    out.y = j.value("y", 0);
    out.scale = j.value("scale", 1.0);
    out.current.width = j.value("width", 0);
    out.current.height = j.value("height", 0);
    if (j.contains("refreshRate"))
        out.current.refreshRate = static_cast<int>(j["refreshRate"].get<double>());
    else if (j.contains("refresh_rate"))
        out.current.refreshRate = static_cast<int>(j["refresh_rate"].get<double>());
    else
        out.current.refreshRate = 60;

    out.modes.clear();
    if (j.contains("modes") && j["modes"].is_array())
    {
        for (const auto &m : j["modes"])
        {
            DisplayMode mode;
            mode.width = m.value("width", 0);
            mode.height = m.value("height", 0);
            if (m.contains("refreshRate"))
                mode.refreshRate = static_cast<int>(m["refreshRate"].get<double>());
            else if (m.contains("refresh_rate"))
                mode.refreshRate = static_cast<int>(m["refresh_rate"].get<double>());
            else
                mode.refreshRate = 60;
            if (mode.width > 0 && mode.height > 0 && mode.refreshRate > 0)
                out.modes.push_back(mode);
        }
    }
    if (out.modes.empty() && out.current.width > 0 && out.current.height > 0 && out.current.refreshRate > 0)
        out.modes.push_back(out.current);
    return true;
}
} // namespace

std::vector<MonitorInfo> DisplaySettings::getMonitors()
{
    std::vector<MonitorInfo> result;
    std::string stdout_str;
    std::string stderr_str;
    int exit_status = 0;
    try
    {
        Glib::spawn_sync("", {"hyprctl", "monitors", "-j"}, Glib::SpawnFlags::SEARCH_PATH,
                         {}, &stdout_str, &stderr_str, &exit_status);
    }
    catch (const Glib::Error &)
    {
        return result;
    }
    if (exit_status != 0 || stdout_str.empty())
        return result;
    json j;
    try
    {
        j = json::parse(stdout_str);
    }
    catch (const json::exception &)
    {
        return result;
    }
    if (!j.is_array())
        return result;
    for (const auto &mon : j)
    {
        MonitorInfo info;
        if (parseMonitor(mon, info))
            result.push_back(info);
    }
    return result;
}

bool DisplaySettings::applyMonitor(const std::string &name, int width, int height, int refreshRate,
                                   int posX, int posY, double scale, std::string *error)
{
    if (name.empty() || width <= 0 || height <= 0 || refreshRate <= 0)
    {
        if (error)
            *error = "Invalid parameters";
        return false;
    }
    std::ostringstream cmd;
    cmd << width << "x" << height << "@" << refreshRate << "," << posX << "x" << posY << "," << scale;
    std::string arg = cmd.str();
    std::vector<std::string> argv = {"hyprctl", "keyword", "monitor", name + "," + arg};
    std::string stdout_str, stderr_str;
    int exit_status = 0;
    try
    {
        Glib::spawn_sync("", argv, Glib::SpawnFlags::SEARCH_PATH, {}, &stdout_str, &stderr_str, &exit_status);
    }
    catch (const Glib::Error &e)
    {
        if (error)
            *error = e.what();
        return false;
    }
    if (exit_status != 0)
    {
        if (error)
            *error = stderr_str.empty() ? stdout_str : stderr_str;
        if (error && error->empty())
            *error = "hyprctl failed with exit code " + std::to_string(exit_status);
        return false;
    }
    return true;
}
