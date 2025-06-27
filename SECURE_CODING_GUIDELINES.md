# TuyaOpen 安全编码规范

## 概述

本文档为 TuyaOpen 项目提供安全编码规范和最佳实践，旨在防止常见的安全漏洞，提高代码质量和系统安全性。

## 1. 内存管理安全

### 1.1 内存分配

**推荐做法：**
```c
// 使用安全的内存分配函数
void *ptr = tal_malloc(size);
if (!ptr) {
    // 处理分配失败
    return OPRT_MALLOC_FAILED;
}

// 初始化分配的内存（tal_malloc已自动初始化为0）
// 或使用 tal_calloc 进行零初始化
void *ptr = tal_calloc(count, size);
```

**避免的做法：**
```c
// 不检查返回值
void *ptr = tal_malloc(size);  // 危险：未检查NULL
*ptr = value;  // 可能导致段错误

// 分配过大的内存
void *ptr = tal_malloc(SIZE_MAX);  // 危险：可能导致整数溢出
```

### 1.2 内存释放

**推荐做法：**
```c
// 使用安全释放函数
tal_free_safe((void**)&ptr);  // 自动设置ptr为NULL

// 或者手动设置为NULL
tal_free(ptr);
ptr = NULL;
```

**避免的做法：**
```c
// 双重释放
tal_free(ptr);
tal_free(ptr);  // 危险：双重释放

// 释放后继续使用
tal_free(ptr);
*ptr = value;  // 危险：使用已释放的内存
```

## 2. 字符串处理安全

### 2.1 字符串复制

**推荐做法：**
```c
// 使用安全的字符串复制函数
char dest[MAX_SIZE];
strncpy(dest, src, MAX_SIZE - 1);
dest[MAX_SIZE - 1] = '\0';  // 确保字符串终止

// 或使用安全包装函数
hex2str_safe(dest, sizeof(dest), src, len);
```

**避免的做法：**
```c
// 使用不安全的函数
strcpy(dest, src);  // 危险：可能导致缓冲区溢出
sprintf(dest, "%s", src);  // 危险：无长度检查
```

### 2.2 字符串格式化

**推荐做法：**
```c
// 使用安全的格式化函数
char buffer[256];
int ret = snprintf(buffer, sizeof(buffer), "Value: %d", value);
if (ret >= sizeof(buffer)) {
    // 处理截断情况
    buffer[sizeof(buffer) - 1] = '\0';
}
```

**避免的做法：**
```c
// 使用不安全的格式化函数
sprintf(buffer, "Value: %d", value);  // 危险：无长度检查
```

## 3. 输入验证

### 3.1 参数验证

**推荐做法：**
```c
OPERATE_RET function_name(const char *input, size_t input_len, char *output, size_t output_size)
{
    // 验证输入参数
    if (!input || !output) {
        return OPRT_INVALID_PARM;
    }
    
    if (input_len == 0 || output_size == 0) {
        return OPRT_INVALID_PARM;
    }
    
    if (input_len >= output_size) {
        return OPRT_INVALID_PARM;
    }
    
    // 处理逻辑
    return OPRT_OK;
}
```

### 3.2 边界检查

**推荐做法：**
```c
// 数组访问前检查边界
if (index >= 0 && index < array_size) {
    value = array[index];
} else {
    // 处理越界情况
    return OPRT_INDEX_OUT_OF_RANGE;
}

// 循环中的边界检查
for (int i = 0; i < count && i < MAX_COUNT; i++) {
    // 安全的循环操作
}
```

## 4. 错误处理

### 4.1 统一错误处理

**推荐做法：**
```c
OPERATE_RET function_name(void)
{
    OPERATE_RET ret = OPRT_OK;
    void *ptr = NULL;
    
    ptr = tal_malloc(size);
    if (!ptr) {
        ret = OPRT_MALLOC_FAILED;
        goto error_exit;
    }
    
    // 主要逻辑
    ret = some_operation(ptr);
    if (ret != OPRT_OK) {
        goto error_exit;
    }
    
    tal_free_safe((void**)&ptr);
    return OPRT_OK;
    
error_exit:
    if (ptr) {
        tal_free_safe((void**)&ptr);
    }
    return ret;
}
```

### 4.2 资源清理

**推荐做法：**
```c
// 使用RAII模式或确保资源清理
void cleanup_function(void)
{
    if (resource1) {
        release_resource1(resource1);
        resource1 = NULL;
    }
    
    if (resource2) {
        release_resource2(resource2);
        resource2 = NULL;
    }
}
```

## 5. 安全宏定义

### 5.1 输入验证宏

```c
// 指针验证宏
#define VALIDATE_POINTER(ptr) \
    do { \
        if (!(ptr)) { \
            PR_ERR("Invalid pointer: %s", #ptr); \
            return OPRT_INVALID_PARM; \
        } \
    } while(0)

// 缓冲区大小验证宏
#define VALIDATE_BUFFER_SIZE(buf, size, min_size) \
    do { \
        if (!(buf) || (size) < (min_size)) { \
            PR_ERR("Invalid buffer or size"); \
            return OPRT_INVALID_PARM; \
        } \
    } while(0)

// 数组边界检查宏
#define CHECK_ARRAY_BOUNDS(index, max_size) \
    do { \
        if ((index) < 0 || (index) >= (max_size)) { \
            PR_ERR("Array index out of bounds: %d", (index)); \
            return OPRT_INDEX_OUT_OF_RANGE; \
        } \
    } while(0)
```

### 5.2 安全内存操作宏

```c
// 安全内存分配宏
#define SAFE_MALLOC(ptr, size) \
    do { \
        (ptr) = tal_malloc(size); \
        if (!(ptr)) { \
            PR_ERR("Memory allocation failed: size=%zu", (size_t)(size)); \
            return OPRT_MALLOC_FAILED; \
        } \
    } while(0)

// 安全内存释放宏
#define SAFE_FREE(ptr) \
    do { \
        if (ptr) { \
            tal_free(ptr); \
            (ptr) = NULL; \
        } \
    } while(0)
```

## 6. 线程安全

### 6.1 互斥锁使用

**推荐做法：**
```c
static MUTEX_HANDLE mutex = NULL;

OPERATE_RET thread_safe_function(void)
{
    OPERATE_RET ret = OPRT_OK;
    
    ret = tal_mutex_lock(mutex);
    if (ret != OPRT_OK) {
        return ret;
    }
    
    // 临界区代码
    
    tal_mutex_unlock(mutex);
    return OPRT_OK;
}
```

### 6.2 避免竞态条件

**推荐做法：**
```c
// 使用原子操作或适当的同步机制
static volatile bool initialized = false;
static MUTEX_HANDLE init_mutex = NULL;

OPERATE_RET initialize_once(void)
{
    if (initialized) {
        return OPRT_OK;
    }
    
    tal_mutex_lock(init_mutex);
    if (!initialized) {
        // 初始化代码
        initialized = true;
    }
    tal_mutex_unlock(init_mutex);
    
    return OPRT_OK;
}
```

## 7. 网络安全

### 7.1 输入数据验证

**推荐做法：**
```c
OPERATE_RET process_network_data(const uint8_t *data, size_t data_len)
{
    // 验证数据长度
    if (!data || data_len == 0 || data_len > MAX_PACKET_SIZE) {
        return OPRT_INVALID_PARM;
    }
    
    // 验证数据格式
    if (data[0] != EXPECTED_HEADER) {
        return OPRT_INVALID_FORMAT;
    }
    
    // 处理数据
    return OPRT_OK;
}
```

### 7.2 缓冲区管理

**推荐做法：**
```c
// 使用固定大小的缓冲区
#define MAX_BUFFER_SIZE 1024

OPERATE_RET safe_copy_data(const uint8_t *src, size_t src_len, uint8_t *dst, size_t dst_size)
{
    if (!src || !dst || src_len == 0 || dst_size == 0) {
        return OPRT_INVALID_PARM;
    }
    
    if (src_len > dst_size) {
        return OPRT_BUFFER_TOO_SMALL;
    }
    
    memcpy(dst, src, src_len);
    return OPRT_OK;
}
```

## 8. 代码审查检查清单

### 8.1 内存安全检查
- [ ] 所有 malloc/calloc 调用都检查返回值
- [ ] 所有指针在使用前都进行NULL检查
- [ ] 所有动态分配的内存都有对应的释放
- [ ] 没有双重释放或使用已释放的内存
- [ ] 数组访问都有边界检查

### 8.2 字符串安全检查
- [ ] 使用安全的字符串函数（strncpy, snprintf等）
- [ ] 字符串操作都有长度限制
- [ ] 字符串都正确终止（'\0'）
- [ ] 格式化字符串不包含用户输入

### 8.3 输入验证检查
- [ ] 所有外部输入都进行验证
- [ ] 函数参数都进行有效性检查
- [ ] 数值范围都进行检查
- [ ] 缓冲区大小都进行验证

### 8.4 错误处理检查
- [ ] 所有函数调用都检查返回值
- [ ] 错误路径都有适当的清理代码
- [ ] 资源泄漏都得到防止
- [ ] 错误信息不泄露敏感信息

## 9. 工具和自动化

### 9.1 静态分析工具
- **Clang Static Analyzer**: 检测内存泄漏、空指针解引用等
- **Cppcheck**: 检测编程错误和安全问题
- **PC-lint**: 商业静态分析工具
- **SonarQube**: 代码质量和安全分析平台

### 9.2 动态分析工具
- **Valgrind**: 内存错误检测（Linux）
- **AddressSanitizer**: 地址错误检测
- **MemorySanitizer**: 未初始化内存检测
- **ThreadSanitizer**: 线程安全问题检测

### 9.3 模糊测试
- **AFL (American Fuzzy Lop)**: 通用模糊测试工具
- **libFuzzer**: LLVM的模糊测试引擎
- **OSS-Fuzz**: Google的开源模糊测试平台

## 10. 培训和意识

### 10.1 开发者培训
- 定期进行安全编码培训
- 分享安全漏洞案例研究
- 建立安全编码知识库
- 进行代码审查培训

### 10.2 持续改进
- 定期更新安全编码规范
- 收集和分析安全问题
- 改进开发流程和工具
- 建立安全反馈机制

---

**注意**: 本规范应该定期更新，以反映最新的安全威胁和最佳实践。所有开发者都应该熟悉并遵循这些规范。