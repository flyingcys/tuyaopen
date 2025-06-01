#!/usr/bin/env python3
"""
TuyaOpen构建工具 (Python版本)
严格按照原始bash脚本的流程实现，完全替代bash脚本构建体系
"""

import os
import sys
import subprocess
import argparse
import shutil
import re
from pathlib import Path
from typing import Optional, List, Dict


class Colors:
    """终端颜色支持"""
    RESET = '\033[0m'
    RED = '\033[31m'
    GREEN = '\033[32m'
    YELLOW = '\033[33m'
    BLUE = '\033[34m'
    PURPLE = '\033[35m'
    CYAN = '\033[36m'
    WHITE = '\033[37m'


class Logger:
    """日志输出类，模拟bash版本的show函数"""
    
    @staticmethod
    def show(msg: str, color: str = ""):
        """模拟bash版本的show函数"""
        if color:
            print(f"{color}{msg}{Colors.RESET}")
        else:
            print(msg)
    
    @staticmethod
    def error(msg: str):
        Logger.show(msg, Colors.RED)
    
    @staticmethod
    def warning(msg: str):
        Logger.show(msg, Colors.YELLOW)
    
    @staticmethod
    def info(msg: str):
        Logger.show(msg, Colors.GREEN)


class ConfigManager:
    """配置管理，严格按照bash版本的config_choice_exec实现"""
    
    def __init__(self, project_root: Path, sdk_root: Path):
        self.project_root = project_root
        self.sdk_root = sdk_root
        self.kconfig_tools = sdk_root / 'tools' / 'kconfiglib'
        self.project_ini = project_root / '.build' / 'cache' / 'using.config'
    
    def check_project_file(self) -> bool:
        """检查项目CMakeLists.txt文件"""
        project_file = self.project_root / 'CMakeLists.txt'
        if not project_file.exists():
            Logger.show("Can't found [CMakeLists.txt].", Colors.RED)
            return False
        return True
    
    def get_config_choices(self) -> List[Path]:
        """获取可用配置选择，按照bash版本的逻辑"""
        config_dir = self.project_root / 'config'
        
        # 检查项目配置目录
        if config_dir.exists():
            configs = list(config_dir.glob('*.config'))
            if configs:
                return sorted(configs)
        
        # 项目配置为空，使用boards默认配置
        Logger.show(f"[{config_dir}] is empty.", Colors.YELLOW)
        Logger.show("Using boards default config file.", Colors.YELLOW)
        
        # 调用multi_config_choice.sh获取选择
        return self._run_multi_config_choice()
    
    def _run_multi_config_choice(self) -> List[Path]:
        """运行multi_config_choice.sh脚本"""
        script_path = self.sdk_root / 'tools' / 'multi_config_choice.sh'
        boards_path = self.sdk_root / 'boards'
        
        try:
            # 先获取所有配置文件
            configs = []
            for board_config_dir in boards_path.glob('*/config'):
                configs.extend(board_config_dir.glob('*.config'))
            return sorted(configs)
        except Exception as e:
            Logger.error(f"Failed to get config choices: {e}")
            return []
    
    def select_config_interactive(self, configs: List[Path]) -> Optional[Path]:
        """交互式选择配置，模拟bash版本的输出格式"""
        print("========================")
        print("Configs")
        for i, config in enumerate(configs, 1):
            print(f"  {i}. {config.name}")
        print("------------------------")
        
        try:
            choice = int(input("Please select: ")) - 1
            print("------------------------")
            if 0 <= choice < len(configs):
                return configs[choice]
            else:
                Logger.error("Invalid selection")
                return None
        except (ValueError, KeyboardInterrupt):
            Logger.error("Selection cancelled")
            return None
    
    def using_config(self, force: bool = False):
        """using_config函数的Python实现"""
        dot_config_dir = self.project_ini.parent
        kconfig_catalog = "CatalogKconfig"
        dot_config = "using.config"
        boards_dir = self.sdk_root / 'boards'
        
        # 确保目录存在
        dot_config_dir.mkdir(parents=True, exist_ok=True)
        
        # 进入目录
        original_cwd = os.getcwd()
        os.chdir(dot_config_dir)
        
        try:
            # 运行set_catalog_config.py
            subprocess.run([
                sys.executable, 
                str(self.kconfig_tools / 'set_catalog_config.py'),
                '-b', str(boards_dir),
                '-s', str(self.sdk_root / 'src'),
                '-a', str(self.project_root),
                '-o', kconfig_catalog
            ], check=True)
            
            # 检查是否需要生成配置文件
            dot_config_path = dot_config_dir / dot_config
            if not dot_config_path.exists() or force:
                app_default_config = self.kconfig_tools / 'app_default.config'
                project_app_default = self.project_root / 'app_default.config'
                
                if project_app_default.exists():
                    app_default_config = project_app_default
                
                # 运行defconfig.py
                subprocess.run([
                    sys.executable,
                    str(self.kconfig_tools / 'defconfig.py'),
                    '--kconfig', kconfig_catalog,
                    '--dconfig', dot_config,
                    str(app_default_config)
                ], check=True)
                
        finally:
            os.chdir(original_cwd)
    
    def check_platform_change(self):
        """检查平台变化，按照bash版本实现"""
        if not self.project_ini.exists():
            return
        
        old_project_ini = self.project_root / '.using_config'
        if not old_project_ini.exists():
            shutil.copy2(self.project_ini, old_project_ini)
            return
        
        def extract_config_value(config_file: Path, key: str) -> str:
            """从配置文件中提取值，使用bash版本的正则表达式"""
            try:
                with open(config_file, 'r') as f:
                    content = f.read()
                # 使用bash版本相同的正则表达式
                pattern = f'{key}="([^"]*)"'
                match = re.search(pattern, content)
                return match.group(1) if match else ""
            except:
                return ""
        
        now_platform = extract_config_value(self.project_ini, 'CONFIG_PLATFORM_CHOICE')
        old_platform = extract_config_value(old_project_ini, 'CONFIG_PLATFORM_CHOICE')
        now_chip = extract_config_value(self.project_ini, 'CONFIG_CHIP_CHOICE')
        old_chip = extract_config_value(old_project_ini, 'CONFIG_CHIP_CHOICE')
        
        if not now_platform or not old_platform:
            Logger.show("Can't found [CONFIG_PLATFORM_CHOICE].", Colors.YELLOW)
            return
        
        if now_platform != old_platform or now_chip != old_chip:
            Logger.show(f"Platform: [{old_platform}] -> [{now_platform}]")
            Logger.show(f"Chip: [{old_chip}] -> [{now_chip}]")
            Logger.show("Platform or chip changed.", Colors.YELLOW)
            Logger.show("The platform or chip has been modified and needs to be cleared.", Colors.YELLOW)
            
            # 按照bash版本的逻辑处理
            now_project_ini_tmp = self.project_root / '.using_config.tmp'
            shutil.copy2(self.project_ini, now_project_ini_tmp)
            shutil.copy2(old_project_ini, self.project_ini)
            
            # 执行fullclean
            self._fullclean()
            
            # 恢复配置
            dot_config_dir = self.project_ini.parent
            dot_config_dir.mkdir(parents=True, exist_ok=True)
            shutil.move(now_project_ini_tmp, self.project_ini)
            shutil.copy2(self.project_ini, old_project_ini)
    
    def _fullclean(self):
        """完全清理"""
        build_dir = self.project_root / '.build'
        using_config_file = self.project_root / '.using_config'
        
        if build_dir.exists():
            shutil.rmtree(build_dir)
        if using_config_file.exists():
            using_config_file.unlink()
        
        Logger.show("Fullclean success.", Colors.GREEN)
    
    def config_choice_exec(self, target_config: Optional[str] = None) -> bool:
        """config_choice_exec函数的Python实现"""
        # 检查项目文件
        if not self.check_project_file():
            return False
        
        app_default_config = self.project_root / 'app_default.config'
        config_dir = self.project_root / 'config'
        
        if target_config:
            # 指定了配置文件
            target_path = Path(target_config)
            if not target_path.is_absolute():
                # 如果是相对路径，可能在config目录中
                if config_dir.exists():
                    potential_path = config_dir / target_config
                    if potential_path.exists():
                        target_path = potential_path
        else:
            # 交互式选择
            if not config_dir.exists() or not any(config_dir.glob('*.config')):
                # 使用boards默认配置
                configs = self.get_config_choices()
                if not configs:
                    Logger.error("No configuration files found")
                    return False
                target_path = self.select_config_interactive(configs)
            else:
                # 使用项目配置目录
                configs = sorted(config_dir.glob('*.config'))
                if not configs:
                    Logger.error("No configuration files found")
                    return False
                target_path = self.select_config_interactive(configs)
        
        if not target_path or not target_path.exists():
            Logger.show("Failed to select the configuration file.", Colors.RED)
            return False
        
        # 复制配置文件
        if target_path.resolve() != app_default_config.resolve():
            shutil.copy2(target_path, app_default_config)
        else:
            Logger.show(f"Config file [{target_path}] is already in place.", Colors.YELLOW)
        
        # 调用using_config
        self.using_config(force=True)
        
        # 检查平台变化
        self.check_platform_change()
        
        Logger.show(f"Use [{target_path}].", Colors.GREEN)
        return True


class SystemChecker:
    """系统依赖检查，按照bash版本的check_base_tool实现"""
    
    REQUIRED_TOOLS = {
        'bash': ('4.0.0', 'bash --version'),
        'grep': ('3.0.0', 'grep --version'),
        'sed': ('4.0.0', 'sed --version'),
        'python3': ('3.6.0', 'python3 --version'),
        'git': ('2.0.0', 'git --version'),
        'ninja': ('1.6.0', 'ninja --version'),
        'cmake': ('3.16.0', 'cmake --version')
    }
    
    @staticmethod
    def check_command_version(tool: str, min_version: str, version_cmd: str) -> bool:
        """检查工具版本，按照bash版本的check_command_version实现"""
        note_command = f"Please install [{tool}], and version > [{min_version}]."
        
        try:
            # 检查命令是否存在
            subprocess.run(['which', tool], capture_output=True, check=True)
        except subprocess.CalledProcessError:
            Logger.show(note_command, Colors.RED)
            return False
        
        try:
            # 获取版本信息
            result = subprocess.run(version_cmd.split(), capture_output=True, text=True, check=True)
            
            # 提取版本号，使用bash版本相同的正则表达式
            version_match = re.search(r'(\d+\.\d+(?:\.\d+)?)', result.stdout)
            if not version_match:
                Logger.show(note_command, Colors.RED)
                return False
            
            current_version = version_match.group(1)
            
            # 版本比较 - 使用简单的字符串比较来模拟sort -V
            def version_tuple(v):
                return tuple(map(int, v.split('.')))
            
            current_tuple = version_tuple(current_version)
            min_tuple = version_tuple(min_version)
            
            if current_tuple < min_tuple:
                note_version = f"Please update [{tool}]({current_version}) > [{min_version}]."
                Logger.show(note_version, Colors.YELLOW)
                return False
            
            note_ok = f"Check [{tool}]({current_version}) > [{min_version}]: OK."
            Logger.show(note_ok, Colors.GREEN)
            return True
            
        except subprocess.CalledProcessError:
            Logger.show(note_command, Colors.RED)
            return False
    
    @classmethod
    def check_base_tool(cls) -> bool:
        """检查所有基础工具"""
        exit_flag = False
        
        for tool, (min_version, version_cmd) in cls.REQUIRED_TOOLS.items():
            if not cls.check_command_version(tool, min_version, version_cmd):
                exit_flag = True
        
        return not exit_flag


class MirrorManager:
    """Git镜像管理，按照bash版本的enable_mirror/disable_mirror实现"""
    
    def __init__(self, sdk_root: Path):
        self.sdk_root = sdk_root
        self.mirror_file = sdk_root / '.mirror'
        self.mirror_status = None
        self._load_mirror_status()
    
    def _load_mirror_status(self):
        """加载镜像状态"""
        if self.mirror_file.exists():
            self.mirror_status = self.mirror_file.read_text().strip()
    
    def _get_country_code(self) -> str:
        """获取国家代码，判断是否需要镜像"""
        try:
            # 安装依赖
            requirements_file = self.sdk_root / 'tools' / 'requirements.txt'
            if requirements_file.exists():
                subprocess.run([
                    sys.executable, '-m', 'pip', 'install', '-r', str(requirements_file)
                ], capture_output=True)
            
            # 运行国家检测脚本
            country_script = self.sdk_root / 'tools' / 'get_conutry.py'
            if country_script.exists():
                result = subprocess.run([sys.executable, str(country_script)], 
                                      capture_output=True, text=True)
                return result.stdout.strip() if result.returncode == 0 else '0'
        except:
            pass
        return '0'
    
    def enable_mirror(self):
        """启用Git镜像"""
        if self.mirror_status is None:
            Logger.show("get_country_code ...")
            country = self._get_country_code()
            Logger.show(f"MIRROR={country}")
            self.mirror_file.write_text(country)
            self.mirror_status = country
        
        if self.mirror_status == '1':
            Logger.show("enable git mirror...")
            mirror_script = self.sdk_root / 'tools' / 'git-mirror.sh'
            if mirror_script.exists():
                subprocess.run(['bash', str(mirror_script), 'set'], cwd=self.sdk_root)
    
    def disable_mirror(self):
        """禁用Git镜像"""
        if self.mirror_status == '1':
            Logger.show("disable git mirror...")
            mirror_script = self.sdk_root / 'tools' / 'git-mirror.sh'
            if mirror_script.exists():
                subprocess.run(['bash', str(mirror_script), 'unset'], cwd=self.sdk_root)


class SubmoduleManager:
    """子模块管理，按照bash版本的check_submodules实现"""
    
    def __init__(self, sdk_root: Path):
        self.sdk_root = sdk_root
        self.mirror_manager = MirrorManager(sdk_root)
        
    def check_hooks(self):
        """检查Git钩子"""
        hooks_dir = self.sdk_root / '.git' / 'hooks'
        if hooks_dir.exists():
            pre_commit = hooks_dir / 'pre-commit'
            if not pre_commit.exists():
                source_hook = self.sdk_root / 'tools' / 'hooks' / 'pre-commit'
                if source_hook.exists():
                    shutil.copy2(source_hook, pre_commit)
    
    def check_submodules(self):
        """检查并初始化子模块"""
        self.check_hooks()
        Logger.show("Check submodules.", Colors.GREEN)
        
        gitmodules_path = self.sdk_root / '.gitmodules'
        if not gitmodules_path.exists():
            return
        
        # 解析.gitmodules文件获取子模块路径，使用bash版本相同的方式
        with open(gitmodules_path, 'r') as f:
            content = f.read()
        
        submodule_paths = re.findall(r'^\s*path\s*=\s*(.+)$', content, re.MULTILINE)
        
        self.mirror_manager.enable_mirror()
        
        original_cwd = os.getcwd()
        os.chdir(self.sdk_root)
        
        try:
            for path in submodule_paths:
                path = path.strip()
                submodule_path = self.sdk_root / path
                if not submodule_path.exists() or not any(submodule_path.iterdir()):
                    print(f"{path} is empty, initializing submodule...")
                    subprocess.run(['git', 'submodule', 'update', '--init', path], 
                                 check=True)
        finally:
            os.chdir(original_cwd)
            self.mirror_manager.disable_mirror()


class PlatformManager:
    """平台管理，按照bash版本的download_platform_by_name实现"""
    
    def __init__(self, sdk_root: Path):
        self.sdk_root = sdk_root
        self.platforms_dir = sdk_root / 'platform'
        self.platform_yaml = self.platforms_dir / 'platform_config.yaml'
        self.mirror_manager = MirrorManager(sdk_root)
    
    def download_platform_by_name(self, platform_name: str, platform_chip: str = "") -> bool:
        """通过名称下载平台，按照bash版本实现"""
        if not self.platform_yaml.exists():
            Logger.show("Platform config file not found", Colors.RED)
            return False
        
        # 解析YAML文件获取平台信息，使用bash版本相同的awk逻辑
        with open(self.platform_yaml, 'r') as f:
            content = f.read()
        
        # 模拟bash版本的awk命令：awk "/^- name: $PLATFORM_NAME[[:space:]]*$/{flag=1; next} /^- name:.*$/{flag=0} flag"
        lines = content.split('\n')
        platform_info_lines = []
        flag = False
        
        for line in lines:
            if re.match(f'^- name: {re.escape(platform_name)}\\s*$', line):
                flag = True
                continue
            elif re.match('^- name:.*$', line):
                flag = False
            elif flag:
                platform_info_lines.append(line)
        
        platform_info = '\n'.join(platform_info_lines)
        
        if not platform_info:
            Logger.show(f"Can't find the [platform name].", Colors.RED)
            return False
        
        # 提取repo和commit，使用bash版本相同的grep逻辑
        repo_match = re.search(r'repo: (.+)', platform_info)
        commit_match = re.search(r'commit: (.+)', platform_info)
        
        if not repo_match or not commit_match:
            Logger.show("Can't find the [platform repo].", Colors.RED)
            return False
        
        platform_repo = repo_match.group(1).strip()
        platform_commit = commit_match.group(1).strip()
        
        if not platform_repo:
            Logger.show("Can't find the [platform repo].", Colors.RED)
            return False
        if not platform_commit:
            Logger.show("Can't find the [platform commit].", Colors.RED)
            return False
        
        return self._download_platform(platform_name, platform_repo, platform_commit, platform_chip)
    
    def _download_platform(self, platform_name: str, platform_repo: str, 
                          platform_commit: str, platform_chip: str) -> bool:
        """下载平台，按照bash版本的download_platform实现"""
        platform_path = self.platforms_dir / platform_name
        
        self.mirror_manager.enable_mirror()
        
        try:
            if platform_path.exists():
                Logger.show("Update platform ...")
                original_cwd = os.getcwd()
                os.chdir(platform_path)
                try:
                    subprocess.run(['git', 'checkout', '-q', platform_commit], check=True)
                finally:
                    os.chdir(original_cwd)
            else:
                Logger.show("Download platform ...")
                subprocess.run(['git', 'clone', platform_repo, str(platform_path)], check=True)
                original_cwd = os.getcwd()
                os.chdir(platform_path)
                try:
                    subprocess.run(['git', 'checkout', '-q', platform_commit], check=True)
                finally:
                    os.chdir(original_cwd)
            
            # 运行平台准备脚本
            prepare_script = platform_path / 'platform_prepare.sh'
            if prepare_script.exists():
                Logger.show(f"Run [{prepare_script}].")
                original_cwd = os.getcwd()
                os.chdir(platform_path)
                try:
                    subprocess.run(['./platform_prepare.sh', platform_chip], check=True)
                finally:
                    os.chdir(original_cwd)
            
            return True
            
        except subprocess.CalledProcessError:
            Logger.show(f"Clone repo [{platform_repo}] failed, please try again.", Colors.RED)
            if platform_path.exists():
                Logger.show(f"Delete [{platform_path}] first.", Colors.YELLOW)
            return False
        finally:
            self.mirror_manager.disable_mirror()


class BuildManager:
    """构建管理，严格按照bash版本的build_exec实现"""
    
    def __init__(self, project_root: Path, sdk_root: Path):
        self.project_root = project_root
        self.sdk_root = sdk_root
        self.config_manager = ConfigManager(project_root, sdk_root)
        self.platform_manager = PlatformManager(sdk_root)
        self.submodule_manager = SubmoduleManager(sdk_root)
    
    def build_exec(self) -> bool:
        """build_exec函数的Python实现"""
        # 检查项目文件
        project_file = self.project_root / 'CMakeLists.txt'
        if not project_file.exists():
            Logger.show("Can't found [CMakeLists.txt].", Colors.RED)
            return False
        
        # 调用using_config
        self.config_manager.using_config()
        
        # 检查配置文件 - check_ini
        if not self.config_manager.project_ini.exists():
            Logger.show(f"Not found [{self.config_manager.project_ini}].", Colors.YELLOW)
            Logger.show(f"You could run [tos menuconfig].", Colors.GREEN)
            return False
        
        # 检查基础工具 - check_base_tool
        if not SystemChecker.check_base_tool():
            return False
        
        # 检查子模块 - check_submodules
        self.submodule_manager.check_submodules()
        
        # 从配置文件提取变量
        config_vars = self._extract_config_vars()
        if not config_vars:
            return False
        
        proj = config_vars['PROJECT_NAME']
        project_platform = config_vars['PROJECT_PLATFORM']
        project_framework = config_vars['PROJECT_FRAMEWORK']
        project_chip = config_vars['PROJECT_CHIP']
        project_board = config_vars['PROJECT_BOARD']
        
        # 检查Arduino框架
        if project_framework == 'arduino':
            if not self._check_arduino():
                return False
        
        # 检查平台是否存在，不存在则下载
        platform_path = self.sdk_root / 'platform' / project_platform
        if not platform_path.exists():
            if not self.platform_manager.download_platform_by_name(project_platform, project_chip):
                Logger.show(f"Download platform [{project_platform}] failed.", Colors.RED)
                return False
        
        # 创建构建目录
        build_dir = self.project_root / '.build'
        build_dir.mkdir(exist_ok=True)
        
        original_cwd = os.getcwd()
        os.chdir(build_dir)
        
        try:
            # 运行build_setup.sh
            build_setup_script = platform_path / 'build_setup.sh'
            if build_setup_script.exists():
                subprocess.run([str(build_setup_script), proj, project_platform, 
                              project_framework, project_chip], check=True)
            
            # 设置CMake参数
            cmake_args = [
                'cmake', '-G', 'Ninja',
                str(self.sdk_root),
                f'-DTOS_PROJECT_NAME={proj}',
                f'-DTOS_PROJECT_ROOT={self.project_root}',
                f'-DTOS_PROJECT_PLATFORM={project_platform}',
                f'-DTOS_FRAMEWORK={project_framework}',
                f'-DTOS_PROJECT_CHIP={project_chip}',
                f'-DTOS_PROJECT_BOARD={project_board}'
            ]
            
            # 添加详细输出参数
            if os.getenv('TOS_DEBUG') == '1':
                cmake_args.append('-DCMAKE_VERBOSE_MAKEFILE=ON')
            
            # 配置阶段
            subprocess.run(cmake_args, check=True)
            
            # 保存配置
            old_project_ini = self.project_root / '.using_config'
            if self.config_manager.project_ini.exists():
                shutil.copy2(self.config_manager.project_ini, old_project_ini)
            
            # 构建阶段
            ninja_args = ['ninja', 'example']
            if os.getenv('TOS_DEBUG') == '1':
                ninja_args.append('--verbose')
            
            subprocess.run(ninja_args, check=True)
            
            # 运行build_hook.sh (注意原bash中是build_hock.sh，可能是拼写错误)
            build_hook_script = platform_path / 'build_hock.sh'
            if build_hook_script.exists():
                subprocess.run([str(build_hook_script), proj, project_platform,
                              project_framework, project_chip], check=True)
            
            return True
            
        except subprocess.CalledProcessError:
            return False
        finally:
            os.chdir(original_cwd)
    
    def _extract_config_vars(self) -> Optional[Dict[str, str]]:
        """从配置文件提取变量，按照bash版本的逻辑"""
        try:
            with open(self.config_manager.project_ini, 'r') as f:
                content = f.read()
            
            def extract_var(key: str) -> str:
                """使用bash版本相同的正则表达式"""
                pattern = f'{key}="([^"]*)"'
                match = re.search(pattern, content)
                return match.group(1) if match else ""
            
            proj = extract_var('CONFIG_PROJECT_NAME')
            if not proj:
                Logger.show(f"Can't found [CONFIG_PROJECT_NAME] in {self.config_manager.project_ini}.", Colors.RED)
                return None
            
            project_platform = extract_var('CONFIG_PLATFORM_CHOICE')
            if not project_platform:
                Logger.show(f"Can't found [CONFIG_PLATFORM_CHOICE] in {self.config_manager.project_ini}.", Colors.RED)
                return None
            
            project_framework = extract_var('CONFIG_FRAMEWORK_CHOICE')
            if not project_framework:
                Logger.show(f"Can't found [PROJECT_FRAMEWORK] in {self.config_manager.project_ini}.", Colors.RED)
                return None
            
            project_chip = extract_var('CONFIG_CHIP_CHOICE')
            project_board = extract_var('CONFIG_BOARD_CHOICE')
            
            return {
                'PROJECT_NAME': proj,
                'PROJECT_PLATFORM': project_platform,
                'PROJECT_FRAMEWORK': project_framework,
                'PROJECT_CHIP': project_chip,
                'PROJECT_BOARD': project_board
            }
        except Exception as e:
            Logger.error(f"Failed to extract config vars: {e}")
            return None
    
    def _check_arduino(self) -> bool:
        """检查Arduino支持，按照bash版本的check_arduino实现"""
        arduino_repo = "https://github.com/tuya/arduino-tuyaopen.git"
        arduino_path = self.sdk_root / 'arduino-tuyaopen'
        
        original_cwd = os.getcwd()
        os.chdir(self.sdk_root)
        
        try:
            if arduino_path.exists():
                Logger.show(f"Skip download, exist [{arduino_path}].")
                Logger.show("If you want to download it again, please remove it.", Colors.YELLOW)
                return True
            
            self.platform_manager.mirror_manager.enable_mirror()
            
            try:
                subprocess.run(['git', 'clone', arduino_repo], check=True)
                if subprocess.run(['test', '!', '0', '=', '$?'], shell=True).returncode != 0:
                    Logger.show(f"Clone repo [{arduino_repo}] failed, please try again.", Colors.RED)
                    if arduino_path.exists():
                        Logger.show(f"Delete [{arduino_path}] first.", Colors.YELLOW)
                    return False
            finally:
                self.platform_manager.mirror_manager.disable_mirror()
            
            os.chdir(arduino_path)
            try:
                # 获取最新tag
                result = subprocess.run(['git', 'describe', '--tags', '--abbrev=0'], 
                                      capture_output=True, text=True, check=True)
                tag = result.stdout.strip()
                subprocess.run(['git', 'checkout', tag], check=True)
                subprocess.run(['git', 'submodule', 'update', '--init'], check=True)
                return True
            except subprocess.CalledProcessError:
                Logger.show(f"Failed to setup arduino repository.", Colors.RED)
                return False
            
        finally:
            os.chdir(original_cwd)
    
    def clean_exec(self):
        """clean_exec函数的Python实现"""
        build_ninja = self.project_root / '.build' / 'build.ninja'
        if not build_ninja.exists():
            return
        
        original_cwd = os.getcwd()
        os.chdir(self.project_root / '.build')
        try:
            subprocess.run(['ninja', 'clean_all'])
            Logger.show("Clean success.", Colors.GREEN)
        finally:
            os.chdir(original_cwd)
    
    def fullclean_exec(self):
        """fullclean_exec函数的Python实现"""
        self.clean_exec()
        build_dir = self.project_root / '.build'
        using_config = self.project_root / '.using_config'
        
        if build_dir.exists():
            shutil.rmtree(build_dir)
        if using_config.exists():
            using_config.unlink()
        
        Logger.show("Fullclean success.", Colors.GREEN)


class TuyaOpenBuilder:
    """TuyaOpen构建工具主类"""
    
    def __init__(self):
        self.sdk_root = self._find_sdk_root()
        self.project_root = Path.cwd()
        self.config_manager = ConfigManager(self.project_root, self.sdk_root)
        self.submodule_manager = SubmoduleManager(self.sdk_root)
        self.build_manager = BuildManager(self.project_root, self.sdk_root)
        self.platform_manager = PlatformManager(self.sdk_root)
    
    def _find_sdk_root(self) -> Path:
        """查找SDK根目录，按照bash版本的OPEN_SDK_ROOT逻辑"""
        # bash版本: OPEN_SDK_ROOT=$(cd "$(dirname "$0")" && pwd)
        current = Path(__file__).parent.absolute()
        
        # 检查当前文件所在目录
        if (current / 'CMakeLists.txt').exists() and (current / 'tools').exists():
            return current
        
        # 从环境变量查找
        sdk_path = os.getenv('TUYAOPEN_ROOT')
        if sdk_path and Path(sdk_path).exists():
            return Path(sdk_path)
        
        Logger.error("未找到TuyaOpen SDK根目录")
        sys.exit(1)
    
    def version(self):
        """version_exec函数的Python实现"""
        original_cwd = os.getcwd()
        os.chdir(self.sdk_root)
        
        try:
            result = subprocess.run(['git', 'describe', '--tags'], 
                                  capture_output=True, text=True, check=True)
            version = result.stdout.strip()
            Logger.show(version, Colors.GREEN)
        except subprocess.CalledProcessError:
            # 如果没有tags，尝试获取commit hash
            try:
                result = subprocess.run(['git', 'rev-parse', '--short', 'HEAD'], 
                                      capture_output=True, text=True, check=True)
                commit = result.stdout.strip()
                Logger.show(f"dev-{commit}", Colors.GREEN)
            except subprocess.CalledProcessError:
                Logger.show("unknown", Colors.GREEN)
        finally:
            os.chdir(original_cwd)
    
    def config_choice(self, config_path: Optional[str] = None):
        """配置选择"""
        if not self.config_manager.config_choice_exec(config_path):
            sys.exit(1)
    
    def build(self):
        """构建项目"""
        if not self.build_manager.build_exec():
            sys.exit(1)
    
    def clean(self):
        """清理构建"""
        self.build_manager.clean_exec()
    
    def fullclean(self):
        """完全清理"""
        self.build_manager.fullclean_exec()
    
    def menuconfig(self):
        """menuconfig_exec函数的Python实现"""
        # 检查项目文件
        project_file = self.project_root / 'CMakeLists.txt'
        if not project_file.exists():
            Logger.show("Can't found [CMakeLists.txt].", Colors.RED)
            return False
        
        # 调用using_config
        self.config_manager.using_config()
        
        dot_config_dir = self.project_root / '.build' / 'cache'
        kconfig_catalog = "CatalogKconfig"
        dot_config = "using.config"
        cmake_config = "using.cmake"
        header_dir = self.project_root / '.build' / 'include'
        header_file = "tuya_kconfig.h"
        header_in_path = self.sdk_root / 'tools' / 'kconfiglib' / 'config.h.in'
        
        dot_config_dir.mkdir(parents=True, exist_ok=True)
        
        original_cwd = os.getcwd()
        os.chdir(dot_config_dir)
        
        try:
            # 运行menuconfig
            menuconfig_script = self.sdk_root / 'tools' / 'kconfiglib' / 'run_menuconfig.sh'
            subprocess.run(['bash', str(menuconfig_script), kconfig_catalog, dot_config], check=True)
            
            # 检查平台变化
            self.config_manager.check_platform_change()
            
            # 生成CMake配置
            subprocess.run([
                sys.executable,
                str(self.sdk_root / 'tools' / 'kconfiglib' / 'conf2cmake.py'),
                '-c', str(dot_config_dir / dot_config),
                '-o', cmake_config
            ], check=True)
            
            # 生成头文件
            header_dir.mkdir(parents=True, exist_ok=True)
            os.chdir(header_dir)
            subprocess.run([
                sys.executable,
                str(self.sdk_root / 'tools' / 'kconfiglib' / 'conf2h.py'),
                '-c', str(dot_config_dir / dot_config),
                '-o', header_file,
                '-i', str(header_in_path)
            ], check=True)
            
        finally:
            os.chdir(original_cwd)
    
    def savedef(self):
        """savedef_exec函数的Python实现"""
        dot_config_dir = self.project_root / '.build' / 'cache'
        kconfig_catalog = "CatalogKconfig"
        dot_config = "using.config"
        app_default_config = self.project_root / 'app_default.config'
        boards_dir = self.sdk_root / 'boards'
        
        dot_config_dir.mkdir(parents=True, exist_ok=True)
        
        original_cwd = os.getcwd()
        os.chdir(dot_config_dir)
        
        try:
            dot_config_path = dot_config_dir / dot_config
            if not dot_config_path.exists():
                Logger.show(f"Warning: No file [{dot_config}].", Colors.YELLOW)
            
            # 设置目录配置
            subprocess.run([
                sys.executable,
                str(self.sdk_root / 'tools' / 'kconfiglib' / 'set_catalog_config.py'),
                '-b', str(boards_dir),
                '-s', str(self.sdk_root / 'src'),
                '-a', str(self.project_root),
                '-o', kconfig_catalog
            ], check=True)
            
            # 保存默认配置
            subprocess.run([
                sys.executable,
                str(self.sdk_root / 'tools' / 'kconfiglib' / 'savedefconfig.py'),
                '--kconfig', kconfig_catalog,
                '--out', str(app_default_config),
                '--dconfig', dot_config
            ], check=True)
            
        finally:
            os.chdir(original_cwd)
    
    def new(self, template_name: Optional[str] = None):
        """new_exec函数的Python实现"""
        if not template_name:
            template_name = "base"
        
        template_path = self.sdk_root / 'tools' / 'app_template' / template_name
        if not template_path.exists():
            Logger.show(f"No template named [{template_name}] exists.", Colors.RED)
            sys.exit(1)
        
        proj_name = input("Input project name: ").strip()
        if not proj_name:
            Logger.show("Project name is empty.", Colors.YELLOW)
            sys.exit(1)
        
        target_path = self.project_root / proj_name
        if target_path.exists():
            Logger.show(f"[{proj_name}] already exists, rename new project or delete the directory.", Colors.YELLOW)
            sys.exit(1)
        
        shutil.copytree(template_path, target_path)
        Logger.show("You can use [tos menuconfig] to configure the project.", Colors.GREEN)
    
    def set_example(self, platform_name: Optional[str] = None):
        """set_example_exec函数的Python实现"""
        now_platform_file = self.sdk_root / '.set_example'
        now_platform = "None"
        if now_platform_file.exists():
            now_platform = now_platform_file.read_text().strip()
        
        Logger.show(f"Now used: {now_platform}", Colors.GREEN)
        
        platforms_dir = self.sdk_root / 'platform'
        
        if not platform_name:
            # 交互式选择平台
            platform_yaml = platforms_dir / 'platform_config.yaml'
            if not platform_yaml.exists():
                Logger.show("Platform config file not found", Colors.RED)
                sys.exit(1)
            
            with open(platform_yaml, 'r') as f:
                content = f.read()
            
            # 提取平台名称
            platform_names = re.findall(r'^- name: (.+)$', content, re.MULTILINE)
            if not platform_names:
                Logger.show("No platforms found", Colors.RED)
                sys.exit(1)
            
            print("========================")
            print("Platforms")
            for i, name in enumerate(platform_names, 1):
                print(f"  {i}. {name}")
            print("------------------------")
            
            try:
                choice = int(input("Please select: ")) - 1
                print("------------------------")
                if 0 <= choice < len(platform_names):
                    platform_name = platform_names[choice]
                else:
                    Logger.error("Invalid selection")
                    sys.exit(1)
            except (ValueError, KeyboardInterrupt):
                Logger.error("Selection cancelled")
                sys.exit(1)
        
        if not platform_name:
            Logger.show("Can't find the [platform name].", Colors.RED)
            sys.exit(1)
        
        platform_path = platforms_dir / platform_name
        if not platform_path.exists():
            if not self.platform_manager.download_platform_by_name(platform_name):
                Logger.show(f"Download platform [{platform_name}] failed.", Colors.RED)
                sys.exit(1)
        
        root_example_dir = self.sdk_root / 'examples'
        platform_example_dir = platform_path / 'examples'
        
        # 删除现有的examples链接
        if root_example_dir.exists():
            if root_example_dir.is_symlink():
                root_example_dir.unlink()
            else:
                shutil.rmtree(root_example_dir)
        
        if not platform_example_dir.exists():
            Logger.show(f"[{platform_name}] has no examples.", Colors.RED)
            sys.exit(1)
        
        # 创建符号链接
        root_example_dir.symlink_to(platform_example_dir)
        
        note = f"""
Set [{platform_name}] example success.
Please check {root_example_dir}
"""
        Logger.show(note, Colors.GREEN)
        now_platform_file.write_text(platform_name)
    
    def new_platform(self, platform_name: Optional[str] = None):
        """new_platform_exec函数的Python实现"""
        if not platform_name:
            Logger.show("new_platform command need an argument [PLATFORM_NAME]", Colors.YELLOW)
            sys.exit(1)
        
        kernel_porting_script = self.sdk_root / 'tools' / 'kernel_porting.sh'
        if kernel_porting_script.exists():
            subprocess.run(['bash', str(kernel_porting_script), platform_name], check=True)
        else:
            Logger.show("kernel_porting.sh script not found", Colors.RED)
    
    def _download_tyutool(self):
        """下载tyutool工具"""
        download_script = self.sdk_root / 'tools' / 'tyutool' / 'download_tyutool.sh'
        tyutool_dir = self.sdk_root / 'tools' / 'tyutool'
        
        if not download_script.exists():
            Logger.show("Download script not found", Colors.RED)
            return False
        
        result = subprocess.run(['bash', str(download_script), str(tyutool_dir)])
        if result.returncode != 0:
            Logger.show("Error: Download tyutool failed.", Colors.RED)
            return False
        return True
    
    def flash(self, args: List[str]):
        """flash_exec函数的Python实现"""
        if not self._download_tyutool():
            sys.exit(1)
        
        tyutool = self.sdk_root / 'tools' / 'tyutool' / 'tyutool_cli'
        
        if args:
            Logger.show(f"tyutool params: {' '.join(args)}")
            Logger.show(f"{tyutool} {' '.join(args)}", Colors.GREEN)
            subprocess.run([str(tyutool)] + args)
            return
        
        # 检查配置文件
        if not self.config_manager.project_ini.exists():
            Logger.show(f"Not found [{self.config_manager.project_ini}].", Colors.YELLOW)
            Logger.show("You could run [tos menuconfig].", Colors.GREEN)
            return
        
        # 读取配置
        with open(self.config_manager.project_ini, 'r') as f:
            content = f.read()
        
        def extract_var(key: str) -> str:
            pattern = f'{key}="([^"]*)"'
            match = re.search(pattern, content)
            return match.group(1) if match else ""
        
        platform = extract_var('CONFIG_PLATFORM_CHOICE')
        board = extract_var('CONFIG_BOARD_CHOICE')
        chip = extract_var('CONFIG_CHIP_CHOICE')
        project_name = extract_var('CONFIG_PROJECT_NAME')
        project_version = extract_var('CONFIG_PROJECT_VERSION')
        
        device = chip if chip else platform
        baudrate_cfg = ""
        
        # 检查tyutool配置
        tyutool_cfg = self.sdk_root / 'boards' / platform / board / 'tyutool.cfg'
        if tyutool_cfg.exists():
            with open(tyutool_cfg, 'r') as f:
                cfg_content = f.read()
            
            flash_baudrate_match = re.search(r'flash_baudrate="?([^"\s]+)"?', cfg_content)
            if flash_baudrate_match:
                baudrate_cfg = f"-b {flash_baudrate_match.group(1)}"
        
        bin_file = self.project_root / '.build' / 'bin' / f'{project_name}_QIO_{project_version}.bin'
        cmd = f"{tyutool} write -d {device} -f {bin_file} {baudrate_cfg}".strip()
        Logger.show(cmd, Colors.GREEN)
        subprocess.run(cmd, shell=True)
    
    def monitor(self, args: List[str]):
        """monitor_exec函数的Python实现"""
        if not self._download_tyutool():
            sys.exit(1)
        
        tyutool = self.sdk_root / 'tools' / 'tyutool' / 'tyutool_cli'
        
        if args:
            Logger.show(f"tyutool params: {' '.join(args)}")
            cmd = f"{tyutool} monitor {' '.join(args)}"
            Logger.show(cmd, Colors.GREEN)
            subprocess.run(cmd, shell=True)
            return
        
        # 检查配置文件
        if not self.config_manager.project_ini.exists():
            Logger.show(f"Not found [{self.config_manager.project_ini}].", Colors.YELLOW)
            Logger.show("You could run [tos menuconfig].", Colors.GREEN)
            return
        
        # 读取配置
        with open(self.config_manager.project_ini, 'r') as f:
            content = f.read()
        
        def extract_var(key: str) -> str:
            pattern = f'{key}="([^"]*)"'
            match = re.search(pattern, content)
            return match.group(1) if match else ""
        
        platform = extract_var('CONFIG_PLATFORM_CHOICE')
        board = extract_var('CONFIG_BOARD_CHOICE')
        chip = extract_var('CONFIG_CHIP_CHOICE')
        
        device = chip if chip else platform
        baudrate_cfg = ""
        
        # 检查tyutool配置
        tyutool_cfg = self.sdk_root / 'boards' / platform / board / 'tyutool.cfg'
        if tyutool_cfg.exists():
            with open(tyutool_cfg, 'r') as f:
                cfg_content = f.read()
            
            monitor_baudrate_match = re.search(r'monitor_baudrate="?([^"\s]+)"?', cfg_content)
            if monitor_baudrate_match:
                baudrate_cfg = f"-b {monitor_baudrate_match.group(1)}"
        
        cmd = f"{tyutool} monitor -d {device} {baudrate_cfg}".strip()
        Logger.show(cmd, Colors.GREEN)
        subprocess.run(cmd, shell=True)
    
    def update(self):
        """update_exec函数的Python实现"""
        Logger.show("Updating platform ...")
        
        platforms_dir = self.sdk_root / 'platform'
        platform_yaml = platforms_dir / 'platform_config.yaml'
        
        if not platform_yaml.exists():
            Logger.show("Platform config file not found", Colors.RED)
            return
        
        with open(platform_yaml, 'r') as f:
            content = f.read()
        
        # 提取所有平台名称
        platform_names = re.findall(r'^- name: (.+)$', content, re.MULTILINE)
        
        for name in platform_names:
            # 获取平台信息
            lines = content.split('\n')
            platform_info_lines = []
            flag = False
            
            for line in lines:
                if re.match(f'^- name: {re.escape(name)}\\s*$', line):
                    flag = True
                    continue
                elif re.match('^- name:.*$', line):
                    flag = False
                elif flag:
                    platform_info_lines.append(line)
            
            platform_info = '\n'.join(platform_info_lines)
            
            # 提取仓库信息
            repo_match = re.search(r'repo: (.+)', platform_info)
            branch_match = re.search(r'branch: (.+)', platform_info)
            commit_match = re.search(r'commit: (.+)', platform_info)
            
            if not repo_match or not commit_match:
                continue
            
            platform_path = platforms_dir / name
            if not platform_path.exists():
                continue
            
            original_cwd = os.getcwd()
            os.chdir(platform_path)
            
            try:
                # 如果有分支信息，先切换到分支
                if branch_match:
                    branch = branch_match.group(1).strip()
                    subprocess.run(['git', 'checkout', '-q', branch], check=False)
                    subprocess.run(['git', 'pull'], check=False)
                
                # 切换到指定提交
                commit = commit_match.group(1).strip()
                result = subprocess.run(['git', 'checkout', '-q', commit], capture_output=True)
                
                if result.returncode == 0:
                    Logger.show(f"{name} success.", Colors.GREEN)
                else:
                    Logger.show(f"{name} failed.", Colors.RED)
                    
            finally:
                os.chdir(original_cwd)
    
    def help(self):
        """help_exec函数的Python实现"""
        open_build = Path(__file__).name.replace('.py', '')
        note = f"""
Usage: {open_build} COMMAND [ARGS]...

Commands:
    version       - Show TOS version
    check         - Check command and version
    new           - New project [base(default) / arduino]
    build         - Build project
    flash         - Flash project
    monitor       - Display serial log
    clean         - Clean project
    fullclean     - Clean project and delete build path
    menuconfig    - Configuration project features
    savedef       - Saves minimal configuration file (app_default.config)
    config_choice - Select the file in the config directory instead of app_default.config
    set_example   - Set examples from platform
    new_platform  - New platform [platform_name]
    update        - Update the platforms according to the platform_config.yaml
    help          - Help information
"""
        Logger.show(note)
    
    def check(self):
        """check_exec函数的Python实现"""
        Logger.show("Check command and version ...")
        
        if not SystemChecker.check_base_tool():
            Logger.show("Error: Check the required tools and versions.", Colors.RED)
            sys.exit(1)
        
        self.submodule_manager.check_submodules()
        
        # 检查tos工具是否在PATH中
        try:
            subprocess.run(['which', 'tos'], capture_output=True, check=True)
        except subprocess.CalledProcessError:
            Logger.show("Select a way to configure the OpenSDK tool:", Colors.YELLOW)
            Logger.show(f"1. export PATH=$PATH:{self.sdk_root}", Colors.YELLOW)
            Logger.show("2. set step1 to .bashrc / .zshrc / .profile", Colors.YELLOW)


def main():
    """主函数"""
    parser = argparse.ArgumentParser(description='TuyaOpen构建工具 (Python版本)')
    parser.add_argument('command', help='要执行的命令')
    parser.add_argument('args', nargs='*', help='命令参数')
    
    if len(sys.argv) < 2:
        parser.print_help()
        sys.exit(1)
    
    args = parser.parse_args()
    
    try:
        builder = TuyaOpenBuilder()
    except Exception as e:
        Logger.error(f"初始化失败: {e}")
        sys.exit(1)
    
    # 执行对应命令，按照bash版本的case语句
    command = args.command.lower()
    
    try:
        if command == 'version':
            builder.version()
        elif command == 'config_choice':
            config_path = args.args[0] if args.args else None
            builder.config_choice(config_path)
        elif command == 'build':
            builder.build()
        elif command == 'clean':
            builder.clean()
        elif command == 'fullclean':
            builder.fullclean()
        elif command == 'menuconfig':
            builder.menuconfig()
        elif command == 'savedef':
            builder.savedef()
        elif command == 'new':
            template_name = args.args[0] if args.args else None
            builder.new(template_name)
        elif command == 'set_example':
            platform_name = args.args[0] if args.args else None
            builder.set_example(platform_name)
        elif command == 'new_platform':
            platform_name = args.args[0] if args.args else None
            builder.new_platform(platform_name)
        elif command == 'flash':
            builder.flash(args.args)
        elif command == 'monitor':
            builder.monitor(args.args)
        elif command == 'update':
            builder.update()
        elif command == 'help':
            builder.help()
        elif command == 'check':
            builder.check()
        else:
            Logger.show(f"Unknown command [{command}]", Colors.YELLOW)
            builder.help()
    except KeyboardInterrupt:
        Logger.show("用户取消操作")
    except Exception as e:
        Logger.error(f"执行失败: {e}")
        if os.getenv('TOS_DEBUG') == '1':
            import traceback
            traceback.print_exc()
        sys.exit(1)


if __name__ == '__main__':
    main() 