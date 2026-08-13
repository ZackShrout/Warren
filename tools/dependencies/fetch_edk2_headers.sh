#!/bin/sh

set -eu

script_dir=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
repository_root=$(CDPATH= cd -- "$script_dir/../.." && pwd)
manifest="$repository_root/third_party/edk2/headers.sha256"

edk2_commit=b03a21a63e3bd001f52c527e5a57feddb53a690b
archive_sha256=a3160f2a4f6c574cf7ed929d863461530bc5843c2d5c892db2dc8c1ed02ff5f1
archive_prefix="edk2-$edk2_commit"
archive_url="https://github.com/tianocore/edk2/archive/$edk2_commit.tar.gz"

local_root="$repository_root/.warren"
download_dir="$local_root/downloads"
dependency_parent="$local_root/dependencies"
dependency_root="$dependency_parent/edk2-$edk2_commit"
archive_path="$download_dir/edk2-$edk2_commit.tar.gz"

mode=${1:---check}

fail()
{
    printf 'edk2-headers: error: %s\n' "$*" >&2
    exit 1
}

verify_headers()
{
    [ -d "$dependency_root" ] || return 1
    (cd "$dependency_root" && shasum -a 256 -c "$manifest" >/dev/null 2>&1)
}

case "$mode" in
    --root)
        printf '%s\n' "$dependency_root"
        exit 0
        ;;
    --check)
        if ! verify_headers; then
            fail "pinned headers are missing or invalid; run ./tools/bootstrap.sh --install"
        fi
        printf 'EDK2 headers: pinned snapshot verified\n'
        exit 0
        ;;
    --install)
        ;;
    *)
        fail "usage: fetch_edk2_headers.sh [--check | --install | --root]"
        ;;
esac

if verify_headers; then
    printf 'EDK2 headers: pinned snapshot already installed\n'
    exit 0
fi

mkdir -p "$download_dir" "$dependency_parent"

archive_is_valid=false
if [ -f "$archive_path" ]; then
    actual_archive_sha=$(shasum -a 256 "$archive_path" | awk '{ print $1 }')
    if [ "$actual_archive_sha" = "$archive_sha256" ]; then
        archive_is_valid=true
    fi
fi

if [ "$archive_is_valid" = false ]; then
    archive_tmp="$archive_path.tmp"
    rm -f "$archive_tmp"
    printf 'Downloading pinned EDK2 header source...\n'
    curl -fsSL "$archive_url" -o "$archive_tmp"
    actual_archive_sha=$(shasum -a 256 "$archive_tmp" | awk '{ print $1 }')
    if [ "$actual_archive_sha" != "$archive_sha256" ]; then
        rm -f "$archive_tmp"
        fail "archive SHA-256 mismatch"
    fi
    mv "$archive_tmp" "$archive_path"
fi

staging_root=$(mktemp -d "$dependency_parent/.edk2-staging.XXXXXX")
trap 'rm -rf "$staging_root"' EXIT HUP INT TERM

set -- "$archive_prefix/License.txt"
while IFS= read -r manifest_line
do
    relative_path=${manifest_line#*  }
    set -- "$@" "$archive_prefix/$relative_path"
done < "$manifest"

tar -xzf "$archive_path" -C "$staging_root" --strip-components=1 "$@"

(cd "$staging_root" && shasum -a 256 -c "$manifest" >/dev/null) || \
    fail "extracted header checksum verification failed"

case "$dependency_root" in
    "$dependency_parent"/edk2-*) ;;
    *) fail "refusing to replace an unexpected dependency path" ;;
esac

rm -rf "$dependency_root"
mv "$staging_root" "$dependency_root"
trap - EXIT HUP INT TERM

printf 'EDK2 headers: installed and verified at %s\n' "$dependency_root"
