# PCX Level Tool – toteutussuunnitelma (V-Wing-tuki)

## Tavoite

Rakennetaan erillinen V-Wing-kenttäeditori, joka toimii helposti sekä:

* Windows 10/11
* Linux / KDE / Wayland / X11

Editorin pitää pystyä:

1. avaamaan olemassa olevia V-Wing `.LEV`-kenttiä
2. näyttämään kenttä visuaalisesti
3. muokkaamaan niitä pikselitasolla
4. luomaan uusia kenttiä
5. tallentamaan suoraan V-Wingin `.LEV`-muotoon
6. mahdollisuuksien mukaan tuomaan/vientiä alkuperäiseen PCX-muotoon
7. säilyttämään yhteensopivuus alkuperäisen pelin kanssa

Käytettävissä on:

* alkuperäinen V-Wing level converter
* converteriin liittyvä dokumentaatio
* alkuperäisiä `.LEV`-tiedostoja
* freeware V-Wing -julkaisun kenttiä
* freeware-version ilmoitettu versio on 1.95, mutta executable/data vaikuttaa tätä uudemmalta

Alkuperäisiä `.LEV`-tiedostoja pitää käyttää formaatin reverse engineering -referenssinä.

---

# 1. Teknologiavalinta

Käytä:

**C++17 + Qt 6**

Älä tee alustakohtaista käyttöliittymää.

Qt:n pitää hoitaa:

* ikkunointi
* hiiri/näppäimistö
* tiedostodialogit
* valikot
* canvas-widget
* Windows
* Linux
* Wayland
* X11

Projektin pitää kääntyä vähintään:

```text
Windows:
- MSVC
- mahdollisuuksien mukaan MinGW

Linux:
- GCC
- Clang
```

Build system:

```text
CMake
```

Älä käytä raskaita lisäkirjastoja ilman selvää tarvetta.

Ensimmäisen version riippuvuudet:

```text
Qt 6
C++ standard library
```

Pidä formaattikoodi täysin erillään Qt-käyttöliittymästä.

---

# 2. Projektin rakenne

Pidä rakenne yksinkertaisena.

Esimerkiksi:

```text
vwing-editor/
    CMakeLists.txt

    src/
        main.cpp

        level.h
        level.cpp

        lev_reader.h
        lev_reader.cpp

        lev_writer.h
        lev_writer.cpp

        pcx_reader.h
        pcx_reader.cpp

        pcx_writer.h
        pcx_writer.cpp

        main_window.h
        main_window.cpp

        level_canvas.h
        level_canvas.cpp

        palette_widget.h
        palette_widget.cpp

    tools/
        levdump/
        levcompare/

    tests/
        lev_tests.cpp

    samples/
        README.md

    docs/
        lev-format.md
        reverse-engineering.md
```

Älä rakenna erillisiä service-, manager-, controller-, repository- tai dependency injection -kerroksia.

---

# 3. Sisäinen kenttämalli

Editorin sisäinen kenttä ei saa riippua `.LEV`-formaatista.

Lähtökohtainen malli:

```cpp
struct RGB {
    uint8_t r;
    uint8_t g;
    uint8_t b;
};

struct Level {
    std::string name;

    static constexpr int Width = 640;
    static constexpr int Height = 800;

    std::array<uint8_t, Width * Height> pixels;
    std::array<RGB, 256> palette;
};
```

Jokainen kentän pikseli on paletti-indeksi:

```text
0...255
```

Älä tallenna canvas-dataa RGB-kuvana.

Paletti-indeksi on pelillisesti merkityksellinen.

Esimerkiksi tietyt indeksit tarkoittavat:

* vettä
* jäätä
* räjähdettä
* basea
* palavaa materiaalia
* tuhoutumatonta materiaalia
* turret-komponentteja

Editorin pitää säilyttää indeksit täsmälleen.

---

# 4. Ensimmäinen työvaihe: `.LEV`-formaatin selvitys

Älä aloita varsinaista GUI-editoria ennen kuin `.LEV`-formaatti on riittävän hyvin tunnettu.

Ensimmäinen milestone on:

> Pysty lukemaan alkuperäiset `.LEV`-tiedostot deterministisesti.

Tee ensin komentorivityökalut.

## `levdump`

Esimerkki:

```text
levdump LEVEL01.LEV
```

Sen pitää aluksi näyttää ainakin:

```text
file size
hex header
ASCII strings
entropy / repeating regions
suspected dimensions
suspected palette
suspected compressed blocks
```

Formaattia ei saa arvailla hiljaisesti.

Dokumentoi kaikki havainnot.

---

# 5. Alkuperäisten tiedostojen analyysi

Käytä useita alkuperäisiä `.LEV`-tiedostoja.

Vertaa:

```text
file size
header
footer
toistuvat rakenteet
kenttien nimet
paletit
mahdolliset offset-taulut
pakkaus
```

Tee `levcompare`:

```text
levcompare LEVEL1.LEV LEVEL2.LEV
```

Tulosta esimerkiksi:

```text
same byte ranges
different byte ranges
first difference
block boundaries
```

Lisäksi tee tarvittaessa hexdumpit.

Älä rakenna formaattiparseria ennen kuin rakenne voidaan osoittaa useammalla tiedostolla.

---

# 6. Converterin käyttäminen formaatin selvittämiseen

Alkuperäinen converter on tärkeä oracle/reference implementation.

Jos converter voidaan suorittaa DOSBoxissa tai vastaavassa ympäristössä, tee hallittuja testikenttiä.

Esimerkiksi PCX:

```text
test_00.pcx
kaikki pikselit = index 0

test_01.pcx
kaikki pikselit = index 1

test_57.pcx
kaikki pikselit = index 57
```

Sen jälkeen:

```text
test_single.pcx
kaikki = 0
pixel (0,0) = 1
```

Seuraavat:

```text
pixel (1,0)
pixel (639,0)
pixel (0,1)
pixel (0,799)
pixel (639,799)
```

Näillä selvitetään:

* row-major vs column-major
* y-suunnan järjestys
* kentän koko
* offsetit
* pakkaus

Tee lisäksi kuviot:

```text
vertical stripes
horizontal stripes
checkerboard
long runs
random pixels
```

Näillä voidaan tunnistaa mahdollinen:

```text
RLE
PackBits-tyylinen koodaus
scanline compression
block compression
raw bitmap
```

---

# 7. Freeware 1.95 -version kentät

Analysoi freeware-jakelun `.LEV`-tiedostot erillisenä aineistona.

Älä oleta, että formaatti on identtinen vanhimman converterin tuottaman formaatin kanssa.

Vertaa:

```text
vanha converter output
alkuperäiset vanhat kentät
freeware-version kentät
```

Selvitä:

```text
onko header sama
onko versiotunnistetta
onko tiedostokoko muuttunut
onko palettiformaatti muuttunut
onko kenttädata muuttunut
onko uusia metadata-kenttiä
```

Jos formaatteja on useampi:

```cpp
enum class LevVersion {
    Unknown,
    Classic,
    Freeware
};
```

Älä kuitenkaan lisää tätä ennen kuin formaattiero on todistettu.

Jos tiedostot ovat identtistä formaattia, käytä vain yhtä parseria.

---

# 8. `.LEV` parseri

Kun formaatti tunnetaan, toteuta:

```cpp
bool loadLev(const std::filesystem::path&, Level&, std::string& error);
```

Parserin pitää:

* tarkistaa tiedoston koko
* tarkistaa tunnettu header/signature jos sellainen löytyy
* estää buffer overflow
* validoida offsetit
* validoida pakattu data
* tuottaa täsmälleen 640×800 indeksikartta
* lukea paletti jos paletti sisältyy formaattiin

Älä tee aggressiivista "korjaavaa" parseria.

Jos tiedosto on rikki:

```text
return error
```

älä arvaa.

---

# 9. `.LEV` writer

Kun reader toimii luotettavasti, toteuta writer.

```cpp
bool saveLev(const std::filesystem::path&, const Level&, std::string& error);
```

Writerin ensimmäinen vaatimus:

> Sen tuottaman kentän pitää latautua alkuperäisessä V-Wingissä.

Toinen vaatimus:

> Mahdollisuuksien mukaan sen pitää tuottaa alkuperäisen converterin kanssa binäärisesti identtinen tiedosto samasta inputista.

Jos binäärinen identtisyys ei ole mahdollista mutta pelillinen tiedosto on identtinen, dokumentoi ero.

---

# 10. Round-trip testit

Testaa:

```text
original.lev
    ↓ load
Level
    ↓ save
roundtrip.lev
```

Vertaa:

```text
original.lev
roundtrip.lev
```

Jos formaatti sisältää epäolennaisia kenttiä tai pakkaus voi muodostua eri tavalla, vertaa myös dekoodattua sisältöä.

Pakollinen testi:

```text
load(original)
save(temp)
load(temp)

level1.pixels == level2.pixels
level1.palette == level2.palette
```

---

# 11. PCX-tuki

Toteuta PCX vain siltä osin kuin V-Wing sitä tarvitsee.

Tuettava muoto:

```text
640×800
8-bit indexed
256 colors
```

Ei tarvitse tehdä yleistä PCX-kirjastoa.

Tarvitaan:

```cpp
bool loadPcx(...);
bool savePcx(...);
```

Paletti-indeksien pitää säilyä.

Älä konvertoi kuvaa automaattisesti truecolor → indexed ilman käyttäjän erillistä toimintoa.

---

# 12. GUI-editori

Kun formaattikerros toimii, tee editori.

Pääikkuna:

```text
+--------------------------------------------------+
| File Edit View Tools Help                       |
+-------------+------------------------------------+
| Materials   |                                    |
|             |                                    |
| Water       |                                    |
| Ice         |             LEVEL                  |
| Explosive   |             CANVAS                 |
| Terrain     |                                    |
| ...         |                                    |
|             |                                    |
+-------------+------------------------------------+
| status: x=123 y=456 index=57                    |
+--------------------------------------------------+
```

---

# 13. Canvas

Canvas näyttää 640×800 pikselin kentän.

Pakolliset:

```text
zoom
pan
pixel-perfect rendering
nearest-neighbour scaling
```

Zoom esimerkiksi:

```text
25 %
50 %
100 %
200 %
400 %
800 %
```

Älä käytä bilinear filteringia.

Yksi kenttäpikseli pitää aina näkyä terävänä ruutuna suurennettaessa.

---

# 14. Piirtotyökalut

Ensimmäiseen versioon:

```text
Pencil
Eraser
Line
Rectangle
Filled rectangle
Flood fill
Eyedropper
```

Ei brush-antialiasingia.

Ei alpha blendingia.

Kaikki kirjoittavat vain:

```cpp
pixels[y * Width + x] = selectedIndex;
```

---

# 15. Materiaalipaletti

Älä esittele käyttäjälle vain RGB-värivalitsinta.

Pääasiallinen työkalu on V-Wingin materiaalipaletti.

Esimerkiksi:

```text
WATER
16 Water
17 Waterfall
18 ...
19 ...

ICE
39 Ice

EXPLOSIVE
45 Plastic explosive

BASE
50 Base

NORMAL TERRAIN
57
58
59
...
149

BURNABLE
151
...
174

INDESTRUCTIBLE
221
...
243
```

Käytä converterin dokumentaation oikeita merkityksiä.

Jos jonkin indeksin tarkoituksesta ei ole varmuutta:

```text
Unknown / undocumented
```

Älä keksi nimeä.

---

# 16. Palette editor

RGB-palettia pitää voida tarkastella.

Näytä jokaiselle:

```text
index
RGB
terrain meaning
```

Esimerkiksi:

```text
57    #705020    Normal terrain
58    #806030    Normal terrain
```

Paletin värin muuttaminen ei saa muuttaa indeksiä.

---

# 17. Undo / redo

Tee yksinkertainen undo/redo.

Älä kopioi koko 512 kt kenttää jokaisesta pikselistä, jos piirretään hiirellä jatkuvasti.

Yksi hiiren veto = yksi undo-operaatio.

Tallenna muuttuneet:

```text
offset
oldValue
newValue
```

tai vaihtoehtoisesti muuttuneen alueen before/after-data.

Pidä ratkaisu yksinkertaisena.

---

# 18. Tiedostotoiminnot

Pakolliset:

```text
New
Open .LEV
Save
Save As
Import PCX
Export PCX
Exit
```

Windowsissa ja Linuxissa käytetään Qt:n tiedostodialogeja.

---

# 19. Projektiformaattia ei välttämättä tarvita

Älä keksi omaa `.vwl`-projektiformaattia ensimmäisessä versiossa.

Jos `.LEV` sisältää kaiken tarvittavan, editorin pitää työskennellä suoraan `.LEV`:llä.

Oma projektiformaatti lisätään vain jos myöhemmin tarvitaan editorikohtaista dataa kuten:

```text
layers
notes
selection sets
custom palette labels
```

---

# 20. V-Wing validator

Kun formaatti tunnetaan paremmin, lisää:

```text
Tools -> Validate Level
```

Tarkista converterin dokumentaation rajoitukset.

Esimerkiksi:

```text
reserved palette indices
virheelliset material combinations
erityisobjektien väärä rakenne
turret-rakenne
base-rakenne
mahdolliset spawn/goal-rajoitukset
```

Validatori ei saa muuttaa kenttää.

Se raportoi vain:

```text
Error
Warning
Info
```

---

# 21. Cross-platform-vaatimukset

Kaikki paths:

```cpp
std::filesystem::path
```

Älä käytä kovakoodattuja:

```text
C:\
/home/user
/
```

Älä käytä WinAPI:a ellei se ole ehdottomasti tarpeen.

Älä käytä POSIX-spesifisiä API-kutsuja ellei niitä eristetä.

Qt:n pitää hoitaa alustariippuvaiset GUI-asiat.

---

# 22. Windows-jakelu

Tavoite:

```text
PCXLvlTool.exe
```

Käyttäjän ei pidä asentaa:

```text
Visual Studio
Qt SDK
CMake
Python
DOSBox
```

julkaisuversion käyttämiseksi.

Tee Release-paketti esimerkiksi:

```text
PCXLvlTool-Windows-x64.zip
```

jossa ovat:

```text
PCXLvlTool.exe
tarvittavat Qt DLL:t
platforms/qwindows.dll
LICENSE / README
```

Tarvittaessa käytä:

```text
windeployqt
```

---

# 23. Linux-jakelu

Ensisijainen kehitysversio saa olla tavallinen executable.

Julkaisua varten suosi:

```text
AppImage
```

tai vaihtoehtoisesti:

```text
tar.gz + executable + Qt dependencies
```

Flatpak voidaan tehdä myöhemmin.

Ensimmäisessä vaiheessa ei tarvitse tehdä distrokohtaisia:

```text
.deb
.rpm
Arch package
```

---

# 24. CI

Kun perusprojekti toimii, lisää GitHub Actions.

Buildaa vähintään:

```text
Windows x64
Ubuntu x64
```

Jokaisessa:

```text
configure
build
tests
```

Älä aloita projektia CI-konfiguraatiosta.

Lisää se vasta kun paikallinen build toimii.

---

# 25. Ensimmäiset milestone-tavoitteet

## Milestone 1 – Format research

Valmis kun:

```text
- pystytään analysoimaan alkuperäisiä LEV-tiedostoja
- formaatin rakenne on dokumentoitu
- compression tunnistettu
- palette/data/header tunnistettu
```

Ei GUI:ta.

---

## Milestone 2 – LEV reader

Valmis kun:

```text
levdump level.lev
```

pystyy purkamaan kentän:

```text
640×800 indeksidataksi
+
paletiksi
```

---

## Milestone 3 – PCX export

Valmis kun:

```text
LEV -> Level -> PCX
```

tuottaa visuaalisesti oikean kentän.

Tätä verrataan alkuperäiseen peliin/converteriin.

---

## Milestone 4 – LEV writer

Valmis kun:

```text
LEV -> load -> save -> LEV
```

toimii ja tiedosto latautuu alkuperäisessä V-Wingissä.

---

## Milestone 5 – Minimal GUI

Sisältää:

```text
Open
Save
canvas
zoom
pan
pencil
material selection
```

---

## Milestone 6 – Usable editor

Lisää:

```text
fill
line
rectangle
eyedropper
undo/redo
palette viewer
PCX import/export
validator
```

---

## Milestone 7 – Releases

Tuota:

```text
Windows x64 ZIP
Linux x64 AppImage
```

---

# 26. Reverse engineering -periaate

Kaikki `.LEV`-formaatista päätellyt asiat dokumentoidaan:

```text
docs/lev-format.md
```

Käytä taulukkoa:

```text
Offset      Size     Meaning
0x0000      ?        ...
0x0014      ?        ...
```

Erottele:

```text
Confirmed
Strongly suspected
Unknown
```

Älä esittele arvausta faktana.

Kirjaa myös kuinka asia vahvistettiin.

Esimerkiksi:

```text
Confirmed:
Changing PCX pixel (0,0) changes decompressed LEV byte 0.

Confirmed:
Pixels are stored row-major.

Suspected:
Bytes 0x20–0x2F may contain level metadata.
```

---

# 27. Tärkeä testiaineisto

Kun alkuperäiset freeware-version kentät toimitetaan projektiin analysoitavaksi, älä muokkaa niitä.

Pidä ne read-only testiaineistona.

Jos niitä ei haluta versionhallintaan tekijänoikeussyistä, rakenna testit niin, että käyttäjä voi sijoittaa ne esimerkiksi:

```text
testdata/original/
```

ja `.gitignore` jättää ne pois.

Testiohjelma voi tällöin ajaa corpus-testin kaikille `.LEV`-tiedostoille:

```text
for every LEV:
    load
    validate
    save temporary
    reload
    compare decoded level
```

Tämä on erittäin tärkeä testi parserille.

---

# 28. Älä tee vielä

Älä toteuta ensimmäisessä versiossa:

```text
layers
scripting
plugins
3D view
game engine
level simulation
multiplayer
online sharing
asset database
custom renderer framework
OpenGL/Vulkan renderer
tilemap abstraction
ECS
```

Qt:n tavallinen 2D-renderöinti riittää 640×800 indeksikuvaan helposti.

---

# 29. Ensimmäinen Codex-tehtävä

Aloita vain formaatin tutkimiseen tarvittavasta rungosta.

Ensimmäinen toteutustehtävä:

```text
Create a C++17/CMake project containing a command-line utility named
levdump.

levdump must:
- accept one .LEV filename
- read the complete file as bytes
- print file size
- print the first 256 bytes as a formatted hex/ASCII dump
- detect printable ASCII strings of length >= 4
- print repeated byte runs longer than 16 bytes
- perform no format assumptions yet

Also create docs/reverse-engineering.md where observations about the
format can be recorded.

Do not create the Qt GUI yet.
Do not implement speculative LEV parsing.
Keep the implementation small and portable between Windows and Linux.
```

Kun ensimmäiset oikeat `.LEV`-tiedostot ovat käytettävissä, jatka niiden vertaamiseen eikä GUI:n rakentamiseen.

---

# Lopullinen tavoite

Käyttäjän näkökulmasta ohjelman pitää lopulta toimia näin:

```text
PCXLvlTool.exe
```

tai Linuxissa:

```text
PCXLvlTool
```

Sitten:

```text
Open LEVEL.LEV
       ↓
edit visually
       ↓
Save
       ↓
LEVEL.LEV
       ↓
copy to V-Wing
       ↓
play
```

Alkuperäistä converteria tai DOSBoxia ei tarvita lopullisessa editorissa.

Converteria käytetään kehityksen aikana vain `.LEV`-formaatin reverse engineeringiin ja oman writerin oikeellisuuden varmistamiseen.

Alkuperäistä convertteria ei saa poistaa
