import numpy as np
import cv2

def euler_to_rotation_matrix(yaw_deg, pitch_deg, roll_deg=0):
    """Calculate rotation matrix from Euler angles"""
    yaw = np.radians(yaw_deg)
    pitch = np.radians(pitch_deg)
    roll = np.radians(roll_deg)
    
    # Rotation around Z-axis (yaw)
    Rz = np.array([
        [np.cos(yaw), -np.sin(yaw), 0],
        [np.sin(yaw),  np.cos(yaw), 0],
        [0, 0, 1]
    ])
    
    # Rotation around X-axis (pitch - tilt down is positive)
    Rx = np.array([
        [1, 0, 0],
        [0, np.cos(pitch), -np.sin(pitch)],
        [0, np.sin(pitch),  np.cos(pitch)]
    ])
    
    # Rotation around Y-axis (roll)
    Ry = np.array([
        [np.cos(roll), 0, np.sin(roll)],
        [0, 1, 0],
        [-np.sin(roll), 0, np.cos(roll)]
    ])
    
    # Combined: Rz * Ry * Rx
    return Rz @ Ry @ Rx

def fov_to_focal_length(fov_deg, image_width):
    """Calculate focal length from horizontal FOV"""
    fov_rad = np.radians(fov_deg)
    focal_length = image_width / (2 * np.tan(fov_rad / 2))
    return focal_length

# ============================================================
# CONFIGURATION
# ============================================================
image_width = 1280
image_height = 800
horizontal_fov = 120  # degrees
pitch_down = 20       # degrees (camera tilt down)

# Calculate focal length
focal_length = fov_to_focal_length(horizontal_fov, image_width)

print("=" * 70)
print("GENERATING CALIBRATION FILES FOR 4-CAMERA SURROUND VIEW")
print("=" * 70)
print(f"\nImage resolution: {image_width}x{image_height}")
print(f"Horizontal FOV: {horizontal_fov}°")
print(f"Calculated focal length: {focal_length:.2f} pixels")
print(f"Camera tilt (pitch): {pitch_down}° down")

# Camera definitions: (idx, name, yaw, pitch, roll, ip, port)
cameras = [
    (0, "Front", 0,   pitch_down, 0, "192.168.45.10", 5020),
    (1, "Left",  90,  pitch_down, 0, "192.168.45.11", 5021),
    (2, "Rear",  180, pitch_down, 0, "192.168.45.12", 5022),
    (3, "Right", 270, pitch_down, 0, "192.168.45.13", 5023),
]

print("\n" + "=" * 70)
print("CAMERA CONFIGURATION")
print("=" * 70)

for idx, name, yaw, pitch, roll, ip, port in cameras:
    print(f"\nCamera {idx} - {name}:")
    print(f"  IP:Port:  {ip}:{port}")
    print(f"  Yaw:      {yaw}° (horizontal rotation)")
    print(f"  Pitch:    {pitch}° (tilt down)")
    print(f"  Roll:     {roll}° (twist)")
    
    # Calculate rotation matrix
    R = euler_to_rotation_matrix(yaw, pitch, roll)
    
    # Create intrinsic matrix
    K = np.array([
        [focal_length, 0, image_width/2],
        [0, focal_length, image_height/2],
        [0, 0, 1]
    ], dtype=np.float32)
    
    # Save to YAML
    filename = f"Camparam{idx}.yaml"
    fs = cv2.FileStorage(filename, cv2.FILE_STORAGE_WRITE)
    fs.write("FocalLength", float(focal_length))
    fs.write("Intrisic", K)
    fs.write("Rotation", R.astype(np.float32))
    fs.write("Translation", np.zeros((3, 1), dtype=np.float64))
    fs.release()
    
    print(f"  ✓ Saved {filename}")
    print(f"  Rotation matrix:")
    print(f"    [{R[0,0]:8.5f}, {R[0,1]:8.5f}, {R[0,2]:8.5f}]")
    print(f"    [{R[1,0]:8.5f}, {R[1,1]:8.5f}, {R[1,2]:8.5f}]")
    print(f"    [{R[2,0]:8.5f}, {R[2,1]:8.5f}, {R[2,2]:8.5f}]")
    
    # Show optical axis (viewing direction)
    optical_axis = R[:, 2]
    print(f"  Viewing direction: [{optical_axis[0]:6.3f}, {optical_axis[1]:6.3f}, {optical_axis[2]:6.3f}]")

# Create default corner warp points for HD output
print("\n" + "=" * 70)
print("GENERATING OUTPUT CROP CONFIGURATION")
print("=" * 70)

# Placeholder values - will be computed at runtime based on actual warped size
output_config = "corner_warppts.yaml"
fs = cv2.FileStorage(output_config, cv2.FILE_STORAGE_WRITE)
fs.write("res_size", np.array([1920, 1080], dtype=np.int32))  # HD resolution
fs.write("tl", np.array([0, 0], dtype=np.int32))
fs.write("tr", np.array([1920, 0], dtype=np.int32))
fs.write("bl", np.array([0, 1080], dtype=np.int32))
fs.write("br", np.array([1920, 1080], dtype=np.int32))
fs.release()


print(f"✓ Saved {output_config} (HD 1920x1080 output)")

print("\n" + "=" * 70)
print("✓ ALL CALIBRATION FILES GENERATED SUCCESSFULLY!")
print("=" * 70)
print("\nFiles created:")
print("  - Camparam0.yaml (Front camera)")
print("  - Camparam1.yaml (Left camera)")
print("  - Camparam2.yaml (Rear camera)")
print("  - Camparam3.yaml (Right camera)")
print("  - corner_warppts.yaml (Output crop)")
print("\nNext: Run 'make' to build the simplified surround view system")