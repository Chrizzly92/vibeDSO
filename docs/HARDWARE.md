# Hardwarezuordnung und Geräteprüfung

Die ältere Quelle benennt STM32F103C8 und PCB 109-13800-00C. Die Firmware-061-Datei bestätigt die folgende GPIO-Konfiguration im Bootpfad bei `0x08005454`; sie stimmt mit `Port_Init()` der bereitgestellten Quellen überein:

| Register | Wert aus 061 |
|---|---|
| GPIOA CRL / CRH | `0xB4444440 / 0x344444B4` |
| GPIOB CRL / CRH | `0x33333333 / 0x888833BB` |
| GPIOC CRH | `0x33300000` |
| GPIOD CRL | `0x00000044` |

Die Signalnamen folgen den vorhandenen Herstellerquellen; Registerwerte allein beweisen nicht die Bestückung einer konkreten Platine.

| Signal | Anschluss |
|---|---|
| Analoger Messkanal | PA0 / ADC1_IN0 |
| Empfindlichkeitsschalter | PA2 und PA1 / ADC2 |
| Kopplungsschalter | PA3 / ADC2 |
| Testsignal | PA7 / TIM3 |
| Externer Triggerpfad des Herstellerkerns | PA8 / TIM1 |
| UART1 TX / RX | PA9 / PA10, 230.400 Baud |
| LED | PA15, aktiv Low |
| TFT D0..D7 | PB0..PB7 |
| Triggerpegel-PWM | PB8 / TIM4_CH3 |
| Inverter-PWM VGEN | PB9 / TIM4_CH4 |
| V-MON, negative Rohspannung vor U4 über R41/R42 | PA6 / ADC_IN6, TP13 |
| TFT nRD / nRESET | PB10 / PB11 |
| Tasten SW4 / SW3 / SW2 / SW1 | PB12 / PB13 / PB14 / PB15, aktiv Low |
| TFT nCS / RS / nWR | PC13 / PC14 / PC15 |
| SWD | PA13 / PA14; JTAG wird zugunsten PB3/PB4 deaktiviert |

Der offene Kern löst den Trigger in Software anhand der ADC-Samples aus. Er nutzt den externen PA8-Triggerpfad der Originalbibliothek nicht für die Triggerentscheidung. Das ist ein wesentlicher Unterschied zum Herstellerkern.

VGEN wird über das V-MON-Feedback geregelt. PA5/TP12 ist ein benachbarter,
nicht dafür verwendeter Testpunkt. Details zur negativen Rohspannung vor U4 und
zur geregelten −5-V-Versorgung: [USB-Versorgung](USB_POWER.md).

## Noch erforderliche Prüfung am Gerät

1. MCU-Aufdruck, Quarz und TFT-Controller feststellen; Originalflash einschließlich `0x0800C000..0x0800C7FF` sichern.
2. Nach Programmierung Reset, 56-MHz-Takt, UART und sichtbare Boot-/Rasterdarstellung prüfen.
3. Alle vier Tasten und drei Schalter prüfen, auch lange Tastendrücke, HOLD und Fokuswechsel.
4. Bei GND-Kopplung Nullpunktabgleich prüfen; danach das interne Testsignal bei mehreren Zeitbasen darstellen.
5. Frequenzgenerator mit bekannter Amplitude/Frequenz verwenden: horizontale Zeitkalibrierung, vertikale Skalierung für jede Empfindlichkeitsstufe, beide Triggerflanken und Triggerpegel prüfen.
6. Auto ohne Signal, Normal ohne Trigger, Single, HOLD/Resume sowie rollende Zeitbasen bis 500 s/div prüfen.
7. Maximale Rate und DMA-Latenz mit Logikanalysator oder SWD bestimmen. Der kontinuierliche DMA-Ring enthält 4096 Samples; bei nominal 1 MS/s füllt sich eine Hälfte in 2,048 ms. Die Verarbeitung muss innerhalb des verfügbaren Datenfensters fertig werden.
8. Einstellungen ändern und Gerät neu starten; Flash-Seitenwechsel und Erhalt des Firmwarebereichs prüfen.

Die vollständige Geräteprüfung steht aus. Hosttests verwenden digitale Modelle, einschließlich eines Modells für Einstellungen im Flash. Sie ersetzen keine Prüfung von ADC-Kalibrierung, Interrupt-Latenzen, Displaybuszeiten oder analogen Signalen.
