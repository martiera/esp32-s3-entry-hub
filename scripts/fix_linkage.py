import os
import glob

def fix_file(filepath):
    with open(filepath, 'r') as f:
        content = f.read()
    
    # Remove extern "C" blocks
    content = content.replace('#ifdef __cplusplus\nextern "C" {\n#endif\n', '')
    content = content.replace('#ifdef __cplusplus\n}\n#endif\n', '')
    content = content.replace('extern "C" {\n', '')
    content = content.replace('}\n', '')
    
    # Add extern to const definitions for C++ external linkage
    # Match: const lv_img_dsc_t name = {
    lines = content.splitlines()
    new_lines = []
    for line in lines:
        if line.startswith('const lv_img_dsc_t ') and ' = {' in line:
            line = 'extern ' + line
        new_lines.append(line)
    
    new_content = '\n'.join(new_lines)
    
    # Clean up multiple newlines
    while '\n\n\n' in new_content:
        new_content = new_content.replace('\n\n\n', '\n\n')
        
    with open(filepath, 'w') as f:
        f.write(new_content)
    print(f"Fixed {filepath}")

# Fix bg_image.cpp
fix_file('src/bg_image.cpp')

# Fix weather icons
for f in glob.glob('src/weather_icons/*.cpp'):
    fix_file(f)

# Fix header
header_path = 'src/weather_icons/weather_icons.h'
with open(header_path, 'r') as f:
    content = f.read()
content = content.replace('#ifdef __cplusplus\nextern "C" {\n#endif\n', '')
content = content.replace('#ifdef __cplusplus\n}\n#endif\n', '')
with open(header_path, 'w') as f:
    f.write(content)
print(f"Fixed {header_path}")
