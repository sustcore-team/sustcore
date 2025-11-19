
# config-ref

`config-ref` 保存项目常用的环境配置脚本与 VS Code 参考配置，便于在不同开发机上快速还原工作区调试/编辑环境。

- `./*setup.sh`: 各种工具的环境初始化脚本。
- `./*.json`: VS Code 工作区配置文件。

## 如何使用

由于脚本会下载解压源码，不建议在项目源码目录下直接运行脚本。推荐的做法是：

```bash
mkdir -p ~/env-setup
cp config-ref/*setup.sh ~/env-setup/
cd ~/env-setup
# 运行所需的脚本，例如:
bash setup.sh
bash qemu-setup.sh
bash gdb-setup.sh
```

可以根据需要修改编译选项，例如 `--prefix` 或 `-j` 等。

默认的二进制文件安装路径为 `~/opt/cross/bin`，请在安装结束后将其添加到 `PATH` 环境变量中，例如在 `~/.bashrc` 中追加：

```bash
export PATH="$HOME/opt/cross/bin:$PATH"
```

## 注意事项

- 如果已经安装过交叉编译工具链，可能会与现有工具链冲突，请卸载旧版本。
- 如果之前使用 `pip` 安装过 `meson`，且默认在 `~/.local/bin` 中，可能会导致 qemu 的 `sudo make install` 报错。请务必先卸载。
