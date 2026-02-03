#include "theme_manager.hpp"
#include <fstream>
#include <sstream>
#include <algorithm>
#include <cstdlib>

// ============================================================================
// Anonymous Namespace - Internal Helper Functions
// ============================================================================

namespace
{
/**
 * Expands a path relative to ~/.config/omarchy/
 * 
 * Why this function exists:
 * - We need to construct absolute paths from the user's home directory
 * - Using $HOME is more portable than hardcoding /home/username
 * - Centralizing path expansion prevents code duplication
 * 
 * @param suffix The path component after ~/.config/omarchy/ (e.g., "theme/colors")
 * @return Full expanded path, or empty string if HOME is not set
 */
std::string expandPath(const char *suffix)
{
    // Get HOME environment variable - this is where user config files live
    // We must use getenv because we can't assume the home directory location
    const char *home = std::getenv("HOME");
    
    // If HOME isn't set (rare but possible), we can't determine the config path
    // Return empty string rather than crashing or using an invalid path
    if (!home || !*home)
        return "";
    
    // Build the full path: $HOME/.config/omarchy/<suffix>
    // This follows XDG Base Directory specification conventions
    std::string path = home;
    path += "/.config/omarchy/";
    path += suffix;
    return path;
}

/**
 * Removes leading and trailing whitespace from a string.
 * 
 * Why we need this:
 * - Theme files are hand-edited and may have inconsistent spacing
 * - Whitespace around keys/values would break color lookups and CSS generation
 * - Better to normalize here than require perfect formatting in config files
 * 
 * @param s The string to trim
 * @return Trimmed string with no leading/trailing whitespace
 */
std::string trim(const std::string &s)
{
    // Find first non-whitespace character
    auto start = s.find_first_not_of(" \t\r\n");
    
    // If the entire string is whitespace, return empty
    if (start == std::string::npos)
        return "";
    
    // Find last non-whitespace character
    auto end = s.find_last_not_of(" \t\r\n");
    
    // Extract the substring between first and last non-whitespace chars
    // This handles the case where 'end' might be npos (though it shouldn't be
    // if we found a 'start', but we're being defensive)
    return s.substr(start, end == std::string::npos ? std::string::npos : end - start + 1);
}
} // namespace

// ============================================================================
// ThemeManager Implementation
// ============================================================================

ThemeManager::ThemeManager() : m_monitor(), m_callback() 
{
    // Initialize with null/empty values
    // The actual theme loading happens in loadThemeAndGetCSS()
    // This separation allows the application to control when loading occurs
}

ThemeManager::~ThemeManager()
{
    // Clean up file monitoring resources
    // This is critical to prevent crashes from callbacks firing after object destruction
    unwatchThemeFile();
}

std::string ThemeManager::getThemePath()
{
    // Check both possible theme file locations in priority order
    // This provides flexibility for users - they can use either location
    
    // Primary location: theme/colors (more specific, takes precedence)
    std::string path1 = expandPath("theme/colors");
    
    // Fallback location: theme.conf (legacy or alternative location)
    std::string path2 = expandPath("theme.conf");
    
    // Return the first path that actually exists
    // Using Glib::file_test is safer than trying to open the file,
    // as it doesn't require file permissions and is faster
    if (Glib::file_test(path1, Glib::FileTest::EXISTS))
        return path1;
    if (Glib::file_test(path2, Glib::FileTest::EXISTS))
        return path2;
    
    // No theme file found - caller should use fallback colors
    return "";
}

std::unordered_map<std::string, std::string> ThemeManager::parseThemeFile(const std::string &path)
{
    std::unordered_map<std::string, std::string> colors;
    
    // Open the theme file for reading
    std::ifstream f(path);
    if (!f)
    {
        // File couldn't be opened (permissions, doesn't exist, etc.)
        // Return empty map - caller will use fallback colors
        return colors;
    }
    
    // Process the file line by line
    // This approach handles files of any size efficiently without loading
    // the entire file into memory at once
    std::string line;
    while (std::getline(f, line))
    {
        // Remove leading/trailing whitespace to normalize input
        // This allows flexible formatting in the theme file
        line = trim(line);
        
        // Skip empty lines - these are just for readability in the config file
        if (line.empty())
            continue;
        
        // Skip comment lines (starting with #)
        // This allows users to document their theme files
        if (line[0] == '#')
            continue;
        
        // Find the '=' separator between key and value
        // Theme file format: key=value (e.g., background=#1e1e2e)
        size_t eq = line.find('=');
        if (eq == std::string::npos)
        {
            // No '=' found - this isn't a valid key=value line
            // Skip it rather than treating it as an error
            continue;
        }
        
        // Split into key and value, trimming both
        // This handles cases like "key = value" or "key=value" uniformly
        std::string key = trim(line.substr(0, eq));
        std::string value = trim(line.substr(eq + 1));
        
        // Validate that we have both a key and a value that looks like a color
        // We check for # (hex colors) or rgb (rgb colors)
        // This filters out non-color entries that might be in the config file
        if (!key.empty() && value.size() >= 4 && 
            (value[0] == '#' || (value.size() >= 3 && value.compare(0, 3, "rgb") == 0)))
        {
            if (value[0] == '#')
            {
                // Hex color - store as-is (e.g., #1e1e2e)
                colors[key] = value;
            }
            else
            {
                // RGB format (e.g., rgb(30, 30, 46))
                // We could normalize this to hex, but GTK4 CSS accepts rgb() directly
                // so we store it as-is for simplicity
                colors[key] = value;
            }
        }
    }
    
    return colors;
}

std::unordered_map<std::string, std::string> ThemeManager::fallbackColors()
{
    // Provide a complete default theme based on Catppuccin Mocha
    // This ensures the application always looks good even without a config file
    // 
    // Why these specific colors:
    // - Catppuccin is a popular, well-designed theme with good contrast
    // - Dark theme reduces eye strain and is preferred by many developers
    // - These colors have been tested for readability and aesthetics
    return {
        {"background", "#1e1e2e"},  // Dark blue-gray background
        {"foreground", "#cdd6f4"},  // Light lavender text
        {"primary", "#89b4fa"},     // Bright blue for highlights
        {"secondary", "#f5c2e7"},   // Pink for secondary elements
        {"accent", "#a6e3a1"},      // Green for accents
        {"border", "#45475a"},      // Gray for borders
    };
}

std::string ThemeManager::buildCSSFromColors(const std::unordered_map<std::string, std::string> &colors)
{
    // Helper lambda to safely get colors with fallback defaults
    // This ensures we always have valid colors even if the theme file is incomplete
    auto get = [&](const std::string &key, const std::string &def) -> std::string {
        auto it = colors.find(key);
        if (it != colors.end())
            return it->second;
        return def;
    };
    
    // Extract common theme colors with sensible defaults
    // We provide defaults for every color to ensure CSS is always valid
    std::string bg = get("background", "#1e1e2e");
    std::string fg = get("foreground", "#cdd6f4");
    
    // For accent, fall back to primary if not specified
    // This maintains visual consistency even with minimal theme files
    std::string default_accent = get("accent", "#89b4fa");
    std::string primary = get("primary", default_accent);
    std::string accent = get("accent", primary);
    std::string border = get("border", "#45475a");
    
    // Button colors need special handling
    // We want buttons to stand out from the background but not be too bright
    std::string buttonBg = get("secondary", "#313244");
    if (buttonBg.empty())
        buttonBg = "#313244";
    
    // Hover state should be noticeably different to provide feedback
    std::string buttonHover = get("accent", primary);
    if (buttonHover.empty())
        buttonHover = "#45475a";
    
    // Build the complete CSS stylesheet
    // We use ostringstream for efficient string concatenation
    std::ostringstream css;
    
    // Window styling - sets the base background and text color for everything
    css << "window { background-color: " << bg << "; color: " << fg << "; }\n";
    
    // Frame styling - adds visual separation between different sections
    // Border radius provides modern rounded corners
    css << "frame { margin: 10px; border: 1px solid " << border 
        << "; border-radius: 8px; padding: 12px; }\n";
    
    // Scale widget highlight - colors the active portion of sliders
    // Using primary color makes it clear where the value is set
    css << "scale highlight { background-color: " << primary << "; }\n";
    
    // Button styling - makes buttons distinct and clickable-looking
    // No border gives a modern flat appearance, with hover state for feedback
    css << "button { margin: 4px; padding: 8px; background-color: " << buttonBg 
        << "; border: none; border-radius: 4px; color: " << fg << "; }\n";
    css << "button:hover { background-color: " << buttonHover << "; }\n";
    
    // Label styling - ensures text is readable and appropriately sized
    css << "label { font-size: 16px; margin: 0 10px; color: " << fg << "; }\n";
    
    // Dropdown/combobox styling - matches button styling for consistency
    // These need explicit colors or they might inherit unwanted defaults
    css << "dropdown, combobox { background-color: " << buttonBg << "; color: " << fg 
        << "; border: 1px solid " << border << "; border-radius: 4px; padding: 6px; }\n";
    css << "dropdown:hover, combobox:hover { background-color: " << buttonHover << "; }\n";
    
    return css.str();
}

std::string ThemeManager::loadThemeAndGetCSS()
{
    // Find which theme file exists (if any)
    std::string path = getThemePath();
    
    // If no theme file exists, immediately return fallback CSS
    // This is expected behavior when the user hasn't configured a custom theme
    if (path.empty())
        return buildCSSFromColors(fallbackColors());
    
    // Parse the theme file into a color map
    auto colors = parseThemeFile(path);
    
    // If parsing failed or produced no colors, use fallback
    // This handles corrupted files or files with no valid color entries
    if (colors.empty())
        return buildCSSFromColors(fallbackColors());
    
    // Generate CSS from the parsed colors
    return buildCSSFromColors(colors);
}

void ThemeManager::watchThemeFile(ThemeChangedCallback callback)
{
    // Store the callback for later invocation
    // We need to keep this around because the file monitor will call it asynchronously
    m_callback = std::move(callback);
    
    // Find the active theme file path
    std::string path = getThemePath();
    
    // If no theme file exists, we can't monitor anything
    // Return silently - this isn't an error, just means no custom theme is configured
    if (path.empty())
        return;
    
    // Clean up any existing monitor before creating a new one
    // This makes the function idempotent - calling it multiple times is safe
    unwatchThemeFile();
    
    try
    {
        // Create a GIO File object for the theme file
        // GIO is the low-level I/O library used by GTK/GLib
        auto f = Gio::File::create_for_path(path);
        
        // Create a file monitor
        // This uses inotify (on Linux) or equivalent platform APIs to efficiently
        // detect file changes without polling
        m_monitor = f->monitor_file();
        
        // Connect our callback to the monitor's changed signal
        // Lambda captures 'this' so we can access m_callback
        m_monitor_conn = m_monitor->signal_changed().connect(
            [this](const Glib::RefPtr<Gio::File> &, 
                   const Glib::RefPtr<Gio::File> &, 
                   Gio::FileMonitor::Event event) 
            {
                // Filter for relevant events
                // CHANGED: File content was modified
                // CHANGES_DONE_HINT: Editor finished saving (some editors trigger this)
                // We ignore other events like DELETED, CREATED, etc.
                if (event == Gio::FileMonitor::Event::CHANGED || 
                    event == Gio::FileMonitor::Event::CHANGES_DONE_HINT)
                {
                    // Invoke the user's callback to reload the theme
                    // This typically reloads CSS and updates the UI
                    if (m_callback)
                        m_callback();
                }
            });
    }
    catch (const Glib::Error &)
    {
        // File monitoring failed (permissions, filesystem doesn't support it, etc.)
        // We ignore this error - the application will still work, just without
        // live theme reloading. This is better than crashing.
    }
}

void ThemeManager::unwatchThemeFile()
{
    // Disconnect the signal handler
    // This is crucial to prevent the callback from firing after the ThemeManager
    // is destroyed, which would cause a use-after-free crash
    m_monitor_conn.disconnect();
    
    // Release the file monitor
    // Using reset() on a RefPtr decrements the reference count and may free the object
    m_monitor.reset();
    
    // After this, m_monitor is null and monitoring is fully stopped
}
