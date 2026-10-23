#!/bin/sh
# Package Crowfield (v1.0). Writes release/:
#   gba/crowfield.gba, gba/downloaded_images/crowfield.png   the R36 card's gba/ folder (EmulationStation)
#   gamelist-entry.xml                                        one <game> block for gba/gamelist.xml
#   crowfield-art.zip                                         the art and description, in a folder per setup
# The card already has its own gba/gamelist.xml. Never replace it: insert the one block before </gameList>.
set -e
cd "$(dirname "$0")/.."
python3 tools/make_cover.py
rm -rf release
mkdir -p release/gba/downloaded_images
cp crowfield.gba release/gba/crowfield.gba
cp art/menu_cover.png release/gba/downloaded_images/crowfield.png   # square, like Rootflow's on the card
DESC="Grow. Defend. Survive. Hungry crows come for your crops every day, stronger and in bigger numbers. Farm four crops, sell your harvest at dusk, and spend it on weapons, scarecrows, crow cannons and a bigger field. Survive five blood moons and the three final nights to save the farm."
cat > release/gamelist-entry.xml <<XML
	<game>
		<path>./crowfield.gba</path>
		<name>CROWFIELD</name>
		<desc>$DESC</desc>
		<image>./downloaded_images/crowfield.png</image>
		<developer>Caleb</developer>
		<genre>Action</genre>
		<players>1</players>
	</game>
XML
A=release/art/crowfield-art
mkdir -p "$A/images" "$A/R36 ArkOS EmuELEC (EmulationStation)/gba/downloaded_images" \
  "$A/RetroArch/thumbnails/Nintendo - Game Boy Advance/Named_Boxarts" \
  "$A/Onion OS (Miyoo Mini)/Roms/GBA/Imgs" "$A/NextUI/Roms/Game Boy Advance (GBA)/.media" \
  "$A/muOS/MUOS/info/catalogue/Nintendo Game Boy Advance/box" "$A/muOS/MUOS/info/catalogue/Nintendo Game Boy Advance/text"
cp art/menu_cover.png "$A/images/crowfield_square_640.png"
cp art/portrait_cover.png "$A/images/crowfield_portrait_250x360.png"
cp art/box_cover.png "$A/images/crowfield_box_326x360_white_strip.png"
printf '%s\n' "$DESC" > "$A/description.txt"
cp art/menu_cover.png "$A/R36 ArkOS EmuELEC (EmulationStation)/gba/downloaded_images/crowfield.png"
cp release/gamelist-entry.xml "$A/R36 ArkOS EmuELEC (EmulationStation)/gamelist-entry.xml"
cp art/menu_cover.png "$A/RetroArch/thumbnails/Nintendo - Game Boy Advance/Named_Boxarts/crowfield.png"
cp art/portrait_cover.png "$A/Onion OS (Miyoo Mini)/Roms/GBA/Imgs/crowfield.png"
cp art/menu_cover.png "$A/NextUI/Roms/Game Boy Advance (GBA)/.media/crowfield.png"
cp art/portrait_cover.png "$A/muOS/MUOS/info/catalogue/Nintendo Game Boy Advance/box/crowfield.png"
printf '%s\n' "$DESC" > "$A/muOS/MUOS/info/catalogue/Nintendo Game Boy Advance/text/crowfield.txt"
cp ../docs/INSTALL.md "$A/HOW_TO_INSTALL.md"
(cd release/art && zip -qr ../crowfield-art.zip crowfield-art)
rm -rf release/art
echo "packaged:"; find release -type f | sort
