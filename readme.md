# 🖥️ Omarchy Display Control Center

`Omarchy Display Control Center` is a lightweight C++ and GTK-based graphical utility for **Hyprland**, designed to provide simple and efficient control over your display settings. It offers a clean, modern interface to manage brightness, color temperature (Night Light), and screen rotation.

This tool was written and tested on **Omarchy**.

✨ **Features**

- **Intuitive UI**: A straightforward graphical interface built with GTK 4.
- **Brightness Control**: Adjust your screen's brightness with a simple slider.
- **Night Light**: Toggle a blue light filter and adjust its color temperature to reduce eye strain.
- **Screen Rotation**: Quickly rotate your display to normal, inverted, left, or right orientations.
- **Resolution & Refresh Rate**: Select a monitor (if multiple are connected), choose a resolution and refresh rate from available modes, and apply with one click. Uses `hyprctl monitors -j` and `hyprctl keyword monitor`.
- **Omarchy Theme Integration**: Automatically loads colors from `~/.config/omarchy/theme/colors` or `~/.config/omarchy/theme.conf` and applies them to the UI. Optional file watcher reloads the theme when the file changes. Falls back to a dark theme if no Omarchy theme is found.
- **Lightweight**: Written in C++ for minimal resource consumption.
- **Customizable**: The look and feel can be easily tweaked via Omarchy theme or embedded CSS.

💡 **Under the Hood**

This utility leverages powerful command-line tools to manage your display:

- **`hyprsunset`**: For the "Night Light" feature, the application uses `hyprsunset` to dynamically adjust the screen's color temperature. When you toggle Night Light, it ensures any existing `hyprsunset` instance is gracefully terminated before applying the new setting.
- **`brightnessctl`**: Screen brightness is controlled by executing `brightnessctl`.
- **`hyprctl`**: Screen rotation and resolution/refresh rate are handled by sending commands to the Hyprland compositor via `hyprctl` (e.g. `hyprctl monitors -j`, `hyprctl keyword monitor ...`).

Because of these tools, this application is designed specifically for the **Hyprland** environment. The build uses CMake FetchContent for **nlohmann/json** (header-only); no system JSON package is required.

---

🛠️ **Installation**

**1. Dependencies**

First, ensure you have the necessary build and runtime dependencies.

- **Build Dependencies**: A C++ compiler (like GCC or Clang), CMake, and `gtkmm-4.0`.
- **Runtime Dependencies**: `brightnessctl`, and `hyprland` (which provides `hyprctl` and `hyprsunset`).

**On Arch Linux (and derivatives like Omarchy):**

```bash
sudo pacman -S gcc cmake gtkmm-4.0 brightnessctl hyprland
```

**2. Clone & Build**

```bash
git clone https://github.com/kalk-ak/omarchy-display-control-center.git
cd omarchy-display-control-center
mkdir build
cd build
cmake ..
make
```

**3. Run or Install**

You can run the application directly from the `build` directory:

```bash
./display_control_center -q
```

or run with logs

```bash
./display_control_center -v
```

Or install it system-wide:

```bash
sudo make install
```

---

🚀 **Usage**

Launch the application to access the controls. The interface is self-explanatory.

The application also supports a few command-line arguments for more control.

| Flag              | Description                           |
| ----------------- | ------------------------------------- |
| `-h`, `--help`    | Show the help message and exit.       |
| `-v`, `--verbose` | Log external commands to the console. |
| `-q`, `--quiet`   | Suppress all console output.          |

Here is the output of the `--help` flag:

```
Display Control Utility

Usage: ./display_control_center [OPTIONS]

Options:
  -h, --help     Show this help message and exit
  -v, --verbose  Enable verbose output (log commands to stdout)
  -q, --quiet    Suppress all output (redirect stdout/stderr to /dev/null)
```

🎥 **Demonstration**

<div align="center">
  <img src="https://raw.githubusercontent.com/kalk-ak/Stash/master/omarchy-display-control-vid/demo.gif" alt="Demonstration GIF" width="80%">
  <p><em>A simple demonstration of the UI controls.</em></p>
</div>

📄 **License**

This project is licensed under the MIT License.
