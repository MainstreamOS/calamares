<!-- SPDX-FileCopyrightText: 2026 VCPU
     SPDX-License-Identifier: GPL-3.0-or-later -->

# NetPrivacy Module

Configures network privacy settings during installation.

## Features

- **MAC Address Randomization** - Prevents tracking across networks
- **IPv6 Privacy Extensions** - RFC 4941 temporary addresses

## Configuration

Edit `netprivacy.conf` to set defaults:

```yaml
macPolicy: 0        # 0=Off, 1=Random, 2=Vendor, 3=Fixed
ipv6Mode: 0         # 0=Standard, 1=Privacy, 2=Disable
```

## Testing Without Installation

Test the module without running a real installation:

```bash
# Create test directory
mkdir -p /tmp/netprivacy-test
export NETPRIVACY_TEST_ROOT=/tmp/netprivacy-test

# Run Calamares in debug mode
sudo -E calamares -d
```

Then verify the generated configuration files:

```bash
cat /tmp/netprivacy-test/etc/NetworkManager/conf.d/80-calamares-mac.conf
cat /tmp/netprivacy-test/etc/systemd/network/80-calamares-mac.link
cat /tmp/netprivacy-test/etc/systemd/network/80-calamares-ipv6.network
```

## Generated Files

| Path | Purpose |
|------|---------|
| `/etc/NetworkManager/conf.d/80-calamares-mac.conf` | MAC randomization (NetworkManager) |
| `/etc/systemd/network/80-calamares-mac.link` | MAC randomization (systemd-networkd) |
| `/etc/NetworkManager/conf.d/80-calamares-ipv6.conf` | IPv6 privacy (NetworkManager) |
| `/etc/systemd/network/80-calamares-ipv6.network` | IPv6 privacy (systemd-networkd) |
| `/etc/sysctl.d/99-calamares-ipv6.conf` | IPv6 disable (if selected) |
