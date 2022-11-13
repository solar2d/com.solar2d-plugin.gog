#!/bin/bash

set -e

path=$(cd "$(dirname "$0")" && pwd)

(
	set -e
	cd "$path"

	rm -rf build out
	xcodebuild -project "Plugin.xcodeproj" -configuration Release clean

	xcodebuild -project "Plugin.xcodeproj" -configuration Release

	OUTPUT="out"
	mkdir -p "$OUTPUT"
	cp ~/Library/Application\ Support/Corona/Simulator/Plugins/plugin_gog.dylib "$OUTPUT"
	# lipo ~/Library/Application\ Support/Corona/Simulator/Plugins/libgog_api.dylib -thin x86_64  -output "$OUTPUT/libgog_api.dylib"
	cp  ~/Library/Application\ Support/Corona/Simulator/Plugins/libGalaxy64.dylib "$OUTPUT"
)
