# Siuntų dėžutė – M5Stack + ESPHome + Home Assistant

## Failai ir jų vieta

| Failas | Paskirtis |
| --- | --- |
| `esphome/siuntu-dezute.yaml` | ESPHome programa M5Stack Basic |
| `esphome/secrets.example.yaml` | Saugus ESPHome paslapčių šablonas |
| `esphome/secrets.yaml` | Tikros Wi-Fi ir slaptažodžių reikšmės; nekelti į Git |
| `homeassistant/packages/siuntu_dezute.yaml` | Home Assistant kodų, įvykių ir pranešimų logika |
| `homeassistant/configuration.yaml` | Įjungia `packages` aplanką |
| `homeassistant/ui-lovelace.yaml` | Home Assistant skydelis |

## 1. Paleisk paslaugas ir įvesk reikšmes

Iš projekto aplanko paleisk:

```sh
docker compose up -d
```

`TZ` yra vienintelė šiame `docker-compose.yml` naudojama aplinkos reikšmė; jei reikia kitos laiko juostos, ją galima nurodyti `.env` faile (pvz., `TZ=Europe/Vilnius`). M5Stack Wi-Fi duomenys nėra Docker aplinkos kintamieji – juos įrašyk ESPHome paslapčių faile:

```sh
cp esphome/secrets.example.yaml esphome/secrets.yaml
```

`esphome/secrets.yaml` įrašyk savo Wi-Fi SSID, Wi-Fi slaptažodį, OTA slaptažodį ir atsarginio prieigos taško slaptažodį. Sugeneruok API raktą komanda `openssl rand -base64 32` ir rezultatą įrašyk kaip `api_encryption_key`. Nenaudok pavyzdinių reikšmių ir nekelk `secrets.yaml` į Git.

## 2. Tikslus išorinių dalių prijungimas prie M5Stack

Visus GPIO numerius jungi prie M5Stack plokštėje taip pažymėtų GPIO kontaktų. Prieš jungdamas išjunk maitinimą.

| Išorinė dalis / kontaktas | M5Stack arba maitinimas |
| --- | --- |
| 4×3 klaviatūra – eilutė R1 | GPIO16 |
| Klaviatūra – eilutė R2 | GPIO17 |
| Klaviatūra – eilutė R3 | GPIO2 |
| Klaviatūra – eilutė R4 | GPIO5 |
| Klaviatūra – stulpelis C1 | GPIO12 |
| Klaviatūra – stulpelis C2 | GPIO13 |
| Klaviatūra – stulpelis C3 | GPIO15 |
| Klaviatūros C4 | Nejungti; naudojama 4×3 klaviatūra |
| FS90R servo signalo laidas (oranžinis / geltonas) | GPIO26 |
| FS90R servo maitinimas (raudonas) | Atskiras stabilizuotas +5 V maitinimo šaltinis |
| FS90R servo žemė (rudas / juodas) | Maitinimo šaltinio GND; šią GND sujunk su M5Stack GND |
| Užrakto galinio jungiklio COM | M5Stack 3V3 |
| Užrakto galinio jungiklio NO | GPIO25; taip prijunk, kad kontaktas užsidarytų skląsčiui pasiekus užrakintą padėtį |
| Durelių mikrojungiklio COM | M5Stack 3V3 |
| Durelių mikrojungiklio NO | GPIO21; durelėms užsidarius jungiklis turi susispausti ir kontaktas užsidaryti |
| KY-032 VCC | M5Stack 3V3 |
| KY-032 GND | M5Stack GND |
| KY-032 OUT | GPIO35 |
| M5Stack ekranas | Integruotas, papildomai nejungiamas |

GPIO12, GPIO13 ir GPIO15 nėra tarp M5Stack Basic v2.7 dokumentacijoje išvardytų 15 įprastai prieinamų IO kontaktų. Jie parodyti atskirame M-Bus išplėtimo jungties žemėlapyje (M-Bus kontaktai 21, 22 ir 23). Jei tavo įrenginyje nėra šių kontaktų išvedimo, klaviatūrai reikės M-Bus išplėtimo / breakout plokštės; nejunk laidų į nepažymėtus kontaktus. Žr. [oficialų Basic v2.7 pinout](https://docs.m5stack.com/en/core/Basic_v2.7).

Jungiklius junk per COM ir NO kontaktus. Durelių mikrojungiklis prijungtas prie GPIO21, o skląsčio jungiklis – prie GPIO25; firmware naudoja vidinius pull-down, todėl išoriniams rezistoriams lituoti ar pirkti nereikia. Neprispaustas jungiklis nuskaitomas LOW, prispaustas – HIGH. Durelių kontaktas turi būti prispaustas uždarius dureles, skląsčio kontaktas – užrakinus skląstį. **Į jokį M5Stack GPIO nejunk 5 V.** GPIO35 yra tik įėjimas. KY-032 OUT ir visi GPIO signalai turi būti 3,3 V logikos.

M5Stack 3V3 ir GND kontaktai yra bendros maitinimo linijos: tinka bet kuris pažymėtas 3V3 ir bet kuris GND kontaktas. Nereikia atskiro 5 V/GND prijungimo kiekvienai įrenginio pusei. Sujunk visų išorinių modulių GND su M5Stack GND. KY-032 junk į 3V3, ne į Grove A 5 V kontaktą.

M5Stack gali būti maitinamas iš laptopo USB. Servo taip pat gali būti maitinamas iš atskiro laptopo USB prievado arba maitinamo USB šakotuvo (5 V ir GND), bet servo maitinimo GND sujunk su M5Stack GND. Servo paleidimo srovė gali sukelti USB įtampos kritimą; jei M5Stack persikrauna ar servo trūkčioja, servo maitink atskiru 5 V USB maitinimo šaltiniu. Nenaudok M5Stack 3V3 kontakto servo maitinimui ir nejunk servo signalo laido prie 5 V.

Klaviatūros laidų tvarką nustatyk pagal jos R1–R4 ir C1–C3 žymėjimą arba patikrink multimetru – vienodas 7 kontaktų kištukas nebūtinai reiškia tą pačią laidų tvarką. Įjungiant M5Stack nepaspausk klaviatūros mygtukų: GPIO2, GPIO5, GPIO12 ir GPIO15 yra ESP32 paleidimo konfigūracijai jautrūs kontaktai.

## 3. Paruošk Home Assistant

`homeassistant/configuration.yaml` jau įjungtas paketų aplankas:

```yaml
homeassistant:
  packages: !include_dir_named packages
```

Telefono programėlės ar pranešimų paslaugos nereikia. Sugeneruotas kodas rodomas Home Assistant skydelyje ir Home Assistant pranešimų juostoje.

## 4. Įrašyk ESPHome programą į M5Stack

ESPHome Device Builder atidaryk `esphome/siuntu-dezute.yaml`, patikrink konfigūraciją ir pasirink **Install → Plug into this computer**. Pirmam įrašymui prijunk M5Stack USB duomenų laidu ir pasirink jo USB/serial prievadą. Kai ESPHome klausia, leisk kompiliuoti ir įrašyti programą. Įsitikink, kad `secrets.yaml` užpildytas prieš kompiliuojant.

Kai M5Stack prisijungs prie Wi-Fi, ESPHome API automatiškai aptiks jį Home Assistant. Jei neranda, patikrink Wi-Fi duomenis ir kad Home Assistant bei M5Stack būtų tame pačiame vietiniame tinkle.

## 5. Susiek su Home Assistant ir patikrink

1. Home Assistant atidaryk **Nustatymai → Įrenginiai ir paslaugos** ir pridėk rastą ESPHome įrenginį.
2. ESPHome įrenginio konfigūracijoje leisk įrenginiui vykdyti Home Assistant veiksmus. Tai būtina, kad HA galėtų atrakinti skląstį ir parodyti pranešimus M5Stack ekrane.
3. Patikrink konfigūraciją ir perkrauk Home Assistant. Atverk „Siuntų dėžutė“ skydelį, sugeneruok kodą, įvesk jį klaviatūra ir patvirtink `#`; `*` ištrina paskutinį skaitmenį.
4. Pirmiausia patikrink servo kryptį ir jungiklius neapkraunant mechaninio skląsčio. Jei užrakinimo kryptis neteisinga, pakeisk `uzrakinimo_kryptis` / `atrakinimo_kryptis`; jei KY-032 būsena atvirkštinė, patikrink modulio išėjimą ir `inverted: true`.

## Veikimo seka

1. Savininkas Home Assistant skydelyje paspaudžia **Sugeneruoti kodą kurjeriui**. HA sugeneruoja 6 skaitmenų kodą, parodo jį skydelyje ir Home Assistant pranešimuose; skydelyje rodoma, ar kodas aktyvus, ir likęs laikas iš 5 minučių.
2. Kurjeris įveda kodą ir paspaudžia `#`. M5Stack siunčia jį į HA per ESPHome API; pats kodas M5Stack netikrinamas.
3. Teisingas kodas panaudojamas vieną kartą ir HA siunčia atrakinimo komandą. Kurjeris turi atidaryti dureles per 30 sekundžių.
4. Uždarius dureles M5Stack užrakina skląstį, patikrina KY-032 jutiklį ir praneša HA, ar dėžutėje aptikta siunta. Trys neteisingi bandymai panaikina kodą ir 5 minutėms užblokuoja klaviatūrą.
5. Durelių atidarymas be galiojančio atrakinimo sukelia įspėjimą savininkui.
