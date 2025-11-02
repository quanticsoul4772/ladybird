# 🚀 START HERE - nsjail Installation

## Ready to Install nsjail in 3 Commands

### ✅ What's Ready

All installation scripts and documentation have been created:

```
Services/Sentinel/Sandbox/
├── 📜 install_nsjail.sh          ← Run this (executable)
├── 📜 verify_nsjail.sh           ← Then run this (executable)
├── 📖 QUICK_START_NSJAIL.md      ← Quick 3-step guide
├── 📖 README.md                  ← Full reference
├── 📖 NSJAIL_INSTALLATION.md     ← Complete documentation
├── 📖 INSTALLATION_SUMMARY.md    ← Detailed analysis
└── 📖 NSJAIL_SETUP_COMPLETE.md   ← Final report
```

---

## 🎯 3 Simple Steps

### Step 1️⃣: Install nsjail

```bash
cd /home/rbsmith4/ladybird/Services/Sentinel/Sandbox
./install_nsjail.sh
```

⏱️ **Duration**: 2-5 minutes
🔑 **Requires**: sudo password

**What happens**:
- Checks for nsjail package in apt
- If unavailable, builds from source
- Installs all dependencies
- Installs to /usr/local/bin/nsjail
- Runs basic verification

---

### Step 2️⃣: Verify Installation

```bash
./verify_nsjail.sh
```

⏱️ **Duration**: 1-2 minutes
✅ **Expected**: 12 tests, all pass

**What's tested**:
- Basic execution
- Resource limits (memory, CPU, file size)
- Time limit enforcement
- Namespace isolation (user, PID, mount, UTS)
- Multiple consecutive executions

---

### Step 3️⃣: Update Documentation

Edit `NSJAIL_INSTALLATION.md` and add:
- Version from `nsjail --version`
- Today's date
- Installation method (package or source)

---

## ✅ Success Checklist

After completing the steps above:

- [ ] `which nsjail` returns `/usr/local/bin/nsjail`
- [ ] `nsjail --version` shows version info
- [ ] All 12 verification tests pass
- [ ] Documentation updated with version

---

## 🔍 Quick Test

Test manually after installation:

```bash
nsjail --mode o --time_limit 5 -- /bin/echo "It works!"
```

**Expected output**:
```
[timestamp][I][init] Mode: STANDALONE_ONCE
It works!
[timestamp][I][subprocDone] PID: XXXXX exited with status: 0
```

---

## 📚 Documentation

| File | Purpose |
|------|---------|
| `QUICK_START_NSJAIL.md` | 3-step quick guide |
| `README.md` | Usage examples & reference |
| `NSJAIL_INSTALLATION.md` | Complete installation guide |
| `INSTALLATION_SUMMARY.md` | Technical details |
| `NSJAIL_SETUP_COMPLETE.md` | Final comprehensive report |

---

## ⏭️ Next Steps

After successful installation, proceed to:

**Phase 2, Week 1, Task 2**: Create Sentinel configuration file
- `SentinelConfig.h`
- `SentinelConfig.cpp`
- `sentinel.conf`

---

## 🆘 Need Help?

**Installation fails?** → See `NSJAIL_INSTALLATION.md` troubleshooting
**Tests fail?** → See `README.md` troubleshooting section
**Build errors?** → Check `INSTALLATION_SUMMARY.md` for dependencies

---

## 💡 One-Liner to Get Started

```bash
cd /home/rbsmith4/ladybird/Services/Sentinel/Sandbox && ./install_nsjail.sh && ./verify_nsjail.sh
```

This runs installation and verification in one command!

---

**Ready? Run the command above to begin! 🚀**
