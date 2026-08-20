#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
SOURCE_DIR="${SOURCE_DIR:-${ROOT}/.build/rhash/sources}"
WORK_DIR="${WORK_DIR:-${ROOT}/.build/rhash/work}"
OUTPUT="${1:-${ROOT}/build/.tmp_update/lib/libbloom-rchash.so}"
CC="${CC:-arm-linux-gnueabihf-gcc}"

case "${WORK_DIR}" in "${ROOT}"/.build/rhash/*) ;; *) echo "unsafe rhash work directory" >&2; exit 2 ;; esac
"${ROOT}/build/rhash/fetch.sh" "${SOURCE_DIR}"
rm -rf -- "${WORK_DIR}"
mkdir -p "${WORK_DIR}/src" "$(dirname "${OUTPUT}")"
for name in raofflineproxy rcheevos libchdr; do
    tar -xzf "${SOURCE_DIR}/${name}.tar.gz" -C "${WORK_DIR}/src"
done

RAP="$(find "${WORK_DIR}/src" -maxdepth 1 -type d -name 'RAOfflineProxy-*' -print -quit)"
RC="$(find "${WORK_DIR}/src" -maxdepth 1 -type d -name 'rcheevos-*' -print -quit)"
CHDR="$(find "${WORK_DIR}/src" -maxdepth 1 -type d -name 'libchdr-*' -print -quit)"
GLUE="${RAP}/third_party/rcheevos_glue"
patch --batch --forward -p1 -d "${RAP}" <"${ROOT}/build/rhash/raofflineproxy-console-hash.patch"

"${CC}" -shared -Os -s -fPIC \
  -D_FILE_OFFSET_BITS=64 -DRC_HASH_NO_ENCRYPTED -DWANT_RAW_DATA_SECTOR=1 -DWANT_SUBCODE=1 -DVERIFY_BLOCK_CRC=1 \
  -I"${RC}/include" -I"${RC}/src" -I"${RC}/src/rhash" \
  -I"${GLUE}" -I"${GLUE}/shim" -I"${CHDR}/include" -I"${CHDR}/src" \
  -I"${CHDR}/deps/miniz-3.1.1" -I"${CHDR}/deps/lzma-25.01/include" -I"${CHDR}/deps/zstd-1.5.7" \
  -o "${OUTPUT}" \
  "${RC}"/src/rhash/{hash.c,hash_rom.c,hash_disc.c,hash_zip.c,cdreader.c,md5.c} \
  "${RC}/src/rc_compat.c" \
  "${GLUE}"/{chd_stream.c,cdfs_chd.c,cdfs_pbp.c,strl_compat.c,rchash_glue.c} \
  "${CHDR}"/src/{libchdr_bitstream.c,libchdr_cdrom.c,libchdr_chd.c,libchdr_codec_cdfl.c,libchdr_codec_cdlz.c,libchdr_codec_cdzl.c,libchdr_codec_cdzs.c,libchdr_codec_flac.c,libchdr_codec_huff.c,libchdr_codec_lzma.c,libchdr_codec_zlib.c,libchdr_codec_zstd.c,libchdr_flac.c,libchdr_huffman.c} \
  "${CHDR}/deps/miniz-3.1.1/miniz.c" "${CHDR}/deps/lzma-25.01/src/LzmaDec.c" "${CHDR}/deps/zstd-1.5.7/zstddeclib.c"

printf 'Built Bloom CHD hash bridge: %s\n' "${OUTPUT}"
