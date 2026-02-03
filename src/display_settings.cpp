#include "display_settings.hpp"
#include <glibmm.h>
#include <sstream>
#include <algorithm>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

// ============================================================================
// DisplayMode Implementation
// ============================================================================

std::string DisplayMode::resolutionStr() const
{
    // Format resolution as "WIDTHxHEIGHT" which is the standard notation
    // used in display settings across most operating systems and tools
    std::ostringstream o;
    o << width << "x" << height;
    return o.str();
}

std::string DisplayMode::refreshStr() const
{
    // Return refresh rate with "Hz" suffix for user-friendly display
    // This makes it immediately clear what the number represents
    return std::to_string(refreshRate) + " Hz";
}

// ============================================================================
// Internal Parsing Helpers
// ============================================================================

namespace
{
/**
 * Parses a single monitor entry from the Hyprland JSON output.
 * 
 * This function handles the complexity of Hyprland's JSON schema, which can
 * vary slightly between versions (e.g., "refreshRate" vs "refresh_rate").
 * 
 * Why this approach:
 * - We need defensive parsing because hyprctl output format may vary
 * - Multiple field name variations must be checked for compatibility
 * - Default values ensure we always have usable data even if fields are missing
 * 
 * @param j   The JSON object representing a single monitor
 * @param out The MonitorInfo structure to populate
 * @return true if the monitor has at least a valid name, false otherwise
 */
bool parseMonitor(const json &j, MonitorInfo &out)
{
    // Name is the only truly required field - without it, we can't identify
    // or configure the monitor, so we return false if it's missing
    if (!j.contains("name") || !j["name"].is_string())
        return false;
    
    out.name = j["name"].get<std::string>();
    
    // Position and scale use defaults if missing - these are safe fallbacks
    // that will work even if Hyprland doesn't report them
    out.x = j.value("x", 0);
    out.y = j.value("y", 0);
    out.scale = j.value("scale", 1.0);
    
    // Extract current mode information
    // This represents what the monitor is currently displaying
    out.current.width = j.value("width", 0);
    out.current.height = j.value("height", 0);
    
    // Hyprland has used different field names for refresh rate across versions
    // We check both "refreshRate" (newer) and "refresh_rate" (older) for compatibility
    // This ensures our code works with multiple Hyprland versions
    if (j.contains("refreshRate"))
        out.current.refreshRate = static_cast<int>(j["refreshRate"].get<double>());
    else if (j.contains("refresh_rate"))
        out.current.refreshRate = static_cast<int>(j["refresh_rate"].get<double>());
    else
        // 60Hz is a safe default - nearly all monitors support this
        out.current.refreshRate = 60;

    // Parse the array of available modes
    // This gives the user all resolution/refresh combinations the monitor supports
    out.modes.clear();
    if (j.contains("modes") && j["modes"].is_array())
    {
        for (const auto &m : j["modes"])
        {
            DisplayMode mode;
            mode.width = m.value("width", 0);
            mode.height = m.value("height", 0);
            
            // Again, handle both possible field names for refresh rate
            if (m.contains("refreshRate"))
                mode.refreshRate = static_cast<int>(m["refreshRate"].get<double>());
            else if (m.contains("refresh_rate"))
                mode.refreshRate = static_cast<int>(m["refresh_rate"].get<double>());
            else
                mode.refreshRate = 60;
            
            // Only add valid modes (non-zero dimensions and refresh rate)
            // This filters out any malformed entries from the JSON
            if (mode.width > 0 && mode.height > 0 && mode.refreshRate > 0)
                out.modes.push_back(mode);
        }
    }
    
    // Fallback: If no modes were provided in the JSON, at least offer the current mode
    // This ensures the user always has something to select, even if mode detection failed
    if (out.modes.empty() && out.current.width > 0 && out.current.height > 0 && out.current.refreshRate > 0)
        out.modes.push_back(out.current);
    
    return true;
}
} // namespace

// ============================================================================
// Public API Implementation
// ============================================================================

std::vector<MonitorInfo> DisplaySettings::getMonitors()
{
    std::vector<MonitorInfo> result;
    std::string stdout_str;
    std::string stderr_str;
    int exit_status = 0;
    
    // Execute hyprctl monitors -j to get JSON output of all monitors
    // Why spawn_sync: We need the output before we can continue, so async wouldn't help
    // Why -j flag: JSON is much more reliable to parse than human-readable text
    try
    {
        Glib::spawn_sync(
            "",                                       // Working directory (empty = inherit)
            {"hyprctl", "monitors", "-j"},            // Command and arguments
            Glib::SpawnFlags::SEARCH_PATH,            // Search PATH for hyprctl
            {},                                       // No custom child setup
            &stdout_str,                              // Capture stdout
            &stderr_str,                              // Capture stderr
            &exit_status                              // Capture exit code
        );
    }
    catch (const Glib::Error &)
    {
        // If hyprctl isn't found or execution fails, return empty list
        // This is better than crashing - the application can still run,
        // just without monitor configuration features
        return result;
    }
    
    // Check for execution failure
    // A non-zero exit status means hyprctl encountered an error
    // Empty output means no data was returned (possibly no Hyprland running)
    if (exit_status != 0 || stdout_str.empty())
        return result;
    
    // Parse the JSON output
    json j;
    try
    {
        j = json::parse(stdout_str);
    }
    catch (const json::exception &)
    {
        // If JSON parsing fails, the output format may have changed
        // or hyprctl may have returned an error message instead of JSON
        // Return empty rather than crashing
        return result;
    }
    
    // The top level should be an array of monitor objects
    // If it's not an array, the format is unexpected - bail out safely
    if (!j.is_array())
        return result;
    
    // Parse each monitor object and add it to results
    // We continue even if individual monitors fail to parse,
    // allowing the user to work with whatever monitors we can detect
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
    // Validate all input parameters before attempting to execute the command
    // This prevents us from sending invalid commands to Hyprland and ensures
    // we catch errors early with clear error messages
    if (name.empty() || width <= 0 || height <= 0 || refreshRate <= 0)
    {
        if (error)
            *error = "Invalid parameters";
        return false;
    }
    
    // Build the monitor configuration string
    // Format: WIDTHxHEIGHT@REFRESH,POSXxPOSY,SCALE
    // Example: 1920x1080@60,0x0,1.0
    // This is the format Hyprland expects for the monitor keyword
    std::ostringstream cmd;
    cmd << width << "x" << height << "@" << refreshRate 
        << "," << posX << "x" << posY << "," << scale;
    std::string arg = cmd.str();
    
    // Construct the full hyprctl command
    // We use "keyword monitor" to dynamically set the monitor configuration
    // The format is: hyprctl keyword monitor NAME,CONFIGURATION
    std::vector<std::string> argv = {"hyprctl", "keyword", "monitor", name + "," + arg};
    
    std::string stdout_str, stderr_str;
    int exit_status = 0;
    
    // Execute the command synchronously
    // Why sync: We need to know immediately if the configuration succeeded
    // so we can report errors to the user
    try
    {
        Glib::spawn_sync(
            "",                                 // Working directory
            argv,                               // Command with arguments
            Glib::SpawnFlags::SEARCH_PATH,      // Search PATH for hyprctl
            {},                                 // No custom setup
            &stdout_str,                        // Capture output
            &stderr_str,                        // Capture errors
            &exit_status                        // Capture exit code
        );
    }
    catch (const Glib::Error &e)
    {
        // Glib::Error means spawn itself failed (command not found, permissions, etc.)
        // This is different from the command running but returning an error exit code
        if (error)
            *error = e.what();
        return false;
    }
    
    // Check if the command succeeded
    // A non-zero exit status means Hyprland rejected the configuration
    // (invalid mode, monitor not found, etc.)
    if (exit_status != 0)
    {
        if (error)
        {
            // Prefer stderr for error messages, fall back to stdout if stderr is empty
            // Hyprland typically reports errors on stderr, but we check both to be safe
            *error = stderr_str.empty() ? stdout_str : stderr_str;
            
            // If both are empty, provide a generic error message with the exit code
            // This gives the user at least some information about what went wrong
            if (error->empty())
                *error = "hyprctl failed with exit code " + std::to_string(exit_status);
        }
        return false;
    }
    
    // Success! The monitor configuration has been applied
    return true;
}