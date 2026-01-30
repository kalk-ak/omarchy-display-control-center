#include <glibmm.h>
#include <gtkmm.h>
#include <iostream>
#include <string>
#include <vector>

const std::string APP_ID = "com.omarchy.display-control-center";
constexpr int TEMP_MIN = 2500;
constexpr int TEMP_MAX = 6500;

const char *CSS = R"(
    window { background-color: #2e3440; color: #eceff4; }
    frame { margin: 10px; border: 1px solid #4c566a; border-radius: 8px; padding: 12px; }
    scale highlight { background-color: #88c0d0; }
    button { margin: 4px; padding: 8px; background-color: #434c5e; border: none; border-radius: 4px; }
    button:hover { background-color: #4c566a; }
    switch { margin-bottom: 5px; }
)";

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
    }

  private:
    Gtk::Box m_vbox;
    Gtk::Scale m_bright_scale, m_temp_scale;
    Gtk::Switch m_night_switch;
    sigc::connection m_debounce_conn;
    bool m_verbose;

    // Helper to execute shell commands asynchronously
    void exec(const std::string &cmd)
    {
        if (m_verbose)
        {
            std::cout << "[CMD]: " << cmd << std::endl;
        }

        try
        {
            Glib::spawn_command_line_async(cmd);
        }
        catch (const Glib::Error &e)
        {
            std::cerr << "Command failed: " << e.what() << std::endl;
        }
    }

    void setup_brightness()
    {
        auto frame = Gtk::make_managed<Gtk::Frame>("☀️ Brightness");
        m_bright_scale.set_range(1, 100);
        m_bright_scale.set_value(80);

        // Use debounce for brightness too to avoid flooding brightnessctl
        m_bright_scale.signal_value_changed().connect(
            [this]
            { exec("brightnessctl s " + std::to_string((int) m_bright_scale.get_value()) + "%"); });

        frame->set_child(m_bright_scale);
        m_vbox.append(*frame);
    }

    void setup_night_light()
    {
        auto frame = Gtk::make_managed<Gtk::Frame>("🌙 Night Light");
        auto box = Gtk::make_managed<Gtk::Box>(Gtk::Orientation::VERTICAL, 8);

        m_night_switch.set_halign(Gtk::Align::CENTER);

        // 6500K is Neutral (Daylight), 2500K is very Warm (Candlelight)
        m_temp_scale.set_range(TEMP_MIN, TEMP_MAX);
        m_temp_scale.set_value(4500);
        m_temp_scale.set_inverted(true);
        m_temp_scale.set_sensitive(false);

        // Toggle Switch Logic (Starts/Stops the Daemon)
        m_night_switch.property_active().signal_changed().connect(
            [this]
            {
                bool active = m_night_switch.get_active();
                m_temp_scale.set_sensitive(active);

                if (active)
                {
                    // Start the daemon with the current slider value
                    int temp = static_cast<int>(m_temp_scale.get_value());
                    // We pkill first to ensure we don't spawn multiple instances if state was
                    // desynced
                    exec("sh -c 'pkill hyprsunset; sleep 0.1; hyprsunset -t " +
                         std::to_string(temp) + "'");
                }
                else
                {
                    // Kill the daemon completely
                    exec("pkill hyprsunset");
                }
            });

        // Slider Logic (Uses IPC for smooth transition)
        m_temp_scale.signal_value_changed().connect(
            [this]
            {
                // Only send updates if the switch is actually on
                if (!m_night_switch.get_active())
                    return;

                m_debounce_conn.disconnect();
                m_debounce_conn = Glib::signal_timeout().connect(
                    [this]()
                    {
                        update_night_light_ipc();
                        return false; // Run once per debounce
                    },
                    30); // 30ms is fast enough for 30fps+ feel, but prevents flood
            });

        box->append(m_night_switch);
        box->append(m_temp_scale);
        frame->set_child(*box);
        m_vbox.append(*frame);
    }

    void update_night_light_ipc()
    {
        int temp = static_cast<int>(m_temp_scale.get_value());
        // Use hyprctl to talk to the running daemon instead of killing it
        exec("hyprctl hyprsunset temperature " + std::to_string(temp));
    }

    void setup_rotation()
    {
        auto frame = Gtk::make_managed<Gtk::Frame>("🔄 Screen Rotation");
        auto grid = Gtk::make_managed<Gtk::Grid>();
        grid->set_column_homogeneous(true);
        grid->set_row_spacing(5);

        auto add_rot = [&](const std::string &label, int val, int x, int y)
        {
            auto btn = Gtk::make_managed<Gtk::Button>(label);
            btn->signal_clicked().connect(
                [this, val] { exec("hyprctl keyword monitor ,transform," + std::to_string(val)); });
            grid->attach(*btn, x, y);
        };

        add_rot("Normal", 0, 1, 0);
        add_rot("Left", 1, 0, 1);
        add_rot("Inverted", 2, 1, 2);
        add_rot("Right", 3, 2, 1);

        frame->set_child(*grid);
        m_vbox.append(*frame);
    }
};

int main(int argc, char *argv[])
{
    // Simple Argument Parsing
    bool verbose = false;
    for (int i = 1; i < argc; ++i)
    {
        std::string arg = argv[i];
        if (arg == "-v" || arg == "--verbose")
        {
            verbose = true;
        }
    }

    auto app = Gtk::Application::create(APP_ID);
    app->signal_startup().connect(
        []
        {
            auto provider = Gtk::CssProvider::create();
            provider->load_from_data(CSS);
            Gtk::StyleContext::add_provider_for_display(Gdk::Display::get_default(), provider,
                                                        GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
        });

    // Pass the verbose flag to the window
    return app->make_window_and_run<DisplayApp>(argc, argv, verbose);
}
