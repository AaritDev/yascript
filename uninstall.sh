#!/bin/bash

set -e

# ANSI Color Codes
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[0;33m'
BLUE='\033[0;34m'
NC='\033[0m'

echo
echo -e "${RED}=======================================${NC}"
echo -e "${RED}       YASCRIPT UNINSTALLATION         ${NC}"
echo -e "${RED}=======================================${NC}"

# 1. Ask where the symlink is located
DEFAULT_BIN_DIR="$HOME/.local/bin"
echo
echo -e "Enter the directory where 'yascript' was linked."
echo -e "Default is: ${BLUE}$DEFAULT_BIN_DIR${NC}"
read -p "Path (Press Enter for default): " USER_DIR

BIN_DIR="${USER_DIR:-$DEFAULT_BIN_DIR}"
TARGET="$BIN_DIR/yascript"

# 2. Remove the symlink if found
echo
if [ -L "$TARGET" ] || [ -f "$TARGET" ]; then
    echo -e "${YELLOW}NOTE: Removing symlink at $TARGET...${NC}"
    rm "$TARGET"
    echo -e "${GREEN}Successfully removed global command link.${NC}"
else
    echo -e "${YELLOW}NOTE: No installation found at $TARGET. Skipping.${NC}"
fi

# 3. Interactive build cleanup
echo
read -p "Do you also want to remove local build files and binaries? (y/N): " CLEAN_CHOICE
if [[ "$CLEAN_CHOICE" =~ ^[Yy]$ ]]; then
    echo -e "${YELLOW}NOTE: Running 'make clean' to remove compiled objects...${NC}"
    make clean
fi

echo
echo -e "${GREEN}=======================================${NC}"
echo -e "${GREEN} Yascript uninstallation complete.      ${NC}"
echo -e "${GREEN}=======================================${NC}"
echo
