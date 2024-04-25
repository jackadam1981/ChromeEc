TMP_DIR="$(mktemp -d -t ecrw.XXXX)"
trap 'rm -rf -- "$TMP_DIR"' EXIT

cbfstool $1 extract -r FW_MAIN_A -n ecrw -f ${TMP_DIR}/ecrw.bin >/dev/null 2>&1
strings ${TMP_DIR}/ecrw.bin | grep -oE '[a-z]+\-([0-9]+\.){2}[0-9]+' | uniq
