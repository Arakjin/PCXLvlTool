# PCX Level Tool – kehittäjän muistilista

Tämä tiedosto kuvaa projektin nykyiset rajat, tärkeimmät tekniset ratkaisut ja
seuraavan julkaisun tavoitteen. Yksityiskohtaiset käyttäjä- ja formaattiohjeet
pidetään `README.md`- ja `docs/`-tiedostoissa.

## Projektin tavoite

PCX Level Tool on C++17- ja Qt 6 -pohjainen pikselintarkka editori vanhojen
indeksoituja PCX-kuvia käyttäville peleille. Ensimmäiset peliprofiilit ovat
V-Wing, Wings ja AUTS. Rakenteen pitää sallia uusien samankaltaisten pelien
lisääminen ilman editorin yhteisten piirto-, taso- ja projektitoimintojen
kopioimista.

Tuetut alustat ovat Windows x86_64 ja Linux x86_64. Julkaisut tuotetaan
Windows ZIP- ja Linux AppImage -paketteina.

## Nykyinen arkkitehtuuri

- `Level` on muuttuvankokoinen 8-bittinen indeksoitu kuva, jossa on 256 värin
  paletti ja enintään viisi tasoa.
- `.pxlp` on kaikkien pelien yhteinen, versioitu ja pelitunnisteella varustettu
  muokattava projektimuoto. Pelin käyttämä `.LEV` on erillinen julkaisuformaatti.
- `GameProfile` kuvaa pelin tunnisteen, kokorajat ja valinnaiset ominaisuudet.
  Uusi peli lisätään ensisijaisesti profiilina ja omana formaattikirjoittajana.
- Formaatti- ja projektikoodi kuuluu Qt-käyttöliittymästä riippumattomaan
  `vwing_level_format`-kirjastoon.
- Käyttöliittymä saa kysyä vain valitun peliprofiilin tukemia asetuksia.
- Paletti-indeksit ovat pelillistä dataa. Niitä ei saa muuttaa RGB-väreiksi tai
  numeroida käyttöliittymässä eri tavalla kuin tiedostossa.
- Pelille julkaistaessa vain näkyvät tasot tasoitetaan yhdeksi indeksoiduksi
  kuvaksi. Muokattavat tasot säilyvät `.pxlp`-projektissa.

## Peliprofiilit

### V-Wing

- Kiinteä kenttäkoko 640 x 800.
- Klassisen converter 1.91 -yhteensopivan `.LEV`-muodon luku ja kirjoitus.
- Pelissä näkyvä nimi tallennetaan LEV-tiedostoon, enintään 20 tulostettavaa
  ASCII-merkkiä ja tallennettaessa isot kirjaimet.
- Materiaaliryhmät ja varatut paletti-indeksit perustuvat `CONVERT.TXT`:hen.

### Wings

- Kenttäkoko 157 x 90 ... 1000 x 1000, oletuksena 400 x 400.
- Oma Wings `.LEV` -kirjoittaja vastaa MAKELEV-formaatin rakennetta.
- Valinnaisen parallax-taustan koko on `width / 2 + 78` x
  `height / 2 + 45`, käyttäen kokonaislukujakoa.
- Kenttä ja parallax-tausta ovat käyttöliittymässä erilliset välilehdet, mutta
  saman projektin dokumentteja ja käyttävät samaa palettia.
- Wings-asetuksiin kuuluvat tähdet, sade, lumi, pommitus, siviilit ja
  aseistettujen siviilien todennäköisyys.
- Kiinteitä ja varattuja Wings-indeksejä ei saa tarjota muokattaviksi.
- Wingsin paikallinen referenssiaineisto on `Wings/`-hakemistossa. Hakemisto on
  aina Gitin ja julkaisupakettien ulkopuolella eikä sen tiedostoja muokata.

### AUTS

- Kiinteä kenttäkoko 320 x 400 ja kiinteä 256 värin AUTS-paletti.
- Dokumentoidut erikoisindeksit ovat avaruus 0, rikkoutumaton 7, vesi 39 ja
  telakointilevy 92–95.
- Uudessa ja julkaistussa kentässä on alkuperäisen BMP2LEV-converterin tavoin
  kahden pikselin rikkoutumaton reunus indeksillä 7.
- AUTS `.LEV` voidaan lukea ja kirjoittaa suoraan. Muoto on RLE-pakattu eikä
  sisällä palettia tai erillistä kentän nimeä; nimi toimii DOS-tiedostonimenä.
- Pelikohtainen BMP-tuonti hyväksyy vain 320 x 400, 8-bittisen pakkaamattoman
  indeksoidun kuvan. Indeksit säilytetään ja kiinteä AUTS-paletti otetaan
  käyttöön.
- Paikallinen `AUTSCONV/`-aineisto on read-only-referenssiä ja aina Gitin sekä
  julkaisupakettien ulkopuolella.

## Uuden pelin lisäämisen reitti

1. Lisää vakaa `GameId`, profiili ja ominaisuusliput tiedostoihin
   `src/game_profile.h` ja `src/game_profile.cpp`.
2. Lisää pelin tarkka oletuspaletti ja paletti-indeksien säännöt erillään muiden
   pelien säännöistä.
3. Toteuta pelikohtainen reader/writer Qt-riippumattomana. Älä lisää formaatin
   haaroja yleisiin piirto- tai tasoluokkiin.
4. Laajenna `.pxlp`-asetusten tageja taaksepäin luettavasti. Tuntemattomat pelit
   ja virheelliset asetukset hylätään selkeällä virheellä.
5. Näytä uuden pelin asetukset uuden kentän dialogissa vain ominaisuuslippujen
   perusteella.
6. Lisää formaatti-, projekti-, paletti- ja profiilitestit sekä dokumentoi
   alkuperäislähteistä vahvistetut rajoitukset.

Arvauksia ei kirjoiteta tiedostoformaatin säännöiksi. Havainnot erotellaan
vahvistettuihin ja vielä epävarmoihin, ja alkuperäisiä tiedostoja käsitellään
vain read-only-referenssinä.

## Version 0.2.0 tavoite

0.2.0 on ensimmäinen monen pelin kehitysversio. Sen pääsisältö on:

- yhteinen `.pxlp`-projektimuoto V-Wingille, Wingsille ja myöhemmille peleille
- peliprofiileihin perustuva laajennettava backend
- uuden kentän pelivalinta sekä pelikohtaiset koko- ja ominaisuusasetukset
- Wingsin muuttuvankokoiset kentät ja tarkka oletuspaletti
- Wingsin peliasetukset, parallax-taustan oma välilehti ja oikea kokolaskenta
- Wings-yhteensopiva `.LEV`-julkaisu ilman alkuperäisen MAKELEV-ohjelman ajoa
- pelin aina käyttäjältä kysyvä indeksoidun kuvan tuonti: PCX kaikille
  tuetuille peleille sekä BMP AUTSille
- AUTS-kenttien luonti ja `.LEV`-julkaisu ilman alkuperäisen BMP2LEV-ohjelman
  ajoa
- V-Wingin nykyisten muokkaus- ja julkaisutoimintojen säilyminen

### Ennen 0.2.0-julkaisua

- Viimeistele käyttöohje kattamaan V-Wing, Wings ja AUTS.
- Tee manuaalinen smoke test alkuperäisissä V-Wing-, Wings- ja AUTS-peleissä:
  tavallinen Wings-kenttä, parallax-kenttä, AUTS-kenttä ja V-Wing-regressiotesti.
- Varmista `.pxlp`-round-trip molemmilla peliprofiileilla ja enintään viidellä
  tasolla.
- Aja GCC- ja Clang-buildit sekä kaikki CTest-testit Linuxissa.
- Varmista GitHub Actionsin Windows- ja Linux-paketointi puhtaasta tagista.
- Tarkista, ettei `Wings/`, `AUTSCONV/`, build-hakemistoja tai muuta
  referenssiaineistoa ole lähde- tai binääripaketeissa.
- Päivitä `CMakeLists.txt` versionumeroon 0.2.0 vasta julkaisuvalmiina, viimeistele
  `CHANGELOG.md` ja luo sen jälkeen tagi `v0.2.0`.

## Kehityssäännöt

- Muokkaa vain `LEVTOOLS`-repositorion tiedostoja. Ylemmän `VWing`-hakemiston,
  `Wings/`-hakemiston ja `AUTSCONV/`-hakemiston aineisto on
  read-only-referenssiä.
- Pidä riippuvuudet vähäisinä: Qt 6 Widgets/SVG ja C++-standardikirjasto.
- Säilytä Windows- ja Linux-yhteensopivuus; älä kovakoodaa paikallisia polkuja.
- Yksi käyttäjän piirtoele on yksi undo-operaatio. Rasteroidun esikatselun ja
  lopullisen piirron pitää käyttää samaa geometriaa.
- Älä muuta alkuperäisiä `CONV.EXE`, `CONVERT.TXT` tai `FILE_ID.DIZ` -tiedostoja.
  Niiden tarkistussummat ja erilliset jakeluehdot tarkistetaan CI:ssä.
- Lisää regressiotesti jokaiselle korjatulle formaatti- tai kaatumisvirheelle.
- Aja ennen committia:

  ```sh
  cmake --build build --parallel
  ctest --test-dir build --output-on-failure
  ```

## Myöhempi jatkokehitys

- Lisää seuraavat indeksoituja kuvia käyttävät pelit yksi profiili ja formaatti
  kerrallaan.
- Lisää pelikohtainen validointi yhteiseen raportointirajapintaan.
- Lisää LEV-tuonti Wingsille vasta, kun reader voidaan toteuttaa ja testata
  deterministisesti oikealla aineistolla.
- Arvioi englanninkieliset käyttöohjeet ja käyttöliittymän lokalisointi erillisinä
  julkaisutavoitteina.

Vältä tarpeettomia service/controller/plugin-kerroksia. Laajennettavuus tässä
projektissa tarkoittaa selkeitä peliprofiileja ja formaattirajoja, ei dynaamista
plugin-järjestelmää.
