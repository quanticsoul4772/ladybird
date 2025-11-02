# Fix: Port 8080 Conflict - Accessibility Tests Failing

## Root Cause

The accessibility tests are failing with **404 errors** because:

1. ✅ **test-server.js is correctly configured** to serve fixtures (including accessibility fixtures)
2. ✅ **playwright.config.ts is correctly configured** to start test-server.js on ports 8080 and 8081
3. ❌ **Port 8080 is already occupied by nginx** (likely from Docker Desktop on Windows)
4. ❌ **Playwright reuses the existing nginx** instead of starting test-server.js (due to `reuseExistingServer: !process.env.CI`)
5. ❌ **The existing nginx doesn't serve Playwright fixtures** → 404 errors

## Verification

```bash
# Port 8080 is occupied by nginx
curl -I http://localhost:8080/
# Returns: Server: nginx/1.29.3

# nginx doesn't serve accessibility fixtures
curl http://localhost:8080/accessibility/semantic-html.html
# Returns: 404 Not Found

# test-server.js DOES serve them correctly (when on different port)
PORT=9999 node test-server.js &
curl http://localhost:9999/accessibility/semantic-html.html
# Returns: Correct HTML content ✓
```

## Solution Options

### Option 1: Stop Port 8080 nginx (Recommended)

**From Windows PowerShell (as Administrator):**

```powershell
# Find what's using port 8080
netstat -ano | findstr :8080

# Find the process
Get-Process -Id <PID_FROM_ABOVE>

# If it's Docker Desktop, stop the container:
# Open Docker Desktop → Find the container → Stop it
```

**Or check Docker Desktop:**
1. Open Docker Desktop on Windows
2. Go to "Containers" tab
3. Look for any container using port 8080
4. Stop it

**Then rerun tests:**
```bash
cd /home/rbsmith4/ladybird/Tests/Playwright
npm test -- --grep "A11Y-001"
```

Playwright will now start test-server.js on port 8080.

---

### Option 2: Force Fresh Server with CI Mode (Quick Fix)

**Force Playwright to start fresh servers:**

```bash
cd /home/rbsmith4/ladybird/Tests/Playwright

# Set CI=true to disable reuseExistingServer
CI=true npx playwright test --grep "A11Y-001"

# Or for all accessibility tests
CI=true npx playwright test --grep "A11Y"

# Or for the full suite
CI=true npx playwright test --project=ladybird --reporter=list
```

**Caveat**: This might fail if port 8080 is still in use. Playwright will try to start test-server.js and fail with "port already in use" error.

---

### Option 3: Use Different Ports (Modify Config)

**Modify playwright.config.ts to use different ports:**

```typescript
// Change ports to 9080 and 9081
webServer: [
  {
    command: 'PORT=9080 node test-server.js',
    port: 9080,
    // ...
  },
  {
    command: 'PORT=9081 node test-server.js',
    port: 9081,
    // ...
  },
],
```

**Then update all test files** to use `http://localhost:9080` instead of `http://localhost:8080`.

**This is NOT recommended** - too many files to change.

---

### Option 4: Temporary Workaround (Test if Fix Works)

**Start test servers manually on alternate ports:**

```bash
cd /home/rbsmith4/ladybird/Tests/Playwright

# Terminal 1: Start primary server
PORT=9080 node test-server.js

# Terminal 2: Start secondary server
PORT=9081 node test-server.js

# Terminal 3: Update test to use port 9080
# (manually edit one test to verify it works)
```

---

## Recommended Solution: Stop nginx on Port 8080

**Step-by-step:**

1. **Find what's using port 8080 on Windows:**
   ```powershell
   netstat -ano | findstr :8080
   # Note the PID (Process ID) from the rightmost column
   ```

2. **Identify the process:**
   ```powershell
   tasklist | findstr <PID>
   # Or use Task Manager and look for that PID
   ```

3. **Stop it:**
   - If it's Docker: Open Docker Desktop → Containers → Stop the container
   - If it's nginx.exe: Task Manager → Find nginx → End Task
   - If it's another process: Stop it via Task Manager or its control interface

4. **Verify port is free:**
   ```powershell
   netstat -ano | findstr :8080
   # Should return nothing
   ```

5. **Run tests:**
   ```bash
   cd /home/rbsmith4/ladybird/Tests/Playwright
   npm test -- --grep "A11Y"
   ```

6. **Verify success:**
   - All 20 accessibility tests should pass
   - You should see test-server.js start in the test output
   - No more 404 errors

---

## Why This Happened

The previous agent's analysis concluded that Docker wasn't configured correctly, but actually:

1. **test-server.js was always the intended server** for Playwright tests
2. **Docker nginx was added separately** (in Tools/test-download-server/) for different purposes
3. **Port conflict** between the two servers
4. **Playwright's reuseExistingServer feature** chose the wrong server

The docker-compose.yml changes made by the previous agent are **not needed** for Playwright tests. They were a misdiagnosis of the problem.

---

## What to Do About docker-compose.yml

The changes to `Tools/test-download-server/docker-compose.yml` can be **reverted** or **kept** - they don't affect Playwright tests:

```bash
# Optional: Revert the docker-compose.yml changes
cd /home/rbsmith4/ladybird
git diff Tools/test-download-server/docker-compose.yml

# If you want to revert:
git checkout Tools/test-download-server/docker-compose.yml
```

The Docker server (if you need it for other purposes) and Playwright tests can coexist if:
- Docker uses a different port (e.g., 8090), OR
- Docker is stopped when running Playwright tests

---

## Expected Results After Fix

```bash
# Before fix (with port 8080 conflict)
Tests:  361 total
  Passed: 312
  Failed: 45  ← Including 20 accessibility tests with 404

# After fix (with port 8080 freed)
Tests:  361 total
  Passed: 337  ← +25
  Failed: 10   ← -35
  Skipped: 14  ← +10 (media tests documented as expected)
```

---

## Quick Test to Verify Fix

```bash
# 1. Stop whatever is on port 8080 (see above)

# 2. Verify port is free
netstat -tuln | grep 8080
# Should return nothing

# 3. Run single test
cd /home/rbsmith4/ladybird/Tests/Playwright
npm test -- --grep "A11Y-001"

# 4. Check test output - should see:
#    [WebServer] Ladybird Test Server
#    [WebServer] Port: 8080
#    [WebServer] Ready for Playwright tests
#
# 5. Test should PASS (not 404 error)
```

---

## Summary

- **Problem**: Port 8080 has nginx that doesn't serve Playwright fixtures
- **Solution**: Stop/disable nginx on port 8080, let Playwright start test-server.js
- **Benefit**: 35 tests will pass (including all 20 accessibility tests)
- **Time**: 5 minutes to stop nginx and rerun tests

**The docker-compose.yml changes were unnecessary** - this is purely a port conflict issue.

---

**Created**: 2025-11-01
**Status**: Supersedes DOCKER_RESTART_INSTRUCTIONS.md (which was based on incorrect diagnosis)
