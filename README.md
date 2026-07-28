# 🖥️ Monitor CPU/RAM/GPU

Overlay transparente para Windows que muestra uso de **CPU**, **RAM** y **GPU** en tiempo real. Sin bloat, sin dependencias externas, solo lo esencial.

![Monitor by RoanDev](Monitor_by_RoanDev.jpg)

---

## 🎯 Por qué este monitor

La mayoría de monitores de sistema vienen cargados de gráficos, historiales, alertas y features que rara vez se usan. Este proyecto va en la dirección opuesta:

- **1 solo archivo** — ~530 líneas de C, sin dependencias externas
- **~158 KB el EXE** — compilado con optimización de tamaño
- **Solo APIs nativas** — Win32 GDI + PDH, cero librerías de terceros
- **Interfaz minimalista** — tres barras, tres porcentajes, nada más
- **Overlay transparente** — siempre visible, nunca molesta
- **Recuerda posición** — al reiniciar aparece donde lo dejaste

Lo que realmente importa: **cuánto están usando tu CPU, RAM y GPU ahora mismo**. Sin distracciones.

---

## ⚙️ Compilar

### MinGW-w64 (~43 KB)
```bat
build_mingw.bat
```

### MSVC (requiere Visual Studio Build Tools, ~158 KB)
```bat
build_msvc.bat
```

📦 **Salida**: `monitor.exe` — siempre bajo 300 KB

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
| Salir | Menú → "Salir" |

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
| **Posición** | Registry `HKCU\Software\RoanDev\MonitorOverlay` — se guarda automáticamente al mover |

---

## 📝 Notas

- MinGW requiere flag `-municode` para `wWinMain`
- MSVC requiere `/utf-8` para strings Unicode con caracteres acentuados
- Icono y detalles de archivo embebidos en el EXE vía `icon.rc`
- Actualización cada 1 segundo vía timer
- GPU reenumera engines cada 5s para detectar nuevos procesos
- Mutex `MonitorCPU_RAM_GPU_SingleInstance` previene múltiples instancias

---

## 📸 Screenshot

![Screenshot](Monitor_by_RoanDev.jpg)

---

<div align="center">

**Monitor by RoanDev**

![GitHub](https://img.shields.io/badge/GitHub-RoanDev-181717?style=for-the-badge&logo=github)
![Windows](https://img.shields.io/badge/Windows-7%2B-0078D4?style=for-the-badge&logo=windows)

</div>
