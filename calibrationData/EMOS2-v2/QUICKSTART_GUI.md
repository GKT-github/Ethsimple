# GUI Version - Quick Start Guide

## What Changed

Instead of pressing 'c' on the keyboard, you now click a button in a GUI window!

## Installation (One-time)

```bash
# Install GTK3
sudo apt install -y libgtk-3-dev

# Copy files to your project directory
cd ~/Documents/Surround-View-master/calibrationData/EMOS2
# (copy camera_capture_gui.cpp and compile_gui.sh here)
```

## Compile

```bash
cd ~/Documents/Surround-View-master/calibrationData/EMOS2
chmod +x compile_gui.sh
./compile_gui.sh
```

Or compile directly:
```bash
g++ -std=c++11 -o camera_capture_gui camera_capture_gui.cpp \
    $(pkg-config --cflags gstreamer-1.0 gstreamer-app-1.0 glib-2.0 gtk+-3.0) \
    $(pkg-config --libs gstreamer-1.0 gstreamer-app-1.0 glib-2.0 gtk+-3.0) \
    -lyaml-cpp -lpthread
```

## Run

```bash
./camera_capture_gui
```

## What You'll See

1. **Terminal** - Shows startup messages
2. **Video Window** - Live camera feed (same as before)
3. **Control Window** - NEW! Small window with:
   - Camera info
   - Capture button
   - Status messages
   - Image counter

## How to Use

1. Click the **"📸 Capture Image"** button
2. Button turns gray while capturing
3. Status shows "✓ Image saved: [filename]"
4. Button becomes clickable again
5. Counter updates
6. Repeat!

## To Quit

Close the control window (click X) or close the video window.

## Comparison

### Before (Keyboard):
- Press 'c' to capture
- Press 'q' to quit
- Terminal must have focus
- Text feedback only

### After (GUI):
- Click button to capture
- Close window to quit
- No focus needed
- Visual feedback in window

## Screenshot of Control Window

```
┌─────────────────────────────────────┐
│  Camera Capture Control        [X]  │
├─────────────────────────────────────┤
│                                     │
│  Camera: 192.168.45.3:5020         │
│  Saving to: test_images            │
│  ─────────────────────────────────  │
│                                     │
│  ┌───────────────────────────────┐ │
│  │   📸 Capture Image           │ │
│  └───────────────────────────────┘ │
│                                     │
│  Images captured: 5                │
│  ─────────────────────────────────  │
│                                     │
│  ✓ Image saved:                    │
│  test_images/capture_...0005.jpg   │
│                                     │
└─────────────────────────────────────┘
```

## Tips

- Keep the control window visible
- Don't click the button too fast (wait for previous capture to finish)
- Check the status label for feedback
- Video window and control window are separate - both will open

## Troubleshooting

**Control window doesn't appear:**
- Check if you have a display: `echo $DISPLAY`
- Try Alt+Tab to find the window
- Make sure GTK3 is installed

**"cannot open display" error:**
- You need X11 or Wayland
- If SSH, use `ssh -X` for X forwarding
- Or use the keyboard version instead

**Button doesn't work:**
- Wait for video to start first
- Check console for errors
- Make sure camera is streaming

## Files You Need

- `camera_capture_gui.cpp` - Source code
- `camera_config.yaml` - Config file (same as before)
- `compile_gui.sh` - Compilation script

## Both Versions Available

Keep both if you want:
- `camera_capture` - Keyboard version (no GTK needed)
- `camera_capture_gui` - GUI version (easier to use)

Use whichever you prefer!
