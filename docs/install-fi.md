# PCX Level Tool – asennusohje

PCX Level Tool ei tarvitse perinteistä asennusohjelmaa ensimmäisessä
testijulkaisussa. Windows- ja Linux-versiot jaetaan erillisinä paketteina;
Windows-ohjelmaa ei voi käyttää suoraan Linuxissa eikä päinvastoin.

## Windows 10/11 x64

Julkaisutiedosto on esimerkiksi:

```text
PCXLvlTool-<versio>-Windows-x86_64.zip
```

1. Lataa ZIP-tiedosto.
2. Napsauta tiedostoa hiiren oikealla ja valitse **Pura kaikki**. Älä käynnistä
   ohjelmaa suoraan pakatun ZIP-tiedoston sisältä.
3. Avaa purettu kansio ja käynnistä `PCXLvlTool.exe`.

Järjestelmänvalvojan oikeuksia, Qt-asennusta tai erillistä asennusohjelmaa ei
tarvita. Pidä mukana toimitetut DLL-tiedostot ja alikansiot samassa kansiossa
kuin `PCXLvlTool.exe`.

Allekirjoittamaton ensimmäinen testiversio voi näyttää Microsoft Defender
SmartScreen -varoituksen. Varmista, että paketti on ladattu projektin oikealta
julkaisusivulta ja että tarkistussumma täsmää ennen kuin valitset varoituksesta
**Lisätietoja > Suorita joka tapauksessa**.

Ohjelman voi poistaa sulkemalla sen ja poistamalla puretun ohjelmakansion.
Muualla olevia `.LEV`-, `.pxlp`- ja `.pal`-tiedostoja ei poisteta.

Windows-paketin `VWingConverter`-alikansiossa on alkuperäinen DOS-ohjelma
`CONV.EXE` sekä sen dokumentit. PCX Level Tool ei tarvitse converteria
toimiakseen. Jos säilytät tai jaat converterin edelleen, pidä kaikki alikansion
tiedostot muuttamattomina yhdessä.

## Linux x86_64

Suositeltu julkaisutiedosto on:

```text
PCXLvlTool-<versio>-x86_64.AppImage
```

AppImagea ei pureta eikä asenneta. Tee siitä suoritettava ja käynnistä se:

```sh
chmod +x PCXLvlTool-<versio>-x86_64.AppImage
./PCXLvlTool-<versio>-x86_64.AppImage
```

Saman voi tehdä tiedostonhallinnassa avaamalla tiedoston ominaisuudet,
sallimalla suorittamisen ohjelmana ja kaksoisnapsauttamalla tiedostoa.

Jos järjestelmä ilmoittaa FUSE-virheestä, AppImagen voi käynnistää väliaikaisesti
purkavana vaihtoehtona:

```sh
./PCXLvlTool-<versio>-x86_64.AppImage --appimage-extract-and-run
```

Ohjelman voi poistaa sulkemalla sen ja poistamalla AppImage-tiedoston. Muualla
olevat käyttäjän kentät ja projektit säilyvät.

Alkuperäinen V-Wing-converter sisältyy myös AppImageen ja toimitetaan Linux-
julkaisun yhteydessä helpommin avattavana erillisenä
`VWing-Level-Converter-1.91.zip`-tiedostona. Se on DOS-ohjelma ja tarvitsee
Linuxissa esimerkiksi DOSBoxin. Converter ei ole välttämätön PCX Level Toolin
käyttämiseen.

## Linux: kääntäminen lähdekoodista

Jos AppImage ei sovi käytössä olevaan jakeluun, ohjelman voi kääntää
lähdekoodista. Tarvitaan C++17-kääntäjä, CMake sekä Qt 6:n Widgets- ja SVG-
kehityspaketit.

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
ctest --test-dir build --output-on-failure
./build/PCXLvlTool
```

Qt-pakettien nimet vaihtelevat Linux-jakelun mukaan.

## Tarkistussumman varmistaminen

Julkaisun mukana pitäisi olla SHA-256-tarkistussummat. Linuxissa tarkistus
tehdään komennolla:

```sh
sha256sum PCXLvlTool-<versio>-x86_64.AppImage
```

Windows PowerShellissä:

```powershell
Get-FileHash .\PCXLvlTool-<versio>-Windows-x86_64.zip -Algorithm SHA256
```

Vertaa tulosta julkaisusivulla ilmoitettuun arvoon.

## Miksi molemmat eivät ole ZIP-tiedostoja?

Windowsissa ZIP on hyvä kannettava paketti: se sisältää ohjelman, Qt:n
ajonaikaiset DLL:t ja tarvittavat plugin-kansiot. Linux-binääri ei yleensä ole
sellaisenaan jakeluriippumaton, sillä Qt-, C++- ja järjestelmäkirjastojen versiot
vaihtelevat. AppImage kokoaa tarvittavat sovelluskirjastot yhdeksi tiedostoksi.
AppImage-projekti suosittelee jakamaan AppImagen sellaisenaan eikä pakkaamaan
sitä vielä ZIP- tai TAR-arkistoon.

Lisätietoja:

- [Qt:n Windows-julkaisuohje](https://doc.qt.io/qt-6/windows-deployment.html)
- [Qt:n Linux-julkaisuohje](https://doc.qt.io/qt-6/linux-deployment.html)
- [AppImagen käyttöohje](https://docs.appimage.org/introduction/quickstart.html)
- [AppImagen jakeluohje](https://docs.appimage.org/packaging-guide/distribution.html)
