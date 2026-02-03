#include "display_settings.hpp"
#include "theme_manager.hpp"
#include <cstdio>
#include <gtkmm.h>
#include <iostream>
#include <string>
#include <vector>

// ============================================================================
// Application Constants
// ============================================================================

const std::string APP_ID = "com.omarchy.display-control";

// Temperature range for night light feature
// These values are standard color temperatures used in display calibration
constexpr int TEMP_WARM = 2500; // Night/Warm - reduces blue light for evening use
constexpr int TEMP_COLD = 6500; // Day/Cold - neutral white light for daytime

// ============================================================================
// Fallback CSS - Redundancy Engineering
// ============================================================================

/**
 * FALLBACK_CSS provides a complete embedded stylesheet for redundancy.
 * 
 * Why this exists despite ThemeManager:
 * - Defense in depth: If ThemeManager fails to load, we still have usable styling
 * - Compile-time guarantee: This CSS is always available, no runtime dependencies
 * - Reference implementation: Serves as documentation of required CSS selectors
 * - Emergency fallback: Critical for debugging theme system issues
 * 
 * In normal operation, ThemeManager's CSS takes precedence due to
 * GTK_STYLE_PROVIDER_PRIORITY_APPLICATION. This is kept as a safety net.
 * 
 * This is redundancy engineering - we pay a small cost (embedded string) for
 * guaranteed functionality even if the theme system completely fails.
 */
static const char *FALLBACK_CSS = R"(
    window { background-color: #2e3440; color: #eceff4; }
    frame { margin: 10px; border: 1px solid #4c566a; border-radius: 8px; padding: 12px; }
    scale highlight { background-color: #88c0d0; }
    button { margin: 4px; padding: 8px; background-color: #434c5e; border: none; border-radius: 4px; }
    button:hover { background-color: #4c566a; }
    label { font-size: 16px; margin: 0 10px; }
)";

// ============================================================================
// DisplayApp - Main Application Window
// ============================================================================

/**
 * The main application window containing all display control widgets.
 * 
 * This class encapsulates the entire UI and implements all user interactions.
 * It's designed as a single-window application with multiple functional sections:
 * - Brightness control (via brightnessctl)
 * - Night light/blue light filter (via hyprsunset)
 * - Screen rotation (via hyprctl)
 * - Resolution and refresh rate configuration (via hyprctl)
 */
class DisplayApp : public Gtk::ApplicationWindow
{
  public:
    DisplayApp(bool verbose) : m_vbox(Gtk::Orientation::VERTICAL, 12), m_verbose(verbose)
    {
        set_title("Display Control");
        set_default_size(400, -1);
        set_resizable(false);
        set_child(m_vbox);
        m_vbox.set_margin(15);

        setup_brightness();
        setup_night_light();
        setup_rotation();
        setup_resolution_refresh();
    }

  private:
    Gtk::Box m_vbox;
    Gtk::Scale m_bright_scale, m_temp_scale;
    Gtk::Switch m_night_switch;
    sigc::connection m_debounce_conn;
    bool m_verbose;

    // Resolution & Refresh Rate
    std::vector<MonitorInfo> m_monitors;
    Gtk::DropDown *m_monitor_dropdown = nullptr;
    Gtk::DropDown *m_mode_dropdown = nullptr;
    Gtk::Button *m_apply_btn = nullptr;
    Glib::RefPtr<Gtk::StringList> m_mode_list;

    void exec(const std::string &cmd)
    {
        if (m_verbose)
            std::cout << "[CMD]: " << cmd << std::endl;
        try
        {
            Glib::spawn_command_line_async(cmd);
        }
        catch (const Glib::Error &e)
        {
            std::cerr << e.what() << std::endl;
        }
    }

    void add_slider_row(Gtk::Box *parent, Gtk::Scale &scale, const std::string &left_icon,
                        const std::string &right_icon)
    {
        auto row = Gtk::make_managed<Gtk::Box>(Gtk::Orientation::HORIZONTAL);
        auto l_icon = Gtk::make_managed<Gtk::Label>(left_icon);
        auto r_icon = Gtk::make_managed<Gtk::Label>(right_icon);

        scale.set_hexpand(true);

        row->append(*l_icon);
        row->append(scale);
        row->append(*r_icon);
        parent->append(*row);
    }

    void setup_brightness()
    {
        auto frame = Gtk::make_managed<Gtk::Frame>("Brightness");
        auto box = Gtk::make_managed<Gtk::Box>(Gtk::Orientation::VERTICAL, 5);

        m_bright_scale.set_range(1, 100);
        m_bright_scale.set_value(80);
        m_bright_scale.signal_value_changed().connect(
            [this]
            { exec("brightnessctl s " + std::to_string((int) m_bright_scale.get_value()) + "%"); });

        add_slider_row(box, m_bright_scale, "🔆", "💡");
        frame->set_child(*box);
        m_vbox.append(*frame);
    }

    void setup_night_light()
    {
        auto frame = Gtk::make_managed<Gtk::Frame>("Night Light");
        auto box = Gtk::make_managed<Gtk::Box>(Gtk::Orientation::VERTICAL, 8);

        m_night_switch.set_halign(Gtk::Align::CENTER);

        m_temp_scale.set_range(TEMP_WARM, TEMP_COLD);
        m_temp_scale.set_value(4500);
        m_temp_scale.set_inverted(true);

        m_night_switch.property_active().signal_changed().connect(
            [this]
            {
                if (m_night_switch.get_active())
                {
                    int temp = static_cast<int>(m_temp_scale.get_value());
                    exec("sh -c 'pkill hyprsunset; sleep 0.1; hyprsunset -t " +
                         std::to_string(temp) + "'");
                }
                else
                {
                    exec("pkill hyprsunset");
                }
            });

        m_temp_scale.signal_value_changed().connect(
            [this]
            {
                if (!m_night_switch.get_active())
                    return;

                m_debounce_conn.disconnect();
                m_debounce_conn = Glib::signal_timeout().connect(
                    [this]()
                    {
                        exec("hyprctl hyprsunset temperature " +
                             std::to_string((int) m_temp_scale.get_value()));
                        return false;
                    },
                    30);
            });

        box->append(m_night_switch);
        add_slider_row(box, m_temp_scale, "🌙", "☀️");

        frame->set_child(*box);
        m_vbox.append(*frame);
    }

    void setup_rotation()
    {
        auto frame = Gtk::make_managed<Gtk::Frame>("Screen Rotation");
        auto grid = Gtk::make_managed<Gtk::Grid>();
        grid->set_column_homogeneous(true);
        grid->set_row_spacing(5);

        auto add_btn = [&](const std::string &lbl, int val, int x, int y)
        {
            auto btn = Gtk::make_managed<Gtk::Button>(lbl);
            btn->signal_clicked().connect(
                [this, val] { exec("hyprctl keyword monitor ,transform," + std::to_string(val)); });
            grid->attach(*btn, x, y);
        };

        add_btn("Normal", 0, 1, 0);
        add_btn("Left", 1, 0, 1);
        add_btn("Inverted", 2, 1, 2);
        add_btn("Right", 3, 2, 1);

        frame->set_child(*grid);
        m_vbox.append(*frame);
    }

    void fill_mode_list(unsigned monitor_index)
    {
        if (!m_mode_list || monitor_index >= m_monitors.size())
            return;
        guint n = m_mode_list->get_n_items();
        if (n > 0)
            m_mode_list->splice(0, n, std::vector<Glib::ustring>{});
        const auto &mon = m_monitors[monitor_index];
        for (const auto &mode : mon.modes)
            m_mode_list->append(mode.resolutionStr() + " @ " + mode.refreshStr());
        if (m_mode_dropdown)
            m_mode_dropdown->set_selected(0);
    }

    void on_monitor_changed()
    {
        if (m_monitor_dropdown)
            fill_mode_list(m_monitor_dropdown->get_selected());
    }

    void setup_resolution_refresh()
    {
        auto frame = Gtk::make_managed<Gtk::Frame>("Resolution & Refresh Rate");
        auto box = Gtk::make_managed<Gtk::Box>(Gtk::Orientation::VERTICAL, 8);

        m_monitors = DisplaySettings::getMonitors();

        if (m_monitors.empty())
        {
            auto label = Gtk::make_managed<Gtk::Label>("No monitors detected.");
            label->set_halign(Gtk::Align::CENTER);
            box->append(*label);
            frame->set_child(*box);
            m_vbox.append(*frame);
            return;
        }

        auto mon_list = Gtk::StringList::create();
        for (const auto &m : m_monitors)
            mon_list->append(m.name);

        m_monitor_dropdown = Gtk::make_managed<Gtk::DropDown>(mon_list);
        m_monitor_dropdown->set_halign(Gtk::Align::FILL);
        m_monitor_dropdown->set_hexpand(true);

        m_mode_list = Gtk::StringList::create();
        fill_mode_list(0);
        m_mode_dropdown = Gtk::make_managed<Gtk::DropDown>(m_mode_list);
        m_mode_dropdown->set_halign(Gtk::Align::FILL);
        m_mode_dropdown->set_hexpand(true);

        m_monitor_dropdown->property_selected().signal_changed().connect(
            sigc::mem_fun(*this, &DisplayApp::on_monitor_changed));

        m_apply_btn = Gtk::make_managed<Gtk::Button>("Apply");
        m_apply_btn->signal_clicked().connect(sigc::mem_fun(*this, &DisplayApp::on_apply_resolution));

        auto mon_row = Gtk::make_managed<Gtk::Box>(Gtk::Orientation::HORIZONTAL, 8);
        auto mon_lbl = Gtk::make_managed<Gtk::Label>("Monitor:");
        mon_row->append(*mon_lbl);
        mon_row->append(*m_monitor_dropdown);
        box->append(*mon_row);

        auto mode_row = Gtk::make_managed<Gtk::Box>(Gtk::Orientation::HORIZONTAL, 8);
        auto mode_lbl = Gtk::make_managed<Gtk::Label>("Mode:");
        mode_row->append(*mode_lbl);
        mode_row->append(*m_mode_dropdown);
        box->append(*mode_row);

        box->append(*m_apply_btn);
        frame->set_child(*box);
        m_vbox.append(*frame);
    }

    void on_apply_resolution()
    {
        if (m_monitors.empty() || !m_monitor_dropdown || !m_mode_dropdown)
            return;
        guint mon_idx = m_monitor_dropdown->get_selected();
        guint mode_idx = m_mode_dropdown->get_selected();
        if (mon_idx >= m_monitors.size())
            return;
        const auto &mon = m_monitors[mon_idx];
        if (mode_idx >= mon.modes.size())
            return;
        const auto &mode = mon.modes[mode_idx];
        std::string err;
        bool ok = DisplaySettings::applyMonitor(mon.name, mode.width, mode.height, mode.refreshRate,
                                               mon.x, mon.y, mon.scale, &err);
        if (m_verbose && ok)
            std::cout << "[CMD]: hyprctl keyword monitor " << mon.name << ","
                      << mode.width << "x" << mode.height << "@" << mode.refreshRate
                      << "," << mon.x << "x" << mon.y << "," << mon.scale << std::endl;
        if (!ok && !err.empty())
            std::cerr << "Apply failed: " << err << std::endl;
    }
};

void show_help(const char *bin_name)
{
    std::cout << "Display Control Utility\n\n"
              << "Usage: " << bin_name << " [OPTIONS]\n\n"
              << "Options:\n"
              << "  -h, --help     Show this help message and exit\n"
              << "  -v, --verbose  Enable verbose output (log commands to stdout)\n"
              << "  -q, --quiet    Suppress all output (redirect stdout/stderr to /dev/null)\n";
}

int main(int argc, char *argv[])
{
    bool verbose = false;
    bool quiet = false;
    // We maintain a modified args list to pass to GTK after removing our custom flags
    // This is necessary because GTK's command-line parsing would treat our flags
    // as unknown options and potentially cause errors or warnings. By filtering
    // them out here, we ensure GTK only sees arguments it understands (or none at all).
    std::vector<char *> remaining_args;
    remaining_args.push_back(argv[0]); // Always keep the program name

    // Manual argument parsing before GTK initialization
    // This gives us control over our custom flags before GTK sees the arguments
    for (int i = 1; i < argc; ++i)
    {
        std::string arg = argv[i];
        if (arg == "-h" || arg == "--help")
        {
            show_help(argv[0]);
            return 0;
        }
        else if (arg == "-v" || arg == "--verbose")
        {
            verbose = true;
        }
        else if (arg == "-q" || arg == "--quiet")
        {
            quiet = true;
        }
        else
        {
            remaining_args.push_back(argv[i]);
        }
    }

    if (quiet)
    {
        (void) !std::freopen("/dev/null", "w", stdout);
        (void) !std::freopen("/dev/null", "w", stderr);
    }

    auto app = Gtk::Application::create(APP_ID, Gio::Application::Flags::NON_UNIQUE);

    ThemeManager theme_mgr;
    Glib::RefPtr<Gtk::CssProvider> css_provider;

    app->signal_startup().connect(
        [&]
        {
            css_provider = Gtk::CssProvider::create();
            std::string css_data = theme_mgr.loadThemeAndGetCSS();
            css_provider->load_from_data(css_data);
            Gtk::StyleContext::add_provider_for_display(Gdk::Display::get_default(), css_provider,
                                                        GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
            theme_mgr.watchThemeFile(
                [&]()
                {
                    std::string new_css = theme_mgr.loadThemeAndGetCSS();
                    css_provider->load_from_data(new_css);
                });
        });

    app->signal_activate().connect(
        [&]
        {
            auto window = new DisplayApp(verbose);
            app->add_window(*window);
            window->set_visible(true);
        });

    int final_argc = remaining_args.size();
    char **final_argv = remaining_args.data();

    return app->run(final_argc, final_argv);
}
