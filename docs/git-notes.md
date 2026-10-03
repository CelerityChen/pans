# Git 操作笔记

本文按主题记录 pans 开发中用到的 git 命令与操作要点，新的主题按章节追加。

目录：

1. [Submodule](#1-submodule)

---

## 1. Submodule

### 1.1 添加与拉取

```bash
# 添加（指定分支）
git submodule add -b master https://github.com/jbeder/yaml-cpp.git third_party/yaml-cpp

# 克隆含 submodule 的仓库
git clone --recurse-submodules <repo-url>

# 已克隆但 submodule 目录为空时
git submodule update --init --recursive
```

在 CMake 中使用：

```cmake
add_subdirectory(third_party/yaml-cpp)
target_link_libraries(pans PUBLIC yaml-cpp::yaml-cpp)
```

### 1.2 删除

以删除路径为 `<path>` 的 submodule 为例，在仓库根目录执行：

```bash
# 1. 清空工作区里的 submodule 目录，并从 .git/config 注销
git submodule deinit -f <path>

# 2. 从索引中移除，同时自动删除 .gitmodules 里对应的段落
git rm -f <path>

# 3. 删除 git 缓存的 submodule 仓库数据
rm -rf .git/modules/<path>

# 4. 提交
git commit -m "移除 submodule <path>"
```

submodule 的信息分散在四个地方，三条命令各自负责一部分：

| 位置 | 内容 | 由哪一步清理 |
|------|------|--------------|
| 工作区 `<path>/` | submodule 检出的文件 | `deinit` 清空，`git rm` 删除目录 |
| `.git/config` | `submodule.<name>.url` / `active` | `deinit` |
| `.gitmodules` | `[submodule "<name>"]` 段落 | `git rm`（并自动暂存该改动） |
| `.git/modules/<path>/` | submodule 的完整 git 仓库数据 | 只能手动 `rm -rf` |

### 1.3 删除时的易错点

**不要只删目录。** 直接 `rm -rf <path>` 只删了工作区文件，索引、`.gitmodules`、`.git/config`、`.git/modules` 里的记录都还在，`git status` 会显示 submodule 被删除，但配置残留。

**第三步不能省。** `.git/modules/<path>` 不会被 `deinit` 或 `git rm` 删除。残留时，以后在同一路径重新添加 submodule 会报错：

```text
fatal: A git directory for '<path>' is found locally with remote(s): ...
```

**路径与名字不一定相同。** `.gitmodules` 里 `[submodule "<name>"]` 的名字通常等于路径，但不一定。命令参数用的是路径；`.git/modules/` 下的目录名对应的是名字。若两者不同，第三步要按名字删除。可以先查看：

```bash
cat .gitmodules
ls .git/modules
```

### 1.4 验证

```bash
cat .gitmodules                                            # 不再包含被删除的项
git submodule status                                       # 列表中没有该路径
git config --file .git/config --get-regexp '^submodule\.'  # 无残留配置
ls .git/modules                                            # 无对应目录
git status --short                                         # 显示 D <path> 与 M .gitmodules
```

### 1.5 实例：清理重复添加的 yaml-cpp

pans 曾把 yaml-cpp 作为 submodule 添加了两次，指向同一个仓库、同一个提交：

| 路径 | 处理 |
|------|------|
| `yaml-cpp/`（仓库根目录） | 删除 |
| `third_party/yaml-cpp/` | 保留，第三方依赖统一放在 `third_party/` |

实际执行：

```bash
git submodule deinit -f yaml-cpp
git rm -f yaml-cpp
rm -rf .git/modules/yaml-cpp
```

删除后的结果：

```text
.gitmodules       只剩 third_party/yaml-cpp
submodule status  只剩 third_party/yaml-cpp
.git/modules      只剩 third_party/
```
