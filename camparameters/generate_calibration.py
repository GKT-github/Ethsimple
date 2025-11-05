#!/usr/bin/env python3
"""
Surround View Calibration Generator
Generates camera calibration YAML files for 4-camera surround view system
"""

import numpy as np
import cv2
import sys

def euler_to_rotation_matrix(yaw_deg, pitch_deg, roll_deg=0):
    """
    Calculate rotation matrix from Euler angles
    
    Args:
        yaw_deg: Horizontal rotation in degrees (0=front, 90=left, 180=rear, 270=right)
        pitch_deg: Vertical tilt in degrees (positive = down, negative = up)
        roll_deg: Camera twist in degrees (usually 0)
    
    Returns:
        3x3 rotation matrix
    """
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
    """
    Calculate focal length in pixels from horizontal field of view
    
    Args:
        fov_deg: Horizontal field of view in degrees
        image_width: Image width in pixels
    
    Returns:
        Focal length in pixels
    """
    fov_rad = np.radians(fov_deg)
    focal_length = image_width / (2 * np.tan(fov_rad / 2))
    return focal_length

def save_camera_yaml(idx, name, R, focal_length, cx, cy, ip, port):
    """
    Save camera parameters to YAML file
    
    Args:
        idx: Camera index (0-3)
        name: Camera name
        R: 3x3 rotation matrix
        focal_length: Focal length in pixels
        cx, cy: Principal point coordinates
        ip: Camera IP address
        port: Camera port
    """
    # Create intrinsic matrix
    K = np.array([
        [focal_length, 0, cx],
        [0, focal_length, cy],
        [0, 0, 1]
    ], dtype=np.float32)
    
    filename = f"Camparam{idx}.yaml"
    fs = cv2.FileStorage(filename, cv2.FILE_STORAGE_WRITE)
    
    fs.write("FocalLength", float(focal_length))
    fs.write("Intrisic", K)
    fs.write("Rotation", R.astype(np.float32))
    fs.write("Translation", np.zeros((3, 1), dtype=np.float64))
    
    # Add metadata as comments (stored as additional fields)
    fs.write("CameraName", name)
    fs.write("CameraIP", ip)
    fs.write("CameraPort", int(port))
    
    fs.release()
    
    print(f"✓ Saved {filename}")

def main():
    print("=" * 70)
    print("SURROUND VIEW CALIBRATION GENERATOR")
    print("=" * 70)
    
    # ============================================================
    # CONFIGURATION - Modify these values for your setup
    # ============================================================
    
    image_width = 1280
    image_height = 800
    horizontal_fov = 120  # degrees
    pitch_down = 20       # degrees (camera tilt down)
    
    # Calculate focal length
    focal_length = fov_to_focal_length(horizontal_fov, image_width)
    
    print(f"\nConfiguration:")
    print(f"  Image resolution: {image_width}x{image_height}")
    print(f"  Horizontal FOV: {horizontal_fov}°")
    print(f"  Calculated focal length: {focal_length:.2f} pixels")
    print(f"  Camera tilt (pitch): {pitch_down}° down")
    print(f"  Principal point: ({image_width/2:.1f}, {image_height/2:.1f})")
    
    # Camera definitions: (idx, name, yaw, pitch, roll, ip, port)
    cameras = [
        (0, "Front", 0,   pitch_down, 0, "192.168.45.10", 5020),
        (1, "Left",  90,  pitch_down, 0, "192.168.45.11", 5021),
        (2, "Rear",  180, pitch_down, 0, "192.168.45.12", 5022),
        (3, "Right", 270, pitch_down, 0, "192.168.45.13", 5023),
    ]
    
    print("\n" + "=" * 70)
    print("GENERATING CALIBRATION FILES")
    print("=" * 70)
    
    for idx, name, yaw, pitch, roll, ip, port in cameras:
        print(f"\nCamera {idx} - {name}:")
        print(f"  Network: {ip}:{port}")
        print(f"  Orientation: Yaw={yaw}°, Pitch={pitch}°, Roll={roll}°")
        
        # Calculate rotation matrix
        R = euler_to_rotation_matrix(yaw, pitch, roll)
        
        # Save to YAML
        save_camera_yaml(idx, name, R, focal_length, 
                        image_width/2, image_height/2, ip, port)
        
        # Show rotation details
        optical_axis = R[:, 2]
        print(f"  Viewing direction: [{optical_axis[0]:6.3f}, {optical_axis[1]:6.3f}, {optical_axis[2]:6.3f}]")
        
        # Verify determinant (should be 1.0 for valid rotation)
        det = np.linalg.det(R)
        print(f"  det(R) = {det:.6f} {'✓' if abs(det - 1.0) < 0.001 else '✗ ERROR'}")
    
    # Create default output crop configuration
    print("\n" + "=" * 70)
    print("GENERATING OUTPUT CONFIGURATION")
    print("=" * 70)
    
    output_width = 1920
    output_height = 1080
    output_config = "corner_warppts.yaml"  # <--- ADD THIS LINE

    fs = cv2.FileStorage(output_config, cv2.FILE_STORAGE_WRITE)
    fs.write("res_size", np.array([output_width, output_height], dtype=np.int32))
    fs.write("tl", np.array([0, 0], dtype=np.int32))
    fs.write("tr", np.array([output_width, 0], dtype=np.int32))
    fs.write("bl", np.array([0, output_height], dtype=np.int32))
    fs.write("br", np.array([output_width, output_height], dtype=np.int32))
    fs.release()

    
    print(f"✓ Saved corner_warppts.yaml (HD {output_width}x{output_height} output)")
    
    print("\n" + "=" * 70)
    print("✓ CALIBRATION GENERATION COMPLETE!")
    print("=" * 70)
    print("\nGenerated files:")
    print("  - Camparam0.yaml (Front camera)")
    print("  - Camparam1.yaml (Left camera)")
    print("  - Camparam2.yaml (Rear camera)")
    print("  - Camparam3.yaml (Right camera)")
    print("  - corner_warppts.yaml (Output crop)")
    print("\nNext steps:")
    print("  1. Place these files in the 'camparameters' folder")
    print("  2. Build: mkdir build && cd build && cmake .. && make")
    print("  3. Run: ./SurroundViewSimple ../camparameters")
    print("=" * 70)

if __name__ == "__main__":
    main()
