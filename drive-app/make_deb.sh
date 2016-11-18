#!/bin/sh

if [ ! -n "${QT_LIBS}"  ]; then
	echo QT_LIBS variable is not set
	exit 1
fi
if [ ! -n "${QT_PLUGINS}"  ]; then
	echo QT_PLUGINS variable is not set
	exit 1
fi

BUILD_DIR=$PWD
APPNAME=$1
BINARY=$1
DESC_APPNAME=$2

SCRIPT=$(readlink -f "$0")
BASEDIR=$(dirname "$SCRIPT")
VERSION=$(grep SXVERSION[^_] $BASEDIR/sxversion.h | awk -F'"' '{print $2;}')

DEBFOLDERNAME=$BASEDIR/../output/$APPNAME-$VERSION
TARGET=$DEBFOLDERNAME/opt/$APPNAME
BINDIR=$DEBFOLDERNAME/usr/local/bin

ARCH=$(arch)
if  [ "$ARCH" = "i686" ] ; then
	ARCH=i386
elif [ "$ARCH" = "x86_64" ] ; then
	ARCH=amd64
fi

rm -rf $DEBFOLDERNAME
mkdir -p $TARGET/lib
mkdir $TARGET/platforms
mkdir $TARGET/sqldrivers
cd $DEBFOLDERNAME

for LIB in libicudata.so.56 libQt5Widgets.so.5 libicui18n.so.56 libQt5Sql.so.5 libQt5Core.so.5 libQt5Network.so.5 libicuuc.so.56 libQt5XcbQpa.so.5 libQt5Gui.so.5 libQt5DBus.so.5; do
    cp $QT_LIBS/$LIB $TARGET/lib/
done
cp $QT_PLUGINS/platforms/libqxcb.so $TARGET/platforms
cp $QT_PLUGINS/sqldrivers/libqsqlite.so $TARGET/sqldrivers

cp  $BUILD_DIR/$BINARY $TARGET/
cat << EOF > $TARGET/$APPNAME.sh
#!/bin/sh
LD_LIBRARY_PATH=/opt/$APPNAME/lib /opt/$APPNAME/$APPNAME \$@
EOF
chmod 0755 $TARGET/$APPNAME.sh
mkdir -p $BINDIR
ln -s /opt/$APPNAME/$BINARY.sh $BINDIR/$BINARY

mkdir $DEBFOLDERNAME/DEBIAN
cat << EOF > $DEBFOLDERNAME/DEBIAN/control
Package: $APPNAME
Version: $VERSION
Section: web
Priority: optional
Architecture: $ARCH
Installed-Size: $(du -sk $DEBFOLDERNAME | awk '{ print $1; }')
Depends: libc6 (>= 2.14), libgcc1 (>= 1:4.1.1), libssl1.0.0(>= 1.0.0), libstdc++6 (>= 4.5)
Maintainer: Skylable Dev Team <dev-team@skylable.com>
Description: GUI client for Skylable Sx
 $DESC_APPNAME is a multi-platform file-sync application which runs on your
 PCs (Windows, MacOSX, Linux) and mobile devices (Android and iOS phones and
 tablets). It keeps your files synchronized between your Skylable SX cluster and
 the devices you always bring with you. You get the comfort of accessing the
 files stored in your storage cluster as a plain directory on your PC or from a
 lightweight app on your phone, and at the same time you get the protection and
 security of Skylable SX server.
EOF

cd $BASEDIR/../output
fakeroot dpkg-deb --build $APPNAME-$VERSION
