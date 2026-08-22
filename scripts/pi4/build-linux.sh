#!/usr/bin/env bash
# Build a commit-pinned upstream Linux image for QEMU raspi4b and Pi 400.
# SPDX-License-Identifier: GPL-2.0-or-later

set -euo pipefail

script_dir=$(CDPATH= cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)
repo_root=$(CDPATH= cd -- "$script_dir/../.." && pwd)

# shellcheck source=linux.lock
source "$script_dir/linux.lock"

output_dir=${PI4_LINUX_OUTPUT_DIR:-$repo_root/build-pi4-linux}
cache_dir=${PI4_LINUX_CACHE_DIR:-$repo_root/.cache/pi4-linux}
jobs=${JOBS:-$(getconf _NPROCESSORS_ONLN 2>/dev/null || echo 1)}
cross_compile=${CROSS_COMPILE:-}
allow_download=1

usage()
{
    cat <<EOF
Usage: $0 [options]

Options:
  --output DIR          Build and artifact directory
  --cache DIR           Verified source archive/cache directory
  --jobs N              Parallel build jobs (default: online CPUs)
  --cross-compile PFX   Compiler prefix, for example aarch64-linux-gnu-
  --no-download         Require the verified archive to exist in the cache
  -h, --help            Show this help

Environment equivalents: PI4_LINUX_OUTPUT_DIR, PI4_LINUX_CACHE_DIR, JOBS,
and CROSS_COMPILE.
EOF
}

die()
{
    echo "build-linux.sh: $*" >&2
    exit 1
}

while (($#)); do
    case $1 in
    --output)
        (($# >= 2)) || die "--output requires a directory"
        output_dir=$2
        shift 2
        ;;
    --cache)
        (($# >= 2)) || die "--cache requires a directory"
        cache_dir=$2
        shift 2
        ;;
    --jobs)
        (($# >= 2)) || die "--jobs requires a number"
        jobs=$2
        shift 2
        ;;
    --cross-compile)
        (($# >= 2)) || die "--cross-compile requires a prefix"
        cross_compile=$2
        shift 2
        ;;
    --no-download)
        allow_download=0
        shift
        ;;
    -h|--help)
        usage
        exit 0
        ;;
    *)
        die "unknown option: $1"
        ;;
    esac
done

[[ $jobs =~ ^[1-9][0-9]*$ ]] || die "jobs must be a positive integer"

[[ $(uname -s) == Linux ]] ||
    die "a Linux build host is required (use a Linux VM or container)"

if [[ -z $cross_compile && $(uname -m) != aarch64 ]]; then
    cross_compile=aarch64-linux-gnu-
fi

for command in make python3 tar; do
    command -v "$command" >/dev/null ||
        die "required command not found: $command"
done
if ((allow_download)); then
    command -v curl >/dev/null || die "required command not found: curl"
fi
command -v "${cross_compile}gcc" >/dev/null ||
    die "compiler not found: ${cross_compile}gcc (use --cross-compile)"

sha256_file()
{
    if command -v sha256sum >/dev/null; then
        sha256sum "$1" | awk '{ print $1 }'
    elif command -v shasum >/dev/null; then
        shasum -a 256 "$1" | awk '{ print $1 }'
    else
        python3 -c \
            'import hashlib, pathlib, sys
print(hashlib.sha256(pathlib.Path(sys.argv[1]).read_bytes()).hexdigest())' \
            "$1"
    fi
}

mkdir -p "$cache_dir/download" "$cache_dir/source" "$output_dir"
archive=$cache_dir/download/linux-$LINUX_VERSION.tar.xz
download_tmp=
source_tmp=

cleanup()
{
    [[ -z $download_tmp ]] || rm -f -- "$download_tmp"
    [[ -z $source_tmp ]] || rm -rf -- "$source_tmp"
}
trap cleanup EXIT

if [[ -f $archive ]]; then
    actual_sha256=$(sha256_file "$archive")
    [[ $actual_sha256 == "$LINUX_SOURCE_SHA256" ]] ||
        die "cached archive has SHA-256 $actual_sha256," \
            "expected $LINUX_SOURCE_SHA256"
else
    ((allow_download)) || die "verified archive is not cached: $archive"
    download_tmp=$(mktemp "$cache_dir/download/.linux-$LINUX_VERSION.XXXXXX")
    echo "Downloading $LINUX_SOURCE_URL"
    curl --fail --location --output "$download_tmp" "$LINUX_SOURCE_URL"
    actual_sha256=$(sha256_file "$download_tmp")
    [[ $actual_sha256 == "$LINUX_SOURCE_SHA256" ]] ||
        die "download has SHA-256 $actual_sha256, expected $LINUX_SOURCE_SHA256"
    mv "$download_tmp" "$archive"
    download_tmp=
fi

source_dir=$cache_dir/source/linux-$LINUX_VERSION
source_marker=$source_dir/.qemu-pi4-source-sha256
if [[ -d $source_dir ]]; then
    [[ -f $source_marker ]] ||
        die "existing source directory has no verification marker: $source_dir"
    [[ $(<"$source_marker") == "$LINUX_SOURCE_SHA256" ]] ||
        die "existing source directory was extracted from a different archive"
else
    source_tmp=$(mktemp -d "$cache_dir/source/.linux-$LINUX_VERSION.XXXXXX")
    tar -xJf "$archive" --strip-components=1 -C "$source_tmp"
    printf '%s\n' "$LINUX_SOURCE_SHA256" >"$source_tmp/.qemu-pi4-source-sha256"
    mv "$source_tmp" "$source_dir"
    source_tmp=
fi

build_dir=$output_dir/build
artifacts_dir=$output_dir/artifacts
mkdir -p "$build_dir" "$artifacts_dir"

echo "Configuring Linux $LINUX_VERSION ($LINUX_GIT_COMMIT)"
ARCH=arm64 "$source_dir/scripts/kconfig/merge_config.sh" -m -r \
    -O "$build_dir" \
    "$source_dir/arch/arm64/configs/defconfig" \
    "$script_dir/linux.config"

export ARCH=arm64
export CROSS_COMPILE=$cross_compile
export KBUILD_BUILD_HOST=qemu-pi4
export KBUILD_BUILD_TIMESTAMP=$LINUX_BUILD_TIMESTAMP
export KBUILD_BUILD_USER=qemu-pi4
export KBUILD_BUILD_VERSION=1
export SOURCE_DATE_EPOCH=$LINUX_SOURCE_DATE_EPOCH

make -C "$source_dir" O="$build_dir" olddefconfig

while IFS= read -r requested; do
    grep -Fqx "$requested" "$build_dir/.config" ||
        die "requested kernel configuration was not retained: $requested"
done < <(grep -E '^(CONFIG_[A-Za-z0-9_]+=|# CONFIG_[A-Za-z0-9_]+ is not set$)' \
           "$script_dir/linux.config")

make -C "$source_dir" O="$build_dir" -j"$jobs" \
    Image \
    broadcom/bcm2711-rpi-4-b.dtb \
    broadcom/bcm2711-rpi-400.dtb

init_binary=$build_dir/pi4-lab-init
"${cross_compile}gcc" -static -Os -s -fno-ident -Wl,--build-id=none \
    -o "$init_binary" "$script_dir/init.c"
python3 "$script_dir/mkinitramfs.py" --mtime "$LINUX_SOURCE_DATE_EPOCH" \
    "$init_binary" "$artifacts_dir/initramfs.cpio.gz"

hardware_init_binary=$build_dir/pi4-lab-hardware-init
"${cross_compile}gcc" -static -Os -s -fno-ident -Wl,--build-id=none \
    -o "$hardware_init_binary" "$script_dir/hardware-init.c"
python3 "$script_dir/mkinitramfs.py" --mtime "$LINUX_SOURCE_DATE_EPOCH" \
    "$hardware_init_binary" "$artifacts_dir/initramfs-hardware.cpio.gz"

install -m 0644 "$build_dir/arch/arm64/boot/Image" \
    "$artifacts_dir/Image"
install -m 0644 \
    "$build_dir/arch/arm64/boot/dts/broadcom/bcm2711-rpi-4-b.dtb" \
    "$artifacts_dir/bcm2711-rpi-4-b.dtb"
install -m 0644 \
    "$build_dir/arch/arm64/boot/dts/broadcom/bcm2711-rpi-400.dtb" \
    "$artifacts_dir/bcm2711-rpi-400.dtb"
install -m 0644 "$build_dir/.config" "$artifacts_dir/linux.config"
install -m 0644 "$script_dir/cmdline-hardware.txt" \
    "$artifacts_dir/cmdline-hardware.txt"
install -m 0644 "$script_dir/tryboot.txt" "$artifacts_dir/tryboot.txt"

manifest=$artifacts_dir/manifest.txt
{
    printf 'linux_version=%s\n' "$LINUX_VERSION"
    printf 'linux_git_commit=%s\n' "$LINUX_GIT_COMMIT"
    printf 'linux_source_url=%s\n' "$LINUX_SOURCE_URL"
    printf 'linux_source_sha256=%s\n' "$LINUX_SOURCE_SHA256"
    for artifact in Image bcm2711-rpi-4-b.dtb bcm2711-rpi-400.dtb \
                    initramfs.cpio.gz initramfs-hardware.cpio.gz \
                    cmdline-hardware.txt tryboot.txt linux.config; do
        printf '%s_sha256=%s\n' "$artifact" \
            "$(sha256_file "$artifacts_dir/$artifact")"
    done
} >"$manifest"

compiler_version=$("${cross_compile}gcc" --version)
compiler_version=${compiler_version%%$'\n'*}
{
    printf 'compiler=%s\n' "$compiler_version"
    printf 'build_arch=%s\n' "$(uname -m)"
    printf 'jobs=%s\n' "$jobs"
} >"$artifacts_dir/build-provenance.txt"

echo
echo "Pi 4/400 Linux artifacts: $artifacts_dir"
cat "$manifest"
