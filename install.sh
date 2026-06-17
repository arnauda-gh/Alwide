#!/usr/bin/env bash

set -e

# Configuration
REPO="arnauda-gh/Alwide"
INSTALL_DIR="$HOME/.local/bin"
CONFIG_DIR="$HOME/.config/alwide"
TMP_DIR=$(mktemp -d)

echo "🚀 Installing Alwide..."

# Check dependencies
if ! command -v curl &> /dev/null; then
    echo "❌ Error: curl is not installed."
    exit 1
fi

if ! command -v tar &> /dev/null; then
    echo "❌ Error: tar is not installed."
    exit 1
fi

if ! command -v unzip &> /dev/null; then
    echo "❌ Error: unzip is not installed."
    exit 1
fi

# Detect architecture
ARCH=$(uname -m)
case "$ARCH" in
    x86_64)
        BINARY_PATTERN="linux-x86_64.tar.gz"
        ;;
    aarch64|arm64)
        BINARY_PATTERN="linux-arm64.tar.gz"
        ;;
    *)
        echo "❌ Error: Unsupported architecture: $ARCH"
        exit 1
        ;;
esac

echo "🔍 Detected architecture: $ARCH"

# Get latest release info
echo "🔍 Fetching latest release information..."
RELEASE_INFO=$(curl -s "https://api.github.com/repos/$REPO/releases/latest")

# Check if we got a valid response
if echo "$RELEASE_INFO" | grep -q "message.*Not Found"; then
    echo "❌ Error: No release found. Please create a release (tag starting with 'v') on GitHub first."
    exit 1
fi

BINARY_URL=$(echo "$RELEASE_INFO" | grep "browser_download_url" | grep "$BINARY_PATTERN" | head -n 1 | cut -d '"' -f 4)
ASSETS_URL=$(echo "$RELEASE_INFO" | grep "browser_download_url" | grep "alwide-assets.zip" | head -n 1 | cut -d '"' -f 4)

if [ -z "$BINARY_URL" ] || [ -z "$ASSETS_URL" ]; then
    echo "❌ Error: Could not find release assets. Please check if a release exists on GitHub."
    exit 1
fi

# Create directories
mkdir -p "$INSTALL_DIR"
mkdir -p "$CONFIG_DIR"

# Download and install binary
echo "📥 Downloading binary..."
curl -L "$BINARY_URL" -o "$TMP_DIR/alwide.tar.gz"
tar -xzf "$TMP_DIR/alwide.tar.gz" -C "$TMP_DIR"
mv "$TMP_DIR/al" "$INSTALL_DIR/al"
chmod +x "$INSTALL_DIR/al"

# Download and install assets
echo "📥 Downloading assets..."
curl -L "$ASSETS_URL" -o "$TMP_DIR/assets.zip"
unzip -o "$TMP_DIR/assets.zip" -d "$TMP_DIR"
cp -r "$TMP_DIR/assets/"* "$CONFIG_DIR/"

# Cleanup
rm -rf "$TMP_DIR"

echo "✅ Alwide installed successfully!"
echo "📍 Binary: $INSTALL_DIR/al"
echo "📍 Config & Assets: $CONFIG_DIR"
echo ""

# Check if INSTALL_DIR is in PATH
case ":$PATH:" in
    *":$INSTALL_DIR:"*)
        echo "You can now run 'al' to start the editor."
        ;;
    *)
        echo "⚠️  $INSTALL_DIR is not in your PATH."
        echo "You can add it by running:"
        if [ -n "$SHELL" ] && [ "${SHELL##*/}" = "zsh" ]; then
            echo "    echo 'export PATH=\"\$PATH:\$HOME/.local/bin\"' >> ~/.zshrc"
            echo "    source ~/.zshrc"
        else
            echo "    echo 'export PATH=\"\$PATH:\$HOME/.local/bin\"' >> ~/.bashrc"
            echo "    source ~/.bashrc"
        fi
        echo ""
        echo "Then, you can run 'al' to start the editor."
        ;;
esac
