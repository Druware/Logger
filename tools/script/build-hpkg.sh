#!/bin/bash
#
# Copyright 2025-2026, Dru Satori @ Druware Software Designs.
# All rights reserved. Distributed under the terms of the MIT License.
#
# Builds a release of Logger and packages it as a Haiku package in packages/.
#
# Usage: tools/script/build-hpkg.sh <version>
#
#	<version> is a Haiku package version, major[.middle[.minor]][~pre][-rev],
#	such as 1.0, 1.2.3, 1.0~beta1 or 1.2.3-2. The revision defaults to 1.
#
# The package is packages/logger-<version>-<revision>-<arch>.hpkg. The version
# is also written into the app_version resource of the packaged binary.

set -e

if [ $# -ne 1 ]; then
	echo "usage: $(basename "$0") <version>" >&2
	exit 1
fi

VERSION_PATTERN='^([0-9]+)(\.([0-9]+))?(\.([0-9]+))?(~([A-Za-z0-9_.]+))?(-([0-9]+))?$'
if [[ ! "$1" =~ $VERSION_PATTERN ]]; then
	echo "error: '$1' is not a valid version, such as 1.2.3 or 1.2.3-1" >&2
	exit 1
fi
MAJOR=${BASH_REMATCH[1]}
MIDDLE=${BASH_REMATCH[3]:-0}
MINOR=${BASH_REMATCH[5]:-0}
PRE_RELEASE=${BASH_REMATCH[7]}
REVISION=${BASH_REMATCH[9]:-1}
VERSION="$MAJOR${BASH_REMATCH[2]}${BASH_REMATCH[4]}${BASH_REMATCH[6]}"

# The variety of the app_version resource follows the pre-release tag.
case "$PRE_RELEASE" in
	"")		VARIETY=f ;;
	alpha*)	VARIETY=a ;;
	beta*)	VARIETY=b ;;
	rc*)	VARIETY=g ;;
	*)		VARIETY=d ;;
esac

NAME=logger
APP=Logger
ARCH=$(getarch)
PROJECT_DIR=$(cd "$(dirname "$0")/../.." && pwd)
PACKAGES_DIR="$PROJECT_DIR/packages"
PACKAGE_FILE="$PACKAGES_DIR/$NAME-$VERSION-$REVISION-$ARCH.hpkg"

WORK_DIR=$(mktemp -d /tmp/$NAME-hpkg.XXXXXX)
trap 'rm -rf "$WORK_DIR"' EXIT
STAGE_DIR="$WORK_DIR/stage"

# Build a release binary straight into the staging directory.
mkdir -p "$STAGE_DIR/apps" "$STAGE_DIR/data/deskbar/menu/Applications"
make -C "$PROJECT_DIR" TARGET_DIR="$STAGE_DIR/apps"

setversion "$STAGE_DIR/apps/$APP" -app "$MAJOR" "$MIDDLE" "$MINOR" \
	"$VARIETY" 0 -short "$APP" \
	-long "$APP ©2025-2026 Dru Satori @ Druware Software Designs. MIT License."
mimeset -f "$STAGE_DIR/apps/$APP"
ln -s "../../../../apps/$APP" \
	"$STAGE_DIR/data/deskbar/menu/Applications/$APP"

cat > "$STAGE_DIR/.PackageInfo" <<EOF
name			$NAME
version			$VERSION-$REVISION
architecture	$ARCH
summary			"A viewer for the system log and other log files"
description		"Logger is a native Haiku application for viewing, following \
and filtering the system log (syslog) and other log files."
packager		"Dru Satori <dru@druware.com>"
vendor			"Druware Software Designs"
copyrights		{
	"2025-2026 Dru Satori @ Druware Software Designs"
}
licenses		{
	"MIT"
}
provides		{
	$NAME = $VERSION
	app:$APP = $VERSION
}
requires		{
	haiku
	lib:libstdc++
	lib:libgcc_s
}
EOF

mkdir -p "$PACKAGES_DIR"
rm -f "$PACKAGE_FILE"
package create -C "$STAGE_DIR" "$PACKAGE_FILE"

echo "Created $PACKAGE_FILE"
