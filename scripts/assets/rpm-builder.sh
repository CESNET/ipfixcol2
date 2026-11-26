set -euox pipefail
shopt -s globstar

mkdir /out_rpms

cd /
git clone -b master https://github.com/CESNET/libfds.git
cd libfds
mkdir build
cd build
cmake -DCPACK_PACKAGE_CONTACT="$PACKAGER" -DPACKAGE_BUILDER_RPM=ON ..
make -j rpm
dnf install -y pkg/rpm/rpmbuild/RPMS/x86_64/*.rpm

cd /
if [ -e /ipfixcol2-src ]; then
    cp -r /ipfixcol2-src /ipfixcol2
    rm -rf /ipfixcol2/build
else
    git clone https://github.com/CESNET/ipfixcol2.git
fi
cd ipfixcol2

mkdir build
cd build
cmake -DCPACK_PACKAGE_CONTACT="$PACKAGER" -DPACKAGE_BUILDER_RPM=ON ..
make -j rpm
dnf install -y pkg/rpm/rpmbuild/RPMS/x86_64/*.rpm

if [ "$WITH_UNIREC" = 1 ]; then
    cd /ipfixcol2/extra_plugins/output/unirec/
    UNIREC_VERSION_MAJOR="$(sed -n 's/set(UNIREC_VERSION_MAJOR \([0-9]*\))/\1/p' CMakeLists.txt)"
    UNIREC_VERSION_MINOR="$(sed -n 's/set(UNIREC_VERSION_MINOR \([0-9]*\))/\1/p' CMakeLists.txt)"
    UNIREC_VERSION_PATCH="$(sed -n 's/set(UNIREC_VERSION_PATCH \([0-9]*\))/\1/p' CMakeLists.txt)"
    UNIREC_VERSION="${UNIREC_VERSION_MAJOR}.${UNIREC_VERSION_MINOR}.${UNIREC_VERSION_PATCH}"
    rm -rf build
    mkdir build
    cd build
    cmake ..
    make -j

    rm -rf /root/rpmbuild
    rpmdev-setuptree
    mkdir "ipfixcol2-unirec-output-${UNIREC_VERSION}"
    rsync -av --progress /ipfixcol2/extra_plugins/output/unirec/ "ipfixcol2-unirec-output-${UNIREC_VERSION}/" --exclude build/
    tar -czvf "/root/rpmbuild/SOURCES/ipfixcol2-unirec-output-${UNIREC_VERSION}.tar.gz" "ipfixcol2-unirec-output-${UNIREC_VERSION}"
    cp *.spec /root/rpmbuild/SPECS/
    sed -i '/Packager:/c\Packager:       '"$PACKAGER" /root/rpmbuild/SPECS/*.spec
    rpmbuild -bs /root/rpmbuild/SPECS/*.spec
    rpmbuild -bb /root/rpmbuild/SPECS/*.spec

    cp /root/rpmbuild/RPMS/**/*.rpm /out_rpms/
    cp /root/rpmbuild/SRPMS/*.src.rpm /out_rpms/
fi

if [ "$WITH_CLICKHOUSE" = 1 ]; then
    cd /ipfixcol2/extra_plugins/output/clickhouse/
    CH_VERSION_MAJOR="$(sed -n 's/set(CH_VERSION_MAJOR \([0-9]*\))/\1/p' CMakeLists.txt)"
    CH_VERSION_MINOR="$(sed -n 's/set(CH_VERSION_MINOR \([0-9]*\))/\1/p' CMakeLists.txt)"
    CH_VERSION_PATCH="$(sed -n 's/set(CH_VERSION_PATCH \([0-9]*\))/\1/p' CMakeLists.txt)"
    CH_VERSION="${CH_VERSION_MAJOR}.${CH_VERSION_MINOR}.${CH_VERSION_PATCH}"
    rm -rf build
    mkdir build
    cd build
    cmake ..
    make -j

    rm -rf /root/rpmbuild
    rpmdev-setuptree
    mkdir "/ipfixcol2-clickhouse-output-${CH_VERSION}"
    rsync -av --progress /ipfixcol2/extra_plugins/output/clickhouse/ "/ipfixcol2-clickhouse-output-${CH_VERSION}/" --exclude build/
    tar -czvf "/root/rpmbuild/SOURCES/ipfixcol2-clickhouse-output-${CH_VERSION}.tar.gz" "/ipfixcol2-clickhouse-output-${CH_VERSION}"
    cp /ipfixcol2/extra_plugins/output/clickhouse/*.spec /root/rpmbuild/SPECS/
    sed -i '/Packager:/c\Packager:       '"$PACKAGER" /root/rpmbuild/SPECS/*.spec
    rpmbuild -bs /root/rpmbuild/SPECS/*.spec
    rpmbuild -bb /root/rpmbuild/SPECS/*.spec

    cp /root/rpmbuild/RPMS/**/*.rpm /out_rpms/
    cp /root/rpmbuild/SRPMS/*.src.rpm /out_rpms/
fi

cp /ipfixcol2/build/pkg/rpm/rpmbuild/RPMS/**/*.rpm /out_rpms/
cp /ipfixcol2/build/pkg/rpm/rpmbuild/SRPMS/*.src.rpm /out_rpms/

cp /libfds/build/pkg/rpm/rpmbuild/RPMS/**/*.rpm /out_rpms/
cp /libfds/build/pkg/rpm/rpmbuild/SRPMS/*.src.rpm /out_rpms/
