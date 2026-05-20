#!/bin/bash

set -e

echoerr() { echo "$@" 1>&2; }

function get_platform() {
    # Will return "linux" for GNU/Linux
    #   I'd just like to interject for a moment...
    #   https://wiki.installgentoo.com/index.php/Interjection
    # Will return "macos" for macOS/OS X
    # Will return "windows" for Windows/MinGW/msys

    _platform=$(uname | tr '[:upper:]' '[:lower:]')
    if [[ $_platform == "darwin" ]]; then
        _platform="macos";
    elif [[ $_platform == "msys"* ]]; then
        _platform="windows";
    elif [[ $_platform == "mingw"* ]]; then
        _platform="windows";
    elif [[ $_platform == "linux" ]]; then
        # Nothing to do
        true;
    else
        echoerr "ERROR: $_platform is not a valid platform";
        exit 1;
    fi

    echo $_platform;
}

function get_version() {
    $(dirname "$0")/getversion.sh;
}

function get_arch() {
    _arch="$(uname -m)"
    echo $_arch;
}

platform=$(get_platform)
version=$(get_version)
arch=$(get_arch)
echo "Platform: $platform, arch: $arch, version: $version"

function build_zip() {
    echo "Zipping executables..."
    pushd dist;
    filename="komutracker-${version}-${platform}-${arch}.zip"
    echo "Name of package will be: $filename"

    if [[ $platform == "windows"* ]]; then
        7z a $filename komutracker;
    else
        zip -r $filename komutracker;
    fi
    popd;
    echo "Zip built!"
}

function build_setup() {
    filename="komutracker-${version}-${platform}-${arch}-setup.exe"
    echo "Name of package will be: $filename"

    innosetupdir="/c/Program Files (x86)/Inno Setup 6"
    if [ ! -d "$innosetupdir" ]; then
        echo "ERROR: Couldn't find innosetup which is needed to build the installer. We suggest you install it using chocolatey. Exiting."
        exit 1
    fi

    # Windows installer version should not include 'v' prefix, see: https://github.com/microsoft/winget-pkgs/pull/17564
    version_no_prefix="$(echo $version | sed -e 's/^v//')"
    env AW_VERSION=$version_no_prefix "$innosetupdir/iscc.exe" scripts/package/komutracker-setup.iss
    mv dist/komutracker-setup.exe dist/$filename
    echo "Setup built!"
}

function build_deb() {
    echo "Building .deb package..."

    app_name="komutracker"
    package_root="dist/komutracker-deb"
    install_dir="/opt/$app_name"

    filename="${app_name}-${version}-${platform}-${arch}.deb"

    echo "Name of package will be: $filename"

    # Cleanup
    rm -rf "$package_root"

    # Create folders
    mkdir -p "$package_root/DEBIAN"
    mkdir -p "$package_root$install_dir"
    mkdir -p "$package_root/usr/bin"
    mkdir -p "$package_root/usr/share/applications"
    mkdir -p "$package_root/usr/share/icons/hicolor/256x256/apps"

    echo "Copying application files..."

    cp -r dist/komutracker/* "$package_root$install_dir/"

    echo "Creating launcher..."

    cat > "$package_root/usr/bin/$app_name" <<EOF
#!/bin/bash
cd $install_dir
exec ./aw-qt "\$@"
EOF

    chmod +x "$package_root/usr/bin/$app_name"

    echo "Creating desktop entry..."

    cat > "$package_root/usr/share/applications/$app_name.desktop" <<EOF
[Desktop Entry]
Name=KomuTracker
GenericName=Time-tracking application
Comment=Open source time-tracking application with a focus on extensibility and privacy.
Hidden=false
Exec=/usr/bin/$app_name
StartupNotify=true
X-GNOME-Autostart-enabled=true
Icon=$app_name
Type=Application
Categories=Utility;
Terminal=false

EOF
    # echo "Creating desktop entry..."

    # cp aw-qt/resources/aw-qt.desktop "$package_root/usr/share/applications/$app_name.desktop"

    # Optional icon
    if [ -f "aw-qt/media/logo/logo.png" ]; then
        cp aw-qt/media/logo/logo.png \
        "$package_root/usr/share/icons/hicolor/256x256/apps/$app_name.png"
    fi

    echo "Creating control file..."

    cat > "$package_root/DEBIAN/control" <<EOF
Package: $app_name
Version: $version
Section: utils
Architecture: amd64
Maintainer: Duy Nguyen <hoangduy06104@gmail.com>
Depends: libc6,
 libglib2.0-0,
 libxcb-cursor0,
 libxcb-xinerama0,
 libxkbcommon-x11-0,
 libxcb-icccm4,
 libxcb-image0,
 libxcb-keysyms1,
 libxcb-render-util0
Description: KomuTracker desktop application
 Activity tracking application built with PyQt5.
EOF

    chmod 755 "$package_root/DEBIAN"
    chmod 644 "$package_root/DEBIAN/control"

    echo "Building deb..."

    dpkg-deb --build "$package_root" "dist/$filename"

    echo ".deb package built!"
}

build_zip
if [[ $platform == "windows"* ]]; then
    build_setup
fi
if [[ $platform == "linux"* ]]; then
    build_deb
fi

echo
echo "-------------------------------------"
echo "Contents of ./dist"
ls -l dist
echo "-------------------------------------"

