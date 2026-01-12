# nsjail Quick Start Guide

## Installation - 3 Simple Steps

### Step 1: Run Installation Script

```bash
cd /home/rbsmith4/ladybird/Services/Sentinel/Sandbox
./install_nsjail.sh
```

**Duration**: 2-5 minutes
**Requires**: sudo privileges

### Step 2: Verify Installation

```bash
./verify_nsjail.sh
```

**Duration**: 1-2 minutes
**Expected**: 12 tests, all should pass

### Step 3: Update Documentation

Edit `NSJAIL_INSTALLATION.md` and fill in:
- Installed version (from `nsjail --version`)
- Installation date
- Installation method (package or source)

## That's It!

You're now ready to proceed to:
- **Phase 2, Week 1, Task 2**: Create Sentinel configuration file

## Quick Test

Test nsjail manually:

```bash
# Basic test
nsjail --mode o --time_limit 5 -- /bin/echo "It works!"

# Should output:
# [timestamp][I][init] Mode: STANDALONE_ONCE
# It works!
# [timestamp][I][subprocDone] PID: XXXXX exited with status: 0
```

## Files Created

- ✓ `install_nsjail.sh` - Automated installer
- ✓ `verify_nsjail.sh` - Test suite
- ✓ `NSJAIL_INSTALLATION.md` - Full documentation
- ✓ `README.md` - Quick reference
- ✓ `INSTALLATION_SUMMARY.md` - Detailed summary

## Need Help?

See `NSJAIL_INSTALLATION.md` for:
- Troubleshooting
- Manual installation steps
- Detailed test descriptions
- Integration examples
