<!-- SPDX-FileCopyrightText: 2026 VCPU
     SPDX-License-Identifier: GPL-3.0-or-later -->

# NetPrivacy

Configure network privacy during installation.

## Features

- MAC randomization (off/random/vendor-preserved/fixed)
- IPv6 privacy extensions (RFC 4941) or disable

## Configuration

```yaml
macPolicy: 0  # 0=off 1=random 2=vendor 3=fixed
ipv6Mode: 0   # 0=standard 1=privacy 2=disable
```

## Testing

```bash
export NETPRIVACY_TEST_ROOT=/tmp/test
sudo -E calamares -d
```

## Output Files

- `/etc/NetworkManager/conf.d/80-calamares-*.conf`
- `/etc/systemd/network/80-calamares-*.{link,network}`
