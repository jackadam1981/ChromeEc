#!/bin/bash

set -u
TMPD="$(mktemp -d /tmp/$(basename $0).XXXXX)"
trap "/bin/rm -rf ${TMPD}" SIGINT SIGTERM EXIT

if [ -z "${CROS_WORKON_SRCROOT}" ]; then
  echo "$(basename $0): This script must run inside Chrome OS chroot" >&2
  exit 1
fi

EC_ROOT="${CROS_WORKON_SRCROOT}/src/platform/ec"
RESULT_FILE="${EC_ROOT}/build/cr50/ec.bin"
dest_dir=

[ -e "${RESULT_FILE}" ] || dd if=/dev/zero bs=524288 count=1 | \
  tr \\000 \\377 > "${RESULT_FILE}"

verify_ro() {
  local ro_bin="${1}"
  local type_expected="${2}"

  if [ ! -f "${ro_bin}" ]; then
    echo "${ro_bin} not a file!" >&2
    exit 1
  fi

  # Key's lowest byte is byte #5 in the following line.
  line="$(od -Ax -t x1 -v "${ro_bin}" | grep '0001a0 .. .. .. .. .[4567cdef]')"

  if [ -n "${line}" -a "${type_expected}" == "prod" ]; then
      return 0
  fi

  line="$(od -Ax -t x1 -v "${ro_bin}" | grep '0001a0 .. .. .. .. .[012389ab]')"
  if [ -n "${line}" -a "${type_expected}" == "dev" ]; then
      return 0
  fi

  echo "RO key in ${ro_bin} does not match type ${type_expected}" >&2
  exit 1
}

prepare_image() {
  local image_type="${1}"
  local ro_a="${TMPD}/$(basename ${2}).bin"
  local ro_b="${TMPD}/$(basename ${3}).bin"
  local rw_a="$(readlink -f "${4}")"
  local rw_b="$(readlink -f "${5}")"
  local version

  for bin in "${ro_a}" "${ro_b}"; do
    verify_ro "${bin}" "${image_type}"
  done

  if [ "${image_type}" == "prod" ]; then
    extra_param='prod'
  else
    extra_param=
  fi

  if ! ${EC_ROOT}/util/signer/bs ${extra_param} elves "${rw_a}" "${rw_b}"; then
    echo "Failed invoking ${EC_ROOT}/util/signer/bs ${extra_param} elves ${rw_a} ${rw_b}" >&2
    exit 1
  fi
  dd if="${ro_a}" of="${RESULT_FILE}" conv=notrunc
  dd if="${ro_b}" of="${RESULT_FILE}" seek=262144 bs=1 conv=notrunc

  version="$(usb_updater -b "${RESULT_FILE}" |\
     awk '/^RO_A:/ {gsub(/R[OW]_A:/, ""); print "r" $1 ".w" $2}')"
  dest_dir="cr50.${version}"

  if [ ! -d "${dest_dir}" ]; then
    mkdir "${dest_dir}"
  fi

  cp "${RESULT_FILE}" "${dest_dir}/cr50.bin.${image_type}"
  echo "saved ${image_type} binary in ${dest_dir}/cr50.bin.${image_type}"
}

if [ "${#*}" != "6" ]; then
  echo "six parameters are required: <prod RO A>.hex " \
    "<prod RO B>.hex <dev RO A>.hex <dev RO B>.hex <RW.elf> <RW_B.elf>" >&2
fi

prod_ro_a="${1}"
prod_ro_b="${2}"
dev_ro_a="${3}"
dev_ro_b="${4}"
rw_a="${5}"
rw_b="${6}"

for f in "${prod_ro_a}" "${prod_ro_b}" "${dev_ro_a}"  "${dev_ro_b}"; do
  if ! objcopy -I ihex "${f}" -O binary "${TMPD}/$(basename ${f}).bin"; then
    echo "failed to convert ${f} from hex to bin" >&2
    exit 1
  fi
done

prepare_image 'dev' "${dev_ro_a}" "${dev_ro_b}" "${rw_a}" "${rw_b}"
prepare_image 'prod' "${prod_ro_a}" "${prod_ro_b}" "${rw_a}" "${rw_b}"
tar jcf  "${dest_dir}.tbz2" "${dest_dir}"
rm -rf "${dest_dir}"
echo "SUCCESS!!!!!!"
