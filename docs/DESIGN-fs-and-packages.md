# UniOS 文件系统结构与程序安装体系设计

状态：设计稿（第八步实现文件系统、第十一步后实现 upk 前的图纸）
适用范围：整个 UniOS 生命周期，后续所有步骤必须遵守本设计

---

## 1. 设计原则

1. 类 UNIX 集中式管理（已在项目记忆中锁定），反对 Windows 式随处放。
2. 只取 UNIX/Linux 精华，抛弃历史包袱：
   - 不搞 /bin 与 /usr/bin、/sbin 与 /usr/sbin 的历史分裂，可执行文件只有一个系统目录。
   - 不设 /opt、/srv 这类语义模糊目录，第三方软件统一进 /apps。
3. 程序 = ELF64，无强制扩展名，靠文件头识别。
4. 所有已安装软件必须在包注册表中有记录，无论以哪种方式安装。
5. 应用相互隔离：每个应用独占一个目录，卸载 = 删目录 + 删注册项，零残留。

---

## 2. 系统目录树（定稿）

```
/
├── boot/               内核 kernel.elf、GRUB、启动配置
├── bin/                核心系统命令（随系统发行，用户不可写）
├── lib/                核心共享库、字库等系统资源
├── etc/                系统配置（全部纯文本 UTF-8）
│   ├── passwd          用户账户
│   ├── group           用户组
│   ├── shadow          口令散列
│   ├── hostname        主机名
│   ├── fstab           挂载表
│   └── pkg/            包管理数据库
│       ├── db          已安装包注册表
│       └── exports     命令名 -> ELF 路径 映射表
├── dev/                设备节点（内核虚拟生成）
├── proc/               进程与内核信息（内核虚拟生成）
├── run/                运行时状态（易失，开机清空）
├── tmp/                临时文件（易失，开机清空）
├── var/                持久可变数据（含用户数据目录，全小写）
│   ├── log/            系统与应用日志
│   ├── cache/          缓存
│   ├── spool/          队列（打印、任务等）
│   ├── www/            服务器软件的站点与服务数据
│   ├── dcim/           相机/截图类图像（仅此一个，不拆 DCIM 与 pictures）
│   ├── downloads/      下载
│   ├── documents/      文档
│   └── musics/         音乐
├── home/               普通用户主目录 /home/<user>
├── root/               root 主目录
├── apps/               全部已安装应用（UPK 与直装 ELF 统一在此）
│   └── <id>/           每应用一个目录
│       ├── manifest    应用清单
│       ├── bin/        应用可执行文件
│       ├── lib/        应用私有库
│       └── res/        应用资源
├── mnt/                手动临时挂载点
└── media/              可移动介质自动挂载点
```

### 与传统 FHS 的差异（刻意为之）

| 传统 | UniOS | 理由 |
|---|---|---|
| /bin /sbin /usr/bin /usr/sbin /usr/local/bin | 仅 /bin | 历史分裂无现代价值 |
| /opt + /usr/local + 散装 | 仅 /apps/<id>/ | 应用隔离、卸载零残留 |
| /usr/lib /lib /lib64 | 仅 /lib | 单架构系统无需多目录 |
| /srv | 无 | 语义模糊，不需要 |

### /var 用户数据目录（定稿约定）

- 系统面向高级用户/黑客用途，允许安装服务器类软件，服务数据统一放 /var/www。
- 用户媒体目录全部小写、单数语义唯一：dcim / downloads / documents / musics。
- 图像只有 /var/dcim 一个目录，不学 Android 拆成 DCIM 与 Pictures 两处。
- 第八步建目录树时随系统初始化一并创建。

### 虚拟与持久划分

- 内核虚拟（不落盘）：/dev /proc /run /tmp
- 磁盘持久：其余全部

---

## 3. 程序安装体系

### 3.1 包管理器：upk（Uni PacKage）

一个命令统管两种安装方式，全部写入同一套注册表（/etc/pkg/db 与 /etc/pkg/exports）。

```
upk install <file.upk>      安装 UPK 安装包
upk add <elf> [name]        直装单个 ELF64 可执行文件
upk remove <id|name>        卸载（删 /apps/<id> + 清注册表）
upk list                    列出已安装
upk info <id|name>          查看包信息
upk verify <file.upk>       校验安装包完整性与格式
```

### 3.2 方式一：直装 ELF64（upk add）

流程：
1. 校验目标是合法 ELF64（魔数 0x7F 'E' 'L' 'F'，class=64，machine=x86_64）。
2. 确定包名：显式给出的 name，否则取文件名。
3. 创建 /apps/<name>/bin/ 并复制 ELF 进去。
4. 自动生成最小 manifest（id=name、version=0.0.0、entry、exports=name）。
5. 注册到 /etc/pkg/db，命令名写入 /etc/pkg/exports。

直装本质上是被自动包装成一个单文件 UPK 应用，与方式二完全归一。

### 3.3 方式二：UPK 安装包（upk install）

UPK = Uni PacKage，定位等同 MSI / APK。

容器格式：USTAR（tar）归档，扩展名 .upk。
选择理由：与 initrd 同一格式，解析器一套代码复用；标准格式，宿主机工具链即可打包；后续如需压缩，在 tar 外套 DEFLATE 层，格式字段预留。

包内布局（归档根）：

```
manifest                必需，UTF-8 键值对
bin/                    可执行文件
lib/                    私有库
res/                    资源（图标、数据文件）
```

manifest 格式（键=值，一行一条）：

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

字段说明：
- upk.format：格式版本，当前 1，为未来兼容预留。
- upk.id：全局唯一反域名 ID，即 /apps/ 下的目录名。
- upk.entry：主程序相对路径。
- upk.exports：逗号分隔的命令名列表，安装后可在 shell 直接敲。
- upk.deps：依赖的其他包 ID（首版允许为空，解析器必须实现字段读取）。

安装流程：
1. 读归档中的 manifest，校验 format/arch/id 合法。
2. 冲突检查：id 已存在则要求先 remove 或走升级路径（比较 version）。
3. 解包整树到 /apps/<id>/。
4. exports 中每个命令名写入 /etc/pkg/exports：`<命令名>=/apps/<id>/<entry 或对应 bin>`。
5. 在 /etc/pkg/db 追加记录：id、name、version、安装时间、文件清单。

卸载流程：按 db 中文件清单逆向删除，清理 exports 与 db 记录。

### 3.4 命令解析顺序（shell 规范）

```
1. shell 内建命令
2. /bin
3. /etc/pkg/exports 注册的命令
```

设计说明：FAT32 无符号链接，因此不用 UNIX 惯用的 link farm，改用 exports 注册表完成命令名到 ELF 路径的映射。此方案在任何无符号链接能力的文件系统上都成立。

---

## 4. 与 FAT32 的适配（第八步实现时的强约束）

FAT32 没有 owner/group/权限位/符号链接，而 UniOS 采用 UNIX 权限模型，适配方案：

1. 每目录一个隐藏元数据文件 `.unimeta`（UTF-8 文本），记录本目录内各文件的 owner、group、mode。
2. VFS 层负责读写 .unimeta 并在内存中合成完整 inode 属性；上层（shell、程序）只看到标准 UNIX 语义。
3. 缺失 .unimeta 时的默认值：root:root 0755（目录）/ 0644（文件），/bin 与 /apps 下 ELF 默认 0755。
4. 符号链接暂不提供，系统设计中一律不依赖符号链接（见 3.4）。

---

## 5. 后续步骤的落地位置

- 第八步（存储与文件系统）：实现本目录树的创建、VFS、.unimeta 权限合成。
- 第七步（用户态）：ELF64 加载器按本设计的路径约定查找程序。
- 第十一步（桌面）：文件管理器按本目录树展示；应用图标读 /apps/<id>/res/。
- upk 实现顺序：先内核 shell 版（第八步后），后 GUI 安装器（第十一步）。
