# 🖥️ Omarchy-Display-Control-Center

`display_control_center` is a lightweight C++ and GTK-based graphical utility for Linux, designed to provide simple and efficient control over your display settings. It offers a clean, modern interface to manage brightness, color temperature (Night Light), and screen rotation.

✨ **Features**

- **Intuitive UI**: A straightforward graphical interface built with GTK 4.
- **Brightness Control**: Adjust your screen's brightness with a simple slider.
- **Night Light**: Toggle a blue light filter and adjust its color temperature to reduce eye strain.
- **Screen Rotation**: Quickly rotate your display to normal, inverted, left, or right orientations.
- **Lightweight**: Written in C++ for minimal resource consumption.
- **Customizable**: The look and feel can be easily tweaked via embedded CSS.

🛠️ **Installation & Build**

First, ensure you have a C++ compiler (like GCC or Clang), CMake, and the `gtkmm-4.0` development libraries installed on your system.

**On Arch Linux:**

```bash
sudo pacman -S gcc cmake gtkmm4
```

**1. Clone the Repository**

```bash
git clone https://github.com/yourusername/omarchy-display-control-center.git
cd omarchy-display-control-center
```

**2. Build the Project**

We use a standard out-of-source build with CMake.

```bash
mkdir build
cd build
cmake ..
make
```

**3. Run the Executable**

Once built, you can run the application directly.

```bash
./display_control_center
```

You can also install it system-wide.

```bash
sudo make install
```

🚀 **Usage**

Launch the application to access the controls.

- **Brightness**: Drag the slider to set your desired screen brightness.
- **Night Light**: Click the toggle switch to activate the blue light filter. Drag the slider to make the color temperature warmer or cooler.
- **Screen Rotation**: Click the buttons to instantly rotate your screen.

The application also supports a few command-line arguments:

| Flag              | Description                           |
| ----------------- | ------------------------------------- |
| `-h`, `--help`    | Show the help message and exit.       |
| `-v`, `--verbose` | Log external commands to the console. |
| `-q`, `--quiet`   | Suppress all console output.          |

🎥 **Demonstration**

<div align="center">
  <img src="https://github.com/kalk-ak/Stash/tree/master/omarchy-display-control-vid/demo.git" alt="Demonstration GIF" width="80%">
  <p><em>A simple demonstration of the UI controls. (You can replace this with your own GIF)</em></p>
</div>

📄 **License**

This project is licensed under the MIT License.
