#include <cstdio>
#include <gtkmm.h>
#include <iostream>
#include <string>
#include <vector>

const std::string APP_ID = "com.omarchy.display-control";
constexpr int TEMP_WARM = 2500; // Night/Warm
constexpr int TEMP_COLD = 6500; // Day/Cold

const char *CSS = R"(
    window { background-color: #2e3440; color: #eceff4; }
    frame { margin: 10px; border: 1px solid #4c566a; border-radius: 8px; padding: 12px; }
    scale highlight { background-color: #88c0d0; }
    button { margin: 4px; padding: 8px; background-color: #434c5e; border: none; border-radius: 4px; }
    button:hover { background-color: #4c566a; }
    label { font-size: 16px; margin: 0 10px; }
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
    std::vector<char *> remaining_args;
    remaining_args.push_back(argv[0]);

    // Manually parse arguments before GTK starts
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

    // use NON_UNIQUE and empty APP_ID to ensure it doesn't try to "proxy" arguments
    // to another instance
    auto app = Gtk::Application::create(APP_ID, Gio::Application::Flags::NON_UNIQUE);

    app->signal_startup().connect(
        [&]
        {
            auto css = Gtk::CssProvider::create();
            css->load_from_data(CSS);
            Gtk::StyleContext::add_provider_for_display(Gdk::Display::get_default(), css,
                                                        GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
        });

    app->signal_activate().connect(
        [&]
        {
            auto window = new DisplayApp(verbose);
            app->add_window(*window);
            window->set_visible(true);
        });

    // Pass the modified args list to GTK (only containing the binary name)
    int final_argc = remaining_args.size();
    char **final_argv = remaining_args.data();

    return app->run(final_argc, final_argv);
}
