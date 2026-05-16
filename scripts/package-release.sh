#!/usr/bin/env bash
set -euo pipefail

# Draft local release packager.
#
# This script is intentionally manual. It builds checked-in PlatformIO projects,
# stages firmware binaries under release-artifacts/, creates ZIP packages, and
# checks generated text for local path leaks. It does not create GitHub Releases,
# upload firmware, open serial ports, or touch hardware.
#
# Do not commit release-artifacts/.

VERSION="${1:-v0.1.0}"
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
OUT_DIR="${ROOT}/release-artifacts"

projects=(
  "esp32-spd-tool:ESP32 SPD Tool:firmware/esp32-spd-tool"
  "esp32-ddr5-sniffer:DDR5 Boot Sniffer:firmware/esp32-boot-sniffer"
)

require_tool() {
  if ! command -v "$1" >/dev/null 2>&1; then
    echo "Missing required tool: $1" >&2
    exit 1
  fi
}

copy_if_present() {
  local src="$1"
  local dst="$2"
  if [[ -f "$src" ]]; then
    cp "$src" "$dst"
  fi
}

build_project() {
  local slug="$1"
  local title="$2"
  local rel_path="$3"
  local project_dir="${ROOT}/${rel_path}"

  if [[ ! -f "${project_dir}/platformio.ini" ]]; then
    echo "Skipping ${title}: ${rel_path}/platformio.ini not found"
    return 0
  fi

  echo "Building ${title} from ${rel_path}"
  pio run -d "$project_dir"

  local env_dir
  env_dir="$(find "${project_dir}/.pio/build" -mindepth 1 -maxdepth 1 -type d | sort | head -n 1)"
  if [[ -z "$env_dir" ]]; then
    echo "No PlatformIO build environment found for ${title}" >&2
    return 1
  fi

  local env_name
  env_name="$(basename "$env_dir")"
  local stage="${OUT_DIR}/${slug}-${VERSION}"

  rm -rf "$stage"
  mkdir -p "$stage"

  copy_if_present "${env_dir}/firmware.bin" "${stage}/firmware.bin"
  copy_if_present "${env_dir}/bootloader.bin" "${stage}/bootloader.bin"
  copy_if_present "${env_dir}/partitions.bin" "${stage}/partitions.bin"
  copy_if_present "${env_dir}/boot_app0.bin" "${stage}/boot_app0.bin"

  cat > "${stage}/README-FLASHING.txt" <<EOF
${title}
Version: ${VERSION}
Source project: ${rel_path}
PlatformIO environment: ${env_name}

This is a prebuilt firmware package. Normal users do not need the full
repository if using this package.

Flashing is separate from DDR5 wiring. Select the correct serial port for your
ESP32. Wiring mistakes can damage ESP32 boards, DIMMs, motherboards, or power
supplies. Management-plane tests do not prove DRAM stability. SPD editing is
experimental.

Typical ESP32 Arduino offsets, when all files are present:
  0x1000  bootloader.bin
  0x8000  partitions.bin
  0xe000  boot_app0.bin
  0x10000 firmware.bin

Example:
  esptool.py --chip esp32 --port PORT --baud 460800 write_flash \\
    0x1000 bootloader.bin \\
    0x8000 partitions.bin \\
    0xe000 boot_app0.bin \\
    0x10000 firmware.bin
EOF

  {
    echo "${title}"
    echo "Version: ${VERSION}"
    echo "Git commit: $(git -C "$ROOT" rev-parse HEAD)"
    echo "Source project: ${rel_path}"
    echo "PlatformIO environment: ${env_name}"
    echo
    echo "Included files:"
    find "$stage" -maxdepth 1 -type f -printf '%f\n' | sort
    echo
    echo "SHA256:"
    (cd "$stage" && sha256sum *.bin 2>/dev/null || true)
  } > "${stage}/BUILD-INFO.txt"

  (cd "$OUT_DIR" && zip -qr "${slug}-${VERSION}.zip" "${slug}-${VERSION}")
  echo "Created ${OUT_DIR}/${slug}-${VERSION}.zip"
}

main() {
  require_tool pio
  require_tool zip
  require_tool sha256sum

  mkdir -p "$OUT_DIR"

  for item in "${projects[@]}"; do
    IFS=":" read -r slug title rel_path <<< "$item"
    build_project "$slug" "$title" "$rel_path"
  done

  local local_home="${HOME:-}"
  if [[ -n "$local_home" ]]; then
    if grep -RInF "$local_home" "$OUT_DIR" >/tmp/package-release-path-leaks.txt 2>/dev/null; then
      echo "Found local path leaks in release-artifacts:" >&2
      cat /tmp/package-release-path-leaks.txt >&2
      exit 1
    fi
  fi

  echo
  echo "Done. Review release-artifacts/ manually."
  echo "Do not commit release-artifacts/."
  echo "This script did not create a GitHub Release."
}

main "$@"
