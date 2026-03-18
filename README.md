# CapStash Miner — Android (Termux)

Whirlpool-512 CPU miner for CapStash — built specifically for Android via Termux.

Compiled natively on your device with `-march=native` for maximum performance on your exact CPU.

Supports both **solo mining** (direct RPC to your node) and **pool mining** (stratum protocol).

---

## 📱 Requirements

- Android device (ARM64 — any modern Android phone)
- [Termux](https://f-droid.org/en/packages/com.termux/) from F-Droid (not Play Store)
- Stable internet connection
- At least 500MB free storage
- A CapStash wallet address (`cap1...`, `C...`, or `8...`)

---

## ⚡ Quick Start

### 1. Install Termux
Download from [F-Droid](https://f-droid.org/en/packages/com.termux/) — do **not** use the Play Store version, it is outdated and will fail to build.

### 2. Download and Run Setup Script

Open Termux and run:

```bash
curl -O https://raw.githubusercontent.com/CapStash/capstash-miner-android/main/setup_capstash_miner.sh
chmod +x setup_capstash_miner.sh
./setup_capstash_miner.sh
```

### 3. Configure During Setup

The interactive setup will ask:

- **Mining mode**: Solo (direct to your node) or Pool (stratum)
- **Solo**: Node IP, RPC port, RPC username, RPC password
- **Pool**: Pool URL, backup pool (optional), worker name
- **Reward address**: Your CapStash wallet address
- **Threads**: How many CPU threads to use (auto-detected optimal shown)

### 4. Start Mining

```bash
cd ~/capstash-miner
./start.sh
```

---

## 🎯 Solo vs Pool Mining

### Solo Mining
Connect directly to your own CapStash node. You keep 100% of the block reward.
Requires a running CapStash node with RPC enabled.

Your node's `CapStash.conf` must include:
```
server=1
rpcuser=youruser
rpcpassword=yourpass
rpcbind=127.0.0.1
rpcbind=100.x.x.x        ← your Tailscale IP
rpcallowip=127.0.0.1
rpcallowip=100.64.0.0/10  ← Tailscale subnet
rpcport=8332
```

Example setup:
```
Pool/Node: http://100.x.x.x:8332
User:      your_rpc_user
Pass:      your_rpc_password
```

### Pool Mining
Connect to a mining pool via stratum. Rewards split proportionally by hashrate.

Example setup:
```
Pool URL:  stratum+tcp://pool.capstash.net:3333
User:      cap1qyouraddress.phone-1
Pass:      x
```

---

## 🔄 Managing Your Miner

### Start Mining
```bash
cd ~/capstash-miner && ./start.sh
```

### Start on Backup Pool (pool mode only)
```bash
cd ~/capstash-miner && ./start-backup.sh
```

### View Current Configuration
```bash
cd ~/capstash-miner && ./info.sh
```

### Reconfigure (change pool, address, threads)
```bash
cd ~/capstash-miner && ./reconfigure.sh
```

### Keep Mining After Closing Termux
```bash
termux-wake-lock
cd ~/capstash-miner && ./start.sh
# Close Termux — mining continues in background
```

### Stop Mining
```bash
# Press Ctrl+C in Termux
termux-wake-unlock
```

---

## 📊 Thread Count Guide

The miner auto-detects the optimal thread count (50% of CPU threads) to avoid thermal throttling. You can override this during setup.

| Device Type | Cores | Recommended Threads |
|-------------|-------|-------------------|
| Budget (4-core) | 4 | 2 |
| Mid-range (6-core) | 6 | 3 |
| Mid-range (8-core) | 8 | 4 |
| Flagship (12-core) | 12 | 6 |

**Why 50%?** Whirlpool-512 uses large lookup tables (16KB). Beyond ~50% core usage these tables get evicted from L1 cache causing performance to drop. Test different thread counts and watch hashrate — the optimal point is where `hashrate/thread` starts declining.

---

## ⚠️ Important Notes

### Battery & Heat
- Keep phone plugged in while mining
- Monitor temperature — stop if device exceeds 45°C
- Remove phone case for better airflow
- Start with fewer threads and increase gradually

### Performance Tips
- Close all other apps while mining
- Enable Performance mode in phone settings if available
- Disable battery optimization for Termux
- Mine during cooler parts of the day

### Thermal Throttling Signs
- Hashrate drops significantly after a few minutes
- Device gets hot to touch
- Fix: reduce thread count by 1-2

---

## 🔧 Building Manually

If the setup script fails, build manually:

```bash
# Install dependencies
pkg update && pkg upgrade
pkg install clang cmake make git

# Clone repo
git clone https://github.com/CapStash/capstash-miner-android ~/capstash-miner
cd ~/capstash-miner

# Build
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release -DCMAKE_C_FLAGS="-O3 -march=native"
make -j$(nproc)
cp capstash-miner ..
cd ..
```

---

## 🔄 Updating

```bash
cd ~/capstash-miner
git pull
mkdir -p build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release -DCMAKE_C_FLAGS="-O3 -march=native"
make -j$(nproc)
cp capstash-miner ..
```

---

## 📁 File Structure

```
~/capstash-miner/
├── capstash-miner        ← compiled binary
├── start.sh              ← start mining
├── start-backup.sh       ← start on backup pool (pool mode)
├── reconfigure.sh        ← change settings
├── info.sh               ← view current config
└── mining-config.txt     ← saved configuration
```

---

## 🆘 Troubleshooting

**Build fails**
- Run `pkg update && pkg upgrade` then retry
- Ensure 500MB+ free storage: `df -h ~`
- Check internet connection

**"Address decode failed"**
- Verify address starts with `cap1`, `C`, or `8`
- Bech32 addresses (`cap1...`) must be lowercase

**"Failed to start — check node connection"**
- Solo: verify node IP, port, and RPC credentials
- Solo: confirm `rpcbind` includes your device's IP
- Pool: verify pool URL format `stratum+tcp://host:port`
- Test node from Termux: `curl -s --user "user:pass" --data '{"method":"getblockcount","params":[],"id":1}' -H 'Content-Type: application/json' http://NODE_IP:8332/`

**Low hashrate / drops over time**
- Reduce thread count — thermal throttling
- Keep phone cool and plugged in
- Check `info.sh` to verify thread count

**Miner stops after closing Termux**
- Run `termux-wake-lock` before starting

---

## ⚖️ Disclaimer

Mining generates significant heat and drains battery. Monitor your device. This software is provided as-is. Always mine responsibly.

---

*WALLET OF THE WASTELAND · STACK CAPS · SURVIVE*
