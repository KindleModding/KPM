#!/bin/sh

set -e # Halt on failure!

# Find which chattr to use
OLD_CHATTR="/bin/chattr"
NEW_CHATTR="/bin/chattr.e2fsprogs" # KT6 5.18.1.5+ and PW6, KS, & KS2 5.18.5+
if [ -f "${OLD_CHATTR}" ]; then
    CHATTR="${OLD_CHATTR}"
elif [ -f "${NEW_CHATTR}" ]; then
    CHATTR="${NEW_CHATTR}"
else
    # We couldn't find one, show an error, and pretend it's the old one I guess
    echo "Error: Could not find chattr command. This is bad."
    CHATTR="${OLD_CHATTR}" # won't be found, but better than nothing?
fi

###
# Helper functions
###
make_mutable() {
    local my_path="${1}"
    # NOTE: Can't do that on symlinks, hence the hoop-jumping...
    if [ -d "${my_path}" ] ; then
        find "${my_path}" -type d -exec ${CHATTR} -i '{}' \;
        find "${my_path}" -type f -exec ${CHATTR} -i '{}' \;
    elif [ -f "${my_path}" ] ; then
        ${CHATTR} -i "${my_path}"
    fi
}

make_immutable() {
    local my_path="${1}"
    if [ -d "${my_path}" ] ; then
        find "${my_path}" -type d -exec ${CHATTR} +i '{}' \;
        find "${my_path}" -type f -exec ${CHATTR} +i '{}' \;
    elif [ -f "${my_path}" ] ; then
        ${CHATTR} +i "${my_path}"
    fi
}

make_mutable /var/local/kmc
echo "Copying KPM files..."
cp -rf ./kmc/* /var/local/kmc
for PLATFORM in kindlepw2 kindlehf
do
    chmod +x /var/local/kmc/${PLATFORM}/bin/kpm
    chmod +x /var/local/kmc/${PLATFORM}/lib/libkpm.so
done
make_immutable /var/local/kmc
