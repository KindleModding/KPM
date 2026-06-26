#!/bin/sh

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
