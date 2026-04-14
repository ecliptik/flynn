#!/bin/bash
# Create releases on Forgejo, Codeberg, and GitHub with changelog and artifacts
#
# Prerequisites:
#   - jq: sudo apt install jq
#   - gh: sudo apt install gh (for GitHub releases)
#   - FORGEJO_TOKEN env var (create at Forgejo Settings > Applications)
#   - CODEBERG_TOKEN env var (create at Codeberg Settings > Applications)
#   - gh auth login (for GitHub)
#
# Usage:
#   ./scripts/release.sh              # Release current version from CMakeLists.txt
#   ./scripts/release.sh v0.9.0       # Release a specific tag
#   ./scripts/release.sh --hierarchical # Release current + all prior unreleased tags

set -e

SCRIPT_DIR="$(cd "$(dirname "$0")/.." && pwd)"
FORGEJO_URL="${FORGEJO_URL:-https://forgejo.ecliptik.com}"
FORGEJO_REPO="${FORGEJO_REPO:-ecliptik/flynn}"
CODEBERG_URL="${CODEBERG_URL:-https://codeberg.org}"
CODEBERG_REPO="${CODEBERG_REPO:-ecliptik/flynn}"
GITHUB_REPO="${GITHUB_REPO:-ecliptik/flynn}"

# Extract changelog section for a given version (without the v prefix)
extract_changelog() {
    local ver="$1"
    # Extract lines between ## [ver] and the next ## [, stripping the header
    sed -n "/^## \[${ver}\]/,/^## \[/{/^## \[${ver}\]/d;/^## \[/d;p}" "$SCRIPT_DIR/CHANGELOG.md"
}

# Collect all release artifact files into RELEASE_FILES array
collect_artifacts() {
    local ver="$1"
    local build_dir="$SCRIPT_DIR/build"
    RELEASE_FILES=()
    for artifact in \
        "Flynn-${ver}.dsk" "Flynn-${ver}.hqx" "Flynn-${ver}.sit" \
        "Flynn-Lite-${ver}.dsk" "Flynn-Lite-${ver}.hqx" "Flynn-Lite-${ver}.sit" \
        "Flynn-Minimal-${ver}.dsk" "Flynn-Minimal-${ver}.hqx" "Flynn-Minimal-${ver}.sit"; do
        [ -f "$build_dir/$artifact" ] && RELEASE_FILES+=("$build_dir/$artifact")
    done
}

# Create a release on Forgejo
release_forgejo() {
    local tag="$1"
    local name="$2"
    local body="$3"

    if [ -z "$FORGEJO_TOKEN" ]; then
        echo "Warning: FORGEJO_TOKEN not set, skipping Forgejo release"
        return 0
    fi

    # Check if release already exists
    local existing
    existing=$(curl -s -o /dev/null -w "%{http_code}" \
        "$FORGEJO_URL/api/v1/repos/$FORGEJO_REPO/releases/tags/$tag" \
        -H "Authorization: token $FORGEJO_TOKEN")
    if [ "$existing" = "200" ]; then
        echo "  Forgejo release for $tag already exists, skipping"
        return 0
    fi

    echo "Creating Forgejo release for $tag..."
    local response
    response=$(curl -s -X POST "$FORGEJO_URL/api/v1/repos/$FORGEJO_REPO/releases" \
        -H "Authorization: token $FORGEJO_TOKEN" \
        -H "Content-Type: application/json" \
        -d "$(jq -n --arg tag "$tag" --arg name "$name" --arg body "$body" '{
            tag_name: $tag,
            name: $name,
            body: $body,
            draft: false,
            prerelease: false
        }')")

    local release_id
    release_id=$(echo "$response" | jq -r '.id // empty')
    if [ -z "$release_id" ]; then
        echo "Error creating Forgejo release: $response"
        return 1
    fi
    echo "  Created release ID: $release_id"

    # Upload artifacts
    for file in "${RELEASE_FILES[@]}"; do
        local filename
        filename=$(basename "$file")
        echo "  Uploading $filename..."
        curl -s -X POST \
            "$FORGEJO_URL/api/v1/repos/$FORGEJO_REPO/releases/$release_id/assets?name=$filename" \
            -H "Authorization: token $FORGEJO_TOKEN" \
            -H "Content-Type: application/octet-stream" \
            --data-binary @"$file" > /dev/null
    done
    echo "  Forgejo release complete: $FORGEJO_URL/$FORGEJO_REPO/releases/tag/$tag"
}

# Create a release on Codeberg (Forgejo-compatible API)
release_codeberg() {
    local tag="$1"
    local name="$2"
    local body="$3"

    if [ -z "$CODEBERG_TOKEN" ]; then
        echo "Warning: CODEBERG_TOKEN not set, skipping Codeberg release"
        return 0
    fi

    # Check if release already exists
    local existing
    existing=$(curl -s -o /dev/null -w "%{http_code}" \
        "$CODEBERG_URL/api/v1/repos/$CODEBERG_REPO/releases/tags/$tag" \
        -H "Authorization: token $CODEBERG_TOKEN")
    if [ "$existing" = "200" ]; then
        echo "  Codeberg release for $tag already exists, skipping"
        return 0
    fi

    echo "Creating Codeberg release for $tag..."
    local response
    response=$(curl -s -X POST "$CODEBERG_URL/api/v1/repos/$CODEBERG_REPO/releases" \
        -H "Authorization: token $CODEBERG_TOKEN" \
        -H "Content-Type: application/json" \
        -d "$(jq -n --arg tag "$tag" --arg name "$name" --arg body "$body" '{
            tag_name: $tag,
            name: $name,
            body: $body,
            draft: false,
            prerelease: false
        }')")

    local release_id
    release_id=$(echo "$response" | jq -r '.id // empty')
    if [ -z "$release_id" ]; then
        echo "Error creating Codeberg release: $response"
        return 1
    fi
    echo "  Created release ID: $release_id"

    # Upload artifacts
    for file in "${RELEASE_FILES[@]}"; do
        local filename
        filename=$(basename "$file")
        echo "  Uploading $filename..."
        curl -s -X POST \
            "$CODEBERG_URL/api/v1/repos/$CODEBERG_REPO/releases/$release_id/assets?name=$filename" \
            -H "Authorization: token $CODEBERG_TOKEN" \
            -H "Content-Type: application/octet-stream" \
            --data-binary @"$file" > /dev/null
    done
    echo "  Codeberg release complete: $CODEBERG_URL/$CODEBERG_REPO/releases/tag/$tag"
}

# Create a release on GitHub
release_github() {
    local tag="$1"
    local name="$2"
    local body="$3"

    if ! command -v gh >/dev/null 2>&1; then
        echo "Warning: gh CLI not installed, skipping GitHub release"
        return 0
    fi

    if ! gh auth status >/dev/null 2>&1; then
        echo "Warning: gh not authenticated, skipping GitHub release"
        return 0
    fi

    # Check if release already exists
    if gh release view "$tag" --repo "$GITHUB_REPO" >/dev/null 2>&1; then
        echo "  GitHub release for $tag already exists, skipping"
        return 0
    fi

    echo "Creating GitHub release for $tag..."

    # Ensure tags are pushed to GitHub
    if git remote get-url github >/dev/null 2>&1; then
        git push github "$tag" 2>/dev/null || true
    fi

    gh release create "$tag" \
        --repo "$GITHUB_REPO" \
        --title "$name" \
        --notes "$body" \
        "${RELEASE_FILES[@]}"

    echo "  GitHub release complete: https://github.com/$GITHUB_REPO/releases/tag/$tag"
}

# Build all three presets (build.sh produces correctly-named artifacts)
build_all_presets() {
    local ver="$1"
    local build_dir="$SCRIPT_DIR/build"

    echo "Building all presets for $ver..."

    # Full preset → Flynn-{ver}.*
    echo "  Building full preset..."
    "$SCRIPT_DIR/scripts/build.sh" --preset full

    # Lite preset → Flynn-Lite-{ver}.*
    echo "  Building lite preset..."
    "$SCRIPT_DIR/scripts/build.sh" --preset lite

    # Minimal preset → Flynn-Minimal-{ver}.*
    echo "  Building minimal preset..."
    "$SCRIPT_DIR/scripts/build.sh" --preset minimal

    echo "  All presets built:"
    ls -la "$build_dir"/Flynn*-${ver}.* 2>/dev/null
}

# Update README.md download links to point to this version
update_readme_downloads() {
    local tag="$1"
    local ver="${tag#v}"
    local readme="$SCRIPT_DIR/README.md"

    if [ ! -f "$readme" ]; then
        echo "  Warning: README.md not found, skipping download link update"
        return 0
    fi

    # Replace version in Codeberg release download URLs
    # Matches: /releases/download/vX.Y.Z/Flynn...-X.Y.Z.ext
    if grep -q "releases/download/v" "$readme"; then
        sed -i -E "s|releases/download/v[0-9]+\.[0-9]+\.[0-9]+/([A-Za-z-]*)-[0-9]+\.[0-9]+\.[0-9]+\.|releases/download/${tag}/\1-${ver}.|g" "$readme"
        echo "  Updated README.md download links to $tag"
    else
        echo "  Warning: No download links found in README.md"
    fi
}

# Release a single version
do_release() {
    local tag="$1"
    local ver="${tag#v}"

    echo "=== Releasing Flynn $tag ==="

    # Extract changelog
    local body
    body=$(extract_changelog "$ver")
    if [ -z "$body" ]; then
        echo "Warning: No changelog entry found for version $ver, using tag message"
        body="Release Flynn $tag"
    fi

    # Verify tag exists
    if ! git tag -l "$tag" | grep -q "$tag"; then
        echo "Error: Tag $tag does not exist"
        return 1
    fi

    # Build all presets if artifacts don't exist yet
    if [ ! -f "$SCRIPT_DIR/build/Flynn-${ver}.dsk" ]; then
        build_all_presets "$ver"
    fi

    # Collect all artifacts
    collect_artifacts "$ver"

    if [ ${#RELEASE_FILES[@]} -eq 0 ]; then
        echo "Warning: No artifacts found for $ver"
        echo "  Expected: Flynn-${ver}.dsk/.hqx/.sit, Flynn-Lite-${ver}.dsk/.hqx/.sit, Flynn-Minimal-${ver}.dsk/.hqx/.sit"
    else
        echo "  Artifacts: ${#RELEASE_FILES[@]} files"
        for f in "${RELEASE_FILES[@]}"; do echo "    $(basename "$f")"; done
    fi

    # Append preset descriptions to release body
    body="${body}

---

### Downloads

| Edition | Description | Memory |
|---------|-------------|--------|
| **Flynn** | Full build — 4 sessions, 192-line scrollback, all features including 256-color | ~768KB |
| **Flynn Lite** | Recommended — 1 session, 96-line scrollback, most features | ~384KB |
| **Flynn Minimal** | Bare-bones — 1 session, no scrollback, stripped features | ~256KB |

Each edition ships in three formats — pick whichever suits your transfer path:
- **.dsk** — 800K floppy disk image; mount in an emulator or write to a real floppy
- **.hqx** — BinHex archive; safe for email, BBS, and text-only channels
- **.sit** — StuffIt 1.5.1 archive; expand with StuffIt Expander on the Mac

See [BUILD.md](https://codeberg.org/$CODEBERG_REPO/src/branch/main/docs/BUILD.md) for custom build options."

    local name="Flynn $tag"
    release_forgejo "$tag" "$name" "$body"
    release_codeberg "$tag" "$name" "$body"
    release_github "$tag" "$name" "$body"
    update_readme_downloads "$tag"
    echo ""
}

# Check for existing releases on Forgejo
forgejo_release_exists() {
    local tag="$1"
    if [ -z "$FORGEJO_TOKEN" ]; then
        return 1
    fi
    local status
    status=$(curl -s -o /dev/null -w "%{http_code}" \
        "$FORGEJO_URL/api/v1/repos/$FORGEJO_REPO/releases/tags/$tag" \
        -H "Authorization: token $FORGEJO_TOKEN")
    [ "$status" = "200" ]
}

# Check for existing releases on Codeberg
codeberg_release_exists() {
    local tag="$1"
    if [ -z "$CODEBERG_TOKEN" ]; then
        return 1
    fi
    local status
    status=$(curl -s -o /dev/null -w "%{http_code}" \
        "$CODEBERG_URL/api/v1/repos/$CODEBERG_REPO/releases/tags/$tag" \
        -H "Authorization: token $CODEBERG_TOKEN")
    [ "$status" = "200" ]
}

# Check for existing releases on GitHub
github_release_exists() {
    local tag="$1"
    if ! command -v gh >/dev/null 2>&1; then
        return 1
    fi
    gh release view "$tag" --repo "$GITHUB_REPO" >/dev/null 2>&1
}

# Main
if [ "$1" = "--hierarchical" ]; then
    # Release all tags that don't have releases yet
    echo "Checking for unreleased tags..."
    for tag in $(git tag -l 'v*' --sort=version:refname); do
        local_forgejo=$(forgejo_release_exists "$tag" && echo "yes" || echo "no")
        local_codeberg=$(codeberg_release_exists "$tag" && echo "yes" || echo "no")
        local_github=$(github_release_exists "$tag" && echo "yes" || echo "no")
        if [ "$local_forgejo" = "yes" ] && [ "$local_codeberg" = "yes" ] && [ "$local_github" = "yes" ]; then
            echo "  $tag: already released on all platforms, skipping"
        else
            [ "$local_forgejo" = "yes" ] && echo "  $tag: already on Forgejo"
            [ "$local_codeberg" = "yes" ] && echo "  $tag: already on Codeberg"
            [ "$local_github" = "yes" ] && echo "  $tag: already on GitHub"
            do_release "$tag"
        fi
    done
elif [ -n "$1" ]; then
    # Release specific tag
    do_release "$1"
else
    # Release current version from CMakeLists.txt
    VERSION=$(grep -oP 'project\(Flynn VERSION \K[0-9]+\.[0-9]+\.[0-9]+' "$SCRIPT_DIR/CMakeLists.txt")
    if [ -z "$VERSION" ]; then
        echo "Error: Could not read version from CMakeLists.txt"
        exit 1
    fi
    do_release "v$VERSION"
fi
