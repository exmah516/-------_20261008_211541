# Git 使用说明

本目录是独立 Git 仓库，默认分支为 `main`，远程仓库为 `origin`。

日常源码入口：

- `介入机器人/A主端/工程`
- `介入机器人/B数据端/工程`
- `造影机器人/B数据端`
- `影像传输/B数据端/ScreenCut源码`

编译输出、IDE 缓存、日志、本机密钥和现场配置按 `.gitignore` 处理。克隆后需在介入 A/B 的 `工程/config` 恢复现场密钥和设备配置。

常用命令：

```powershell
git status
git diff
git add --all
git diff --cached --stat
git commit -m "说明本次修改"
git log --oneline -5
```

本次目录迁移没有修改 PLC、协议或控制逻辑。