#!/bin/bash
# Create releases on Forgejo and GitHub with changelog and artifacts
#
# Prerequisites:
#   - jq: sudo apt install jq
#   - gh: sudo apt install gh (for GitHub releases)
#   - FORGEJO_TOKEN env var (create at Forgejo Settings > Applications)
#   - gh auth login (for GitHub)
#
# Usage:
#   ./release.sh              # Release current version from CMakeLists.txt
#   ./release.sh v0.9.0       # Release a specific tag
#   ./release.sh --hierarchical # Release current + all prior unreleased tags

set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
FORGEJO_URL="${FORGEJO_URL:-https://forgejo.ecliptik.com}"
FORGEJO_REPO="${FORGEJO_REPO:-ecliptik/flynn}"
GITHUB_REPO="${GITHUB_REPO:-ecliptik/flynn}"

# Extract changelog section for a given version (without the v prefix)
extract_changelog() {
    local ver="$1"
    # Extract lines between ## [ver] and the next ## [, stripping the header
    sed -n "/^## \[${ver}\]/,/^## \[/{/^## \[${ver}\]/d;/^## \[/d;p}" "$SCRIPT_DIR/CHANGELOG.md"
}

# Create a release on Forgejo
release_forgejo() {
    local tag="$1"
    local name="$2"
    local body="$3"
    local dsk="$4"
    local hqx="$5"

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
    for file in "$dsk" "$hqx"; do
        if [ -f "$file" ]; then
            local filename
            filename=$(basename "$file")
            echo "  Uploading $filename..."
            curl -s -X POST \
                "$FORGEJO_URL/api/v1/repos/$FORGEJO_REPO/releases/$release_id/assets?name=$filename" \
                -H "Authorization: token $FORGEJO_TOKEN" \
                -H "Content-Type: application/octet-stream" \
                --data-binary @"$file" > /dev/null
        fi
    done
    echo "  Forgejo release complete: $FORGEJO_URL/$FORGEJO_REPO/releases/tag/$tag"
}

# Create a release on GitHub
release_github() {
    local tag="$1"
    local name="$2"
    local body="$3"
    local dsk="$4"
    local hqx="$5"

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

    # Build file args
    local files=()
    [ -f "$dsk" ] && files+=("$dsk")
    [ -f "$hqx" ] && files+=("$hqx")

    gh release create "$tag" \
        --repo "$GITHUB_REPO" \
        --title "$name" \
        --notes "$body" \
        "${files[@]}"

    echo "  GitHub release complete: https://github.com/$GITHUB_REPO/releases/tag/$tag"
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

    # Find artifacts (check versioned names, fall back to build dir)
    local dsk="$SCRIPT_DIR/build/Flynn-${ver}.dsk"
    local hqx="$SCRIPT_DIR/build/Flynn-${ver}.hqx"

    if [ ! -f "$dsk" ] && [ ! -f "$hqx" ]; then
        echo "Warning: No artifacts found for $ver (looked for Flynn-${ver}.dsk/.hqx in build/)"
        echo "  Run ./build.sh first, or artifacts will be skipped"
    fi

    local name="Flynn $tag"
    release_forgejo "$tag" "$name" "$body" "$dsk" "$hqx"
    release_github "$tag" "$name" "$body" "$dsk" "$hqx"
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
        local_github=$(github_release_exists "$tag" && echo "yes" || echo "no")
        if [ "$local_forgejo" = "yes" ] && [ "$local_github" = "yes" ]; then
            echo "  $tag: already released on both platforms, skipping"
        else
            [ "$local_forgejo" = "yes" ] && echo "  $tag: already on Forgejo, checking GitHub..."
            [ "$local_github" = "yes" ] && echo "  $tag: already on GitHub, checking Forgejo..."
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
