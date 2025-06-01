# TuyaOpen构建工具 Python重构指南

## 概述

本文档说明如何将TuyaOpen项目的bash构建脚本重构为Python实现，以实现更好的跨平台兼容性。

## 重构的原因

1. **跨平台兼容性**: bash脚本在Windows上支持有限，Python提供了更好的跨平台支持
2. **可维护性**: Python代码结构更清晰，更易于维护和扩展
3. **错误处理**: Python提供了更好的错误处理机制
4. **现代化**: 使用现代语言特性，支持类型提示等

## 功能对比

### 原bash版本 (tos)
- 基于bash脚本实现
- 依赖Linux/Unix环境
- 命令: `tos config_choice`, `tos build`, `tos clean`等

### 新Python版本 (tos.py)
- 基于Python 3.6+实现
- 支持Windows/Linux/macOS
- 相同的命令接口，保持兼容性

## 文件结构

```
TuyaOpen/
├── tos                    # 原bash版本（保留）
├── tos.py                 # 新Python版本
├── tos.bat               # Windows启动脚本
├── tos_py                # Unix启动脚本
├── requirements_tos.txt  # Python依赖
└── TOS_MIGRATION_GUIDE.md # 本文档
```

## 安装和使用

### 1. 安装依赖

```bash
# 安装Python依赖
pip3 install -r requirements_tos.txt
```

### 2. 设置环境

#### Linux/macOS:
```bash
# 方法1: 直接使用Python脚本
python3 tos.py --help

# 方法2: 使用包装脚本
./tos_py --help

# 方法3: 添加到PATH（推荐）
export PATH=$PATH:$PWD
# 然后可以直接使用: tos_py command
```

#### Windows:
```batch
REM 方法1: 直接使用Python脚本
python tos.py --help

REM 方法2: 使用批处理文件
tos.bat --help

REM 方法3: 添加到PATH
set PATH=%PATH%;%CD%
REM 然后可以直接使用: tos.bat command
```

### 3. 命令使用

Python版本保持与bash版本相同的命令接口：

```bash
# 检查系统依赖
tos.py check

# 配置选择
tos.py config_choice

# 构建项目
tos.py build

# 清理项目
tos.py clean

# 完全清理
tos.py fullclean

# 配置菜单
tos.py menuconfig

# 设置示例
tos.py set_example

# 烧录固件
tos.py flash

# 显示版本
tos.py version

# 显示帮助
tos.py help
```

## 功能特性

### 1. 系统检查
- 自动检查Python、Git、CMake、Ninja版本
- 跨平台的版本检查机制

### 2. 配置管理
- 兼容原有的.config文件格式
- 支持项目配置和板级默认配置
- 交互式配置选择菜单

### 3. 构建系统
- 集成CMake和Ninja构建
- 支持调试模式 (`TOS_DEBUG=1`)
- 自动环境变量设置

### 4. 平台管理
- 支持YAML配置的平台管理
- 自动下载和更新平台
- 示例项目设置

### 5. 烧录功能
- 集成tyutool烧录工具
- 支持自动和手动烧录模式

## 兼容性说明

### 与原bash版本的兼容性
- **命令接口**: 完全兼容
- **配置文件**: 完全兼容
- **构建输出**: 完全兼容
- **环境变量**: 完全兼容

### 系统要求
- **Python**: 3.6及以上版本
- **操作系统**: Windows 10+, Linux, macOS
- **其他依赖**: Git, CMake, Ninja（与原版本相同）

## 迁移步骤

### 1. 保留原版本
```bash
# 保留原bash版本作为备份
cp tos tos.bash.backup
```

### 2. 测试Python版本
```bash
# 在现有项目中测试
cd apps/tuya_cloud/switch_demo
python3 /path/to/tos.py config_choice
python3 /path/to/tos.py build
```

### 3. 逐步迁移
```bash
# 如果测试成功，可以创建别名或符号链接
alias tos='python3 /path/to/tos.py'
# 或
ln -sf /path/to/tos.py /usr/local/bin/tos
```

## 开发和扩展

### 代码结构
```python
tos.py
├── Colors          # 终端颜色支持
├── Logger           # 日志输出
├── SystemChecker    # 系统依赖检查
├── GitManager       # Git仓库管理
├── ConfigManager    # 配置管理
├── PlatformManager  # 平台管理
├── BuildManager     # 构建管理
├── FlashManager     # 烧录管理
└── TuyaOpenBuilder  # 主控制类
```

### 扩展新功能
1. 在对应的Manager类中添加新方法
2. 在TuyaOpenBuilder类中添加对应的命令处理
3. 在main函数中添加命令分发

### 调试模式
```bash
# 启用调试输出
export TOS_DEBUG=1
python3 tos.py build
```

## 问题排查

### 常见问题

1. **Python版本不兼容**
   ```bash
   # 检查Python版本
   python3 --version
   # 确保版本 >= 3.6
   ```

2. **依赖包缺失**
   ```bash
   # 安装依赖
   pip3 install PyYAML colorama
   ```

3. **权限问题（Linux/macOS）**
   ```bash
   # 设置可执行权限
   chmod +x tos_py
   ```

4. **路径问题**
   ```bash
   # 检查SDK根目录设置
   echo $TUYAOPEN_ROOT
   # 或确保在正确的目录中运行
   ```

### 日志和诊断
```bash
# 启用详细日志
export TOS_DEBUG=1

# 检查系统状态
python3 tos.py check

# 查看配置
python3 tos.py config_choice
```

## 性能对比

| 功能 | Bash版本 | Python版本 | 备注 |
|------|----------|-------------|------|
| 启动时间 | ~100ms | ~200ms | Python解释器启动开销 |
| 配置解析 | 快 | 快 | 性能相当 |
| 构建速度 | 快 | 快 | 主要时间在CMake/Ninja |
| 跨平台性 | 差 | 优 | Python版本明显优势 |
| 可维护性 | 中 | 优 | Python代码更清晰 |

## 总结

Python重构版本在保持完全兼容性的同时，提供了：
- 更好的跨平台支持
- 更清晰的代码结构
- 更好的错误处理
- 更易于维护和扩展

建议在充分测试后逐步迁移到Python版本，同时保留bash版本作为备份。 