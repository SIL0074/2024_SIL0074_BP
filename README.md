# Přehrávač internetového rádia využívající technologii WiFi 🇨🇿 🇸🇰

Tento repozitář obsahuje zdrojové kódy k bakalářské práci, která se zabývá návrhem a konstrukcí zařízení pro přehrávání internetových rádií pomocí modulu ESP32 s integrovanou WiFi technologií.

Práce zahrnuje teoretickou část, která se věnuje stručně WiFi a Bluetooth, mikrokontroléru ESP32, zvukovému modulu VS1053B, LCD displeji, IR ovladači a pulznímu enkodéru. Důkladně popisuje rozložení pinů těchto modulů, jejich funkce a možné využití. Dále obsahuje rešerši komerčních WiFi rádií. Cílem práce je nejen navrhnout a sestrojit funkční zařízení, ale také detailně prozkoumat technické aspekty WiFi rádií a uživatelského rozhraní. Tento projekt má potenciál přispět k rozvoji oblasti internetových rádií a sloužit jako inspirace pro další inovace v této oblasti.

## Struktura repozitáře

- `Radio_FW`: Firmware pro hlavní mikrokontrolér (ESP32). Zajišťuje připojení k Wi-Fi síti, streaming síťového audio streamu, komunikaci s audio dekodérem VS1053B a obsluhu periferního uživatelského rozhraní.
- `Hardware`: Documentation zapojení, detailní rozložení pinů (pinout) jednotlivých použitých modulů, popis jejich funkcí a schémata vzájemného propojení subsystémů.
- `3D`: Obsahuje 3D modely navržené ochranné krabičky pro rádio ve formátu `.stl` určené pro 3D tisk.
- `Dokumentace`: Teoretická část práce obsahující rešerši komerčních WiFi rádií, rozbor bezdrátových technologií (Wi-Fi, Bluetooth) a analýzu periferních komponent.

## Použitý hardware

- Mikrokontrolér ESP32 (Wi-Fi / Bluetooth SoC)
- Zvukový dekódovací modul VS1053B, LCD displej
- Infračervený (IR) ovladač a přijímač, rotační pulzní enkodér

## Autor

Bc. Pavel Šiller (2024)

**Usnesení:** Tento projekt byl vytvořen primárně pro účely vysokoškolské bakalářské práce. Zdrojové kódy a hardwarové návrhy jsou poskytovány „tak jak jsou“ bez záruky vhodnosti pro produkční využití nebo průmyslové nasazení. Autor nenese odpovědnost za případné škody způsobené jejich použitím. Uvedená řešení mohou sloužit jako vzdělávací demonstrace a inspirace pro vývoj IoT a audio systémů.

---

# Internet Radio Player Using WiFi Technology 🇬🇧 🇺🇸

This repository contains the source code for a bachelor’s thesis focused on the design and construction of a device for playing internet radios using the ESP32 module with integrated WiFi technology.

The work includes a theoretical part that addresses WiFi and Bluetooth technology, the ESP32 microcontroller, the VS1053B audio module, an LCD display, an IR controller, and a rotary encoder. It thoroughly describes the pinout of these modules, their functions, and potential applications. Furthermore, it includes a review of commercial WiFi radios. The aim of the thesis is not only to design and build a functional device but also to extensively explore the technical aspects of WiFi radios, and user interface. This project has the potential to contribute to the development of internet radios and serve as inspiration for further innovations in this field.

## Repository Structure

- `Radio_FW`: Main firmware architecture for the ESP32 microcontroller. Manages Wi-Fi network connectivity, handles network audio streaming, communicates with the VS1053B audio codec, and operates the peripheral user interface.
- `Hardware`: Wiring documentation, detailed module pinout configurations, subsystem interoperability layouts, and connection schematics.
- `3D`: Contains 3D printable `.stl` models for the custom designed protective device enclosure.
- `Documentation`: Theoretical framework including a state-of-the-art review of commercial WiFi radios, alongside wireless technology (Wi-Fi, Bluetooth) and peripheral component analysis.

## Used hardware

- ESP32 Microcontroller (Wi-Fi / Bluetooth SoC)
- VS1053B Audio Decoder Module, LCD Display Module
- Infrared (IR) remote controller & receiver, rotary pulse encoder

## Author

Bc. Pavel Siller (2024)

**Disclaimer:** This project was created primarily for the purposes of a university bachelor’s thesis. Source codes and hardware designs are provided "as is" without warranty of fitness for production use or industrial deployment. The author takes no responsibility for any damages caused by their use. The provided solutions serve as educational demonstrations and inspiration for developing IoT and audio systems.
