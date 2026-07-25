# UniOS UGC 生态开发者指南

> 适用对象：希望在 UniOS 上开发、打包、分发第三方应用（UGC）的开发者。
> 版本：对应 UniOS 0.1.x 内核 ABI（syscall 1–19）。
> 阅读完本文，你将能：写出一个能在 UniOS 桌面上开窗口的 C 程序，把它打包成 `.upk` 安装包，理解 `.upk` 的本质就是 **tar（USTAR 归档）**，并掌握如何调用文件对话框和读写磁盘文件。

---

## 0. 一句话结论

**UPK 就是 tar。** 它不是一个私有格式，而是一个符合 POSIX USTAR（tar）规范的归档文件，里面固定放一个 `manifest` 文本清单 + 若干目录（`bin/`、`res/`、`lib/`）。你可以用系统的 `tar` 命令直接打、直接解、直接看内容。UniOS 内核里只需实现一个约 200 行的极简 USTAR 读取器即可完成安装。

---

## 1. UPK 格式详解

### 1.1 容器：USTAR（tar）

选择 tar/USTAR 的理由：

- 结构简单（每文件一个 512 字节头，无压缩），内核解析器代码量极小。
- 是开放标准，无专利风险；宿主机器上任何 `tar` 工具都能打包/解包。
- 与系统 initrd 可共用同一套解析代码。
- 未来如需压缩，只需在 tar 外层套一层 DEFLATE，格式字段已预留。

### 1.2 归档内部结构

```
myapp.upk                 # 本质：一个 .tar 文件
├── manifest              # 必需：纯文本 key=value 元数据
├── bin/                  # 可执行 ELF64 文件目录
│   └── myapp             # 主程序（路径须与 manifest 的 upk.entry 一致）
├── res/                  # 资源（图标 .svg、数据文件等）
└── lib/                  # 应用私有库（可选）
```

### 1.3 manifest 字段

`manifest` 是纯文本，每行一条 `key=value`，与 UniOS 的 locale（`.kv`）格式相同。

```
upk.format=1
upk.id=com.example.myapp
upk.name=My Application
upk.version=1.0.0
upk.arch=x86_64
upk.entry=bin/myapp
upk.exports=myapp,myapp-cli
upk.deps=
```

| 字段 | 必填 | 含义 |
|------|------|------|
| `upk.format` | 是 | UPK 格式版本，当前固定 `1`（为未来兼容预留）。 |
| `upk.id` | 是 | 反域名全局唯一 ID，也是安装后的目录名 `/apps/<id>`。 |
| `upk.name` | 是 | 人类可读的应用名（开始菜单显示用）。 |
| `upk.version` | 是 | 语义化版本 `major.minor.patch`。 |
| `upk.arch` | 是 | 目标架构，当前只接受 `x86_64`。 |
| `upk.entry` | 是 | 归档内主程序相对路径，如 `bin/myapp`。 |
| `upk.exports` | 否 | 逗号分隔的命令名列表，安装后可在 shell 直接敲。 |
| `upk.deps` | 否 | 依赖的其他 UPK id 列表，逗号分隔（首版允许为空）。 |

**ID 命名规范**：必须为反域名 style，且全小写、点分隔，例如 `com.blabbyco.calculator`、`io.github.user.hello`。它与 `/apps/` 下目录名严格一一对应，重复即冲突。

### 1.4 用 tar 验证「UPK 即 tar」

打包完成后，直接用任意 tar 工具检查（以官方示例 `calculator.upk` 为例）：

```bash
# 列出内容（证明它就是 tar）
tar -tf build/calculator.upk
# 输出类似：
#   manifest
#   bin/
#   bin/app
#   res/

# 解包查看 manifest
tar -xf build/calculator.upk -O manifest
```

能在你的 Linux/macOS/Windows(Git Bash) 上被 `tar` 正常列出与解出的文件，就是合格的 UPK。

### 1.5 安装后的磁盘布局

当包管理器 `upk install myapp.upk` 执行后：

```
/apps/com.example.myapp/
├── manifest        # manifest 副本
├── bin/
│   └── myapp       # ELF64 主程序
├── res/
│   └── icon.svg
└── .unimeta        # owner/group/mode

/etc/pkg/
├── db/
│   └── com.example.myapp   # 安装注册表记录（含文件清单、安装时间）
└── exports
    ├── myapp=com.example.myapp
    └── myapp-cli=com.example.myapp
```

### 1.6 命令解析顺序（shell 规范）

用户输入命令时，shell 按以下顺序解析：

1. shell 内建命令
2. `/bin`（系统核心命令）
3. `/etc/pkg/exports` 注册表中记录的应用命令名

即：装好一个导出 `myapp` 的包后，直接在 shell 敲 `myapp` 即可启动 `/apps/com.example.myapp/bin/myapp`。FAT32 不支持符号链接，因此不用 UNIX 的 link farm，而是用 `exports` 注册表完成「命令名 → ELF 路径」映射——这套机制在任何无符号链接能力的文件系统上都成立。

### 1.7 与 FAT32 的权限适配（重要约束）

UniOS 当前根文件系统是 **FAT32**，它本身没有 owner/group/权限位/符号链接。UNIX 权限语义由 VFS 层在内存中合成：

- 每个目录可带一个隐藏文本文件 `.unimeta`，记录本目录内各文件的 owner/group/mode。
- 缺 `.unimeta` 时默认值：`root:root`、目录 `0755`、文件 `0644`；`/bin` 与 `/apps` 下的 ELF 默认 `0755`。
- **系统设计中一律不依赖符号链接。** 你的应用也不要试图创建或读取符号链接。

---

## 2. 开发指南

### 2.1 工具链

| 组件 | 要求 |
|------|------|
| 编译器 | LLVM/Clang，target `x86_64-unknown-none-elf` |
| 链接器 | `ld.lld`（LLVM 的 LLD） |
| 扁平化 | `llvm-objcopy`（`-O binary`） |
| 打包 | 系统 `tar`，或官方 `tools/upk_pack.py`（Python 3） |
| 标准库 | `src/user/Uni.h` + `syscall.h`（极简内核 ABI，暂无完整 libc） |

项目根目录的 `build.sh` 已经用这套工具链把四个系统应用编译并打包，可直接参考。

### 2.2 标准项目目录结构

```
myapp/
├── manifest          # 必须，见 1.3
├── bin/
│   └── main.c        # 入口源文件（可多文件，最终编出一个 ELF）
├── res/              # 资源文件（可选）
└── lib/              # 私有库（可选）
```

### 2.3 程序入口与链接脚本

- **入口符号固定为 `_start`**，对应 `linker.ld` 的 `ENTRY(_start)`，不是 `main`。
- 不能使用 libc 的 `main`/`exit` 机制，进程退出请调用 `sys_exit()`。
- 应用运行在 ring3 用户态，加载地址在高端（`0x8000000000` 起），因此必须 `-mcmodel=large`。

**重要：`-mcmodel=large` 会把普通函数放进 `.ltext` section。**

如果 `linker.ld` 的输出段只写 `*(.text*)`，它不会匹配 `.ltext`，导致 `.ltext` 成为 orphan section 而被 lld 按内部优先级放到 offset 0，把 `_start` 挤到后面。内核加载时就会从错误的地址开始执行，产生 #PF。**正确的 linker.ld 写法：**

```ld
ENTRY(_start)

SECTIONS
{
    .text :
    {
        KEEP(*(.entry))   /* _start 必须在 .entry，确保在最前面 */
        *(.ltext)          /* -mcmodel=large 产生的普通函数 */
        *(.text*)          /* 普通 .text */
        *(.rodata*)
        *(.lrodata*)
        *(.srodata*)
        *(.data*)
        . = ALIGN(8);
        *(.bss*)
        *(.lbss*)
        *(.sbss*)
        *(COMMON)
        . = ALIGN(8);
    }
    /DISCARD/ : { *(.comment) *(.note) *(.eh_frame*) }
}
```

`_start` 必须放在 `.entry` section 并用 `noreturn` 标记（这是 `-mcmodel=large` 下 lld 的要求）：

```c
__attribute__((section(".entry"))) __attribute__((noreturn)) void _start(void)
{
    // ...
    sys_exit(0);
}
```

### 2.4 编译 / 链接 / 扁平化命令

以下命令直接取自 `build.sh`，对任意应用通用：

```bash
CLANG="clang --target=x86_64-unknown-none-elf"
LLD="ld.lld"

# 1) 编译（注意：freestanding / nostdlib / mcmodel=large / mno-red-zone）
$CLANG -ffreestanding -fno-stack-protector -fno-builtin -nostdlib \
    -mno-red-zone -mcmodel=large -O2 -std=c11 -Wall -Wextra \
    -Isrc/user \
    -c src/apps/<app>/bin/main.c -o build/<app>_u.o

# 2) 链接（--image-base 必须用高端地址；每个应用唯一）
$LLD --image-base=0x8000040000 -T src/user/linker.ld \
    -o build/<app>.elf -nostdlib build/<app>_u.o

# 3) 扁平化为纯二进制（内核加载器直接映射这段机器码）
llvm-objcopy -O binary build/<app>.elf build/<app>.bin
```

### 2.5 Uni.h API 参考（syscall 1–19）

应用只需 `#include "Uni.h"`。所有能力通过 `int $0x80` 系统调用进入内核，头文件里已经用内联汇编封装好，直接调用即可。

#### 基础系统调用（syscall.h，号 1–5）

```c
long sys_write(const char *buf, unsigned long len);   // 写串口日志
long sys_read(char *buf);                              // 读一个按键
long sys_yield(void);                                  // 主动让出 CPU
long sys_getpid(void);                                 // 取当前进程 ID
void sys_exit(int code);                               // 结束进程
```

#### 窗口（syscall 6–7）

```c
int Uni_Window(const char *title, int x, int y, int w, int h);
```
创建窗口，返回窗口句柄（win id，`>0` 成功，`<0` 失败）。`x/y` 为屏幕坐标，`w/h` 为窗口尺寸（像素）。

```c
int Uni_Destroy(int win_id);
```
销毁窗口。

```c
int Uni_Show(int win_id, int visible);
```
显示/隐藏窗口（`visible` 非 0 即显示）。

#### 控件（syscall 8）

```c
int Uni_Widget(int win_id, struct UniWidgetReq *req);
```
向窗口添加一个控件，返回控件索引（`>=0` 成功，`<0` 失败）。

```c
struct UniWidgetReq {
    int  type;        // 控件类型
    int  x, y, w, h;  // 位置与尺寸（像素）
    char label[64];   // 文本，最长 63 字符
};
```

可用控件类型：

| 宏 | 值 | 含义 |
|----|----|------|
| `UNI_WIDGET_BUTTON` | 1 | 按钮，点击触发事件 |
| `UNI_WIDGET_TOGGLE` | 2 | 开关（当前 `Uni.h` 尚未暴露） |
| `UNI_WIDGET_TEXTINPUT` | 3 | 单行文本输入框 |
| `UNI_WIDGET_TEXT` | 4 | 静态文本标签 |

控件索引索引从 0 递增，每次 `Uni_Widget` 调用返回下一个可用索引。后续的 `Uni_Label`、`Uni_SetText`、`Uni_GetText` 都通过该索引引用控件。

#### 静态文本标签（syscall 11–12）

```c
int Uni_Text(int win_id, int x, int y, const char *text);
```
在窗口客户区的 `(x,y)` 处绘制一行文本（UTF-8/ASCII），返回该文本的槽索引（用于后续 `Uni_Label` 更新）。

```c
int Uni_Label(int win_id, int text_idx, const char *str);
```
更新之前通过 `Uni_Text` 创建的文本标签内容。无需重绘窗口即可原地更新文字。适用于计算器显示值、时钟更新时间等场景。

**使用示例：**
```c
int status = Uni_Text(win, 16, 20, "initial");   // 创建标签，记下索引
// 后续更新：
Uni_Label(win, status, "updated value");          // 窗口上文字立刻变为 "updated value"
```

#### 事件轮询（syscall 9）

```c
int Uni_Poll(int win_id, struct UniEvent *ev);
```
轮询窗口事件。如果 `ev` 非空，返回的事件结构定义如下：

```c
#define UNI_EV_NONE   0
#define UNI_EV_CLICK  1
#define UNI_EV_SUBMIT 2

struct UniEvent {
    unsigned int win_id;
    unsigned int widget_idx;     // 触发事件的控件索引
    unsigned char type;          // 事件类型
};
```

当前内核实现：按钮点击时通过 `UNI_EV_CLICK` 通知、文本框回车时通过 `UNI_EV_SUBMIT` 通知。如果当前无事件，返回 `0`。

**事件循环范式：**
```c
for (;;) {
    struct UniEvent ev;
    if (Uni_Poll(win, &ev) > 0 && ev.type == UNI_EV_CLICK) {
        if (ev.widget_idx == 0) {
            // 按钮被点击，do something
        }
    }
    sys_yield();
}
```

#### 文本输入框读写（syscall 13, 19）

```c
int Uni_GetText(int win_id, int widget_idx, char *buf, int cap);
```
读取指定文本输入框（`UNI_WIDGET_TEXTINPUT`）的当前内容。`buf` 由调用者提供，`cap` 为缓冲区大小。返回实际写入的字符数（不含 NUL）。

```c
int Uni_SetText(int win_id, int widget_idx, const char *str);
```
设置指定文本输入框的内容。等价于在键盘上覆盖输入。

#### 文件 I/O（syscall 14–18）

ring3 应用现在可以直接读写磁盘文件。文件描述符是**固定槽位**（0-3），由应用自行分配。

```c
int Uni_FOpen(int fd, const char *path);
```
打开文件。`fd` 为槽位号（0–3），`path` 为绝对路径。返回 `0` 成功，`<0` 失败。

```c
int Uni_FClose(int fd);
```
关闭文件。返回 `0` 成功。

```c
int Uni_FRead(int fd, char *buf, int max);
```
从文件读取最多 `max` 字节到 `buf`。返回实际读取的字节数（可能小于 `max`），`<=0` 表示 EOF 或错误。

```c
int Uni_FWrite(int fd, const char *buf, int len);
```
写入 `len` 字节到文件。返回实际写入的字节数。

```c
int Uni_FList(const char *path, struct UniStat *out, int max);
```
列出目录内容。`path` 为目录路径，`out` 为 `UniStat` 数组，`max` 为数组容量：

```c
#define UNI_FLIST_MAX 32

struct UniStat {
    char name[256];
    unsigned int size;
    unsigned char type;   // 0=文件, 1=目录
};
```

返回实际填充的条目数。

**文件 I/O 示例（读取文件）：**
```c
char buf[512];
int n;

if (Uni_FOpen(0, "/var/documents/text.txt") == 0) {
    n = Uni_FRead(0, buf, 512);
    if (n > 0) {
        buf[n] = 0;      // 确保 NUL 结尾
        // 使用 buf...
    }
    Uni_FClose(0);
}
```

### 2.6 系统调用完整表

| 号 | 宏 | Uni.h 封装 | 说明 |
|----|----|-----------|------|
| 1 | `SYS_WRITE` | `sys_write(buf,len)` | 写串口日志。 |
| 2 | `SYS_YIELD` | `sys_yield()` | 主动让出 CPU。 |
| 3 | `SYS_GETPID` | `sys_getpid()` | 取当前进程 ID。 |
| 4 | `SYS_EXIT` | `sys_exit(code)` | 结束进程。 |
| 5 | `SYS_READ` | `sys_read(buf)` | 读一个按键字符。 |
| 6 | — | `Uni_Window(title,x,y,w,h)` | 创建窗口。 |
| 7 | — | `Uni_Destroy(win_id)` | 销毁窗口。 |
| 8 | — | `Uni_Widget(win_id,req)` | 添加控件。 |
| 9 | — | `Uni_Poll(win_id,ev)` | 轮询事件。 |
| 10 | — | `Uni_Show(win_id,visible)` | 显示/隐藏窗口。 |
| 11 | — | `Uni_Text(win_id,x,y,text)` | 创建文本标签。 |
| 12 | — | `Uni_Label(win_id,idx,str)` | 更新文本标签。 |
| 13 | — | `Uni_GetText(win_id,idx,buf,cap)` | 读文本框内容。 |
| 14 | — | `Uni_FOpen(fd,path)` | 打开文件。 |
| 15 | — | `Uni_FClose(fd)` | 关闭文件。 |
| 16 | — | `Uni_FRead(fd,buf,max)` | 读文件。 |
| 17 | — | `Uni_FWrite(fd,buf,len)` | 写文件。 |
| 18 | — | `Uni_FList(path,out,max)` | 列出目录。 |
| 19 | — | `Uni_SetText(win_id,idx,str)` | 设置文本框内容。 |

### 2.7 标准事件循环范式

```c
#include "Uni.h"

__attribute__((section(".entry")))
__attribute__((noreturn)) void _start(void)
{
    int win = Uni_Window("My App", 100, 100, 320, 240);
    if (win < 0) sys_exit(1);

    Uni_Text(win, 16, 20, "hello unios");
    Uni_Show(win, 1);

    for (;;) {
        struct UniEvent ev;
        if (Uni_Poll(win, &ev) > 0) {
            if (ev.type == UNI_EV_CLICK) {
                // handle widget click
            }
        }
        sys_yield();  // 必须让出 CPU，否则独占调度
    }
}
```

---

## 3. 调用文件对话框（UDLL 内核共享库）

### 3.1 UDLL 是什么

UDLL（Universal Dynamic Link Library）是 UniOS 的内核态动态共享库机制。通用功能（如文件对话框）被编译成一个 `.udll` 文件，放在磁盘 `/lib/` 目录下，需要时由内核或内核模块加载。

**注意：UDLL 是内核空间机制（ring0）。** ring3 应用本身无法直接加载 UDLL，但内核内置窗口（如系统记事本、文件管理器）可以使用 UDLL 共享对话框。对于 ring3 应用，文件 I/O 已通过 syscall 14–18 提供（见 2.5 节），而文件对话框仍需通过内核窗口间接使用。

### 3.2 文件对话框 UDLL 接口

`/lib/filedlg.udll` 导出一个函数：

```c
void file_dialog_show(
    int mode,                  // 0=Open, 1=Save
    const char *title,         // 窗口标题
    const char *initial_path,  // 初始路径（Save 模式下可包含默认文件名）
    const char *ok_text,       // 确认按钮文字（NULL=默认）
    const char *cancel_text,   // 取消按钮文字（NULL=默认）
    void (*cb)(int result, const char *path, int encoding, void *ctx),
    void *ctx                  // 不透明用户上下文
);
```

- **mode**: `0` 为打开、`1` 为保存。
- **title**: 窗口标题。
- **initial_path**: 初始目录。Save 模式下若路径末尾有文件名，则自动填入文件名输入框。
- **ok_text**: 确认按钮标签。为 `NULL` 时 Open 模式显示 `"Open"`、Save 模式显示 `"Save"`。
- **cancel_text**: 取消按钮标签。为 `NULL` 时显示 `"Cancel"`。
- **cb**: 回调函数，对话框关闭后调用。

回调参数：

| 参数 | 含义 |
|------|------|
| `result` | `1`=用户确认，`0`=用户取消 |
| `path` | 选中文件路径（取消时为 `NULL`） |
| `encoding` | Save 模式下选择的编码索引：`0`=UTF-8、`1`=ASCII、`2`=UTF-16；Open 模式为 `-1` |
| `ctx` | 创建对话框时传入的上下文指针 |

### 3.3 加载 UDLL（内核模块示例）

以下代码展示内核窗口如何加载并使用文件对话框（来自系统记事本 `src/kernel/desktop.c`）：

```c
static struct UdllHandle *g_filedlg;
typedef void (*fdlg_show_t)(int mode, const char *title, const char *initial_path,
                            const char *ok_text, const char *cancel_text,
                            void (*cb)(int, const char *, int, void *), void *ctx);
static fdlg_show_t g_filedlg_show;

static void ensure_filedlg(void)
{
    if (!g_filedlg) {
        g_filedlg = udll_load("/lib/filedlg.udll");
        if (g_filedlg)
            g_filedlg_show = (fdlg_show_t)udll_get_proc(g_filedlg, "file_dialog_show");
    }
}

static void on_save_result(int result, const char *path, int encoding, void *ctx)
{
    (void)encoding; (void)ctx;
    if (result && path)
        vfs_write(path, g_text, g_text_len);
}

static void show_save(void)
{
    ensure_filedlg();
    if (g_filedlg_show)
        g_filedlg_show(1, "Save File", "/var/documents/text.txt",
                       "Save", "Cancel", on_save_result, 0);
}
```

### 3.4 UDLL 系统设计参考

详见 `docs/DESIGN-udll.md`，涵盖：

- UDLL 文件格式（header/reloc/fixup/export/text/data/bss）
- 构建流程（编译 → `udll_pack.py` → 写入 FAT32）
- 内核加载器实现
- 内核符号表注册
- 安全限制（运行在 ring0，需信任源）

---

## 4. 打包指南

### 4.1 方法 A：使用官方工具 `tools/upk_pack.py`

脚本签名：`upk_pack.py <app_dir> <elf_bin> <out.upk>`

```bash
python3 tools/upk_pack.py src/apps/calculator build/calculator.bin build/calculator.upk
```

它会读取 `<app_dir>/manifest`，把 `manifest`、`bin/`（放你的 ELF）、`res/` 按 USTAR 格式拼成 `.upk`，末尾补两个 512 字节的零块作为 tar 结束符。

### 4.2 方法 B：直接用系统 `tar`（推荐——UPK 就是 tar）

你完全不需要 `upk_pack.py`。只要目录结构正确，一条 `tar` 命令即可产出合法 UPK：

```bash
# 假设当前目录有：manifest  bin/myapp  res/icon.svg
tar --format=ustar -cf myapp.upk manifest bin/ res/
```

要点：

- 必须 `--format=ustar`（POSIX USTAR），不要用 GNU 长名扩展，内核解析器按 100 字节文件名 / 512 字节块实现。
- `bin/` 下的 ELF 名必须与 `upk.entry` 完全一致（本例 `bin/myapp`）。
- 不要压缩（不要 `-z`/`-j`），当前格式不套压缩层。

验证：

```bash
tar -tf myapp.upk      # 能正常列出即合格
```

### 4.3 manifest 模板（复制即用）

```
upk.format=1
upk.id=com.example.myapp
upk.name=My Application
upk.version=1.0.0
upk.arch=x86_64
upk.entry=bin/myapp
upk.exports=myapp
upk.deps=
```

### 4.4 安装包校验（安装器会做的检查）

`upk`/安装器在写入磁盘前会校验：

1. USTAR 格式完整性（头校验和、512 对齐）。
2. `manifest` 必填字段齐全（`format`/`id`/`name`/`version`/`arch`/`entry`）。
3. 架构为 `x86_64`。
4. `upk.entry` 指向的文件是合法 ELF64（魔数 `0x7F 'E' 'L' 'F'`，class=2 即 64 位，machine=x86_64）。
5. 依赖（`upk.deps`）是否可用。
6. 同 ID 是否已安装（升级需比较 `version`）。

开发者在分发前建议自己先 `tar -tf` 看一遍，确保 `manifest` 存在、`bin/<entry>` 存在且是 ELF64。

---

## 5. 安装与分发

包管理器命令（目标运行行为，由 `upk` 提供）：

```
upk install <file.upk>    安装 UPK 安装包（解包到 /apps/<id>，注册 exports 与 db）
upk add <elf> [name]      直装单个 ELF64（自动包装成单文件 UPK 应用，与 install 归一）
upk remove <id|name>      卸载（删 /apps/<id> + 清注册表，零残留）
upk list                  列出已安装
upk info <id|name>        查看包信息
upk verify <file.upk>     仅校验安装包完整性与格式，不安装
```

`upk add` 本质上是把单个 ELF 自动包成一个最小 UPK（id=文件名、version=0.0.0、entry、exports=文件名），与 `upk install` 共用同一套注册表，二者完全归一。

分发渠道：把 `.upk` 文件交给用户，用户在 UniOS 的「应用管理器」或 shell 执行 `upk install <file.upk>` 即可。因为 UPK 是标准 tar，用户甚至可以在自己的 PC 上先 `tar -tf` 检查内容再装。

---

## 6. 源码参考

以下是系统内置四个应用的**完整源码**，作为你开发 UGC 应用的参考模板。

### 6.1 Calculator

`src/apps/calculator/manifest`

```
upk.format=1
upk.id=com.BlabbyCo.calculator
upk.name=Calculator
upk.version=1.0.0
upk.arch=x86_64
upk.entry=bin/calculator
upk.exports=calc,calculator
upk.deps=
```

`src/apps/calculator/bin/main.c`

```c
#include "Uni.h"

static char g_expr[64];
static int g_len;
static int g_shown;
static int g_disp;

static void show_expr(int win)
{
    if (g_len == 0) { Uni_Label(win, g_disp, "0"); return; }
    const char *p = g_expr;
    if (g_len > 30) p = g_expr + g_len - 30;
    Uni_Label(win, g_disp, p);
}

static void press(int win, int slot, char c)
{
    (void)slot;
    if (g_shown && (c >= '0' && c <= '9'))
    { g_len = 0; g_shown = 0; }
    if (g_len < 60) { g_expr[g_len++] = c; g_expr[g_len] = 0; }
    if (c == 'C')
    { g_len = 0; g_shown = 0; g_expr[0] = 0; show_expr(win); return; }
    show_expr(win);
}

__attribute__((section(".entry")))
__attribute__((noreturn)) void _start(void)
{
    int win = Uni_Window("Calculator", 440, 20, 280, 380);
    if (win < 0) { sys_exit(1); }

    g_disp = Uni_Text(win, 16, 20, "0");
    // ... （按钮布局略，见完整源码）
    Uni_Show(win, 1);

    for (;;) {
        struct UniEvent ev;
        if (Uni_Poll(win, &ev) > 0 && ev.type == UNI_EV_CLICK) {
            char lbl[64];
            Uni_GetText(win, ev.widget_idx, lbl, 64);
            press(win, ev.widget_idx, lbl[0]);
        }
        sys_yield();
    }
}
```

### 6.2 Notepad

`src/apps/notepad/manifest`

```
upk.format=1
upk.id=com.BlabbyCo.notepad
upk.name=Notepad
upk.version=1.0.0
upk.arch=x86_64
upk.entry=bin/notepad
upk.exports=notepad,edit
upk.deps=
```

`src/apps/notepad/bin/main.c`

```c
#include "Uni.h"

static int g_win;
static int g_status;
static int g_edit;
static char g_msg[64];

__attribute__((section(".entry")))
__attribute__((noreturn)) void _start(void)
{
    g_win = Uni_Window("Notepad", 860, 20, 380, 320);
    if (g_win < 0) sys_exit(1);

    struct UniWidgetReq req;
    req.type = UNI_WIDGET_TEXTINPUT;
    req.x = 16; req.y = 32; req.w = 330; req.h = 190;
    req.label[0] = 0;
    g_edit = Uni_Widget(g_win, &req);

    req.type = UNI_WIDGET_BUTTON;
    req.x = 16; req.y = 240; req.w = 60; req.h = 28;
    req.label[0] = 'S'; req.label[1] = 'a'; req.label[2] = 'v'; req.label[3] = 'e';
    req.label[4] = 0;
    Uni_Widget(g_win, &req);

    req.x = 86; req.label[0] = 'L'; req.label[1] = 'o'; req.label[2] = 'a';
    req.label[3] = 'd'; req.label[4] = 0;
    Uni_Widget(g_win, &req);

    g_status = Uni_Text(g_win, 16, 280, "Ready");
    Uni_Show(g_win, 1);

    for (;;) {
        struct UniEvent ev;
        if (Uni_Poll(g_win, &ev) > 0 && ev.type == UNI_EV_CLICK) {
            char lbl[16];
            Uni_GetText(g_win, ev.widget_idx, lbl, 16);
            if (lbl[0] == 'S') {
                // Save: write edit text to file
                char buf[512];
                int n = Uni_GetText(g_win, g_edit, buf, 512);
                if (Uni_FOpen(0, "/var/documents/note.txt") == 0) {
                    Uni_FWrite(0, buf, n);
                    Uni_FClose(0);
                    Uni_Label(g_win, g_status, "Saved");
                }
            } else if (lbl[0] == 'L') {
                // Load: read file into edit
                char buf[512];
                int n;
                if (Uni_FOpen(0, "/var/documents/note.txt") == 0) {
                    n = Uni_FRead(0, buf, 512);
                    if (n > 0) { buf[n] = 0; Uni_SetText(g_win, g_edit, buf); }
                    Uni_FClose(0);
                    Uni_Label(g_win, g_status, "Loaded");
                }
            }
        }
        sys_yield();
    }
}
```

### 6.3 Terminal

`src/apps/terminal/manifest`

```
upk.format=1
upk.id=com.BlabbyCo.terminal
upk.name=Terminal
upk.version=1.0.0
upk.arch=x86_64
upk.entry=bin/terminal
upk.exports=terminal,term
upk.deps=
```

`src/apps/terminal/bin/main.c`

```c
#include "Uni.h"

static int g_win;
static int g_status;

__attribute__((section(".entry")))
__attribute__((noreturn)) void _start(void)
{
    g_win = Uni_Window("Terminal", 440, 380, 400, 340);
    if (g_win < 0) sys_exit(1);

    struct UniWidgetReq req;
    req.type = UNI_WIDGET_TEXTINPUT;
    req.x = 16; req.y = 120; req.w = 300; req.h = 28;
    req.label[0] = 't'; req.label[1] = 'y'; req.label[2] = 'p'; req.label[3] = 'e';
    req.label[4] = ' '; req.label[5] = 'c'; req.label[6] = 'm'; req.label[7] = 'd';
    req.label[8] = 0;
    Uni_Widget(g_win, &req);

    req.type = UNI_WIDGET_BUTTON;
    req.x = 322; req.y = 120; req.w = 50; req.h = 28;
    req.label[0] = 'R'; req.label[1] = 'u'; req.label[2] = 'n'; req.label[3] = 0;
    Uni_Widget(g_win, &req);

    g_status = Uni_Text(g_win, 16, 168, "type a command");
    Uni_Show(g_win, 1);

    for (;;) {
        struct UniEvent ev;
        if (Uni_Poll(g_win, &ev) > 0 && ev.type == UNI_EV_CLICK) {
            // ... command handling
        }
        sys_yield();
    }
}
```

### 6.4 Clock

`src/apps/clock/manifest`

```
upk.format=1
upk.id=com.BlabbyCo.clock
upk.name=Clock
upk.version=1.0.0
upk.arch=x86_64
upk.entry=bin/clock
upk.exports=clock,time
upk.deps=
```

`src/apps/clock/bin/main.c`

```c
#include "Uni.h"

__attribute__((section(".entry")))
__attribute__((noreturn)) void _start(void)
{
    int win = Uni_Window("Clock", 860, 380, 240, 180);
    if (win < 0) sys_exit(1);
    Uni_Text(win, 20, 24, "UniOS Clock v1.0");
    Uni_Text(win, 20, 50, "ring3 user app");
    Uni_Text(win, 20, 76, "com.BlabbyCo.clock");

    Uni_Show(win, 1);

    for (;;) {
        struct UniEvent ev;
        Uni_Poll(win, &ev);
        sys_yield();
    }
}
```

### 6.5 用户态头文件与链接脚本

`src/user/Uni.h`（完整 API 参考见 2.5 节）

`src/user/syscall.h`（syscall 1–5，基础系统调用）

`src/user/linker.ld`（见 2.3 节）

### 6.6 官方打包脚本 `tools/upk_pack.py`

```python
#!/usr/bin/env python3
import sys
import os
import tarfile
import io
import struct

USAGE = "upk_pack.py <app_dir> <elf_bin> <out.upk>"

BLOCK = 512

def pad_size(sz):
    r = sz % BLOCK
    if r == 0:
        return sz
    return sz + (BLOCK - r)

def mk_tar_header(name, size):
    hdr = bytearray(BLOCK)
    n = name[:100].encode("utf-8")
    hdr[0:len(n)] = n
    fmt = b"%011o" % 0o644
    hdr[100:107] = fmt
    fmt = b"%011o" % 0
    hdr[108:115] = fmt
    fmt = b"%011o" % 0
    hdr[116:123] = fmt
    fmt = b"%011o" % size
    hdr[124:135] = fmt
    fmt = b"%011o" % 0
    hdr[136:147] = fmt
    hdr[156] = ord('0')
    chk = 0
    for i in range(BLOCK):
        chk += hdr[i] if i < BLOCK else 0
    for i in range(148, 156):
        hdr[i] = 32
    fmt = b"%06o" % (chk & 0o777777)
    hdr[148:148+len(fmt)] = fmt
    hdr[154] = 0
    hdr[155] = 0
    return bytes(hdr)

def make_tar(name, data):
    hdr = mk_tar_header(name, len(data))
    padded = data + b'\x00' * (pad_size(len(data)) - len(data))
    return hdr + padded

def pack(app_dir, elf_bin, out_path):
    with open(os.path.join(app_dir, "manifest"), "rb") as f:
        manifest = f.read()
    with open(elf_bin, "rb") as f:
        elf_data = f.read()

    records = []
    records.append(make_tar("manifest", manifest))
    records.append(make_tar("bin/", b""))
    records.append(make_tar("bin/app", elf_data))
    records.append(make_tar("res/", b""))

    body = b"".join(records)
    trailer = b"\x00" * (BLOCK * 2)

    with open(out_path, "wb") as f:
        f.write(body)
        f.write(trailer)

    size = len(body) + len(trailer)
    print(f"UPK packed: {out_path}  ({size} bytes)")
    print(f"  id: {app_dir}")

if __name__ == "__main__":
    if len(sys.argv) < 4:
        print(USAGE)
        sys.exit(1)
    pack(sys.argv[1], sys.argv[2], sys.argv[3])
```

> 注意：该脚本把 ELF 固定写成 `bin/app`，与 manifest 的 `upk.entry` 不一致。若你的安装流程依赖 `upk.entry` 精确匹配，请改用 `tar --format=ustar`（方法 B，见 4.2 节）并把 ELF 放到 `upk.entry` 指定的路径。

---

## 7. 常见陷阱（Checklist）

- [ ] 入口必须是 `_start`，且放在 `.entry` section：`__attribute__((section(".entry")))`。
- [ ] `_start` 必须标记 `__attribute__((noreturn))`，否则 lld 可能优化异常。
- [ ] 编译必须 `-nostdlib -ffreestanding -mcmodel=large -mno-red-zone`，否则高端地址加载会崩。
- [ ] `linker.ld` 输出段必须包含 `KEEP(*(.entry))` + `*(.ltext)` + `*(.text*)`。
- [ ] 二进制在包内的路径 **必须** 等于 `upk.entry`（如 `bin/myapp`）。
- [ ] 打包用 `--format=ustar`，**不要** 加压缩。
- [ ] `label` 是 63 字符上限的 NUL 结尾串，手写时要补 NUL。
- [ ] 主循环必须 `sys_yield()`，否则独占总调度。
- [ ] 只用 `Uni.h` 导出的控件类型（`BUTTON`/`TEXTINPUT`/`TEXT`），保证 ABI 兼容。
- [ ] `upk.id` 用反域名、全小写、全局唯一。
- [ ] 不要依赖符号链接、不要假设 UNIX 权限位（FAT32 无此能力）。
- [ ] 文件描述符槽位是固定 0–3，应用自行分配，不能与其他应用共享。
- [ ] 文件 I/O 目前不支持追加写（`O_APPEND`），写操作会覆盖文件内容。
- [ ] 最大文件名长度：`UniStat.name` 为 256 字节；`UniWidgetReq.label` 为 64 字节。
- [ ] 文件列表 `Uni_FList` 的最大条目数为 `UNI_FLIST_MAX`（=32）。

---

## 8. 路线图（与本文档相关的未实现项）

- **运行时 `upk install` 安装器**：当前系统应用以扁平二进制形式直接编入内核镜像；`upk` 命令的完整运行时安装/卸载/注册逻辑将在文件系统和进程管理就绪后落地。
- **进程管理**：`execv/fork/wait` 用户态 API 与 shell 联动尚未开放。
- **UDP/TCP 网络 syscall**：尚无网络栈。
- **控件事件回调（完整）**：当前 `Uni_Poll` 支持 `CLICK` 和 `SUBMIT`，后续增加更多事件类型。
- **文件系统写入加强**：追加写、目录创建、文件删除等操作待 VFS 扩展。
- **压缩层**：tar 外层预留 DEFLATE 压缩支持，格式字段已留。

---

## 9. 最小可运行示例（复制即用）

目录 `hello/`：

```
hello/manifest
hello/bin/main.c
```

`hello/manifest`：

```
upk.format=1
upk.id=com.example.hello
upk.name=Hello
upk.version=1.0.0
upk.arch=x86_64
upk.entry=bin/hello
upk.exports=hello
upk.deps=
```

`hello/bin/main.c`：

```c
#include "Uni.h"

__attribute__((section(".entry")))
__attribute__((noreturn)) void _start(void)
{
    int win = Uni_Window("Hello", 200, 200, 300, 160);
    if (win < 0) sys_exit(1);
    Uni_Text(win, 16, 24, "Hello from UGC app!");
    Uni_Show(win, 1);
    for (;;) { struct UniEvent ev; Uni_Poll(win, &ev); sys_yield(); }
}
```

打包：

```bash
clang --target=x86_64-unknown-none-elf -ffreestanding -nostdlib \
    -mcmodel=large -mno-red-zone -O2 -std=c11 -Isrc/user \
    -c hello/bin/main.c -o hello_u.o
ld.lld --image-base=0x8000040000 -T src/user/linker.ld \
    -o hello.elf -nostdlib hello_u.o
llvm-objcopy -O binary hello.elf hello.bin

# 方法 A：官方脚本（注意会写成 bin/app）
python3 tools/upk_pack.py hello hello.bin hello.upk

# 方法 B：直接 tar（推荐，路径与 upk.entry 一致）
mkdir -p hello/bin && cp hello.bin hello/bin/hello
cd hello && tar --format=ustar -cf ../hello.upk manifest bin/hello && cd ..
tar -tf hello.upk    # 验证：应看到 manifest 与 bin/hello 在归档根上
```

至此你已拥有一个合法 UniOS UPK 安装包 `hello.upk`。
