#!/bin/bash

# Android Super Image 处理脚本
# 功能：转换 sparse image -> 解包分区 -> 挂载分区 -> 打开目录

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
SIMG2IMG="$SCRIPT_DIR/simg2img"
LPUNPACK="$SCRIPT_DIR/lpunpack"

# 检查参数
if [ $# -ne 1 ]; then
    echo "用法: $0 <super.img>"
    exit 1
fi

INPUT_IMG="$1"

if [ ! -f "$INPUT_IMG" ]; then
    echo "错误: 文件 '$INPUT_IMG' 不存在"
    exit 1
fi

# 工作目录
IMAGES_DIR="$HOME/.local/images"
MOUNT_DIR="$IMAGES_DIR/mount"
UNPACK_DIR="$IMAGES_DIR/unpacked"

# 创建目录
mkdir -p "$IMAGES_DIR"
mkdir -p "$MOUNT_DIR"
mkdir -p "$UNPACK_DIR"

# 获取文件名
IMG_FILENAME=$(basename "$INPUT_IMG")
EXT4_IMG="$IMAGES_DIR/${IMG_FILENAME}_ext4"

# 步骤 1: 使用 simg2img 转换
echo "=== 步骤 1: 转换镜像 ==="
if [ ! -f "$EXT4_IMG" ]; then
    echo "正在转换 $INPUT_IMG -> $EXT4_IMG"
    "$SIMG2IMG" "$INPUT_IMG" "$EXT4_IMG"
    if [ $? -ne 0 ]; then
        echo "错误: simg2img 转换失败"
        exit 1
    fi
    echo "转换完成"
else
    echo "$EXT4_IMG 已存在，跳过转换"
fi

# 步骤 2: 使用 lpunpack 解包
echo ""
echo "=== 步骤 2: 解包镜像 ==="
# 清空旧的解包目录
rm -rf "$UNPACK_DIR"/*
echo "正在解包 $EXT4_IMG 到 $UNPACK_DIR"
"$LPUNPACK" "$EXT4_IMG" "$UNPACK_DIR"
if [ $? -ne 0 ]; then
    echo "错误: lpunpack 解包失败"
    exit 1
fi
echo "解包完成"

# 步骤 3: 列出可用分区并选择
echo ""
echo "=== 步骤 3: 选择要挂载的分区 ==="
PARTITION_IMGS=("$UNPACK_DIR"/*.img)

if [ ${#PARTITION_IMGS[@]} -eq 0 ] || [ ! -f "${PARTITION_IMGS[0]}" ]; then
    echo "错误: 未找到任何分区镜像文件"
    exit 1
fi

echo "可用分区:"
for i in "${!PARTITION_IMGS[@]}"; do
    PART_NAME=$(basename "${PARTITION_IMGS[$i]}")
    echo "  $((i+1)). $PART_NAME"
done
echo "  0. 取消"

read -p "请选择分区编号: " CHOICE

if [ "$CHOICE" = "0" ] || [ -z "$CHOICE" ]; then
    echo "已取消"
    exit 0
fi

if ! [[ "$CHOICE" =~ ^[0-9]+$ ]] || [ "$CHOICE" -lt 1 ] || [ "$CHOICE" -gt ${#PARTITION_IMGS[@]} ]; then
    echo "错误: 无效的选择"
    exit 1
fi

SELECTED_IMG="${PARTITION_IMGS[$((CHOICE-1))]}"
SELECTED_NAME=$(basename "$SELECTED_IMG")

# 步骤 4: 挂载
echo ""
echo "=== 步骤 4: 挂载分区 ==="
# 检查挂载点是否已挂载
if mountpoint -q "$MOUNT_DIR"; then
    echo "挂载点已被使用，正在卸载..."
    sudo umount "$MOUNT_DIR"
fi

echo "正在挂载 $SELECTED_NAME 到 $MOUNT_DIR"
sudo mount -o ro "$SELECTED_IMG" "$MOUNT_DIR"
if [ $? -ne 0 ]; then
    echo "错误: 挂载失败"
    exit 1
fi
echo "挂载成功"

# 步骤 5: 打开挂载目录
echo ""
echo "=== 步骤 5: 打开挂载目录 ==="
if command -v xdg-open &> /dev/null; then
    xdg-open "$MOUNT_DIR"
elif command -v nautilus &> /dev/null; then
    nautilus "$MOUNT_DIR"
else
    echo "无法自动打开文件管理器，请手动访问: $MOUNT_DIR"
fi

echo ""
echo "完成！"
echo "挂载目录: $MOUNT_DIR"
echo "卸载命令: sudo umount $MOUNT_DIR"
echo "清理命令: rm -f \"$EXT4_IMG\" && rm -rf \"$UNPACK_DIR\"/*"
