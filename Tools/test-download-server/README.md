# Sentinel Download Test Server

A Docker-based HTTP server for testing Ladybird's Sentinel download scanning feature.

## Quick Start

```bash
# Start the test server
docker-compose up -d

# The server will be available at http://localhost:8080
```

## Test Files Available

The server hosts several test files to verify Sentinel's scanning capabilities:

1. **EICAR Test File** (`/downloads/eicar.txt`)
   - Industry-standard safe malware test file
   - **Expected Behavior**: Should trigger EICAR_Test_File YARA rule
   - **Action**: Sentinel should quarantine this file
   - Size: 68 bytes

2. **Obfuscated JavaScript** (`/downloads/obfuscated.js`)
   - JavaScript with suspicious patterns (eval, atob, unescape)
   - **Expected Behavior**: Should trigger Obfuscated_JavaScript YARA rule
   - **Action**: May trigger warning or quarantine depending on policy

3. **Safe Text File** (`/downloads/safe-file.txt`)
   - Plain text with no suspicious content
   - **Expected Behavior**: Should pass all scans
   - **Action**: Allow download normally

4. **Safe Image** (`/downloads/test-image.png`)
   - Minimal valid PNG image (1x1 pixel)
   - **Expected Behavior**: Should pass all scans
   - **Action**: Allow download normally

## Testing Workflow

1. **Start Sentinel** (if not already running):
   ```bash
   /home/rbsmith4/ladybird/Build/release/bin/Sentinel
   ```

2. **Start Test Server**:
   ```bash
   docker-compose up -d
   ```

3. **Open Ladybird Browser**:
   ```bash
   /home/rbsmith4/ladybird/Build/release/bin/Ladybird
   ```

4. **Navigate to Test Page**:
   - Go to: `http://localhost:8080`
   - You'll see a list of available test files

5. **Test Download Scanning**:
   - Click on "EICAR Test File"
   - Sentinel should intercept the download
   - You should see a security alert/quarantine dialog
   - Check `about:security` for scan results

6. **Verify Quarantine**:
   ```bash
   # List quarantined files
   /home/rbsmith4/ladybird/Build/release/bin/sentinel-cli list-quarantine

   # Check Sentinel status
   /home/rbsmith4/ladybird/Build/release/bin/sentinel-cli status
   ```

7. **Test Safe Downloads**:
   - Download the safe text file or image
   - Should complete without warnings

## Expected Results

### EICAR Test File
```
✓ Sentinel intercepts download
✓ YARA scan detects: EICAR_Test_File
✓ File is quarantined automatically
✓ User sees security alert with:
  - Rule: EICAR_Test_File
  - Severity: low
  - Description: EICAR anti-virus test file
✓ File NOT saved to Downloads folder
✓ File available in quarantine directory
```

### Obfuscated JavaScript
```
✓ Sentinel intercepts download
✓ YARA scan detects: Obfuscated_JavaScript
✓ User sees warning dialog
✓ Options: Allow, Block, Quarantine
✓ Can create policy for future JS files
```

### Safe Files
```
✓ Sentinel intercepts download
✓ YARA scan completes: No threats
✓ File downloads normally
✓ No user alerts
```

## Management Commands

```bash
# Stop the test server
docker-compose down

# Rebuild the server (after changing test files)
docker-compose up -d --build

# View server logs
docker-compose logs -f

# Access server shell
docker-compose exec test-server sh
```

## Troubleshooting

### Server won't start
```bash
# Check if port 8080 is already in use
lsof -i :8080

# Use a different port by editing docker-compose.yml:
ports:
  - "9090:80"  # Use port 9090 instead
```

### EICAR not detected
1. Check Sentinel is running: `ps aux | grep Sentinel`
2. Check socket: `ls -l /tmp/sentinel.sock`
3. Verify browser connection: `about:security` should show "Connected"
4. Check YARA rules loaded: Look at Sentinel terminal output

### Downloads not being scanned
1. Verify SecurityTap is enabled in RequestServer
2. Check Sentinel logs in terminal
3. Try the safe file first to confirm downloads work
4. Check browser console for IPC errors

## Architecture

```
┌─────────────┐
│   Browser   │
│  (Ladybird) │
└──────┬──────┘
       │ HTTP GET
       ▼
┌─────────────┐         ┌──────────────┐
│Docker nginx │         │   Sentinel   │
│   :8080     │────────▶│    Daemon    │
│ Test Files  │ Via IPC │ YARA Scanner │
└─────────────┘         └──────┬───────┘
                               │
                               ▼
                        ┌──────────────┐
                        │  Quarantine  │
                        │  Directory   │
                        └──────────────┘
```

## Clean Up

```bash
# Stop and remove server
docker-compose down

# Remove test files
rm -rf test-files/

# Clear quarantine
/home/rbsmith4/ladybird/Build/release/bin/sentinel-cli list-quarantine
# (manually restore or delete as needed)
```

## Notes

- **EICAR is safe**: The EICAR test file is recognized by all antivirus software but contains no actual malicious code. It's the industry standard for testing.
- **Test in isolation**: This server is for testing only. Don't expose it to public networks.
- **PolicyGraph**: Each quarantine action creates a policy entry. Check with `sentinel-cli list-policies`.
- **Performance**: First download may be slower due to YARA rule compilation. Subsequent scans are cached.

## What's Being Tested

This test server validates:
- ✅ Download interception via SecurityTap
- ✅ IPC communication between browser and Sentinel
- ✅ YARA rule matching and scanning
- ✅ Threat detection and classification
- ✅ User alert UI
- ✅ Quarantine workflow
- ✅ Policy creation from user decisions
- ✅ Safe file pass-through
- ✅ Database threat logging
- ✅ Metrics collection

## Further Testing

After basic testing works, try:
1. Create custom policies via `about:security`
2. Block specific URLs or MIME types
3. Test "Remember this decision" checkbox
4. Verify policy hit counts increment
5. Test quarantine restore functionality
6. Monitor performance metrics
7. Test with larger files
8. Test concurrent downloads
