# !/bin/bash
ulimit -c unlimited
sudo bash -c "echo core > /proc/sys/kernel/core_pattern "