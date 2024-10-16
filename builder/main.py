import os
import shutil
import subprocess
import configparser
from SCons.Script import (
    COMMAND_LINE_TARGETS,
    Default,
    DefaultEnvironment,
    Environment,
)


print(f"COMMAND_LINE_TARGETS: {COMMAND_LINE_TARGETS}")

env: Environment = DefaultEnvironment(ENV=os.environ)
platform = env.PioPlatform()
board = env.BoardConfig()
platform_path = platform.get_dir()
project_path = env.subst("$PROJECT_DIR")

# print(f"{env.Dictionary()}")  # all
print(f"platform: {platform}")
print(f"board: {board}")
print(f"platform_path: {platform_path}")
print(f"project_path: {project_path}")

cmake_bin_path = os.path.join(platform.get_package_dir("tool-cmake"), "bin")
env["ENV"]["PATH"] = os.pathsep.join([cmake_bin_path, env["ENV"]["PATH"]])

TOS_PATH = os.path.join(platform_path, "tos")


def do_command(cmd, cwd=None):
    print(f"do_command: [{cmd}] in [{cwd}]")
    process = subprocess.Popen(cmd, cwd=cwd, shell=True)
    stdout, stderr = process.communicate()


# init
def project_init(target, source, env):
    framework = env.subst("$PIOFRAMEWORK")
    template_dir = os.path.join(platform_path, "tools", "app_template",
                                framework)
    print(f"template_dir: {template_dir}")
    if not os.path.isdir(template_dir):
        print("Template path is not exists.")
        return
    for root, _, files in os.walk(template_dir):
        for fil in files:
            source_file_path = os.path.join(root, fil)
            ralative_path = os.path.relpath(source_file_path, template_dir)
            target_file_path = os.path.join(project_path, ralative_path)
            target_file_dir = os.path.dirname(target_file_path)
            os.makedirs(target_file_dir, exist_ok=True)
            if not os.path.exists(target_file_path):
                shutil.copyfile(source_file_path, target_file_path)
    pass


# add new project to project_build.ini
def add_new_to_ini():
    ini = os.path.join(project_path, "project_build.ini")
    pioenv = env.subst("$PIOENV")
    platform = board.get("build.tuya_platform", "t2")
    framework = env.subst("$PIOFRAMEWORK")

    config = configparser.ConfigParser()
    config.read(ini)
    config[f"project:{pioenv}"] = {
        "platform": platform,
        "framework": framework,
    }

    with open(ini, 'w') as f:
        config.write(f)
    pass


# tos_build
def tos_build_action(target, source, env):
    add_new_to_ini()

    pioenv = env.subst("$PIOENV")
    cmd = f"{TOS_PATH} build {pioenv}"
    do_command(cmd, project_path)
    pass


# tos_clean
def tos_clean_action(target, source, env):
    pioenv = env.subst("$PIOENV")
    cmd = f"{TOS_PATH} clean {pioenv}"
    do_command(cmd, project_path)
    pass


# tos_menuconfig
def tos_menuconfig_action(target, source, env):
    add_new_to_ini()

    # cmd = f"{TOS_PATH} menuconfig"
    # do_command(cmd, project_path)
    pioenv = env.subst("$PIOENV")
    print(f"\033[32mPlease Run: {TOS_PATH} menuconfig {pioenv}\033[0m")
    pass


# debug action
def debug_action(target, source, env):
    cmd = "cmake --version"
    do_command(cmd, project_path)


tos_build_target = env.AddPlatformTarget("tos_build", None,
                                         tos_build_action, "Default")
env.AddPlatformTarget("tos_init", None, project_init, "Project init")
env.AddPlatformTarget("tos_clean", None, tos_clean_action, "Clean")
env.AddPlatformTarget("tos_menuconfig", None,
                      tos_menuconfig_action, "Menuconfig")
# env.AddPlatformTarget("debug", None, debug_action, "Debug")

Default([tos_build_target])
