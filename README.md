# 🖥️ Monitor CPU/RAM/GPU

Overlay transparente para Windows que muestra uso de **CPU**, **RAM** y **GPU** en tiempo real. Sin bloat, sin dependencias externas, solo lo esencial.

![Monitor by RoanDev](Monitor_by_RoanDev.jpg)

---

## 🎯 Por qué este monitor

La mayoría de monitores de sistema vienen cargados de gráficos, historiales, alertas y features que rara vez se usan. Este proyecto va en la dirección opuesta:

- **1 solo archivo** — 514 líneas de C, sin dependencias externas
- **~155 KB el EXE** — compilado con optimización de tamaño, cabe en un disquete
- **Solo APIs nativas** — Win32 GDI + PDH, cero librerías de terceros
- **Interfaz minimalista** — tres barras, tres porcentajes, nada más
- **Overlay transparente** — siempre visible, nunca molesta

Lo que realmente importa: **cuánto están usando tu CPU, RAM y GPU ahora mismo**. Sin distracciones.

---

## ⚙️ Compilar

### MinGW-w64 (recomendado, ~43 KB)
```bat
build_mingw.bat
```

### MSVC (requiere Visual Studio Build Tools, ~155 KB)
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

| 🎯 Acción | ⌨️ Tecla/Acción |
|-----------|-----------------|
| 🖱️ Mover ventana | Click izquierdo + arrastrar |
| 📋 Menú contextual | Click derecho |
| 📌 Fijar (click-through) | Doble-click en bandeja |

---

## 📋 Dependencias

- 💻 Windows 7+ (`_WIN32_WINNT 0x0601`)
- 🔗 Solo APIs del sistema: `user32.lib gdi32.lib shell32.lib advapi32.lib pdh.lib`

---

## 🔧 Cómo funciona

| 🔬 Componente | 📊 Método |
|---------------|-----------|
| 🧠 **CPU** | PDH `\Processor Information(_Total)\% Processor Utility` |
| 💾 **RAM** | `GlobalMemoryStatusEx` — uso de memoria física |
| 🎮 **GPU** | PDH `\GPU Engine(*)\Running Time` — contadores nativos de Windows |

---

## 📝 Notas

- ⚡ MinGW requiere flag `-municode` para `wWinMain`
- 🖼️ Icono y detalles de archivo embebidos en el EXE vía `icon.rc`
- ⏱️ Actualización cada 1 segundo vía timer
- 🎯 GPU reenumera engines cada 5s para detectar nuevos procesos

---

## 📸 Screenshot

![Screenshot](Monitor_by_RoanDev.jpg)

---

<div align="center">

**👨‍💻 Monitor by RoanDev**

![GitHub](https://img.shields.io/badge/GitHub-RoanDev-181717?style=for-the-badge&logo=github)
![Windows](https://img.shields.io/badge/Windows-7%2B-0078D4?style=for-the-badge&logo=windows)

</div>
