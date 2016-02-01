#!/bin/bash
VERSION=$1
if [ -z "$VERSION" ]; then
    echo "Usage: $0 VERSION"
    echo "e.g. $0 0.8.2"
    exit 1
fi

OLD_FILES=$(find . -name "pumpa*$VERSION*")

if [ ! -z "$OLD_FILES" ]; then
    echo "Old files for that version exist, please clean them up first:"
    echo $OLD_FILES
    #exit 2
fi

DIR="pumpa-${VERSION}"

# use this for real
#git clone -b v$VERSION git://pumpa.branchable.com/ pumpa

# use this for testing
git clone .. pumpa

mv pumpa ${DIR}
rm -rf ${DIR}/.git*
rm -rf ${DIR}/doc

# make regular tarball
tar -cJf pumpa-${VERSION}.tar.xz ${DIR}/

# make debian tarball
rm -rf ${DIR}/win32
tar czf pumpa_${VERSION}.orig.tar.gz ${DIR}/

cd ${DIR}
dpkg-buildpackage
cd ..

echo "Running lintian ..."
lintian -i -I --show-overrides pumpa_${VERSION}-1_*.changes

echo 
echo "Read the output above carefully!"
echo "If all is OK you can run something like:"
echo "dput mentors pumpa_${VERSION}-1_amd64.changes"
