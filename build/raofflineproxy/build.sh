#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
SOURCE_DIR="${SOURCE_DIR:-${ROOT}/.build/raofflineproxy/sources}"
WORK_DIR="${WORK_DIR:-${ROOT}/.build/raofflineproxy/work}"
OUTPUT_DIR="${OUTPUT_DIR:-${ROOT}/.build/raofflineproxy/output}"
CC="${CC:-arm-linux-gnueabihf-gcc}"
EPOCH=1787097600

case "${WORK_DIR}" in
  "${ROOT}"/.build/raofflineproxy/*) ;;
  *) echo "WORK_DIR must remain under ${ROOT}/.build/raofflineproxy" >&2; exit 2 ;;
esac

python3 "${ROOT}/tools/validate_raofflineproxy_sources.py" \
    "${ROOT}/build/raofflineproxy/sources.json" --source-dir "${SOURCE_DIR}"

rm -rf -- "${WORK_DIR}"
mkdir -p "${WORK_DIR}/src" "${WORK_DIR}/package/runtime" "${OUTPUT_DIR}"
tar -xzf "${SOURCE_DIR}/raofflineproxy.tar.gz" -C "${WORK_DIR}/src"
tar -xzf "${SOURCE_DIR}/rcheevos.tar.gz" -C "${WORK_DIR}/src"
tar -xzf "${SOURCE_DIR}/libchdr.tar.gz" -C "${WORK_DIR}/src"
tar -xzf "${SOURCE_DIR}/python-runtime.tar.gz" -C "${WORK_DIR}/package/runtime"

# The durable Bloom component root is FAT32, which cannot represent the
# standalone runtime's Unix symlinks. Remove developer-only payloads and make
# the one runtime entrypoint a regular file before packaging.
PYTHON_ROOT="${WORK_DIR}/package/runtime/python"
rm -rf -- "${PYTHON_ROOT}/include" "${PYTHON_ROOT}/share" "${PYTHON_ROOT}/lib/pkgconfig"
rm -f -- "${PYTHON_ROOT}/bin/2to3" "${PYTHON_ROOT}/bin/2to3-3.9" \
  "${PYTHON_ROOT}/bin/idle3" "${PYTHON_ROOT}/bin/idle3.9" \
  "${PYTHON_ROOT}/bin/pydoc3" "${PYTHON_ROOT}/bin/pydoc3.9" \
  "${PYTHON_ROOT}/bin/python" "${PYTHON_ROOT}/bin/python3" \
  "${PYTHON_ROOT}/bin/python3-config" "${PYTHON_ROOT}/bin/python3.9-config" \
  "${PYTHON_ROOT}/lib/libpython3.9.so"
cp "${PYTHON_ROOT}/bin/python3.9" "${PYTHON_ROOT}/bin/python3"
LIBUTIL="$(${CC} -print-file-name=libutil.so.1)"
if [ ! -f "${LIBUTIL}" ]; then
  echo "Pinned toolchain does not provide libutil.so.1" >&2
  exit 1
fi
cp -L "${LIBUTIL}" "${PYTHON_ROOT}/lib/libutil.so.1"
if find "${WORK_DIR}/package/runtime" -type l -print -quit | grep -q .; then
  echo "FAT runtime still contains symbolic links" >&2
  exit 1
fi

RAP="$(find "${WORK_DIR}/src" -maxdepth 1 -type d -name 'RAOfflineProxy-*' -print -quit)"
RC="$(find "${WORK_DIR}/src" -maxdepth 1 -type d -name 'rcheevos-*' -print -quit)"
CHDR="$(find "${WORK_DIR}/src" -maxdepth 1 -type d -name 'libchdr-*' -print -quit)"
GLUE="${RAP}/third_party/rcheevos_glue"
OUT="${WORK_DIR}/package/raofflineproxy"
mkdir -p "${OUT}/licenses"
cp -R "${RAP}/linux/raofflineproxy" "${OUT}/raofflineproxy"
cp "${RAP}/LICENSE" "${OUT}/licenses/RAOfflineProxy-GPL-3.0.txt"
cp "${RC}/LICENSE" "${OUT}/licenses/rcheevos-MIT.txt"
cp "${CHDR}/LICENSE.txt" "${OUT}/licenses/libchdr-BSD-3-Clause.txt"
mkdir -p "${WORK_DIR}/package/bin"
printf '%s\n' '#!/bin/sh' \
  'HERE="$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)"' \
  'CA_FILE="${HERE}/runtime/python/lib/python3.9/site-packages/pip/_vendor/certifi/cacert.pem"' \
  '[ -f "${CA_FILE}" ] || { echo "raofflineproxy: CA bundle unavailable" >&2; exit 1; }' \
  'export PYTHONPATH="${HERE}/raofflineproxy"' \
  'export LD_LIBRARY_PATH="${HERE}/runtime/python/lib:${HERE}/raofflineproxy${LD_LIBRARY_PATH:+:${LD_LIBRARY_PATH}}"' \
  'export SSL_CERT_FILE="${CA_FILE}"' \
  'export RAOFFLINEPROXY_CA_FILE="${CA_FILE}"' \
  'exec "${HERE}/runtime/python/bin/python3" -m raofflineproxy.main "$@"' \
  > "${WORK_DIR}/package/bin/raofflineproxy"
chmod 0755 "${WORK_DIR}/package/bin/raofflineproxy"

"${CC}" -shared -Os -s -fPIC \
  -ffile-prefix-map="${WORK_DIR}"=/build/raofflineproxy \
  -fmacro-prefix-map="${WORK_DIR}"=/build/raofflineproxy \
  -Wl,--build-id=none \
  -DRC_HASH_NO_ENCRYPTED -DWANT_RAW_DATA_SECTOR=1 -DWANT_SUBCODE=1 -DVERIFY_BLOCK_CRC=1 \
  -I"${RC}/include" -I"${RC}/src" -I"${RC}/src/rhash" \
  -I"${GLUE}" -I"${GLUE}/shim" -I"${CHDR}/include" -I"${CHDR}/src" \
  -I"${CHDR}/deps/miniz-3.1.1" -I"${CHDR}/deps/lzma-25.01/include" -I"${CHDR}/deps/zstd-1.5.7" \
  -o "${OUT}/libraproxy_rchash.so" \
  "${RC}"/src/rhash/{hash.c,hash_rom.c,hash_disc.c,hash_zip.c,cdreader.c,md5.c} \
  "${RC}/src/rc_compat.c" \
  "${GLUE}"/{chd_stream.c,cdfs_chd.c,cdfs_pbp.c,strl_compat.c,rchash_glue.c} \
  "${CHDR}"/src/{libchdr_bitstream.c,libchdr_cdrom.c,libchdr_chd.c,libchdr_codec_cdfl.c,libchdr_codec_cdlz.c,libchdr_codec_cdzl.c,libchdr_codec_cdzs.c,libchdr_codec_flac.c,libchdr_codec_huff.c,libchdr_codec_lzma.c,libchdr_codec_zlib.c,libchdr_codec_zstd.c,libchdr_flac.c,libchdr_huffman.c} \
  "${CHDR}/deps/miniz-3.1.1/miniz.c" "${CHDR}/deps/lzma-25.01/src/LzmaDec.c" "${CHDR}/deps/zstd-1.5.7/zstddeclib.c"

find "${WORK_DIR}/package" -exec touch -h -d "@${EPOCH}" {} +
if find "${WORK_DIR}/package" -type l -print -quit | grep -q .; then
  echo "RAOfflineProxy package is not FAT-compatible" >&2
  exit 1
fi
tar --sort=name --mtime="@${EPOCH}" --owner=0 --group=0 --numeric-owner \
    -C "${WORK_DIR}/package" -cf - . | gzip -n -9 \
    > "${OUTPUT_DIR}/raofflineproxy-1.11.1-alpha1-armv7.tar.gz"
sha256sum "${OUTPUT_DIR}/raofflineproxy-1.11.1-alpha1-armv7.tar.gz" \
    > "${OUTPUT_DIR}/raofflineproxy-1.11.1-alpha1-armv7.tar.gz.sha256"
