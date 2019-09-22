#!/bin/sh
#
# This script is run by Jenkins on qa.coreboot.org to upload
# static analysis information to coverity.
#
# It is run in the execute shell step of the ChromeEC-Coverity
# builder like this:
#  export PROJ_EMAIL=<insert email here>
#  export PROJ_TOKEN=<insert token here>
#  util/coverity.sh

# Set up the environment variables
export PATH=$PATH:/data/cache/coverity/bin:/data/cache/futility

export PROJ_NAME="Chromium+EC"
export PROJ_TARBALL="chromium-ec.tgz"
export PROJ_VERSION="Chrome_EC_$(git describe --dirty --always)"
export PROJ_DESCRIPTION="Daily coverity build"

git log --oneline | head
mkdir .failedboards

# qa.coreboot.org's Jenkins builder does not do a full check-out through
# repo but only the pieces that are strictly required for an Chrome EC
# build.
rm -rf tpm2 cryptoc vboot_reference
git clone https://chromium.googlesource.com/chromiumos/third_party/tpm2
git clone https://chromium.googlesource.com/chromiumos/third_party/cryptoc
git clone https://chromium.googlesource.com/chromiumos/platform/vboot_reference

# Rebuild futility
make -C vboot_reference futil
cp vboot_reference/build/futility/futility /data/cache/coverity/bin

# HACK! - to get around the hardcoded calls to sudo, I created a script
# called sudo containing the following:
#   #!/bin/sh
#   printf "%s\n" "$($@)"
# This was placed in the coverity/bin directory which is in the path

# Create the makefile we're going to use to do the build.  This lets us do
# several things - we can run make with the -j option to use all the CPUs,
# while still only building a single board at a time.
#
# We can also build each board in its own subdirectory, to eliminate
# coverity confusion where #defines from one platform show up in a
# different platform and make it think that boundaries are overrun.
# The downside to this is that real errors will show up multiple times,
# and disrupt coverity's error count tracking. :-/  Trade offs...

# Set up the build options
export BUILD_MAKEFILE=Makefile.coverity
export BUILD_OPTIONS="CCACHE= \
    CROSS_COMPILE=arm-eabi- \
    EXTLIB=tpm2 \
    CRYPTOCLIB=cryptoc \
    PDIR= \
    CFLAGS_DEBUG='-g -DCR50_NO_BN_ASM -Wno-error=maybe-uninitialized' \
    COMMON_WARN='-Wall -Wundef -Wno-trigraphs -fno-strict-aliasing \
      -fno-common -Werror-implicit-function-declaration \
      -Wno-format-security -fno-strict-overflow'"

# Maximum builds per day:
# 4 builds per day for projects with fewer than 100K lines of code
# 3 builds per day for projects with 100K to 500K lines of code
# 2 builds per day for projects with 500K to 1 million lines of code
# 1 build per day, for projects with more than 1 million lines of code
export buildcount=3
export buildnum=$(( ( (BUILD_NUMBER - 343) % buildcount) + 1))

export boardlist="$(make print-boards)"
export boardlist="$(echo $boardlist | tr ' ' '\n')"
export boardcount="$(echo "$boardlist" | wc -l)"
export DoY="$(date "+%j" | sed 's/^0//')"
export boardnum="$(( (buildnum + ( DoY % ( (boardcount + buildcount) / buildcount)) * buildcount) % boardcount ))"
export board="$(echo "$boardlist" | sed "${boardnum}q;d")"

if [ -z "$board" ]; then
	board="$(echo "$boardlist" | sed "$(( RANDOM % boardcount ))q;d")"
fi

# Create the temporary makefile
echo "all:" > "$BUILD_MAKEFILE"
printf "	-ln -s %s %s\n" "$PWD" "$board" >> "$BUILD_MAKEFILE"
printf "	-\$(MAKE) -C %s -j --keep-going proj-%s %s\n" "$board" "$board" "$BUILD_OPTIONS" >> "$BUILD_MAKEFILE"
printf "	-unlink %s\n" "$board" >> "$BUILD_MAKEFILE"

# Run the build
sh -x -c "nice -n 20 cov-build --dir cov-int make -f $BUILD_MAKEFILE" || true

# Clean up
rm -f "$BUILD_MAKEFILE"

tail cov-int/build-log.txt || true

# Uncomment for build testing without pushing to coverity.
#export SKIP_UPLOAD="Testing"

# Make sure the build was successful, then archive the output directory
# and send it to coverity.
set +x

# Upload to coverity if requested.
#
# March 16, 2018: Add --insecure option for curl to avoid an error:
# curl: (51) SSL: no alternative certificate subject name matches target host name 'scan.coverity.com'
#
if [ -z "$SKIP_UPLOAD" ]; then
	grep -q 'cov-build utility completed successfully' cov-int/build-log.txt && \
	tar czvf ${PROJ_TARBALL} cov-int && \
	curl --insecure --form token=${PROJ_TOKEN} \
	  --form email=${PROJ_EMAIL} \
	  --form file=@${PROJ_TARBALL} \
	  --form version="${board}_${PROJ_VERSION}" \
	  --form description="${PROJ_DESCRIPTION} for board ${board}" \
	  https://scan.coverity.com/builds?project=${PROJ_NAME}
else
	echo "Skipping upload to coverity."
fi

# fail the build if we didn't get all the compilation units done
if [[ ! grep -q "compilation units (100%)" cov-int/build-log.txt ]]; then
    { echo "Error: Not 100% of compilation units are getting checked"; exit 1; }
fi
