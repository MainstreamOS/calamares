<!-- SPDX-FileCopyrightText: 2026 VCPU
     SPDX-License-Identifier: GPL-3.0-or-later -->

# NetPrivacy

Configure network privacy during installation.

## Features

- MAC randomization (off/random/vendor-preserved/fixed)
- IPv6 privacy extensions (RFC 4941) or disable

## Configuration

```yaml
macPolicy: 0           # 0=off 1=random 2=vendor 3=fixed
ipv6Mode: 0            # 0=standard 1=privacy 2=disable
macAddress: ""         # used when macPolicy=3
selectedVendor: intel  # used when macPolicy=2
perConnectionRandom: false  # randomize per connection instead of fixed random

writeNetworkManagerConfig: true   # write NM configs
writeSystemdNetworkdConfig: true  # write systemd-networkd configs

vendors:  # list of vendor OUIs for vendor mode
  - id: intel
    name: "Intel Corporation"
    oui: "00:1B:21"
```

## Testing

```bash
export NETPRIVACY_TEST_ROOT=/tmp/test
sudo -E calamares -d
```

## Output Files

- `/etc/NetworkManager/conf.d/80-calamares-*.conf`
- `/etc/systemd/network/80-calamares-*.{link,network}`

## Known Issues

- Breaks VirtualBox/VMware DHCP after reboot
- Breaks servers with MAC restrictions (e.g. Hetzner)
- Per-connection mode breaks DHCP leases
- Both NM and systemd-networkd configs written if both enabled

Ref: https://github.com/Kicksecure/security-misc/issues/184

## Implementation Notes

- vendorOUI validated in constructor, falls back to 02:00:00 if invalid
- MAC addresses auto-formatted (trimmed, uppercase)
- Empty vendor selection defaults to first in list
- VM detection checks /sys/class/dmi/id/{product_name,sys_vendor}
