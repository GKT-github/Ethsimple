# Camera Capture with GUI Button

This version includes a graphical control window with a button instead of keyboard controls.

## What's Different from Keyboard Version

- ✅ **GUI Control Window** - Separate window with "Capture Image" button
- ✅ **Real-time Status** - Shows capture status and count in the window
- ✅ **Visual Feedback** - Button disables during capture and shows progress
- ✅ No keyboard monitoring needed
- ✅ Easy to use with mouse clicks

## Prerequisites

Install GTK+ 3.0 in addition to previous dependencies:

```bash
sudo apt install -y \
    libgtk-3-dev \
    libgstreamer1.0-dev \
    libgstreamer-plugins-base1.0-dev \
    libglib2.0-dev \
    libyaml-cpp-dev \
    build-essential \
    pkg-config
```

## Compilation

### Quick Method:

```bash
chmod +x compile_gui.sh
./compile_gui.sh
```

### Manual Method:

```bash
g++ -std=c++11 -o camera_capture_gui camera_capture_gui.cpp \
    $(pkg-config --cflags gstreamer-1.0 gstreamer-app-1.0 glib-2.0 gtk+-3.0) \
    $(pkg-config --libs gstreamer-1.0 gstreamer-app-1.0 glib-2.0 gtk+-3.0) \
    -lyaml-cpp -lpthread
```

## Usage

1. **Ensure you have camera_config.yaml** with your camera settings:
```yaml
# Camera Configuration
camera:
  address: "192.168.45.3"
  port: 5020
```

2. **Run the application:**
```bash
./camera_capture_gui
```

3. **Enter folder name** when prompted

4. **Two windows will open:**
   - **Video Preview Window** - Shows live camera feed
   - **Control Window** - Has the "Capture Image" button

5. **Click "📸 Capture Image"** button to take pictures

6. **Close the control window** to quit

## GUI Features

### Control Window Contains:
- Camera IP and Port information
- Save folder location
- **"Capture Image" button** (main control)
- Image counter (shows how many images captured)
- Status messages (shows capture progress and file paths)

### Button Behavior:
- **Active (clickable)** - Ready to capture
- **Disabled (grayed out)** - Capture in progress
- Automatically re-enables after capture completes

### Status Messages:
- "Ready to capture" - Initial state
- "📸 Capturing..." - Capture in progress
- "✓ Image saved: [filename]" - Success with file path
- "Already capturing, please wait..." - If clicked too quickly
- "✗ Error: ..." - If something went wrong

## Example Session

```
==================================================
GStreamer Image Capture Tool (C++ with GUI)
==================================================

Loaded configuration:
  Camera Address: 192.168.45.3
  Camera Port: 5020

Enter folder name to save images: test_images
Created folder: test_images

==================================================
Starting camera stream...
Camera: 192.168.45.3:5020
Saving images to: test_images
==================================================

Control window opened. Click 'Capture Image' button to take pictures.
Close the control window to quit.

✓ Image 1 saved: test_images/capture_20240130_143022_001_0001.jpg
✓ Image 2 saved: test_images/capture_20240130_143045_234_0002.jpg

Stopping...

==================================================
Session Summary:
Total images captured: 2
Images saved in: test_images
==================================================
```

## Keyboard Version vs GUI Version

| Feature | Keyboard Version | GUI Version |
|---------|-----------------|-------------|
| Control Method | Press 'c' key | Click button |
| Visual Feedback | Terminal text | GUI labels |
| Status Display | Console output | Control window |
| Quit Method | Press 'q' | Close window |
| Ease of Use | Terminal focus needed | Mouse clicks |
| Dependencies | Basic libraries | + GTK3 |

## Troubleshooting

### GTK Error: "cannot open display"
**Solution:** You need a display server (X11 or Wayland)
```bash
echo $DISPLAY  # Should show something like :0 or :1
```

If empty, you're in a headless environment and need to use the keyboard version instead.

### Error: "Package gtk+-3.0 was not found"
**Solution:**
```bash
sudo apt install libgtk-3-dev
```

### Control window doesn't appear
**Solution:** Check if video window appeared. If yes, the control window might be behind it. Alt+Tab to switch windows.

### Button doesn't respond
**Solution:** Make sure the camera is streaming and the video window is visible. Check console for error messages.

## Customization

### Change Button Text:
Edit line in `camera_capture_gui.cpp`:
```cpp
capture_button = gtk_button_new_with_label("Your Text Here");
```

### Change Window Size:
Edit line:
```cpp
gtk_window_set_default_size(GTK_WINDOW(window), 400, 200);  // width, height
```

### Add More Controls:
The GUI uses GTK Box containers. You can add more widgets:
```cpp
GtkWidget *your_button = gtk_button_new_with_label("Your Button");
gtk_box_pack_start(GTK_BOX(vbox), your_button, FALSE, FALSE, 0);
```

## Both Versions Available

You can keep both versions:
- **camera_capture** - Keyboard control (lightweight)
- **camera_capture_gui** - GUI button control (user-friendly)

Use whichever fits your needs better!

## License

Free to use and modify.
