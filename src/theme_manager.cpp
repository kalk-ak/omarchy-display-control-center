#include "theme_manager.hpp"
#include <fstream>
#include <sstream>
#include <algorithm>
#include <cstdlib>

namespace
{
std::string expandPath(const char *suffix)
{
    const char *home = std::getenv("HOME");
    if (!home || !*home)
        return "";
    std::string path = home;
    path += "/.config/omarchy/";
    path += suffix;
    return path;
}

std::string trim(const std::string &s)
{
    auto start = s.find_first_not_of(" \t\r\n");
    if (start == std::string::npos)
        return "";
    auto end = s.find_last_not_of(" \t\r\n");
    return s.substr(start, end == std::string::npos ? std::string::npos : end - start + 1);
}
} // namespace

ThemeManager::ThemeManager() : m_monitor(), m_callback() {}

ThemeManager::~ThemeManager()
{
    unwatchThemeFile();
}

std::string ThemeManager::getThemePath()
{
    std::string path1 = expandPath("theme/colors");
    std::string path2 = expandPath("theme.conf");
    if (Glib::file_test(path1, Glib::FileTest::EXISTS))
        return path1;
    if (Glib::file_test(path2, Glib::FileTest::EXISTS))
        return path2;
    return "";
}

std::unordered_map<std::string, std::string> ThemeManager::parseThemeFile(const std::string &path)
{
    std::unordered_map<std::string, std::string> colors;
    std::ifstream f(path);
    if (!f)
        return colors;
    std::string line;
    while (std::getline(f, line))
    {
        line = trim(line);
        if (line.empty() || line[0] == '#')
            continue;
        size_t eq = line.find('=');
        if (eq == std::string::npos)
            continue;
        std::string key = trim(line.substr(0, eq));
        std::string value = trim(line.substr(eq + 1));
        if (!key.empty() && value.size() >= 4 && (value[0] == '#' || (value.size() >= 3 && value.compare(0, 3, "rgb") == 0)))
        {
            if (value[0] == '#')
                colors[key] = value;
            else
            {
                // optional: normalize rgb(r,g,b) to #rrggbb (simplified)
                colors[key] = value;
            }
        }
    }
    return colors;
}

std::unordered_map<std::string, std::string> ThemeManager::fallbackColors()
{
    return {
        {"background", "#1e1e2e"},
        {"foreground", "#cdd6f4"},
        {"primary", "#89b4fa"},
        {"secondary", "#f5c2e7"},
        {"accent", "#a6e3a1"},
        {"border", "#45475a"},
    };
}

std::string ThemeManager::buildCSSFromColors(const std::unordered_map<std::string, std::string> &colors)
{
    auto get = [&](const std::string &key, const std::string &def) -> std::string {
        auto it = colors.find(key);
        if (it != colors.end())
            return it->second;
        return def;
    };
    std::string bg = get("background", "#1e1e2e");
    std::string fg = get("foreground", "#cdd6f4");
    std::string default_accent = get("accent", "#89b4fa");
    std::string primary = get("primary", default_accent);
    std::string accent = get("accent", primary);
    std::string border = get("border", "#45475a");
    std::string buttonBg = get("secondary", "#313244");
    if (buttonBg.empty())
        buttonBg = "#313244";
    std::string buttonHover = get("accent", primary);
    if (buttonHover.empty())
        buttonHover = "#45475a";

    std::ostringstream css;
    css << "window { background-color: " << bg << "; color: " << fg << "; }\n";
    css << "frame { margin: 10px; border: 1px solid " << border << "; border-radius: 8px; padding: 12px; }\n";
    css << "scale highlight { background-color: " << primary << "; }\n";
    css << "button { margin: 4px; padding: 8px; background-color: " << buttonBg << "; border: none; border-radius: 4px; color: " << fg << "; }\n";
    css << "button:hover { background-color: " << buttonHover << "; }\n";
    css << "label { font-size: 16px; margin: 0 10px; color: " << fg << "; }\n";
    css << "dropdown, combobox { background-color: " << buttonBg << "; color: " << fg << "; border: 1px solid " << border << "; border-radius: 4px; padding: 6px; }\n";
    css << "dropdown:hover, combobox:hover { background-color: " << buttonHover << "; }\n";
    return css.str();
}

std::string ThemeManager::loadThemeAndGetCSS()
{
    std::string path = getThemePath();
    if (path.empty())
        return buildCSSFromColors(fallbackColors());
    auto colors = parseThemeFile(path);
    if (colors.empty())
        return buildCSSFromColors(fallbackColors());
    return buildCSSFromColors(colors);
}

void ThemeManager::watchThemeFile(ThemeChangedCallback callback)
{
    m_callback = std::move(callback);
    std::string path = getThemePath();
    if (path.empty())
        return;
    unwatchThemeFile();
    try
    {
        auto f = Gio::File::create_for_path(path);
        m_monitor = f->monitor_file();
        m_monitor_conn = m_monitor->signal_changed().connect(
            [this](const Glib::RefPtr<Gio::File> &, const Glib::RefPtr<Gio::File> &, Gio::FileMonitor::Event event) {
                if (event == Gio::FileMonitor::Event::CHANGED || event == Gio::FileMonitor::Event::CHANGES_DONE_HINT)
                {
                    if (m_callback)
                        m_callback();
                }
            });
    }
    catch (const Glib::Error &)
    {
        // ignore
    }
}

void ThemeManager::unwatchThemeFile()
{
    m_monitor_conn.disconnect();
    m_monitor.reset();
}
