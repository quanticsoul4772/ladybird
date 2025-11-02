# Docker Container Restart Instructions

## Why This Is Needed

The `docker-compose.yml` file was updated to mount Playwright test fixtures into the nginx container. Docker containers don't automatically pick up configuration changes - they must be restarted.

## Current Situation

```bash
# Trying to access accessibility fixture
curl http://localhost:8080/accessibility/semantic-html.html

# Returns: 404 Not Found
```

**Cause**: Docker container `sentinel-test-server` is running with old configuration (no volume mount).

**Effect**: All 20 accessibility tests fail with 404 errors.

---

## How to Restart (Choose One Method)

### Method 1: Docker Desktop GUI (Recommended)

**Easiest method for Windows users:**

1. **Open Docker Desktop** application on Windows
   - Look for Docker icon in system tray
   - Click to open Docker Desktop

2. **Find the Container**
   - Click "Containers" in left sidebar
   - Look for container named: `sentinel-test-server`
   - Status should show: "Running"

3. **Restart the Container**
   - Hover over the container row
   - Click the "Restart" button (circular arrow icon)
   - Wait for status to show "Running" again (5-10 seconds)

4. **Done!** The container is now running with the new volume mount.

---

### Method 2: PowerShell Command Line

**For users comfortable with command line:**

```powershell
# Navigate to docker-compose directory
cd C:\path\to\your\ladybird\Tools\test-download-server

# Restart the service
docker-compose restart

# Verify it's running
docker-compose ps
```

**Expected output:**
```
NAME                    COMMAND                  SERVICE       STATUS         PORTS
sentinel-test-server    "/docker-entrypoint.…"   test-server   Up 5 seconds   0.0.0.0:8080->80/tcp
```

---

### Method 3: Full Rebuild (If Restart Doesn't Work)

**Only use this if restart doesn't work:**

```powershell
cd C:\path\to\your\ladybird\Tools\test-download-server

# Stop and remove container
docker-compose down

# Rebuild and start with new configuration
docker-compose up -d

# Verify
docker-compose ps
```

---

## Verification Steps

### Step 1: Check Container Status

**In WSL2 Terminal:**
```bash
curl -I http://localhost:8080/
```

**Expected output:**
```
HTTP/1.1 200 OK
Server: nginx/1.29.3
...
```

### Step 2: Check Fixture Access

**In WSL2 Terminal:**
```bash
curl http://localhost:8080/accessibility/semantic-html.html | head -10
```

**Expected output:**
```html
<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>Semantic HTML Test Page</title>
...
```

**NOT this:**
```html
<html>
<head><title>404 Not Found</title></head>
<body>
<center><h1>404 Not Found</h1></center>
...
```

### Step 3: Run a Test

**In WSL2 Terminal:**
```bash
cd /home/rbsmith4/ladybird/Tests/Playwright
npm test -- --grep "A11Y-001"
```

**Expected result:**
```
✓ [ladybird] › tests/accessibility/a11y.spec.ts:47:7 › ... A11Y-001: Proper heading hierarchy ...
✓ [chromium-reference] › tests/accessibility/a11y.spec.ts:47:7 › ... A11Y-001: Proper heading hierarchy ...

2 passed (2s)
```

---

## Troubleshooting

### Container won't start?

**Check logs:**
```powershell
docker-compose logs test-server
```

**Common issues:**
- Port 8080 already in use by another application
- Docker Desktop not running
- Volume path doesn't exist

### Still getting 404?

**Verify volume mount is active:**
```powershell
docker inspect sentinel-test-server
```

Look for "Mounts" section - should show:
```json
"Mounts": [
    {
        "Type": "bind",
        "Source": "C:\\path\\to\\ladybird\\Tests\\Playwright\\fixtures",
        "Destination": "/usr/share/nginx/html",
        "Mode": "ro",
        ...
    }
]
```

If "Mounts" is empty or doesn't show this, try **Method 3** (full rebuild).

### WSL2 can't reach localhost:8080?

**Windows Firewall may be blocking WSL2:**
```powershell
# In PowerShell as Administrator
New-NetFirewallRule -DisplayName "WSL" -Direction Inbound -InterfaceAlias "vEthernet (WSL)" -Action Allow
```

Or try accessing via Windows IP:
```bash
# In WSL2
curl http://$(grep nameserver /etc/resolv.conf | awk '{print $2}'):8080/health
```

---

## What Changed in docker-compose.yml

### Before
```yaml
services:
  test-server:
    build: .
    container_name: sentinel-test-server
    ports:
      - "8080:80"
    restart: unless-stopped
    networks:
      - test-network
    # No volumes - nginx serves only default content
```

### After
```yaml
services:
  test-server:
    build: .
    container_name: sentinel-test-server
    ports:
      - "8080:80"
    restart: unless-stopped
    networks:
      - test-network
    volumes:
      # Mount Playwright test fixtures for accessibility tests
      - ../../Tests/Playwright/fixtures:/usr/share/nginx/html:ro
```

**Key difference**: The `volumes:` section mounts local directory into nginx's web root.

---

## After Restart is Complete

Run the full test suite to see improvements:

```bash
cd /home/rbsmith4/ladybird/Tests/Playwright
npx playwright test --project=ladybird --reporter=list
```

**Expected improvement:**
- Before: 45 failing tests
- After: 10 failing tests (35 fixed!)
- Accessibility tests (A11Y-*): All 20 should pass

---

## Need Help?

- **Full technical details**: See `PARALLEL_WORK_COMPLETION_REPORT.md`
- **Quick reference**: See `QUICK_START_AFTER_PARALLEL_WORK.md`
- **Docker documentation**: https://docs.docker.com/desktop/
- **Test failure details**: Check `test-results/` directory after running tests

---

**Created**: 2025-11-01 for Ladybird Playwright test suite fixes
