#!/bin/bash

# Exit immediately if any command fails
set -e

# ANSI Color Codes
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[0;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

echo
echo -e "${BLUE}=======================================${NC}"
echo -e "${BLUE}       YASCRIPT INSTALLATION           ${NC}"
echo -e "${BLUE}=======================================${NC}"

# 1. Build the project
echo
echo -e "${YELLOW}NOTE: Building yascript...${NC}"
make

# 2. Interactive installation path selection
DEFAULT_BIN_DIR="$HOME/.local/bin"
echo
echo -e "Enter the directory where you want to install the 'yascript' link."
echo -e "Default is: ${BLUE}$DEFAULT_BIN_DIR${NC}"
read -p "Path (Press Enter for default): " USER_DIR

# Use default if user pressed Enter
BIN_DIR="${USER_DIR:-$DEFAULT_BIN_DIR}"
TARGET="$BIN_DIR/yascript"
SOURCE="$(pwd)/bin/yascript"

# 3. Ensure the installation directory exists
echo
echo -e "${YELLOW}NOTE: Ensuring $BIN_DIR exists...${NC}"
mkdir -p "$BIN_DIR"

# 4. Remove older versions if they exist
if [ -L "$TARGET" ] || [ -f "$TARGET" ]; then
    echo
    echo -e "${YELLOW}NOTE: Removing older installation link from $TARGET...${NC}"
    rm "$TARGET"
fi

# 5. Create the symlink
echo
echo -e "${YELLOW}NOTE: Creating symlink in $BIN_DIR...${NC}"
ln -s "$SOURCE" "$TARGET"

# Check if the install directory is in the user's PATH
echo
if [[ ":$PATH:" != *":$BIN_DIR:"* ]]; then
    echo -e "${YELLOW}WARNING: $BIN_DIR is not in your system PATH.${NC}"
    echo -e "You may need to add it to your shell configuration (.bashrc/.zshrc) to run 'yascript' globally."
    echo
fi

echo -e "${GREEN}=======================================${NC}"
echo -e "${GREEN} Yascript successfully installed!      ${NC}"
echo -e "${GREEN}=======================================${NC}"
echo -e "Binary linked at: ${BLUE}$TARGET${NC}"
echo -e "You can now run your scripts using: ${GREEN}yascript <filename>${NC}"
echo -e "Check out the 'examples/' directory for sample code to get started."
echo
