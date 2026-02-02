#ifndef THEME_MANAGER_HPP
#define THEME_MANAGER_HPP

#include <glibmm.h>
#include <gtkmm.h>
#include <functional>
#include <string>
#include <unordered_map>

/**
 * Parses Omarchy theme files and generates GTK4 CSS.
 * Supports ~/.config/omarchy/theme/colors and ~/.config/omarchy/theme.conf.
 * Optional GFileMonitor for live theme reload.
 */
class ThemeManager
{
  public:
    using ThemeChangedCallback = std::function<void()>;

    ThemeManager();
    ~ThemeManager();

    /** Load theme from default paths and return CSS string. Uses fallback on error. */
    std::string loadThemeAndGetCSS();

    /** Build GTK4 CSS from a map of color names to hex values. */
    static std::string buildCSSFromColors(const std::unordered_map<std::string, std::string> &colors);

    /** Start watching the theme file and call callback on change. Idempotent. */
    void watchThemeFile(ThemeChangedCallback callback);

    /** Stop watching. */
    void unwatchThemeFile();

  private:
    static std::string getThemePath();
    static std::unordered_map<std::string, std::string> parseThemeFile(const std::string &path);
    static std::unordered_map<std::string, std::string> fallbackColors();

    Glib::RefPtr<Gio::FileMonitor> m_monitor;
    ThemeChangedCallback m_callback;
    sigc::connection m_monitor_conn;
};

#endif // THEME_MANAGER_HPP
