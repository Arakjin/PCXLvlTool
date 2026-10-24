# OverThrust-kentät

OverThrust-kenttäprojektissa on indeksoitu materiaalikuva, erillinen
indeksoitu piirrettävä tausta, valinnainen RGBA-esitystausta ja rasterista
riippumattomat peliobjektit. Muokattava projekti tallennetaan
`.pxlp`-tiedostoksi. **Publish OverThrust Level…** luo yhden natiivin
`<level-id>.OVL`-binääritiedoston. Sen tyypitetyt lohkot sisältävät manifestin,
materiaalien PCX-kuvan, taustan OPCX-kuvan ja objektit. Jos esitystausta on
tuotu, mukana on lisäksi PNG-lohko. OVL ei ole ZIP-tiedosto eikä hakemisto.
Valmiin OVL-tiedoston voi avata suoraan **File > Open** -toiminnolla, jolloin
materiaalit, erilliset OPCX-taustatasot, asetukset ja objektit palautuvat
editoriin.

OverThrust Layered PCX -muotoinen `background.opcx` sisältää kaikki piirrettävän
taustan tasot erillisinä: kunkin
tason paletti-indeksit, läpinäkyvyysmaski, nimi, näkyvyys ja lukitus säilyvät.
Tiedoston tavallinen PCX-kuva on näkyvien tasojen yhdistetty yhteensopivuuskuva,
mutta se ei korvaa erillisiä tasotietoja.

Kentän leveys ja korkeus annetaan suoraan pikseleinä. Esimerkiksi 400 × 400
tarkoittaa editorissa 400 × 400 pikselin maailmaa. PCX Level Tool ei muuta
mittoja soluiksi tai maailmanyksiköiksi; peli tekee tarvitsemansa muunnoksen.
Myös objektien sijainnit, koot, siirtymät, säteet ja nopeudet kirjoitetaan
pikseleinä.

Kentällä on yksi **Level name** -nimi. Sama arvo kirjoitetaan julkaisun
näyttönimeksi ja tunnisteeksi. Pelaajamäärä kysytään vain, jos **Capture the
flag** on valittu; muissa pelimuodoissa kenttä ei sisällä pelaajamääräasetusta.

## Materiaalit

Paletti-indeksit 0–96 sisältävät materiaalit. Kivellä, peruskalliolla, maalla
ja murskeella on aiempaa enemmän ulkoasuindeksejä. Indeksit 97–255 ovat
muokattavan värisiä tyhjiä soluja, joiden läpi voi lentää. Indeksi 0 on myös
tyhjä. RGB-väri on vain editorin esikatselu; materiaalin pelitoiminta tulee
vakaasta materiaali- ja ulkoasutunnuksesta. Materiaalipaneelin ryhmillä voi
rajata rakenteet, orgaaniset materiaalit, metallit, rakeiset materiaalit,
nesteet, kaasut, pelilliset pinnat ja läpilennettävät värit.

### Materiaali-indeksit

| Indeksi | Materiaali / ulkoasu |
|---:|---|
| 0 | Tyhjä |
| 1–4 | Kivi A–D |
| 5–10 | Peruskallio: luonnollinen A, luonnollinen B, tekninen, luonnollinen C, luonnollinen D, tekninen B |
| 11–14 | Maa A–D |
| 15–16 | Hedelmällinen maa A–B |
| 17–19 | Ruoho A–C |
| 20–22 | Kuiva ruoho A–C |
| 23–25 | Puu A–C |
| 26–28 | Kuiva puu A–C |
| 29–31 | Puu (kasvi) A–C |
| 32–34 | Kuollut puu A–C |
| 35–37 | Lehdet: leveät, havu, tiheät |
| 38–40 | Kuolleet lehdet A–C |
| 41–43 | Köynnös A–C |
| 44–46 | Kuiva köynnös A–C |
| 47–49 | Lasi: kirkas, sininen, vihreä |
| 50–52 | Alumiini A–C |
| 53–55 | Titaani A–C |
| 56–58 | Volframi A–C |
| 59–61 | Haponkestävä materiaali A–C |
| 62–64 | Hiekka A–C |
| 65–67 | Lumi A–C |
| 68–69 | Tuhka A–B |
| 70–73 | Murske A–D |
| 74–75 | Hapon neutraloija A–B |
| 76–77 | Jää A–B |
| 78 | Tukikohdan parannusalusta |
| 79–80 | Vesi A–B |
| 81–82 | Happo A–B |
| 83–84 | Öljy A–B |
| 85–86 | Napalm A–B |
| 87–88 | Laava A–B |
| 89–90 | Savu A–B |
| 91–92 | Höyry A–B |
| 93–94 | Syttyvä kaasu A–B |
| 95–96 | Hapan kaasu A–B |
| 97–255 | Läpilennettävät värit; jokainen indeksi on pelimekaniikaltaan tyhjä solu |

Taulukon vaihtelut ovat samojen materiaalien eri ulkoasuja. Esimerkiksi kaikki
indeksit 1–4 ovat pelimekaniikaltaan kiveä. Indeksit 97–255 saa värittää
vapaasti, mutta niiden RGB-väri ei muuta niitä kiinteäksi materiaaliksi.

## Taustakuva

Editorin **OPCX Background** -välilehdellä piirretään `background.opcx`. Se on
aina käytössä ja täsmälleen kentän kokoinen. Sillä on materiaalikuvasta täysin
erillinen, vapaasti muokattava 256 värin paletti. Tätä taustaa ei vieritetä
eikä parallaksoida. Sen avulla voi piirtää esimerkiksi veden alta näkyvän
kuvituksen vaikuttamatta `materials.pcx`-soluihin. Pikselivalinnan voi kopioida
Materials- ja OPCX Background -välilehtien välillä; liitetyt indeksit käyttävät
kohdevälilehden omaa palettia. OPCX-taustan alin Background-taso on peittävä,
mutta kaikilla ylemmillä tasoilla paletti-indeksi 0 tarkoittaa aina
läpinäkyvyyttä. Siksi kopioidun alueen nollaindeksejä ei tarvitse pyyhkiä
käsin liittämisen jälkeen.

Valinnainen esitystausta voidaan tuoda PNG-, JPEG- tai staattisesta
WebP-kuvasta. Kuvan ei tarvitse käyttää samaa kuvasuhdetta tai pikselikokoa kuin
materiaaliruudukko. Tuonnissa kuva muunnetaan
sRGB RGBA8 -muotoon ja tallennetaan projektiin, joten alkuperäistä tiedostoa ei
tarvita myöhemmin. Julkaistu `background.png` kirjoitetaan aina samalla
kanonisella tavalla. Tuotu PNG on esitystausta eikä piirtoalusta. Sen
esikatselu on oletuksena piilotettu; **Show imported PNG background behind
materials** näyttää sen tarvittaessa materiaalien takana. **Parallax imported
PNG** määrittää, vieriikö vain tämä esitystausta parallaksina. PNG scroll
X/Y -arvot säätävät sen vierinnän nopeutta.

Asetusikkunan **Image limits** näyttää vain kentälle lasketun taustakuvan
enimmäiskoon, esimerkiksi **Maximum size: 1600 x 1600 px**. Pienemmät kuvat
ja eri kuvasuhteet sallitaan.

## Objektit

Objects-paneelissa luodaan liukuportteja, teleportteja, huoltopalloja,
joukkueiden lippuja ja tukikohtia omilla pikapainikkeillaan. Tukikohta merkitään helposti
löytyvällä **Mark team-base area…** -painikkeella. Valitse **Edit objects on
canvas**, jotta objektia voi siirtää vetämällä. Valitun objektin sivukahvoilla
muutetaan leveyttä tai korkeutta, kulmakahvalla molempia ja pyöreällä kahvalla
kiertoa. `Escape` poistaa objektin valinnan. Properties-painikkeen numeroarvot
jäävät tarkkaa säätöä varten. Oven
leveyttä ja korkeutta voi muuttaa erikseen. Portti liukuu oman pitkän sivunsa
suuntaisesti kuvakkeessa olevan nuolen osoittamaan suuntaan. Valitun portin
katkoviivahaamu näyttää sen avoimen sijainnin, ja **Slide distance along
arrow** säätää liikkeen pituutta. Objektit näkyvät materiaalikuvan päällä, mutta niitä ei
piirretä `materials.pcx`-tiedostoon. Objektien luonti, siirto, ominaisuuden
muutos, monistus ja poisto ovat kumottavia toimintoja.

Teleportin kohde valitaan tai kirjoitetaan sen ominaisuuksissa. Kohteeseen
piirretään pisteviiva editorissa. Kun uusi teleporter luodaan ilman kohdetta ja
kentässä on kohteeton teleporter, editori yhdistää ne automaattisesti
molempiin suuntiin.

Joukkuetilat käyttävät joukkuepaikkoihin 1–4 sidottuja team base -objekteja.
Team base luodaan merkitsemällä alueen monikulmion kulmat. Monikulmio
päätetään kaksoisnapsautuksella, hiiren oikealla painikkeella tai Enterillä;
Escape peruu luonnoksen. Luotu tukikohta poistuu valinnasta automaattisesti.
Jokainen napsautettu kulmapiste tallennetaan järjestyksessä julkaistun
OVL-tiedoston `OBJECTS`-lohkon `area_polygon_pixels`-taulukkoon. **Redraw area**
korvaa valitun tukikohdan alueen.
Capture the flag käyttää tukikohdasta erillistä **capture flag** -objektia.
Luo kullekin joukkueelle oma lippu, valitse sama team slot kuin tukikohdalla ja
siirrä lippu haluttuun paikkaan vetämällä. Lippua ei johdeta tukikohdan
monikulmiosta. Julkaisussa CTF-joukkueen sidonta sisältää sekä
`base_object_id`- että `flag_object_id`-tunnisteen.

## Julkaisun tarkistukset

Julkaisu estetään muun muassa, jos materiaalikoodi ei kuulu välille 0–255,
objektit ovat kentän ulkopuolella, tunnisteita on kahteen kertaan,
teleportin kohde puuttuu, tukikohdan monikulmio leikkaa itsensä tai valittu
joukkuetila ei sisällä vähintään kahta kelvollista tukikohtaa. CTF-julkaisu
estetään myös, jos joltain sidotulta joukkueelta puuttuu erillinen lippu. Virhe ei jätä
osittaista `.OVL`-pakettitiedostoa.
