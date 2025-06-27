#!/bin/bash

# TuyaOpen 安全代码检查脚本
# 用于自动化检测常见的安全问题

set -e

# 颜色定义
RED='\033[0;31m'
YELLOW='\033[1;33m'
GREEN='\033[0;32m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# 配置
SRC_DIR="src"
REPORT_DIR="security_reports"
TIMESTAMP=$(date +"%Y%m%d_%H%M%S")
REPORT_FILE="${REPORT_DIR}/security_report_${TIMESTAMP}.txt"

# 创建报告目录
mkdir -p "$REPORT_DIR"

echo -e "${BLUE}=== TuyaOpen 安全代码检查 ===${NC}"
echo "开始时间: $(date)"
echo "源码目录: $SRC_DIR"
echo "报告文件: $REPORT_FILE"
echo ""

# 初始化报告文件
cat > "$REPORT_FILE" << EOF
TuyaOpen 安全代码检查报告
生成时间: $(date)
源码目录: $SRC_DIR

=== 检查摘要 ===
EOF

# 检查函数
check_unsafe_functions() {
    echo -e "${YELLOW}检查不安全函数使用...${NC}"
    echo "\n=== 不安全函数使用检查 ===" >> "$REPORT_FILE"
    
    local unsafe_functions=(
        "strcpy"
        "strcat"
        "sprintf"
        "vsprintf"
        "gets"
        "scanf"
        "sscanf"
        "fscanf"
    )
    
    local found_issues=0
    
    for func in "${unsafe_functions[@]}"; do
        echo "检查函数: $func"
        local results=$(find "$SRC_DIR" -name "*.c" -o -name "*.h" | xargs grep -n "\b$func\s*(") || true
        
        if [ -n "$results" ]; then
            echo "\n发现不安全函数 $func:" >> "$REPORT_FILE"
            echo "$results" >> "$REPORT_FILE"
            found_issues=$((found_issues + 1))
            echo -e "  ${RED}发现 $func 使用${NC}"
        fi
    done
    
    if [ $found_issues -eq 0 ]; then
        echo -e "  ${GREEN}未发现不安全函数使用${NC}"
        echo "未发现不安全函数使用" >> "$REPORT_FILE"
    fi
    
    return $found_issues
}

check_buffer_operations() {
    echo -e "${YELLOW}检查缓冲区操作...${NC}"
    echo "\n=== 缓冲区操作检查 ===" >> "$REPORT_FILE"
    
    local found_issues=0
    
    # 检查可能的缓冲区溢出
    echo "检查 strncpy 使用..."
    local strncpy_results=$(find "$SRC_DIR" -name "*.c" | xargs grep -n "strncpy" | grep -v "\[.*-.*1\].*=.*'\\0'" || true)
    
    if [ -n "$strncpy_results" ]; then
        echo "\n可能的 strncpy 问题:" >> "$REPORT_FILE"
        echo "$strncpy_results" >> "$REPORT_FILE"
        found_issues=$((found_issues + 1))
        echo -e "  ${RED}发现可能的 strncpy 问题${NC}"
    fi
    
    # 检查 snprintf 返回值检查
    echo "检查 snprintf 返回值处理..."
    local snprintf_results=$(find "$SRC_DIR" -name "*.c" | xargs grep -A 3 -B 1 "snprintf" | grep -v "if.*>=\|if.*<" || true)
    
    if [ -n "$snprintf_results" ]; then
        echo "\n可能未检查 snprintf 返回值:" >> "$REPORT_FILE"
        echo "$snprintf_results" >> "$REPORT_FILE"
        found_issues=$((found_issues + 1))
        echo -e "  ${YELLOW}发现可能未检查 snprintf 返回值${NC}"
    fi
    
    if [ $found_issues -eq 0 ]; then
        echo -e "  ${GREEN}缓冲区操作检查通过${NC}"
        echo "缓冲区操作检查通过" >> "$REPORT_FILE"
    fi
    
    return $found_issues
}

check_memory_management() {
    echo -e "${YELLOW}检查内存管理...${NC}"
    echo "\n=== 内存管理检查 ===" >> "$REPORT_FILE"
    
    local found_issues=0
    
    # 检查 malloc 后是否检查返回值
    echo "检查 malloc 返回值检查..."
    local malloc_files=$(find "$SRC_DIR" -name "*.c" | xargs grep -l "malloc\|calloc" || true)
    
    for file in $malloc_files; do
        # 查找 malloc/calloc 调用后没有立即检查返回值的情况
        local unchecked_malloc=$(grep -n -A 3 "malloc\|calloc" "$file" | grep -B 3 -A 1 "=.*malloc\|=.*calloc" | grep -v "if\|NULL\|!" || true)
        
        if [ -n "$unchecked_malloc" ]; then
            echo "\n可能未检查 malloc/calloc 返回值 ($file):" >> "$REPORT_FILE"
            echo "$unchecked_malloc" >> "$REPORT_FILE"
            found_issues=$((found_issues + 1))
        fi
    done
    
    # 检查可能的内存泄漏
    echo "检查可能的内存泄漏..."
    local malloc_count=$(find "$SRC_DIR" -name "*.c" | xargs grep -c "malloc\|calloc" | awk -F: '{sum += $2} END {print sum}' || echo "0")
    local free_count=$(find "$SRC_DIR" -name "*.c" | xargs grep -c "free" | awk -F: '{sum += $2} END {print sum}' || echo "0")
    
    echo "\nMalloc/Calloc 调用次数: $malloc_count" >> "$REPORT_FILE"
    echo "Free 调用次数: $free_count" >> "$REPORT_FILE"
    
    if [ "$malloc_count" -gt "$free_count" ]; then
        echo "\n警告: malloc/calloc 调用次数多于 free 调用次数，可能存在内存泄漏" >> "$REPORT_FILE"
        found_issues=$((found_issues + 1))
        echo -e "  ${YELLOW}可能存在内存泄漏${NC}"
    fi
    
    if [ $found_issues -eq 0 ]; then
        echo -e "  ${GREEN}内存管理检查通过${NC}"
        echo "内存管理检查通过" >> "$REPORT_FILE"
    fi
    
    return $found_issues
}

check_null_pointer_dereference() {
    echo -e "${YELLOW}检查空指针解引用...${NC}"
    echo "\n=== 空指针解引用检查 ===" >> "$REPORT_FILE"
    
    local found_issues=0
    
    # 检查函数参数是否进行空指针检查
    echo "检查函数参数空指针检查..."
    local functions_with_pointers=$(find "$SRC_DIR" -name "*.c" | xargs grep -n "^[a-zA-Z_][a-zA-Z0-9_]*.*\*.*[a-zA-Z_][a-zA-Z0-9_]*.*{" || true)
    
    if [ -n "$functions_with_pointers" ]; then
        echo "\n发现使用指针参数的函数:" >> "$REPORT_FILE"
        echo "$functions_with_pointers" >> "$REPORT_FILE"
        echo "\n建议检查这些函数是否进行了适当的空指针检查" >> "$REPORT_FILE"
        found_issues=$((found_issues + 1))
        echo -e "  ${YELLOW}发现使用指针参数的函数，建议检查空指针验证${NC}"
    fi
    
    if [ $found_issues -eq 0 ]; then
        echo -e "  ${GREEN}空指针检查通过${NC}"
        echo "空指针检查通过" >> "$REPORT_FILE"
    fi
    
    return $found_issues
}

check_integer_overflow() {
    echo -e "${YELLOW}检查整数溢出...${NC}"
    echo "\n=== 整数溢出检查 ===" >> "$REPORT_FILE"
    
    local found_issues=0
    
    # 检查可能的整数溢出
    echo "检查乘法操作..."
    local multiplication_results=$(find "$SRC_DIR" -name "*.c" | xargs grep -n "\*.*malloc\|malloc.*\*" || true)
    
    if [ -n "$multiplication_results" ]; then
        echo "\n发现乘法与 malloc 结合使用:" >> "$REPORT_FILE"
        echo "$multiplication_results" >> "$REPORT_FILE"
        echo "\n建议检查是否存在整数溢出风险" >> "$REPORT_FILE"
        found_issues=$((found_issues + 1))
        echo -e "  ${YELLOW}发现乘法与 malloc 结合使用，建议检查整数溢出${NC}"
    fi
    
    if [ $found_issues -eq 0 ]; then
        echo -e "  ${GREEN}整数溢出检查通过${NC}"
        echo "整数溢出检查通过" >> "$REPORT_FILE"
    fi
    
    return $found_issues
}

check_format_string_vulnerabilities() {
    echo -e "${YELLOW}检查格式化字符串漏洞...${NC}"
    echo "\n=== 格式化字符串漏洞检查 ===" >> "$REPORT_FILE"
    
    local found_issues=0
    
    # 检查可能的格式化字符串漏洞
    echo "检查 printf 系列函数..."
    local printf_results=$(find "$SRC_DIR" -name "*.c" | xargs grep -n "printf.*%.*,.*[a-zA-Z_]" || true)
    
    if [ -n "$printf_results" ]; then
        echo "\n发现 printf 系列函数使用变量作为格式字符串:" >> "$REPORT_FILE"
        echo "$printf_results" >> "$REPORT_FILE"
        found_issues=$((found_issues + 1))
        echo -e "  ${YELLOW}发现可能的格式化字符串问题${NC}"
    fi
    
    if [ $found_issues -eq 0 ]; then
        echo -e "  ${GREEN}格式化字符串检查通过${NC}"
        echo "格式化字符串检查通过" >> "$REPORT_FILE"
    fi
    
    return $found_issues
}

generate_summary() {
    echo -e "${BLUE}生成检查摘要...${NC}"
    
    local total_issues=$1
    
    cat >> "$REPORT_FILE" << EOF

=== 检查完成 ===
总计发现问题: $total_issues
检查完成时间: $(date)

=== 建议 ===
1. 使用静态分析工具进行更深入的检查
2. 进行代码审查
3. 使用动态分析工具进行运行时检查
4. 定期更新安全编码规范

=== 修复优先级 ===
高优先级: 缓冲区溢出、格式化字符串漏洞
中优先级: 内存泄漏、空指针解引用
低优先级: 代码规范问题
EOF

    echo ""
    echo -e "${BLUE}=== 检查完成 ===${NC}"
    echo "总计发现问题: $total_issues"
    echo "详细报告: $REPORT_FILE"
    
    if [ $total_issues -eq 0 ]; then
        echo -e "${GREEN}恭喜！未发现明显的安全问题。${NC}"
    else
        echo -e "${RED}发现 $total_issues 个潜在安全问题，请查看详细报告。${NC}"
    fi
}

# 主检查流程
main() {
    if [ ! -d "$SRC_DIR" ]; then
        echo -e "${RED}错误: 源码目录 '$SRC_DIR' 不存在${NC}"
        exit 1
    fi
    
    local total_issues=0
    
    # 执行各项检查
    check_unsafe_functions
    total_issues=$((total_issues + $?))
    
    check_buffer_operations
    total_issues=$((total_issues + $?))
    
    check_memory_management
    total_issues=$((total_issues + $?))
    
    check_null_pointer_dereference
    total_issues=$((total_issues + $?))
    
    check_integer_overflow
    total_issues=$((total_issues + $?))
    
    check_format_string_vulnerabilities
    total_issues=$((total_issues + $?))
    
    # 生成摘要
    generate_summary $total_issues
    
    # 返回问题数量作为退出码
    if [ $total_issues -gt 255 ]; then
        exit 255
    else
        exit $total_issues
    fi
}

# 显示帮助信息
show_help() {
    cat << EOF
TuyaOpen 安全代码检查脚本

用法: $0 [选项]

选项:
  -h, --help     显示此帮助信息
  -d, --dir DIR  指定源码目录 (默认: src)
  -o, --output DIR 指定报告输出目录 (默认: security_reports)

示例:
  $0                    # 使用默认设置检查
  $0 -d /path/to/src    # 指定源码目录
  $0 -o /path/to/reports # 指定报告目录
EOF
}

# 解析命令行参数
while [[ $# -gt 0 ]]; do
    case $1 in
        -h|--help)
            show_help
            exit 0
            ;;
        -d|--dir)
            SRC_DIR="$2"
            shift 2
            ;;
        -o|--output)
            REPORT_DIR="$2"
            REPORT_FILE="${REPORT_DIR}/security_report_${TIMESTAMP}.txt"
            shift 2
            ;;
        *)
            echo -e "${RED}未知选项: $1${NC}"
            show_help
            exit 1
            ;;
    esac
done

# 运行主程序
main