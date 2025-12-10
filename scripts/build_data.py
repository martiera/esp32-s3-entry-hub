Import("env")
import os
import shutil

def build_web_data(source, target, env):
    """Build script to prepare web data for upload"""
    print("Building web data...")
    
    # Ensure data directory exists
    data_dir = os.path.join(env['PROJECT_DIR'], 'data', 'www')
    if not os.path.exists(data_dir):
        print(f"Creating {data_dir}")
        os.makedirs(data_dir)
    
    print(f"Web files ready in {data_dir}")
    print("Run 'pio run --target uploadfs' to upload to device")

# Register the build script
env.AddPreAction("buildfs", build_web_data)
