#!/usr/bin/env bash
# Publish Artifacts for Ladybird Release
# Handles packaging, signing, and distribution

set -euo pipefail

# Color output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m'

log_info() {
    echo -e "${GREEN}[INFO]${NC} $*"
}

log_warn() {
    echo -e "${YELLOW}[WARN]${NC} $*"
}

log_error() {
    echo -e "${RED}[ERROR]${NC} $*"
}

log_section() {
    echo -e "${BLUE}========================================${NC}"
    echo -e "${BLUE}$*${NC}"
    echo -e "${BLUE}========================================${NC}"
}

# Configuration
BUILD_DIR="${BUILD_DIR:-Build/distribution}"
ARTIFACT_DIR="${ARTIFACT_DIR:-artifacts}"
VERSION="${VERSION:-dev}"
SIGN_ARTIFACTS="${SIGN_ARTIFACTS:-false}"

# Detect OS
detect_os() {
    if [[ "$OSTYPE" == "linux-gnu"* ]]; then
        echo "linux"
    elif [[ "$OSTYPE" == "darwin"* ]]; then
        echo "macos"
    elif [[ "$OSTYPE" == "msys" ]] || [[ "$OSTYPE" == "cygwin" ]]; then
        echo "windows"
    else
        echo "unknown"
    fi
}

# Create artifact directory
setup_artifact_dir() {
    log_info "Creating artifact directory: $ARTIFACT_DIR"
    mkdir -p "$ARTIFACT_DIR"
}

# Package Linux artifacts
package_linux() {
    log_section "Packaging Linux Artifacts"

    cd "$BUILD_DIR"

    # Create tarball
    log_info "Creating tarball..."
    cpack -G TGZ
    mv *.tar.gz "$OLDPWD/$ARTIFACT_DIR/"

    # Create DEB package
    if command -v dpkg-deb &> /dev/null; then
        log_info "Creating DEB package..."
        cpack -G DEB
        mv *.deb "$OLDPWD/$ARTIFACT_DIR/"
    else
        log_warn "dpkg-deb not found. Skipping DEB package."
    fi

    # Create RPM package
    if command -v rpmbuild &> /dev/null; then
        log_info "Creating RPM package..."
        cpack -G RPM
        mv *.rpm "$OLDPWD/$ARTIFACT_DIR/"
    else
        log_warn "rpmbuild not found. Skipping RPM package."
    fi

    # Create AppImage
    if command -v linuxdeploy &> /dev/null; then
        log_info "Creating AppImage..."

        linuxdeploy \
            --appdir AppDir \
            --executable bin/Ladybird \
            --desktop-file "$OLDPWD/Meta/Ladybird.desktop" \
            --icon-file "$OLDPWD/Meta/ladybird.png" \
            --output appimage

        mv Ladybird-*.AppImage "$OLDPWD/$ARTIFACT_DIR/"
    else
        log_warn "linuxdeploy not found. Skipping AppImage."
    fi

    cd - > /dev/null
}

# Package macOS artifacts
package_macos() {
    log_section "Packaging macOS Artifacts"

    cd "$BUILD_DIR"

    # Create tarball
    log_info "Creating tarball..."
    cpack -G TGZ
    mv *.tar.gz "$OLDPWD/$ARTIFACT_DIR/"

    # Create DMG
    log_info "Creating DMG..."
    cpack -G DragNDrop
    mv *.dmg "$OLDPWD/$ARTIFACT_DIR/"

    # Sign the app bundle (if credentials available)
    if [[ "$SIGN_ARTIFACTS" == "true" ]]; then
        sign_macos_bundle
    fi

    cd - > /dev/null
}

# Sign macOS app bundle
sign_macos_bundle() {
    log_info "Signing macOS app bundle..."

    if [ -z "${MACOS_CERTIFICATE:-}" ]; then
        log_warn "MACOS_CERTIFICATE not set. Skipping signing."
        return
    fi

    local app_bundle="Ladybird.app"

    if [ ! -d "$app_bundle" ]; then
        log_error "App bundle not found: $app_bundle"
        return 1
    fi

    # Sign with entitlements
    codesign \
        --deep \
        --force \
        --verify \
        --verbose \
        --sign "${MACOS_SIGNING_IDENTITY}" \
        --options runtime \
        --entitlements "$OLDPWD/Meta/macOS/Entitlements.plist" \
        "$app_bundle"

    log_info "App bundle signed successfully"

    # Notarize with Apple (requires authentication)
    if [ -n "${APPLE_ID:-}" ] && [ -n "${APPLE_APP_PASSWORD:-}" ]; then
        log_info "Notarizing with Apple..."

        xcrun notarytool submit \
            "$app_bundle" \
            --apple-id "$APPLE_ID" \
            --password "$APPLE_APP_PASSWORD" \
            --team-id "${APPLE_TEAM_ID}" \
            --wait

        # Staple notarization ticket
        xcrun stapler staple "$app_bundle"

        log_info "Notarization complete"
    fi
}

# Package Windows artifacts
package_windows() {
    log_section "Packaging Windows Artifacts"

    cd "$BUILD_DIR"

    # Create ZIP archive
    log_info "Creating ZIP archive..."
    cpack -G ZIP
    mv *.zip "$OLDPWD/$ARTIFACT_DIR/"

    # Create NSIS installer
    if command -v makensis &> /dev/null; then
        log_info "Creating NSIS installer..."
        cpack -G NSIS
        mv *.exe "$OLDPWD/$ARTIFACT_DIR/"
    else
        log_warn "makensis not found. Skipping NSIS installer."
    fi

    cd - > /dev/null
}

# Generate checksums
generate_checksums() {
    log_section "Generating Checksums"

    cd "$ARTIFACT_DIR"

    # SHA256 checksums
    log_info "Generating SHA256 checksums..."
    sha256sum * > SHA256SUMS.txt

    # GPG signature (if signing enabled)
    if [[ "$SIGN_ARTIFACTS" == "true" ]] && command -v gpg &> /dev/null; then
        log_info "Signing checksums with GPG..."

        if [ -n "${GPG_KEY_ID:-}" ]; then
            gpg --default-key "$GPG_KEY_ID" --detach-sign --armor SHA256SUMS.txt
            log_info "GPG signature created: SHA256SUMS.txt.asc"
        else
            log_warn "GPG_KEY_ID not set. Skipping GPG signature."
        fi
    fi

    cd - > /dev/null
}

# Create artifact manifest
create_manifest() {
    log_section "Creating Artifact Manifest"

    local manifest_file="$ARTIFACT_DIR/manifest.json"

    cat > "$manifest_file" <<EOF
{
  "version": "$VERSION",
  "timestamp": "$(date -u +"%Y-%m-%dT%H:%M:%SZ")",
  "git_commit": "${GIT_COMMIT:-$(git rev-parse HEAD 2>/dev/null || echo 'unknown')}",
  "git_branch": "${GIT_BRANCH:-$(git rev-parse --abbrev-ref HEAD 2>/dev/null || echo 'unknown')}",
  "build_os": "$(detect_os)",
  "artifacts": [
EOF

    local first=true
    for artifact in "$ARTIFACT_DIR"/*; do
        if [ "$artifact" == "$manifest_file" ]; then
            continue
        fi

        local filename=$(basename "$artifact")
        local size=$(stat -f%z "$artifact" 2>/dev/null || stat -c%s "$artifact" 2>/dev/null || echo 0)
        local sha256=$(sha256sum "$artifact" 2>/dev/null | awk '{print $1}' || echo "")

        if [ "$first" = true ]; then
            first=false
        else
            echo "," >> "$manifest_file"
        fi

        cat >> "$manifest_file" <<EOF
    {
      "filename": "$filename",
      "size": $size,
      "sha256": "$sha256"
    }
EOF
    done

    cat >> "$manifest_file" <<EOF

  ]
}
EOF

    log_info "Manifest created: $manifest_file"
}

# Upload to GitHub Release
upload_to_github() {
    log_section "Uploading to GitHub Release"

    if [ -z "${GITHUB_TOKEN:-}" ]; then
        log_warn "GITHUB_TOKEN not set. Skipping GitHub upload."
        return
    fi

    if ! command -v gh &> /dev/null; then
        log_error "GitHub CLI (gh) not found. Please install it first."
        return 1
    fi

    local release_tag="${RELEASE_TAG:-v$VERSION}"
    local repo="${GITHUB_REPOSITORY:-}"

    if [ -z "$repo" ]; then
        log_error "GITHUB_REPOSITORY not set"
        return 1
    fi

    log_info "Uploading artifacts to $repo release $release_tag..."

    # Check if release exists
    if ! gh release view "$release_tag" --repo "$repo" &> /dev/null; then
        log_error "Release $release_tag not found. Create it first."
        return 1
    fi

    # Upload all artifacts
    for artifact in "$ARTIFACT_DIR"/*; do
        local filename=$(basename "$artifact")
        log_info "Uploading $filename..."

        gh release upload "$release_tag" \
            "$artifact" \
            --repo "$repo" \
            --clobber  # Overwrite if exists
    done

    log_info "Upload complete"
}

# Generate download page
generate_download_page() {
    log_section "Generating Download Page"

    local download_page="$ARTIFACT_DIR/DOWNLOADS.md"

    cat > "$download_page" <<EOF
# Ladybird $VERSION Downloads

Released: $(date -u +"%Y-%m-%d %H:%M UTC")

## Quick Links

EOF

    # List artifacts by platform
    if ls "$ARTIFACT_DIR"/*.deb &> /dev/null; then
        echo "### Linux" >> "$download_page"
        for artifact in "$ARTIFACT_DIR"/*.{deb,rpm,tar.gz,AppImage} 2>/dev/null; do
            [ -f "$artifact" ] || continue
            local filename=$(basename "$artifact")
            local size=$(du -h "$artifact" | cut -f1)
            echo "- [$filename]($filename) ($size)" >> "$download_page"
        done
        echo "" >> "$download_page"
    fi

    if ls "$ARTIFACT_DIR"/*.dmg &> /dev/null; then
        echo "### macOS" >> "$download_page"
        for artifact in "$ARTIFACT_DIR"/*.dmg 2>/dev/null; do
            [ -f "$artifact" ] || continue
            local filename=$(basename "$artifact")
            local size=$(du -h "$artifact" | cut -f1)
            echo "- [$filename]($filename) ($size)" >> "$download_page"
        done
        echo "" >> "$download_page"
    fi

    if ls "$ARTIFACT_DIR"/*.exe &> /dev/null || ls "$ARTIFACT_DIR"/*.zip &> /dev/null; then
        echo "### Windows" >> "$download_page"
        for artifact in "$ARTIFACT_DIR"/*.{exe,zip} 2>/dev/null; do
            [ -f "$artifact" ] || continue
            local filename=$(basename "$artifact")
            local size=$(du -h "$artifact" | cut -f1)
            echo "- [$filename]($filename) ($size)" >> "$download_page"
        done
        echo "" >> "$download_page"
    fi

    cat >> "$download_page" <<'EOF'

## Verification

Verify the integrity of your download:

```bash
# Check SHA256 checksum
sha256sum -c SHA256SUMS.txt

# Verify GPG signature (if available)
gpg --verify SHA256SUMS.txt.asc SHA256SUMS.txt
```

## Installation

See [INSTALL.md](https://github.com/ladybird/ladybird/blob/master/INSTALL.md) for platform-specific installation instructions.
EOF

    log_info "Download page created: $download_page"
}

# Main
main() {
    log_info "Publishing Ladybird artifacts..."
    log_info "Version: $VERSION"
    log_info "Build directory: $BUILD_DIR"
    log_info "Artifact directory: $ARTIFACT_DIR"
    echo ""

    # Verify build exists
    if [ ! -d "$BUILD_DIR" ]; then
        log_error "Build directory not found: $BUILD_DIR"
        exit 1
    fi

    # Setup
    setup_artifact_dir

    # Package based on OS
    local os=$(detect_os)
    case "$os" in
        linux)
            package_linux
            ;;
        macos)
            package_macos
            ;;
        windows)
            package_windows
            ;;
        *)
            log_error "Unsupported OS: $os"
            exit 1
            ;;
    esac

    # Post-processing
    generate_checksums
    create_manifest
    generate_download_page

    # Upload (if configured)
    if [ -n "${GITHUB_TOKEN:-}" ]; then
        upload_to_github
    fi

    log_info "✓ Artifact publishing complete!"
    log_info "Artifacts available in: $ARTIFACT_DIR"
}

# Handle arguments
while [[ $# -gt 0 ]]; do
    case $1 in
        --build-dir)
            BUILD_DIR="$2"
            shift 2
            ;;
        --artifact-dir)
            ARTIFACT_DIR="$2"
            shift 2
            ;;
        --version)
            VERSION="$2"
            shift 2
            ;;
        --sign)
            SIGN_ARTIFACTS="true"
            shift
            ;;
        --help)
            echo "Usage: $0 [options]"
            echo ""
            echo "Options:"
            echo "  --build-dir DIR      Build directory (default: Build/distribution)"
            echo "  --artifact-dir DIR   Artifact output directory (default: artifacts)"
            echo "  --version VERSION    Release version (default: dev)"
            echo "  --sign               Sign artifacts (requires credentials)"
            echo "  --help               Show this help"
            exit 0
            ;;
        *)
            log_error "Unknown option: $1"
            exit 1
            ;;
    esac
done

main "$@"
