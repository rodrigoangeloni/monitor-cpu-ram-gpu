# 🖥️ Monitor CPU/RAM/GPU

Overlay transparente para Windows que muestra uso de **CPU**, **RAM** y **GPU** en tiempo real.

![Monitor by RoanDev](Monitor_by_RoanDev.jpg)

---

## ⚙️ Compilar

### MinGW-w64 (recomendado)
```bat
build_mingw.bat
```

### MSVC (requiere Visual Studio Build Tools)
```bat
build_msvc.bat
```

📦 **Salida**: `monitor.exe` (~61KB con icono embebido)

---

## ▶️ Ejecutar

```bat
monitor.exe
```

---

## 🎮 Controles

| 🎯 Accion | ⌨️ Tecla/Accion |
|-----------|-----------------|
| 🖱️ Mover ventana | Click izquierdo + arrastrar |
| 📋 Menu contextual | Click derecho |
| 📌 Fijar (click-through) | Doble-click en bandeja |

---

## 📋 Dependencias

- 💻 Windows 7+ (`_WIN32_WINNT 0x0601`)
- 🔗 Links: `user32.lib gdi32.lib shell32.lib advapi32.lib pdh.lib`

---

## 🔧 Como funciona

| 🔬 Componente | 📊 Metodo |
|---------------|-----------|
| 🧠 **CPU** | `GetSystemTimes()` - calcula `(kernel + user - idle) * 100 / (kernel + user)` |
| 💾 **RAM** | `GlobalMemoryStatusEx` - uso de memoria fisica |
| 🎮 **GPU** | PDH `\GPU Engine(*)\Utilization Percentage` - counters nativos de Windows |

---

## 📝 Notas

- ⚡ MinGW requiere flag `-municode` para `wWinMain`
- 🖼️ El icono se carga de `favicon.ico` en el directorio actual
- ⏱️ Actualizacion cada 1 segundo via timer
- 🎨 Icono embebido via `icon.rc` y `windres`

---

## 📸 Screenshot

![Screenshot](Monitor_by_RoanDev.jpg)

---

<div align="center">

**👨‍💻 Monitor by RoanDev**

![GitHub](https://img.shields.io/badge/GitHub-RoanDev-181717?style=for-the-badge&logo=github)
![Windows](https://img.shields.io/badge/Windows-7%2B-0078D4?style=for-the-badge&logo=windows)

</div>
