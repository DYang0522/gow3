#!/usr/bin/env bash
# Package existing Bloodborne Linux binaries into an AppImage. This script does not build them.
set -euo pipefail
cd -- "$(dirname -- "$0")/.."
[[ -f out/bb-probe && -f out/bb-gpu-capabilities && -f out/gpu/libbbgpu.so ]] || {
    echo 'Missing: out/bb-probe, out/bb-gpu-capabilities or out/gpu/libbbgpu.so' >&2
    exit 1
}
root=$PWD
{
    echo '['
    for elf in out/bb-probe out/gpu/libbbgpu.so; do
        readelf -d "$elf" | sed -n 's/.*\[\(.*\)\]/\1/p' | tr ':' '\n'
    done | grep -o '^/nix/store/[^/]*' | sort -u | grep -v -- '-nix-shell$' | sed 's/.*/  "&"/'
    echo ']'
} > packaging/runtime-paths-bloodborne.nix
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT
(cd "$work" && nix bundle --impure --bundler github:ralismark/nix-appimage \
    --expr "import $root/packaging/bloodborne.nix {}")
mkdir -p dist
install -m755 "$work/bbport.AppImage" dist/Bloodborne-bbport-x86_64.AppImage
ls -lh dist/Bloodborne-bbport-x86_64.AppImage
