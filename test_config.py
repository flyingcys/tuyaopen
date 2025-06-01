#!/usr/bin/env python3

import os
import sys
from pathlib import Path

# 添加工具路径
sys.path.insert(0, str(Path(__file__).parent / 'tools' / 'kconfiglib'))

# 设置环境变量
os.environ['TOS_PROJECT_ROOT'] = str(Path.cwd() / 'apps' / 'tuya_cloud' / 'switch_demo')

# 导入并运行配置生成
import gen_config
gen_config.main() 