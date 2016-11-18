#!/bin/sh

input_file=$1
output_file=$2

if [ -z $1 ]; then
	echo "first argument (input file) is missing"
	exit 1
fi

if [ ! -f $1 ]; then
	echo "input file does not exists"
	exit 1
fi

if [ -z $2 ]; then
	echo "second argument (output file) is missing"
	exit 1
fi

generateInfoPlist() {
appName=$1
infoPlist=$2
binaryName=$3
productId=$4
showIcon=$5

cat << EOF > $infoPlist
<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN"
"http://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0">
    <dict>
EOF
if ! $showIcon; then
cat << EOF >> $infoPlist
        <key>LSUIElement</key>
        <string>1</string>
EOF
fi

cat << EOF >> $infoPlist
        <key>NSPrincipalClass</key>
        <string>NSApplication</string>
        <key>CFBundlePackageType</key>
        <string>APPL</string>
        <key>CFBundleGetInfoString</key>
        <string>$appName for OS X</string>
        <key>CFBundleSignature</key>
        <string>????</string>
        <key>CFBundleExecutable</key>
        <string>$binaryName</string>
        <key>CFBundleIconFile</key>
        <string>@ICON@</string>
        <key>CFBundleIdentifier</key>
        <string>$productId</string>
    </dict>
</plist>
EOF
}

name=$(grep __applicationName $1 | awk -F= '{print $2}' | awk -F'"' '{print $2}')
domain=$(grep __organizationDomain $1 | awk -F= '{print $2}' | awk -F'"' '{print $2}')
organization=$(grep __organizationName $1 | awk -F= '{print $2}' | awk -F'"' '{print $2}')
product_id=$(echo $name.$domain | awk -F. '{ for (i=NF; i>1; i--) printf "%s.",$i; print tolower($1)}')
binary_name=$(echo $name | awk '{print tolower($0)}')

generateInfoPlist $name $output_file/drive-app/Info.plist $binary_name $product_id false
generateInfoPlist SxScout $output_file/scout-app/Info.plist sxscout "com.skylable.sxscout" true

upgrade_script=$2/drive-app/osxupgrade.sh

cat << EOF > $upgrade_script
#!/bin/sh

notify_error() {
    /Applications/$name.app/Contents/MacOS/$binary_name --update-failed
    exit 1
}

stop_wait() {
    trap "" 1 15
    /Applications/$name.app/Contents/MacOS/$binary_name --close-all
    killall $binary_name
    CNT=1
    while killall -0 $binary_name 2>/dev/null; do
	if [ \$CNT -gt 60 ]; then
	    echo "Failed to stop $binary_name"
            notify_error
	fi
	CNT=\`expr \$CNT + 1\`
	sleep 1
    done
}

pid_stop_wait() {
    trap "" 1 15
    PID=\`cat ~/Library/Caches/$organization/$name\$PROFILE_SUFFIX/$binary_name.pid\`
    if [ \$? -ne 0 ]; then
	echo "Failed to obtain PID number"
	exit 1
    fi
    kill \$PID
    CNT=1
    while kill -0 \$PID 2>/dev/null; do
	if [ \$CNT -eq 50 ]; then
	    kill -9 \$PID
	fi
	if [ \$CNT -gt 60 ]; then
	    echo "Failed to stop $binary_name"
	    exit 1
	fi
	CNT=\`expr \$CNT + 1\`
	sleep 1
    done
}

start_all() {
    /Applications/$name.app/Contents/MacOS/$binary_name --start || exit 1
    /Applications/$name.app/Contents/MacOS/$binary_name --autostart-all
}

perform_update() {
    hdiutil verify \$1 || notify_error
    hdiutil detach /Volumes/$name > /dev/null 2>&1
    hdiutil attach \$1 -nobrowse || notify_error
    if type codesign > /dev/null 2>&1; then
        codesign --verify --verbose /Volumes/$name/$name.app || notify_error
        if [ -z "codesign --display -r- /Volumes/$name/$name.app | grep $product_id" ]; then
            echo "Invalid identifier"
            notify_error
        fi
    fi
    stop_wait
    cp -r /Volumes/$name/$name.app/ /Applications/$name.app/ || exit 1
    hdiutil detach /Volumes/$name
    rm \$1
    start_all
}

if [ \$# -ne 1 -a \$# -ne 2 ]; then
    echo "Usage: \$0 { restart | restart-profile | $binary_name.dmg }"
    exit 1
fi

if [ "\$1" = "restart" ]; then
    stop_wait
    start_all
    exit 0
fi

if [ "\$1" = "restart-profile" ]; then
    if [ \$# -ne 2 ]; then
	echo "Usage: \$0 restart-profile name"
	exit 1
    fi
    test "\$2" != "default" && PROFILE_SUFFIX="-\$2"
    PROFILE_FLAG="--profile \$2"
    pid_stop_wait
    open -n -a /Applications/$name.app --args \$PROFILE_FLAG || exit 1
    exit 0
fi

if [ \$# -ne 2 ]; then
    perform_update \$1
else
    date > \$2
    perform_update \$1 >> \$2 2>&1
fi

EOF

chmod a+x $upgrade_script

upgrade_script=$2/scout-app/osxupgrade.sh
name=SXScout
binary_name=sxscout

cat << EOF > $upgrade_script
#!/bin/sh

stop_wait() {
    trap "" 1 15
    /Applications/$name.app/Contents/MacOS/$binary_name --close-all
    killall $binary_name
    CNT=1
    while killall -0 $binary_name 2>/dev/null; do
        if [ \$CNT -gt 60 ]; then
            echo "Failed to stop $binary_name"
            notify_error
        fi
        CNT=\`expr \$CNT + 1\`
        sleep 1
    done
}

start_all() {
    /Applications/$name.app/Contents/MacOS/$binary_name || exit 1
}

perform_update() {
    hdiutil verify \$1 || notify_error
    hdiutil detach /Volumes/$name > /dev/null 2>&1
    hdiutil attach \$1 -nobrowse || notify_error
    if type codesign > /dev/null 2>&1; then
        codesign --verify --verbose /Volumes/$name/$name.app || notify_error
        if [ -z "codesign --display -r- /Volumes/$name/$name.app | grep $product_id" ]; then
            echo "Invalid identifier"
            notify_error
        fi
    fi
    stop_wait
    cp -r /Volumes/$name/$name.app/ /Applications/$name.app/ || exit 1
    hdiutil detach /Volumes/$name
    rm \$1
    start_all
}

if [ \$# -ne 1 -a \$# -ne 2 ]; then
    echo "Usage: \$0 $binary_name.dmg"
    exit 1
fi

if [ \$# -ne 2 ]; then
    perform_update \$1
else
    date > \$2
    perform_update \$1 >> \$2 2>&1
fi

EOF
chmod a+x $upgrade_script
