#!/bin/sh
# Package for the R36. release/gba/ mirrors the SD card's gba/ folder:
#   release/gba/crowfield.gba
#   release/gba/downloaded_images/crowfield.png   (the game list image: art/menu_cover.png, 640x640)
# The card already has its own gba/gamelist.xml. Never replace it.
# release/gamelist-entry.xml is one <game> block to insert before </gameList> in that file.
set -e
cd "$(dirname "$0")/.."
python3 tools/make_cover.py
rm -rf release
mkdir -p release/gba/downloaded_images
cp crowfield.gba release/gba/crowfield.gba
cp art/menu_cover.png release/gba/downloaded_images/crowfield.png   # square, like Rootflow's on the card
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
