# Solar-Tracking-System

# Solar Tracker - Seguidor Solar Automático

[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](https://opensource.org/licenses/MIT)
[![Arduino](https://img.shields.io/badge/Arduino-Compliant-blue?logo=arduino&logoColor=white)](https://www.arduino.cc/)
<!-- Agrega más badges si usas ESP32, Raspberry Pi, etc. -->

Un sistema **seguidor solar** (solar tracker) de **eje dual** que orienta automáticamente un panel solar hacia el sol para maximizar la captación de energía.

![Demostración o foto del prototipo](https://via.placeholder.com/800x400?text=Foto+del+Solar+Tracker)  
*(Reemplaza esta URL con una foto real de tu montaje – sube la imagen a la carpeta `/images` o usa un enlace externo)*

## 📖 Descripción

Este proyecto implementa un seguidor solar que ajusta la posición de un panel fotovoltaico en **dos ejes** (azimut y elevación) para mantenerlo perpendicular a los rayos solares durante todo el día.

Existen dos enfoques comunes implementados en versiones del proyecto:

- **Modo sensor-based** (más simple y económico): utiliza 4 LDR (fotoresistencias) para detectar la intensidad luminosa y servomotores para corregir la posición.
- **Modo astronómico** (más preciso): calcula la posición del sol usando fecha, hora y coordenadas geográficas (sin depender de sensores de luz).

## ✨ Características principales

- Seguimiento automático de doble eje
- Opción de control basado en sensores LDR o cálculo astronómico
- Bajo consumo energético
- Posibilidad de monitoreo (serial, OLED, web server – según versión)
- Fácil calibración y ajustes de límites físicos
- Protección contra vientos fuertes / posición de seguridad nocturna (según implementación)

## 🛠️ Hardware requerido

| Componente                  | Cantidad | Notas / Modelo recomendado                  |
|-----------------------------|----------|---------------------------------------------|
| Microcontrolador            | 1        | **Arduino Uno / Nano / ESP32 / Mega**       |
| Servomotor                  | 2        | SG90, MG996R o similar (torque según panel) |
| LDR (fotoresistencia)       | 4        | GL5516 o similares                          |
| Resistencia 10 kΩ           | 4        | Divisor de voltaje para LDR                 |
| Panel solar pequeño         | 1        | 5–10 W para pruebas                         |
| Fuente de alimentación      | 1        | 5–6 V estable (o batería + regulador)       |
| Estructura mecánica         | 1        | Impresa 3D, madera, aluminio o kit comercial |
| Opcional: pantalla OLED     | 1        | SSD1306 0.96" para mostrar ángulos/hora     |
| Opcional: RTC               | 1        | DS3231 (para modo astronómico preciso)      |

## 📂 Estructura del repositorio
