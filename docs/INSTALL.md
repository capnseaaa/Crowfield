# Installing Crowfield, with its art and description

Crowfield is a Game Boy Advance ROM (`crowfield.gba`). It runs in any GBA emulator.

## 1. Download
From the [latest release](https://github.com/capnseaaa/Crowfield/releases/latest), download:
- `crowfield.gba`: the game.
- `crowfield-art.zip`: the art and the description, already sorted into one folder per setup below.

The zip holds:
- `images/crowfield_square_640.png`: the square menu image (640 x 640).
- `images/crowfield_portrait_250x360.png`: a tall cover (250 x 360).
- `images/crowfield_box_326x360_white_strip.png`: the box art with a white strip on the left, where a GBA box has
  its silver stripe.
- `description.txt`: the game's description.
- A folder per setup, with the files already named and placed. Copy that folder's contents onto your card.

## 2. Tested setups
These two were played and work.

### R36 handhelds (ArkOS / EmuELEC, EmulationStation)
1. Turn the handheld off and put its SD card in your computer.
2. Copy `crowfield.gba` into the card's `gba/` folder.
3. From the zip's `R36 ArkOS EmuELEC (EmulationStation)/` folder, copy `gba/downloaded_images/crowfield.png` into the
   card's `gba/downloaded_images/` folder.
4. Back up `gba/gamelist.xml` first. Then open it in a text editor and paste the block from `gamelist-entry.xml`
   right before the last line, `</gameList>`. Do not replace the file: it lists every other game on the card.
5. Put the card back and start the handheld. Crowfield shows up in the GBA list with its image and description.

### iPhone and iPad (Delta)
1. Save `crowfield.gba` to Files, then import it in Delta (the + button).
2. Delta finds art by looking the game up in its own database of known games. Crowfield is homebrew and not in
   that database, so it starts with Delta's plain box.
3. Save a cover image to Photos or Files (the square one works well).
4. In Delta, long-press Crowfield, choose **Change Artwork**, and pick the image.

## 3. Other setups (not tested yet)
These follow each setup's documented folder layout. We have not tried Crowfield on them yet. If something does
not show up, check that setup's own guide.

### RetroArch (playlists and thumbnails)
- Put `crowfield.gba` with your GBA games and scan the folder into the "Nintendo - Game Boy Advance" playlist.
- Copy the zip's `RetroArch/thumbnails/...` folders into RetroArch's `thumbnails` folder:
  `thumbnails/Nintendo - Game Boy Advance/Named_Boxarts/crowfield.png`.
- RetroArch 1.17 and later match a thumbnail to the ROM's file name. Older versions need the PNG named exactly like
  the game's title in the playlist.
- RetroArch has no place for a description.

### Onion OS (Miyoo Mini)
- Copy `crowfield.gba` into `Roms/GBA/`.
- Copy `Onion OS (Miyoo Mini)/Roms/GBA/Imgs/crowfield.png` onto the card. The `Imgs` folder needs a capital I.
  Onion's limit is 250 x 360 pixels, which is the size of this image.
- No description is used.

### NextUI
- Copy `crowfield.gba` into `Roms/Game Boy Advance (GBA)/` (use the name of your GBA folder if it differs).
- Copy `NextUI/Roms/Game Boy Advance (GBA)/.media/crowfield.png` into that folder's `.media` folder (make it if it
  is missing). The PNG must have the ROM's name without `.gba`.

### muOS
- Copy `crowfield.gba` into your GBA folder and launch it once, so muOS makes its catalogue folders.
- Copy `box/crowfield.png` and `text/crowfield.txt` from the zip's `muOS/` folder into
  `MUOS/info/catalogue/<your GBA system>/`. The zip names it `Nintendo Game Boy Advance`; if your card's folder
  under `MUOS/info/catalogue/` has another name, use that one (it is the `catalogue=` line in
  `MUOS/info/assign/<system>/global.ini`).

### Batocera, Knulli, RetroPie, ES-DE, and other EmulationStation setups
- These read a `gamelist.xml` file. Add the same block as for the R36 (from `gamelist-entry.xml`) to the GBA
  system's gamelist, and put `crowfield.png` where its `<image>` line points. Change the path in the block if your
  setup keeps images somewhere else.
- ES-DE does not read images from the gamelist: it matches files in its `downloaded_media` folder by the ROM's
  name. Put the square image there as the GBA system's cover named `crowfield.png`. We have not confirmed ES-DE's
  exact subfolder names.

## Found a bug, or want a feature?
Open an issue on [GitHub](https://github.com/capnseaaa/Crowfield/issues). We would love to hear about it.
