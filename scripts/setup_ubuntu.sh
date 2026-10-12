#!/usr/bin/env bash
# Installs everything needed on Ubuntu 22.04/24.04 (also works inside WSL2). Needs sudo.
set -euo pipefail
sudo apt update
sudo apt install -y gcc-arm-none-eabi binutils-arm-none-eabi cmake ninja-build gcc git python3 python3-pip python3-venv \
                    cppcheck clang-tidy gdb-multiarch curl
python3 -m pip install --user --break-system-packages pytest cryptography pyserial matplotlib || python3 -m pip install --user pytest cryptography pyserial matplotlib
if ! command -v renode >/dev/null 2>&1; then
  echo "Installing Renode (portable) into ~/renode ..."
  mkdir -p ~/renode && cd ~/renode
  curl -L -o renode.tgz https://github.com/renode/renode/releases/download/v1.17.0/renode-1.17.0.linux-portable.tar.gz
  tar xzf renode.tgz --strip-components=1 && rm renode.tgz
  echo "Add to your shell profile:   export PATH=\$HOME/renode:\$PATH   (and RENODE=\$HOME/renode/renode)"
fi
echo "OK. Next: ./scripts/build.sh"
