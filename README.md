# 🖥️ Monitor CPU/RAM/GPU

Overlay transparente para Windows que muestra uso de **CPU**, **RAM**, **GPU**, **VRAM** y **red** en tiempo real. Sin bloat, sin dependencias externas, solo lo esencial.

![Monitor by RoanDev v1.6.1](img/Monitor_by_RoanDev_v1.6.1.jpg)

---

## 🎯 Características

- **1 solo archivo** — lógica completa en `monitor.c`
- **<170 KB el EXE** — compilado con optimización de tamaño (MSVC ~169 KB / MinGW ~71 KB)
- **Solo APIs nativas** — Win32 GDI + PDH + DXGI, cero librerías de terceros
- **Interfaz minimalista y compacta** — barras con texto y porcentaje dentro de cada barra
- **Overlay transparente** — siempre visible, nunca molesta
- **Recuerda posición, opacidad y escala** — se guarda automáticamente en el registro
- **Encabezado informativo** — título `MONITOR` con el subtítulo `CPU RAM GPU VRAM`
- **Icono en la bandeja** — clic derecho para fijar/desfijar posición, configuración o salir
- **Menú de configuración** — clic en el engranaje superior derecho

Lo que realmente importa: **cuánto están usando tu CPU, RAM, GPU y red ahora mismo**. Sin distracciones.

---

## ⚙️ Compilar

### MinGW-w64 (~71 KB)
```bat
build_mingw.bat
```

### MSVC (requiere Visual Studio Build Tools, ~169 KB)
```bat
build_msvc.bat
```

📦 **Salida**: `monitor.exe` — siempre bajo 300 KB. En los [releases](https://github.com/rodrigoangeloni/monitor-cpu-ram-gpu/releases/latest) se publican ambas compilaciones: `monitor-mingw.exe` y `monitor-msvc.exe`.

---

## ▶️ Ejecutar

```bat
monitor.exe
```

---

## 🎮 Controles

| Acción | Tecla/Acción |
|--------|--------------|
| Mover ventana | Click izquierdo + arrastrar |
| Menú contextual | Click derecho |
| Fijar posición | Menú → "Fijar posición" (o doble-click en bandeja) |
| Desfijar posición | Menú → "Desfijar posición" (o doble-click en bandeja) |
| Menú de la bandeja | Click derecho sobre el icono de la bandeja del sistema |
| Salir | Menú → "Salir" (ventana o bandeja) |

---

## 📋 Dependencias

- Windows 7+ (`_WIN32_WINNT 0x0601`)
- Solo APIs del sistema: `user32.lib gdi32.lib shell32.lib advapi32.lib pdh.lib`

---

## 🔧 Cómo funciona

| Componente | Método |
|------------|--------|
| **CPU** | PDH `\Processor Information(_Total)\% Processor Utility` |
| **RAM** | `GlobalMemoryStatusEx` — uso de memoria física |
| **GPU** | PDH `\GPU Engine(*)\Running Time` — contadores nativos de Windows |
| **VRAM** | PDH `\GPU Adapter Memory(*)\Dedicated Usage` + DXGI adapter memory |
| **Red** | `GetIfTable` — contadores de interfaces de red |
| **Posición/Opacidad/Escala** | Registry `HKCU\Software\RoanDev\MonitorOverlay` — se guarda automáticamente |

---

## 📝 Notas

- MinGW requiere flag `-municode` para `wWinMain`
- MSVC requiere `/utf-8` para strings Unicode con caracteres acentuados
- Icono y detalles de archivo embebidos en el EXE vía `icon.rc`
- Actualización cada 1 segundo vía timer
- GPU reenumera engines cada 5s para detectar nuevos procesos
- Mutex `MonitorCPU_RAM_GPU_SingleInstance` previene múltiples instancias
- Interfaz compacta tipo primera versión, con escalado 50%-150%

---

## 📸 Screenshot

| v1.0.0 | v1.5.0 | v1.6.1 |
|--------|--------|--------|
| ![v1.0.0](img/Monitor_by_RoanDev_v1.0.0.jpg) | ![v1.5.0](img/Monitor_by_RoanDev_v1.5.0.jpg) | ![v1.6.1](img/Monitor_by_RoanDev_v1.6.1.jpg) |
| [Descargar v1.0.3](https://github.com/rodrigoangeloni/monitor-cpu-ram-gpu/releases/tag/v1.0.3) | [Descargar v1.5.0](https://github.com/rodrigoangeloni/monitor-cpu-ram-gpu/releases/tag/v1.5.0) | [Descargar v1.6.1](https://github.com/rodrigoangeloni/monitor-cpu-ram-gpu/releases/tag/v1.6.1) |

---

<div align="center">

**Monitor by RoanDev**

![GitHub](https://img.shields.io/badge/GitHub-RoanDev-181717?style=for-the-badge&logo=github)
![Windows](https://img.shields.io/badge/Windows-7%2B-0078D4?style=for-the-badge&logo=windows)

</div>
