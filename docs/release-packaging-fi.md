# Ensimmäisen julkaisun paketointi

Suositellut x86_64-julkaisuartefaktit ovat:

```text
PCXLvlTool-<versio>-Windows-x86_64.zip
PCXLvlTool-<versio>-x86_64.AppImage
VWing-Level-Converter-1.91.zip
SHA256SUMS.txt
```

AppImage jaetaan omana tiedostonaan, ei ZIP-paketin sisällä. Lähdekoodi voidaan
tarjota erillisenä versionhallintapalvelun automaattisesti tuottamana arkistona.

## Windows-paketin tekeminen

Käännä Windows-versio Windowsissa samalla arkkitehtuurilla ja kääntäjäperheellä
kuin käytettävä Qt. Seuraava esimerkki käyttää Visual Studio 2022:n x64-
kääntäjää ja Qt:n MSVC-pakettia:

```powershell
$QtDir = "C:\Qt\6.x.x\msvc2022_64"
cmake -S . -B build-windows -G "Visual Studio 17 2022" -A x64 `
  "-DCMAKE_PREFIX_PATH=$QtDir"
cmake --build build-windows --config Release --parallel
ctest --test-dir build-windows -C Release --output-on-failure
```

Luo puhdas paketointihakemisto ja anna Qt:n `windeployqt`-työkalun kopioida
tarvittavat DLL:t ja pluginit:

```powershell
New-Item -ItemType Directory -Force package\PCXLvlTool | Out-Null
Copy-Item build-windows\Release\PCXLvlTool.exe package\PCXLvlTool\
& "$QtDir\bin\windeployqt.exe" `
  --release --compiler-runtime --dir package\PCXLvlTool `
  package\PCXLvlTool\PCXLvlTool.exe
Copy-Item README.md package\PCXLvlTool\
Copy-Item LICENSE package\PCXLvlTool\
Copy-Item THIRD_PARTY_NOTICES.md package\PCXLvlTool\
Copy-Item docs\install-fi.md package\PCXLvlTool\
Copy-Item docs\user-guide-fi.md package\PCXLvlTool\
Copy-Item -Recurse third_party package\PCXLvlTool\third_party
New-Item -ItemType Directory -Force `
  package\PCXLvlTool\VWingConverter | Out-Null
Copy-Item CONV.EXE, CONVERT.TXT, FILE_ID.DIZ `
  package\PCXLvlTool\VWingConverter\
Compress-Archive package\PCXLvlTool\* `
  PCXLvlTool-<versio>-Windows-x86_64.zip
```

`windeployqt` tutkii ohjelman riippuvuudet ja rakentaa Qt:n tarvitsemat DLL- ja
plugin-kansiot. Tarkista erityisesti, että paketissa on
`platforms/qwindows.dll`. Testaa lopullinen ZIP puhtaalla Windows-koneella,
jossa ei ole Qt SDK:ta tai kehitysympäristöä.

## Linux-paketin tekeminen

Linux-julkaisu kannattaa automatisoida CI:ssä AppImageksi. AppImage rakennetaan
mahdollisimman vanhassa tuetussa Linux-ympäristössä, koska uudemmalla glibc:llä
käännetty ohjelma ei välttämättä käynnisty vanhemmassa jakelussa. Älä käytä
Arch Linuxissa paikallisesti käännettyä binääriä yleisenä Linux-julkaisuna ilman
testausta muissa jakeluissa.

Paketoinnin vaiheet ovat:

1. Käännä Release-versio ja aja testit.
2. Asenna executable AppDir-rakenteeseen.
3. Kerää Qt Widgets-, SVG- ja alustapluginien riippuvuudet AppDir-hakemistoon
   esimerkiksi `linuxdeploy`-työkalulla ja sen Qt-pluginilla.
4. Muunna AppDir `appimagetool`-työkalulla AppImageksi.
5. Testaa AppImage vähintään puhtaalla Ubuntu- ja yhdellä muulla tuetulla
   Linux-jakelulla.

AppImage tarvitsee lisäksi projektin sovelluskuvakkeen ja `.desktop`-tiedoston.
Ne kannattaa lisätä ennen automatisoidun Linux-paketoinnin toteuttamista.

## Alkuperäisen V-Wing-converterin paketointi

`CONVERT.TXT`:n jakeluehdon mukaan `CONV.EXE`:ä saa levittää FreeWarena, kun
ohjelma pysyy muuttamattomana ja muuttamaton dokumentti pysyy sen mukana. Älä
muokkaa näitä alkuperäistiedostoja. Pakkaa kaikki kolme yhdessä:

```powershell
Compress-Archive CONV.EXE, CONVERT.TXT, FILE_ID.DIZ `
  VWing-Level-Converter-1.91.zip
```

Windowsin PCX Level Tool -ZIP sisältää samat tiedostot `VWingConverter`-
alikansiossa. Linux-julkaisussa converter-ZIP julkaistaan AppImagen rinnalla,
koska AppImage pitää jakaa sellaisenaan. Converter on DOS-ohjelma eikä PCX
Level Tool käytä sitä suorituksen aikana.

## Tarkistussummat

Kun artefaktit ovat valmiit:

```sh
sha256sum PCXLvlTool-<versio>-Windows-x86_64.zip \
  PCXLvlTool-<versio>-x86_64.AppImage \
  VWing-Level-Converter-1.91.zip > SHA256SUMS.txt
```

Julkaise tarkistussummat samassa julkaisussa tiedostojen kanssa.

## Lisenssit ennen jakelua

PCX Level Tool on julkaistu MIT-lisenssillä. Lisää ylätason `LICENSE` jokaiseen
lähdekoodi- ja binääripakettiin. Säilytä myös KolourPaint- ja Tabler-kuvakkeiden
mukana olevat lisenssit sekä `THIRD_PARTY_NOTICES.md`.

Alkuperäinen `CONV.EXE` ei kuulu MIT-lisenssin alle. Sen oma jakeluehto on
muuttamattomassa `CONVERT.TXT`:ssä, jonka pitää aina seurata muuttamattoman
ohjelman mukana. `FILE_ID.DIZ` säilytetään lisäksi osana alkuperäistä
jakelukokonaisuutta.

Jos Qt jaetaan avoimen lähdekoodin LGPL-ehtojen nojalla, varmista ennen
julkaisua, että paketti ja jakelutapa täyttävät käytetyn Qt-version
lisenssivelvoitteet, mukaan lukien lisenssitekstit, Qt:n käytön maininta ja
Qt-lähdekoodin saatavuus. Tämä tarkistus riippuu käytetystä Qt-jakelusta ja
projektin omasta lisenssivalinnasta.

Viralliset lähteet:

- [Qt Deployment Tool for Windows](https://doc.qt.io/qt-6/windows-deployment.html)
- [Qt deployment CMake-projekteissa](https://doc.qt.io/qt-6/cmake-deployment.html)
- [Qt:n avoimen lähdekoodin lisenssivelvoitteet](https://www.qt.io/development/open-source-lgpl-obligations)
- [AppImagen paketointiopas](https://docs.appimage.org/packaging-guide/index.html)
