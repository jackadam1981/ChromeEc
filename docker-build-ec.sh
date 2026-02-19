#!/usr/bin/env bash
#
# 使用 coreboot-sdk Docker 镜像编译 Chrome EC（Kaisa 对应 board: puff）
# 镜像内自带 arm-eabi- 工具链，无需本地安装。
#
# 用法: ./build-ec.sh [ec_dir]
#   ec_dir: EC 源码目录，默认为本脚本所在目录（EC 项目根目录）
#
# 示例:
#   ./build-ec.sh              # 编译当前 EC 目录，输出 build/puff/ec.bin
#   ./build-ec.sh /path/to/ec  # 指定 EC 目录
#

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
EC_DIR="${1:-$SCRIPT_DIR}"

if [ ! -d "$EC_DIR" ]; then
    echo "错误: EC 目录不存在: $EC_DIR"
    echo "用法: $0 [ec_dir]"
    exit 1
fi

EC_DIR="$(cd "$EC_DIR" && pwd)"

if ! docker image inspect coreboot/coreboot-sdk &>/dev/null; then
    echo "正在拉取 coreboot-sdk 镜像（约 2.5GB，需足够磁盘空间）..."
    if ! docker pull coreboot/coreboot-sdk; then
        echo ""
        echo "拉取失败（常见原因：根分区 / 或 containerd 所在分区空间不足）。"
        echo "可改用本机工具链编译，无需 Docker："
        echo "  make BOARD=puff CROSS_COMPILE_arm=arm-none-eabi-"
        echo "  需先安装: dnf install arm-none-eabi-gcc  或  apt-get install gcc-arm-none-eabi"
        exit 1
    fi
fi

echo "=========================================="
echo "Chrome EC 编译 (CoreBoot SDK)"
echo "  EC 目录: $EC_DIR"
echo "  Board:   puff (Kaisa 使用此 EC)"
echo "  镜像:    coreboot/coreboot-sdk"
echo "=========================================="

# 使用 root 运行，否则挂载的宿主机目录（多为 root 属主）下无法创建 build/
# coreboot-sdk 镜像内 ARM 工具链在 /opt/xgcc/bin/；禁用 ccache 避免路径错误
docker run --rm --user root \
    -v "$EC_DIR:/ec" \
    -w /ec \
    -e CCACHE_DISABLE=1 \
    coreboot/coreboot-sdk \
    bash -c "make -j BOARD=puff CROSS_COMPILE_arm=/opt/xgcc/bin/arm-eabi-"

echo ""
echo ">>> 编译完成"
echo "    固件: $EC_DIR/build/puff/ec.bin"
ls -la "$EC_DIR/build/puff/ec.bin" 2>/dev/null || true
