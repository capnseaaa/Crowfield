#!/bin/sh
# Package for the R36. release/gba/ mirrors the SD card's gba/ folder:
#   release/gba/crowfield.gba
#   release/gba/downloaded_images/crowfield.png   (cover, once art/cover.png exists)
# The card already has its own gba/gamelist.xml. Never replace it.
# release/gamelist-entry.xml is one <game> block to insert before </gameList> in that file.
set -e
cd "$(dirname "$0")/.."
rm -rf release
mkdir -p release/gba/downloaded_images
cp crowfield.gba release/gba/crowfield.gba
if [ -f art/cover.png ]; then cp art/cover.png release/gba/downloaded_images/crowfield.png; fi
cat > release/gamelist-entry.xml <<XML
	<game>
		<path>./crowfield.gba</path>
		<name>CROWFIELD</name>
		<desc>Grow. Defend. Survive. Grow crops, fight off hungry crows, and upgrade your gear.</desc>
		<image>./downloaded_images/crowfield.png</image>
		<developer>Caleb</developer>
		<genre>Action</genre>
		<players>1</players>
	</game>
XML
echo "packaged:"; find release -type f
