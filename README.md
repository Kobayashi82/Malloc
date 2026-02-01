<div align="center">

![System & Kernel](https://img.shields.io/badge/System-brown?style=for-the-badge)
![Memory Management](https://img.shields.io/badge/Memory-Management-blue?style=for-the-badge)
![Dynamic Allocation](https://img.shields.io/badge/Dynamic-Allocation-green?style=for-the-badge)
![C Language](https://img.shields.io/badge/Language-C-red?style=for-the-badge)

*Una reimplementación de malloc y sus funciones asociadas*

</div>

<div align="center">
  <img src="/doc/Malloc.png">
</div>

# Malloc

## 🎯 Descripción

Malloc es un proyecto de la escuela 42 que implementa un sistema completo de gestión de memoria dinámico. Esta implementación va significativamente más allá de los requisitos básicos, incorporando técnicas avanzadas de asignación de memoria utilizadas en asignadores de producción como glibc malloc.

## ✨ Características

### 🔧 Funcionalidades
- **Funciones Estándar**: `malloc()`, `calloc()`, `free()`, `realloc()`
- **Funciones Adicionales**: `reallocarray()`, `aligned_alloc()`, `memalign()`, `posix_memalign()`, `malloc_usable_size()`, `valloc()`, `pvalloc()`
- **Funciones de Depuración**: `mallopt()`, `show_alloc_history()`, `show_alloc_mem()`, `show_alloc_mem_ex()`
- **Thread Safety**: Soporte completo para aplicaciones multi-hilo y forks sin dead-locks
- **Gestión de Zonas**: Sistema de zonas TINY, SMALL y LARGE

### 🚀 Características Avanzadas

#### **Sistema de Arenas**
- **Múltiples Arenas**: Cada hilo puede usar arenas separadas para reducir contención
- **Balanceado de Carga**: Distribución inteligente entre arenas disponibles

#### **Optimizaciones de Memoria**
- **Bins**: Gestión de chunks liberados para optimizar reutilización
- **Coalescing**: Fusión automática de bloques adyacentes libres
- **Alineación**: Alineación óptima de memoria
- **Encabezados**: Uso eficiente del espacio para el encabezado

#### **Protección y Seguridad**
- **Validación de Punteros**: Validación de la dirección dentro del espacio gestionado
- **Comprobación de Corrupcion**: Verificación de integridad de memoria

## 🔧 Instalación

```bash
git clone git@github.com:Kobayashi82/Malloc.git
cd malloc
make

# La librería se genera en ./lib como:
# libft_malloc_$(HOSTTYPE).so

# y se crea el enlace simbolico:
# libft_malloc.so -> libft_malloc_$(HOSTTYPE).so
```

## 🖥️ Uso

### Uso Básico
```bash
# Precargar la librería
export LD_LIBRARY_PATH="[malloc_path]/lib:$LD_LIBRARY_PATH"
export LD_PRELOAD="libft_malloc.so"

# o

export LD_PRELOAD="[malloc_path]/lib/libft_malloc.so"

# o

# Ejecutar loader
./tester/load.sh

# y luego 

# Ejecutar
./program
```

### Integración en Código C
```c
#include <stdlib.h>
#include "malloc.h"

int main() {
    // Usar malloc normalmente
    void *ptr = malloc(1024);
    
    // Visualizar estado de memoria
    show_alloc_mem();
    
    // Liberar memoria
    free(ptr);
    
    return 0;
}
```

### Compilación con la Librería
```bash
# Compilar y enlazar
gcc -o program program.c -I./inc -L./lib -lft_malloc -Wl,-rpath=./lib

# -o program		Nombre del ejecutable
# -I./inc			Busca los .h en ./inc (preprocesador)
# -L./lib			Añade ./lib al buscador de librerias compartidas (linker)
# -lft_malloc		Enlaza con libft_malloc.so
# -Wl,-rpath=./lib	El binario buscará librerías compartidas en ./lib durante la ejecución

# Ejecutar
./program
```

## 🧪 Testing

### Suite de Pruebas
```bash
# Prueba de evaluación
./tester/evaluation.sh

# Pruebas completas
./tester/complete.sh       		 # Todas laspruebas
./tester/complete.sh --main      # Pruebas principales
./tester/complete.sh --alignment # Pruebas de alineamiento
./tester/complete.sh --extra     # Pruebas de funcionalidades adicionales
./tester/complete.sh --stress    # Pruebas de estrés
./tester/complete.sh --help      # Muestra la ayuda

# Prueba de comparación com glibc malloc
./tester/bench.sh program
```

## 🔧 Variables de Entorno

Las siguientes variables de entorno pueden configurar el comportamiento de malloc:

| Variable de entorno      | Equivalente interno       | Descripción                             |
|--------------------------|---------------------------|-----------------------------------------|
| **MALLOC_ARENA_MAX**     | `M_ARENA_MAX`             | Límite máximo de arenas                 |
| **MALLOC_ARENA_TEST**    | `M_ARENA_TEST`            | Umbral de prueba para eliminar arenas   |
| **MALLOC_PERTURB_**      | `M_PERTURB`               | Rellena el heap con un patrón           |
| **MALLOC_CHECK_**        | `M_CHECK_ACTION`          | Acción ante errores de memoria          |
| **MALLOC_MIN_USAGE_**    | `M_MIN_USAGE`             | Umbral mínimo de uso para optimización  |
| **MALLOC_DEBUG**         | `M_DEBUG`                 | Activa el modo debug                    |
| **MALLOC_LOGGING**       | `M_LOGGING`               | Habilita logging                        |
| **MALLOC_LOGFILE**       | *(ruta de archivo)*       | Archivo de log (por defecto `"auto"`)   |

## 📚 Funciones Adicionales

#### MALLOPT

- Configura parámetros del asignador de memoria.

```c
  int mallopt(int param, int value);

  param – option selector (M_* constant).
  value – value assigned to the option.

  • On success: returns 1.
  • On failure: returns 0 and sets errno to:
      – EINVAL: unsupported param or invalid value.

Supported params:
  • M_ARENA_MAX (-8)       (1-64/128):  Maximum number of arenas allowed.
  • M_ARENA_TEST (-7)         (1-160):  Number of arenas at which a hard limit on arenas is computed.
  • M_PERTURB (-6)          (0-32/64):  Sets memory to the PERTURB value on allocation, and to value ^ 255 on free.
  • M_CHECK_ACTION (-5)         (0-2):  Behaviour on abort errors (0: abort, 1: warning, 2: silence).
  • M_MIN_USAGE (3)           (0-100):  Heaps under this usage % are skipped (unless all are under).
  • M_DEBUG (7)                 (0-1):  Enables debug mode (1: errors, 2: system).
  • M_LOGGING (8)               (0-1):  Enables logging mode (1: to file, 2: to stderr).

Notes:
  • Changes are not allowed after the first memory allocation.
  • If both M_DEBUG and M_LOGGING are enabled:
      – uses $MALLOC_LOGFILE if defined, or fallback to "/tmp/malloc_[PID].log"
```

#### SHOW_ALLOC_ MEM

- Muestra información sobre el estado actual de la memoria asignada y proporciona un resumen de los bloques en uso.

**Salida ejemplo:**
```
————————————
 • Arena #1
———————————————————————————————————————
 • Allocations: 7       • Frees: 1
 • TINY: 1              • SMALL: 1
 • LARGE: 0             • TOTAL: 2
———————————————————————————————————————

 SMALL : 0x70000
— — — — — — — — — — — — — — — — —
 0x70010 - 0x707e0 : 2000 bytes
                    — — — — — — —
                     2000 bytes

 TINY : 0xf0000
— — — — — — — — — — — — — — — — —
 0xf0010 - 0xf0020 : 16 bytes
 0xf0030 - 0xf0040 : 16 bytes
 0xf0050 - 0xf0060 : 16 bytes
 0xf0070 - 0xf0080 : 16 bytes
 0xf0090 - 0xf00a0 : 16 bytes
                    — — — — — — —
                     80 bytes

———————————————————————————————————————
 2080 bytes in arena #1


———————————————————————————————————————————————————————————————
 • 7 allocations, 1 free and 2080 bytes across 1 arena
```


#### SHOW_ALLOC_MEM_EX

- Versión extendida de show_alloc_mem que proporciona información más detallada sobre la memoria asignada.

**Salida ejemplo:**
```
——————————————————————————————————————
 • Pointer: 0x703ab8cbf010 (Arena #1)
————————————————————————————————————————————————————————————————————————————————————
 • Size: 112 bytes      • Offset: 0 bytes      • Length: 112 bytes
————————————————————————————————————————————————————————————————————————————————————
 0x703ab8cbf000  71 00 00 00 00 00 00 00  89 67 45 23 01 ef cd ab  q........gE#....
————————————————————————————————————————————————————————————————————————————————————
 0x703ab8cbf010  48 65 6c 6c 6f 20 57 6f  72 6c 64 21 00 00 00 00  Hello World!....
 0x703ab8cbf020  00 00 00 00 00 00 00 00  00 00 00 00 00 00 00 00  ................
 0x703ab8cbf030  00 00 00 00 00 00 00 00  00 00 00 00 00 00 00 00  ................
 0x703ab8cbf040  00 00 00 00 00 00 00 00  00 00 00 00 00 00 00 00  ................
 0x703ab8cbf050  00 00 00 00 00 00 00 00  00 00 00 00 00 00 00 00  ................
 0x703ab8cbf060  00 00 00 00 00 00 00 00  00 00 00 00 00 00 00 00  ................
 0x703ab8cbf070  00 00 00 00 00 00 00 00  00 00 00 00 00 00 00 00  ................
————————————————————————————————————————————————————————————————————————————————————
```
#### SHOW_ALLOC_HISTORY

- Muestra el historial de asignaciones y liberaciones de memoria realizadas por el programa.

## 📄 Licencia

Este proyecto está licenciado bajo la WTFPL – [Do What the Fuck You Want to Public License](http://www.wtfpl.net/about/).

---

<div align="center">

**🌐 Desarrollado como parte del curriculum de 42 School 🌐**

*"Because glibc is too mainstream"*

</div>