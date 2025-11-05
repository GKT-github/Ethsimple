# Troubleshooting Guide

## Issue: Images not being saved (Fixed in v2)

**Problem:** The original version used `multifilesink` with a `valve` element which had timing issues causing captures to fail.

**Solution:** The updated version uses `appsink` which pulls samples directly from the pipeline and saves them reliably. When you press 'c', the application:
1. Sets a flag to capture the next frame
2. Waits for the next sample from appsink
3. Directly writes the JPEG data to a file
4. Confirms the save with a message

## Issue: "Already capturing, please wait..."

**Problem:** This happens when you press 'c' multiple times quickly before the first capture completes.

**Solution:** The application processes one capture at a time. Wait for the "✓ Image saved" message before pressing 'c' again.

## Configuration Issues

### Config file not found
**Error:** `Config file not found. Creating default configuration...`

**Solution:** On first run, the application creates `camera_config.yaml`. Edit it with your camera settings:
```bash
nano camera_config.yaml
```

### Cannot load config
**Error:** `Failed to load configuration from: camera_config.yaml`

**Solution:** Check YAML syntax. The file should look exactly like:
```yaml
# Camera Configuration
camera:
  address: "192.168.45.3"
  port: 5020
```
- Ensure proper indentation (2 spaces)
- Address must be in quotes
- Port is a number without quotes

## Compilation Issues

### yaml-cpp not found
**Error:** `Package yaml-cpp was not found`

**Solution:**
```bash
sudo apt install libyaml-cpp-dev
```

### gstreamer-app not found
**Error:** `Package gstreamer-app-1.0 was not found`

**Solution:**
```bash
sudo apt install libgstreamer-plugins-base1.0-dev
```

## Runtime Issues

### No video display
**Error:** Video pipeline works but no window appears

**Solutions:**
1. Check if you have a display:
   ```bash
   echo $DISPLAY
   ```
2. Try a different video sink by editing the code:
   - Replace `autovideosink` with `xvimagesink`
   - Or use `fakesink` for no preview

### Pipeline errors
**Error:** `Pipeline parsing error` or `Unable to set pipeline to playing state`

**Solutions:**
1. Test your camera with gst-launch first:
   ```bash
   gst-launch-1.0 udpsrc address=192.168.45.3 port=5020 ! \
     application/x-rtp,encoding-name=H264,payload=96 ! \
     rtpjitterbuffer ! rtph264depay ! h264parse ! \
     nvv4l2decoder ! nvvidconv ! autovideosink
   ```
2. Verify camera is streaming
3. Check network connectivity:
   ```bash
   ping 192.168.45.3
   ```
4. Verify correct IP and port in config file

### NVIDIA decoder issues
**Error:** `nvv4l2decoder` not found

**Solution (Jetson Nano/Xavier):**
```bash
sudo apt install gstreamer1.0-nvv4l2 nvidia-l4t-gstreamer
```

**Alternative (non-NVIDIA systems):**
Replace `nvv4l2decoder` with `avdec_h264` in the code

### Permission errors
**Error:** Cannot create folder or save files

**Solutions:**
1. Check write permissions:
   ```bash
   ls -ld .
   ```
2. Ensure the folder doesn't already exist with wrong permissions:
   ```bash
   chmod 755 your_folder_name
   ```

## Performance Issues

### High CPU usage
**Cause:** Software decoding or encoding

**Solutions:**
1. Ensure NVIDIA hardware decoder is being used (Jetson)
2. Check with:
   ```bash
   tegrastats  # On Jetson
   ```
3. Lower capture frequency

### Delayed captures
**Cause:** Network latency or jitter buffer

**Solution:** Adjust jitter buffer in code:
```cpp
"rtpjitterbuffer latency=200 !"  // Increase latency value
```

## Getting Help

If issues persist:
1. Check GStreamer installation:
   ```bash
   gst-inspect-1.0 --version
   gst-inspect-1.0 nvv4l2decoder  # Check NVIDIA plugin
   ```

2. Enable debug output:
   ```bash
   GST_DEBUG=3 ./camera_capture
   ```

3. Verify all dependencies:
   ```bash
   pkg-config --modversion gstreamer-1.0
   pkg-config --modversion gstreamer-app-1.0
   pkg-config --modversion yaml-cpp
   ```

## Differences from Original Version

### What Changed:
1. **Capture Method:** 
   - Old: multifilesink + valve (unreliable)
   - New: appsink with direct file writing (reliable)

2. **Configuration:**
   - Old: Hardcoded IP/port
   - New: YAML config file

3. **Implementation:**
   - Old: Used GstMultiFileSink messages
   - New: Uses appsink callbacks for direct control

### Why These Changes:
- **appsink** gives better control over when and how frames are captured
- **YAML config** makes it easy to change settings without recompiling
- **Direct file writing** ensures captures complete successfully
