#!/usr/bin/env bash

if test -f /etc/debian_version; then
    apt-get autoremove -y --purge
fi

# Clean up any build cache
rm -rf /root/.cache
rm -rf /root/.cargo
rm -rf /.cargo

if test -x /usr/bin/ccache; then
    ccache --show-stats
fi

# Use FreeDesktop.org's DNS resolver, so that any change we need to make is
# instantly propagated.
# Note: any domain not under freedesktop.org will fail to resolve.
echo "nameserver 131.252.210.177" > /etc/resolv.conf
