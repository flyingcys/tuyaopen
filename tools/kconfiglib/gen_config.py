#!/usr/bin/env python3
"""
Kconfig配置生成工具
处理.config文件并生成CMake配置和头文件
"""

import os
import sys
import re
from pathlib import Path


def infer_platform_from_board(config: dict) -> dict:
    """从板级配置推导平台信息"""
    updated_config = config.copy()
    
    # 板级到平台的映射关系
    board_to_platform = {
        'CONFIG_BOARD_CHOICE_ESP32': 'ESP32',
        'CONFIG_BOARD_CHOICE_ESP32_C3': 'ESP32',
        'CONFIG_BOARD_CHOICE_ESP32_S3': 'ESP32',
        'CONFIG_BOARD_CHOICE_T2': 'T2',
        'CONFIG_BOARD_CHOICE_T3': 'T3', 
        'CONFIG_BOARD_CHOICE_T5AI': 'T5AI',
        'CONFIG_BOARD_CHOICE_BK7231X': 'BK7231X',
        'CONFIG_BOARD_CHOICE_LN882H': 'LN882H',
        'CONFIG_BOARD_CHOICE_UBUNTU': 'Ubuntu',
        'CONFIG_BOARD_CHOICE_EWT103_W15': 'EWT103-W15',
    }
    
    # 芯片映射
    board_to_chip = {
        'CONFIG_BOARD_CHOICE_ESP32': 'ESP32',
        'CONFIG_BOARD_CHOICE_ESP32_C3': 'ESP32-C3',
        'CONFIG_BOARD_CHOICE_ESP32_S3': 'ESP32-S3',
        'CONFIG_BOARD_CHOICE_T2': 'T2',
        'CONFIG_BOARD_CHOICE_T3': 'T3',
        'CONFIG_BOARD_CHOICE_T5AI': 'T5AI',
        'CONFIG_BOARD_CHOICE_BK7231X': 'BK7231X',
        'CONFIG_BOARD_CHOICE_LN882H': 'LN882H',
        'CONFIG_BOARD_CHOICE_UBUNTU': 'Ubuntu',
        'CONFIG_BOARD_CHOICE_EWT103_W15': 'EWT103-W15',
    }
    
    # 查找活跃的板级配置
    for board_config, platform in board_to_platform.items():
        if board_config in config and config[board_config] == 'y':
            # 设置平台配置
            updated_config['CONFIG_PLATFORM_CHOICE'] = f'"{platform}"'
            updated_config['CONFIG_CHIP_CHOICE'] = f'"{board_to_chip.get(board_config, platform)}"'
            updated_config['CONFIG_BOARD_CHOICE'] = f'"{platform}"'
            
            # 设置项目默认配置
            if 'CONFIG_PROJECT_NAME' not in updated_config:
                updated_config['CONFIG_PROJECT_NAME'] = '"switch_demo"'
            
            if 'CONFIG_PROJECT_VERSION' not in updated_config:
                updated_config['CONFIG_PROJECT_VERSION'] = '"1.0.0"'
                
            if 'CONFIG_FRAMEWORK' not in updated_config:
                updated_config['CONFIG_FRAMEWORK'] = '"base"'
            
            break
    
    return updated_config


def load_config_file(config_path: Path) -> dict:
    """加载配置文件"""
    config = {}
    
    if not config_path.exists():
        return config
    
    with open(config_path, 'r') as f:
        for line in f:
            line = line.strip()
            
            # 跳过注释和空行
            if not line or line.startswith('#'):
                continue
            
            # 处理配置项
            if '=' in line:
                key, value = line.split('=', 1)
                config[key] = value
            else:
                # 处理布尔型配置 (如 CONFIG_XXX=y)
                config[line] = 'y'
    
    return config


def generate_cmake_config(config: dict, output_path: Path):
    """生成CMake配置文件"""
    cmake_content = []
    cmake_content.append("# Auto-generated CMake configuration")
    cmake_content.append("# DO NOT EDIT MANUALLY")
    cmake_content.append("")
    
    for key, value in config.items():
        # 转换为CMake变量格式
        cmake_key = key
        cmake_value = value.strip('"')  # 移除引号
        
        cmake_content.append(f"set({cmake_key} \"{cmake_value}\")")
    
    cmake_content.append("")
    
    with open(output_path, 'w') as f:
        f.write('\n'.join(cmake_content))


def generate_header_file(config: dict, output_path: Path):
    """生成C头文件"""
    header_content = []
    header_content.append("/* Auto-generated header file */")
    header_content.append("/* DO NOT EDIT MANUALLY */")
    header_content.append("")
    header_content.append("#ifndef __TUYA_KCONFIG_H__")
    header_content.append("#define __TUYA_KCONFIG_H__")
    header_content.append("")
    
    for key, value in config.items():
        if value == 'y':
            # 布尔型配置
            header_content.append(f"#define {key} 1")
        elif value == 'n':
            # 禁用的配置
            header_content.append(f"/* #undef {key} */")
        else:
            # 字符串或数值配置
            if value.startswith('"') and value.endswith('"'):
                # 字符串值
                header_content.append(f"#define {key} {value}")
            else:
                # 数值
                try:
                    int(value)
                    header_content.append(f"#define {key} {value}")
                except ValueError:
                    # 作为字符串处理
                    header_content.append(f"#define {key} \"{value}\"")
    
    header_content.append("")
    header_content.append("#endif /* __TUYA_KCONFIG_H__ */")
    header_content.append("")
    
    with open(output_path, 'w') as f:
        f.write('\n'.join(header_content))


def generate_config_output(config: dict, output_path: Path):
    """生成标准配置文件输出"""
    config_content = []
    config_content.append("# Auto-generated configuration")
    config_content.append("# DO NOT EDIT MANUALLY")
    config_content.append("")
    
    for key, value in config.items():
        if value == 'y':
            config_content.append(f"{key}=y")
        elif value == 'n':
            config_content.append(f"# {key} is not set")
        else:
            config_content.append(f"{key}={value}")
    
    config_content.append("")
    
    with open(output_path, 'w') as f:
        f.write('\n'.join(config_content))


def main():
    """主函数"""
    project_root = Path(os.getenv('TOS_PROJECT_ROOT', os.getcwd()))
    
    # 输入文件
    app_default_config = project_root / 'app_default.config'
    
    # 输出目录和文件
    build_dir = project_root / '.build'
    cache_dir = build_dir / 'cache'
    include_dir = build_dir / 'include'
    
    # 确保目录存在
    cache_dir.mkdir(parents=True, exist_ok=True)
    include_dir.mkdir(parents=True, exist_ok=True)
    
    # 输出文件
    using_config = cache_dir / 'using.config'
    cmake_config = cache_dir / 'using.cmake'
    header_file = include_dir / 'tuya_kconfig.h'
    
    if not app_default_config.exists():
        print(f"错误: 未找到配置文件 {app_default_config}")
        sys.exit(1)
    
    # 加载配置
    config = load_config_file(app_default_config)
    
    # 推导平台信息
    config = infer_platform_from_board(config)
    
    # 生成输出文件
    generate_cmake_config(config, cmake_config)
    generate_header_file(config, header_file)
    generate_config_output(config, using_config)
    
    print(f"配置处理完成:")
    print(f"  CMake配置: {cmake_config}")
    print(f"  头文件: {header_file}")
    print(f"  使用配置: {using_config}")
    
    # 显示关键配置信息
    platform = config.get('CONFIG_PLATFORM_CHOICE', '').strip('"')
    chip = config.get('CONFIG_CHIP_CHOICE', '').strip('"')
    if platform:
        print(f"  平台: {platform}")
    if chip:
        print(f"  芯片: {chip}")


if __name__ == '__main__':
    main() 