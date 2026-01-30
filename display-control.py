#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
Display Control Center for Hyprland (Omarchy-compatible, hyprsunset v0.3+)
- Brightness (brightnessctl)
- Rotation (hyprctl)
- Night light: Reliably manages the hyprsunset process with a reset method.
  - Applies a smooth, continuous fade effect when the slider is released.
  - Toggle switch turns blue when active.
- Generates apply-settings.sh for boot-autostart.
- Robustness: Includes single-instance lock, dependency checks, and foolproof
  process cleanup.
"""

import gi
import subprocess
import os
import json
import signal
import atexit
import time
from typing import Optional, List

# Require a specific version of GTK
gi.require_version("Gtk", "3.0")
from gi.repository import Gtk, GLib, Gio

# ==============================================================================
# --- USER CONFIGURATION ---
# ==============================================================================
APP_ID = "com.omarchy.display-control-center"
CONFIG_DIR = os.path.expanduser(f"~/.config/{APP_ID}")
STATE_FILE = os.path.join(CONFIG_DIR, "state.json")
LOCK_FILE = os.path.join(CONFIG_DIR, "app.lock")
STARTUP_SCRIPT_PATH = os.path.expanduser("~/.config/hypr/apply-settings.sh")
MIN_TEMP = 2500
MAX_TEMP = 6500
FADE_DURATION_S = "0.5"  # Duration for the smooth fade effect in seconds

# ==============================================================================
# --- ROBUSTNESS & UTILITIES ---
# ==============================================================================


def cleanup_on_exit():
    """Ensures child processes and the lock file are removed on exit."""
    run_cmd(["pkill", "-x", "hyprsunset"])
    if os.path.exists(LOCK_FILE):
        os.remove(LOCK_FILE)


def create_lock_file():
    """Creates a lock file to ensure only one instance of the app runs."""
    os.makedirs(CONFIG_DIR, exist_ok=True)
    if os.path.exists(LOCK_FILE):
        print("Another instance of the application is already running.")
        exit(1)
    with open(LOCK_FILE, "w") as f:
        f.write(str(os.getpid()))
    atexit.register(cleanup_on_exit)


def which(tool: str) -> bool:
    """Checks if a command-line tool is available in the system's PATH."""
    from shutil import which as _which

    return _which(tool) is not None


def run_cmd(cmd: List[str], get_output: bool = False):
    """Runs a shell command safely."""
    try:
        if get_output:
            return subprocess.check_output(
                cmd, text=True, stderr=subprocess.DEVNULL
            ).strip()
        else:
            return subprocess.Popen(
                cmd, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL
            )
    except (subprocess.CalledProcessError, FileNotFoundError):
        return None


def show_error_dialog(parent, primary_text, secondary_text):
    """Displays a GTK error message dialog."""
    dialog = Gtk.MessageDialog(
        transient_for=parent,
        flags=0,
        message_type=Gtk.MessageType.ERROR,
        buttons=Gtk.ButtonsType.OK,
        text=primary_text,
    )
    dialog.format_secondary_text(secondary_text)
    dialog.run()
    dialog.destroy()


# ==============================================================================
# --- APPLICATION LOGIC ---
# ==============================================================================

# ---------------- CSS Styling ----------------
CSS_CODE = """
window{background-color:#2e3440}
headerbar{background-color:#3b4252;border-bottom:1px solid #4c566a}
headerbar label{color:#eceff4;font-weight:700}
.section-frame{margin:10px;border:1px solid #4c566a;border-radius:8px;padding:10px}
.section-label{color:#d8dee9;font-weight:700;font-size:14px;background-color:#2e3440;padding:0 5px;margin-top:-20px}
.brightness-box,.night-light-slider-box{padding:0 10px}
.brightness-box label{font-size:20px}
scale trough{background-image:none;background-color:#4c566a;border-radius:5px;min-height:8px}
scale slider{background-image:none;background-color:#88c0d0;border-radius:4px;min-width:12px}
switch{border-radius:12px;background-color:#434c5e;transition:background-color .2s ease;border:1px solid #4c566a}
switch:active{background-color:#8fbcbb}
switch:checked{background-color:#16539B}
switch slider{background-image:none;background-color:#eceff4;border-radius:10px}
.night-light-toggle-label{color:#d8dee9}
.night-light-toggle-box{padding:0 10px 10px}
.temp-label{color:#d8dee9;font-size:12px;font-weight:700;padding-right:10px}
scale.night-light-scale slider{background-image:none;background-color:#ebcb8b;border-radius:4px;min-width:12px}
.rotation-grid{padding:10px}
.rotation-grid button{background-image:none;background-color:#4c566a;border:1px solid #4c566a;border-radius:6px;color:#eceff4;padding:8px;font-weight:700;min-width:80px}
.rotation-grid button:hover{background-color:#5e6878}
.rotation-grid button:active{background-color:#88c0d0;color:#2e3440}
"""


class DisplayControlApp(Gtk.ApplicationWindow):
    """The main class for the application."""

    def __init__(self, app: Gtk.Application) -> None:
        super().__init__(application=app, title="Display Control")

        self.set_position(Gtk.WindowPosition.CENTER)
        self.set_default_size(420, -1)
        self.set_resizable(False)

        style_provider = Gtk.CssProvider()
        style_provider.load_from_data(CSS_CODE.encode())
        Gtk.StyleContext.add_provider_for_screen(
            self.get_screen(), style_provider, Gtk.STYLE_PROVIDER_PRIORITY_APPLICATION
        )

        self.state: dict = self.load_state()
        self.is_programmatic_change: bool = False
        self.persistent_process = None
        self.fade_watch_id = None
        self.current_temp = self.state["manual_temp"]

        header = Gtk.HeaderBar(title="Display Control", show_close_button=True)
        self.set_titlebar(header)

        main_vbox = Gtk.Box(orientation=Gtk.Orientation.VERTICAL, spacing=10, margin=10)
        self.add(main_vbox)

        self._create_brightness_ui(main_vbox)
        self._create_night_light_ui(main_vbox)
        self._create_rotation_ui(main_vbox)

        self.sync_ui_with_state()
        self._start_persistent_night_light()

    def _create_brightness_ui(self, parent_box: Gtk.Box) -> None:
        frame = Gtk.Frame()
        frame.get_style_context().add_class("section-frame")
        label = Gtk.Label(label="☀️ Brightness", use_markup=True)
        label.get_style_context().add_class("section-label")
        frame.set_label_widget(label)
        parent_box.pack_start(frame, True, True, 0)
        hbox = Gtk.Box(orientation=Gtk.Orientation.HORIZONTAL, spacing=10)
        hbox.get_style_context().add_class("brightness-box")
        frame.add(hbox)
        hbox.pack_start(Gtk.Label(label="🌑"), False, False, 0)
        self.brightness_slider = Gtk.Scale.new_with_range(
            Gtk.Orientation.HORIZONTAL, 1, 100, 1
        )
        self.brightness_slider.set_draw_value(True)
        self.brightness_slider.connect("value-changed", self.on_brightness_changed)
        self.brightness_slider.connect("format-value", lambda s, v: f"{v:.0f}%")
        hbox.pack_start(self.brightness_slider, True, True, 0)
        hbox.pack_start(Gtk.Label(label="🌕"), False, False, 0)

    def _create_night_light_ui(self, parent_box: Gtk.Box) -> None:
        frame = Gtk.Frame()
        frame.get_style_context().add_class("section-frame")
        label = Gtk.Label(label="🌙 Night Light", use_markup=True)
        label.get_style_context().add_class("section-label")
        frame.set_label_widget(label)
        parent_box.pack_start(frame, True, True, 0)
        vbox = Gtk.Box(orientation=Gtk.Orientation.VERTICAL, spacing=8)
        vbox.get_style_context().add_class("night-light-toggle-box")
        frame.add(vbox)
        header_box = Gtk.Box(orientation=Gtk.Orientation.HORIZONTAL)
        header_box.pack_start(Gtk.Label(label="Enable Night Light"), False, False, 0)
        self.night_light_toggle = Gtk.Switch(halign=Gtk.Align.END, hexpand=True)
        self.night_light_toggle.connect("notify::active", self.on_night_light_toggle)
        header_box.pack_start(self.night_light_toggle, False, False, 0)
        vbox.pack_start(header_box, False, False, 0)
        slider_box = Gtk.Box(orientation=Gtk.Orientation.HORIZONTAL, spacing=10)
        slider_box.get_style_context().add_class("night-light-slider-box")
        slider_box.pack_start(Gtk.Label(label="🧊"), False, False, 0)
        self.night_slider = Gtk.Scale.new_with_range(
            Gtk.Orientation.HORIZONTAL, 0, 100, 1
        )
        self.night_slider.set_draw_value(False)
        self.night_slider.connect("value-changed", self.on_slider_value_changed)
        self.night_slider.connect("button-release-event", self.on_slider_released)
        slider_box.pack_start(self.night_slider, True, True, 0)
        slider_box.pack_start(Gtk.Label(label="🔥"), False, False, 0)
        vbox.pack_start(slider_box, False, False, 5)

    def _create_rotation_ui(self, parent_box: Gtk.Box) -> None:
        frame = Gtk.Frame()
        frame.get_style_context().add_class("section-frame")
        label = Gtk.Label(label="🔄 Screen Rotation", use_markup=True)
        label.get_style_context().add_class("section-label")
        frame.set_label_widget(label)
        parent_box.pack_start(frame, True, True, 0)
        grid = Gtk.Grid(
            column_spacing=10, row_spacing=10, margin=12, halign=Gtk.Align.CENTER
        )
        frame.add(grid)
        buttons = {"Normal": 0, "Left": 1, "Inverted": 2, "Right": 3}
        positions = [(1, 0, 1, 1), (0, 1, 1, 1), (1, 2, 1, 1), (2, 1, 1, 1)]
        for i, (lbl, val) in enumerate(buttons.items()):
            btn = Gtk.Button.new_with_label(lbl)
            btn.connect("clicked", self.on_rotate, val)
            grid.attach(btn, *positions[i])

    def _pct_to_temp(self, pct: float) -> int:
        return int(MAX_TEMP - (pct * (MAX_TEMP - MIN_TEMP) / 100))

    def _temp_to_pct(self, temp: int) -> float:
        return 100.0 - ((float(temp) - MIN_TEMP) * 100.0) / (MAX_TEMP - MIN_TEMP)

    def load_state(self) -> dict:
        os.makedirs(CONFIG_DIR, exist_ok=True)
        defaults = {
            "night_light_on": False,
            "manual_temp": 4500,
            "brightness_percent": 90,
            "monitor_transform": 0,
        }
        if not os.path.exists(STATE_FILE):
            return defaults
        try:
            with open(STATE_FILE, "r") as f:
                return {**defaults, **json.load(f)}
        except (FileNotFoundError, json.JSONDecodeError):
            return defaults

    def save_state(self) -> None:
        with open(STATE_FILE, "w") as f:
            json.dump(self.state, f, indent=4)
        self.generate_startup_script()

    def generate_startup_script(self) -> None:
        state = self.state
        script = f"""#!/bin/bash
# Apply display settings on startup
brightnessctl s {state["brightness_percent"]}% -q
pkill -x hyprsunset &> /dev/null || true
sleep 0.2
if [ "{str(state["night_light_on"]).lower()}" = "true" ]; then
    hyprsunset -t {state["manual_temp"]} &
fi
hyprctl keyword monitor ,transform,{state["monitor_transform"]}
"""
        with open(STARTUP_SCRIPT_PATH, "w") as f:
            f.write(script)
        os.chmod(STARTUP_SCRIPT_PATH, 0o755)

    def sync_ui_with_state(self) -> None:
        self.is_programmatic_change = True
        if percent_str := run_cmd(["brightnessctl", "g", "-p"], get_output=True):
            self.brightness_slider.set_value(int(percent_str))
        else:
            self.brightness_slider.set_value(self.state["brightness_percent"])
        is_on = self.state["night_light_on"]
        self.night_light_toggle.set_active(is_on)
        self.night_slider.set_sensitive(is_on)
        self.night_slider.set_value(self._temp_to_pct(self.state["manual_temp"]))
        self.is_programmatic_change = False

    def _start_persistent_night_light(self):
        """Stops any old process and starts a new persistent one if enabled."""
        self._stop_persistent_night_light()
        if self.state["night_light_on"]:
            self.persistent_process = run_cmd(
                ["hyprsunset", "-t", str(self.state["manual_temp"])]
            )

    def _stop_persistent_night_light(self):
        """Stops the managed persistent process."""
        if self.persistent_process:
            self.persistent_process.kill()
            self.persistent_process = None

    def on_brightness_changed(self, slider: Gtk.Scale) -> None:
        if self.is_programmatic_change:
            return
        value = int(slider.get_value())
        run_cmd(["brightnessctl", "s", f"{value}%", "-q"])
        self.state["brightness_percent"] = value
        self.save_state()

    def on_night_light_toggle(self, switch: Gtk.Switch, gparam) -> None:
        if self.is_programmatic_change:
            return

        self.state["night_light_on"] = switch.get_active()
        self.night_slider.set_sensitive(self.state["night_light_on"])

        if self.state["night_light_on"]:
            self.current_temp = self.state["manual_temp"]
            self._start_persistent_night_light()
        else:
            self._stop_persistent_night_light()

        self.save_state()

    def on_slider_value_changed(self, slider: Gtk.Scale) -> None:
        """Only updates the internal state, waiting for the user to finish."""
        if self.is_programmatic_change:
            return
        self.state["manual_temp"] = self._pct_to_temp(slider.get_value())

    def on_slider_released(self, widget: Gtk.Scale, event) -> None:
        """Applies a smooth fade, with a workaround for a hyprsunset flicker."""
        if self.is_programmatic_change:
            return

        if self.state["night_light_on"]:
            target_temp = self.state["manual_temp"]

            # WORKAROUND: hyprsunset flickers when fading to a warmer temperature.
            # If the target is warmer (lower Kelvin), set it instantly to avoid the flicker.
            # Otherwise, use the smooth fade which works correctly for cooler temps.
            if target_temp < self.current_temp:
                self._start_persistent_night_light()
                self.current_temp = target_temp
            else:
                fade_cmd = [
                    "hyprsunset",
                    "-f",
                    FADE_DURATION_S,
                    "-t",
                    str(self.state["manual_temp"]),
                ]
                fade_process = run_cmd(fade_cmd)
                if fade_process:
                    GLib.child_watch_add(fade_process.pid, self.on_fade_finished)

        self.save_state()

    def on_fade_finished(self, pid, status):
        """Callback that runs after the fade to update the persistent process."""
        self.current_temp = self.state["manual_temp"]
        self._start_persistent_night_light()

    def on_rotate(self, button: Gtk.Button, transform_value: int) -> None:
        run_cmd(["hyprctl", "keyword", "monitor", f",transform,{transform_value}"])
        self.state["monitor_transform"] = transform_value
        self.save_state()


class Application(Gtk.Application):
    def __init__(self, *args, **kwargs):
        super().__init__(
            *args,
            application_id=APP_ID,
            flags=Gio.ApplicationFlags.FLAGS_NONE,
            **kwargs,
        )
        self.window = None

    def do_activate(self):
        if not self.window:
            self.window = DisplayControlApp(self)
        self.window.show_all()
        self.window.present()

    def do_startup(self):
        Gtk.Application.do_startup(self)
        deps = ["brightnessctl", "hyprctl", "hyprsunset"]
        missing_deps = [dep for dep in deps if not which(dep)]
        if missing_deps:
            show_error_dialog(
                None,
                "Missing Dependencies",
                f"The following required tools are not installed or not in your PATH:\n\n{', '.join(missing_deps)}\n\nPlease install them to continue.",
            )
            self.quit()


if __name__ == "__main__":
    create_lock_file()
    signal.signal(signal.SIGINT, signal.SIG_DFL)
    app = Application()
    app.run(None)
