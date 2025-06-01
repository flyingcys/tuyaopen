#!/usr/bin/env python3

import os
import sys
from pathlib import Path

# 设置项目根目录
project_root = Path.cwd() / 'apps' / 'tuya_cloud' / 'switch_demo'
os.environ['TOS_PROJECT_ROOT'] = str(project_root)

print(f"Project root: {project_root}")

# 检查配置文件
app_default_config = project_root / 'app_default.config'
print(f"Config file: {app_default_config}")
print(f"Config exists: {app_default_config.exists()}")

if app_default_config.exists():
    print(f"Config content: '{app_default_config.read_text()}'")
    
    # 添加工具路径
    sys.path.insert(0, str(Path.cwd() / 'tools' / 'kconfiglib'))
    
    # 导入并运行配置生成
    try:
        import gen_config
        print("Running gen_config.main()...")
        gen_config.main()
        print("gen_config.main() completed")
    except Exception as e:
        print(f"Error: {e}")
        import traceback
        traceback.print_exc()
else:
    print("Config file not found!") 