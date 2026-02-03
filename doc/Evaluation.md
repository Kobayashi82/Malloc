# Malloc

Este proyecto reimplementa un allocator completo en C con zonas TINY, SMALL y LARGE, soporte para multi-threading, bins para reutilización, coalescing (defragmentación), comprobación de corrupción, double free y utilidades de depuración.

## 1. Variables globales

Solamente se usan 2 variables globales.

- `g_manager`: Estructura global
- `tcache`: arena thread‑local

## 2. Arquitectura

La estructura de `malloc` se basa en arenas y heaps.

- `Arenas`: una por hilo; `tcache` apunta a la arena asignada.
- `Heaps` por arena y por tipo (TINY, SMALL, LARGE).
- `Chunks` con header + datos; el header guarda tamaño y flags.
- `Bins`: listas LIFO por tamaño para reutilización rápida.
- `Top chunk`: reserva final del heap que se va “cortando” cuando no hay chunk libre.

### Arenas
- `Arena principal`: está en memoria global (`g_manager.arena`).
- `Arenas secundarias`: se guarda en un heap propio.
- `Hilos`: se utiliza `tcache` para guardar un puntero a su arena asignada.
- `pthread_atfork`: evita deadlocks en fork.
- `Almacenamiento`: la información de arenas y heaps se guarda en páginas de metadatos. La arena principal usa un `t_heap_header` en página aparte; las arenas secundarias guardan sus metadatos en su propia página. Si no cabe, se encadenan más `t_heap_header` con `next`.

### Heaps

Hay 3 tipos de heaps:

- `TINY`: hasta 512 bytes con capacidad para 128 alocaciones.
- `SMALL`: hasta 4096 bytes con capacidad para 128 alocaciones.
- `LARGE`: mayor de 4096 bytes usando un `mmap` dedicado.

### Estructura

| `t_arena`       | Qué guarda                                 | Uso                                     |
|-----------------|--------------------------------------------|-----------------------------------------|
| `id`            | Identificador de arena (0 = main thread)   | Identifica la arena                     |
| `alloc_count`   | Contador total de asignaciones             | Métrica global de alocaciones por arena |
| `free_count`    | Contador total de liberaciones             | Métrica global de frees por arena       |
| `bins[257]`     | Array de bins LIFO por tamaño              | Reutilización rápida de chunks libres   |
| `heap_header`   | Puntero a `t_heap_header`                  | Acceso a metadatos de heaps             |
| `next`          | Siguiente `t_arena`                        | Permite encadenar más  arenas           |
| `mutex`         | Mutex de la arena                          | Thread‑safety a nivel de arena          |
|

| `t_heap_header` | Qué guarda                                 | Uso                                     |
|-----------------|--------------------------------------------|-----------------------------------------|
| `total`         | Máximo de entradas `t_heap` en esta página | Controla capacidad del header           |
| `used`          | Entradas ocupadas                          | Indica cuántos heaps están registrados  |
| `next`          | Siguiente `t_heap_header`                  | Permite encadenar más páginas           |
|

| `t_heap`        | Qué guarda                                 | Uso                                     |
|-----------------|--------------------------------------------|-----------------------------------------|
| `ptr`           | Inicio del heap (primer chunk)             | Base para navegar y liberar             |
| `padding`       | Relleno para alinear                       | Solo usado en LARGE                     |
| `size`          | Tamaño total del heap                      | Límite de asignación                    |
| `free`          | Bytes libres en el heap                    | Métrica de uso                          |
| `free_chunks`   | Número de chunks libres                    | Fragmentación/uso                       |
| `active`        | Heap válido o liberado                     | Detecta punteros a heaps ya liberados   |
| `type`          | TINY, SMALL, LARGE                         | Tipo de zona                            |
| `top_chunk`     | Puntero al top chunk                       | De aquí se corta nueva memoria          |
|

## 3. Chunk

### Estructura `[header][data]`
- **Header (`t_chunk`)**:
	- `size`: tamaño del payload alineado + `flags` en los bits bajos.
	- `magic`: `MAGIC` cuando está en uso, `POISON` cuando se libera.
	- Flags en `size`:
		- `PREV_INUSE`: indica si el chunk anterior está ocupado.
		- `HEAP_TYPE`: marca el tipo de heap.
		- `TOP_CHUNK`: identifica el top chunk del heap.
		- `MMAP_CHUNK`: indica que proviene de `mmap` (LARGE).
- `prev_size`: justo antes del header se guarda el tamaño del chunk anterior cuando hace falta, esto permite saltar hacia atrás en coalescing.
- `next` y `prev`:
	- Un chunk libre reutiliza su payload para guardar el puntero `fd` al siguiente bin.
	- Con `GET_NEXT` y `GET_PREV` se navega entre chunks para fusionar o validar.
- **Uso de espacio liberado**:
	- Al liberar: se escribe `POISON`, se inserta en bin y se usa el espacio del usuario para enlazar los bins.
	- Al reservar: se valida `POISON/MAGIC` y se saca del bin.

### Alineación

#### ¿Porqué se debe alinear la memoria?

- Muchas arquitecturas (ARM, MIPS) requieren que los datos estén alineados o causarán errores de hardware.
- En x86/x64, aunque el acceso desalineado está permitido, es significativamente más lento (puede requerir múltiples ciclos de memoria).
- Las instrucciones SIMD (SSE, AVX) requieren alineación estricta o fallarán.
- El compilador asume alineación correcta para optimizaciones, el código desalineado puede romper estas optimizaciones.

#### Uso dentro de `malloc`

- `Alineación global`: 8 bytes en 32‑bit, 16 bytes en 64‑bit.
- `Redondeo`: tamaños y direcciones se ajustan para respetar la alineación.
- `LARGE`: puede añadir `padding` para garantizar alineación sin romper el tamaño solicitado.
- `Alineación estricta`: `aligned_alloc`, `memalign` y `posix_memalign` validan potencia de 2 y múltiplo de `sizeof(void *)`.

## 4. Flujo de ejecución

### Alocación
- Se asegura init y arena.
- Determina tamaño (TINY, SMALL, LARGE).
- Busca en bins, si no hay, corta del top chunk.
- Si no cabe en el heap, crea nuevo heap.
- Marca MAGIC y aplica PERTURB si está activo.

#### realloc / reallocarray
- Casos especiales : `ptr == NULL` → `malloc`, `size == 0` → `free`.
- Intenta crecer in‑place (chunks libres o top chunk).
- Si no, aloca nuevo, copia y libera.

#### malloc(0)

- Devuelve un puntero único distinto de NULL (no reserva páginas reales).
- Se usa una base fija `ZERO_MALLOC_BASE` con valor `(void *)0x100000000000` y un contador global `alloc_zero_counter` para devolver direcciones alineadas y únicas.
- Al liberar, si el puntero pertenece al rango de `ZERO_MALLOC_BASE`, se considera válido y no se intenta liberar memoria real.

### Liberación
- Valida puntero (alineación, rango, estado).
- Detecta doble free o corrupción (POISON, MAGIC).
- Coalescing con vecinos para reducir fragmentación.
- Inserta en bin y, si el heap queda vacío, puede liberarlo.

### Seguridad
- `MAGIC`: detecta corrupcion de memoria.
- `POISON`: detecta double free.
- `PERTURB`: rellena memoria para detectar usos indebidos.

## 5. Variables de entorno

| Variable            | Equivalente `mallopt` | Descripción                                                                           |
|---------------------|-----------------------|---------------------------------------------------------------------------------------|
| `MALLOC_ARENA_MAX`  | M_ARENA_MAX           | Número máximo de arenas permitidas                                                    |
| `MALLOC_ARENA_TEST` | M_ARENA_TEST          | Número de arena en el que se calcula el límite de arenas                              |
| `MALLOC_PERTURB_`   | M_PERTURB             | Establece la memoria al valor PERTURB en la asignación, y al valor ^ 255 al liberarla |
| `MALLOC_CHECK_`     | M_CHECK_ACTION        | Comportamiento en errores de abort (0: abortar, 1: avisar, 2: ignorar)                |
| `MALLOC_MIN_USAGE_` | M_MIN_USAGE           | Los heaps con uso % inferior a este se omiten (a menos que todos estén por debajo)    |
| `MALLOC_DEBUG`      | M_DEBUG               | Activa el modo debug (1: errores, 2: sistema)                                         |
| `MALLOC_LOGGING`    | M_LOGGING             | Activa el modo logging (1: archivo, 2: stderr)                                        |
| `MALLOC_LOGFILE`    | -                     | Archivo de log (por defecto `"auto"`)                                                 |
|

## 6. Funciones implementadas

| API                  | Tipo  | Descripción                                                                                                    |
|----------------------|-------|----------------------------------------------------------------------------------------------------------------|
| `malloc`             | Main  | Reserva un bloque de memoria sin inicializar                                                                   |
| `free`               | Main  | Libera un bloque previamente asignado                                                                          |
| `realloc`            | Main  | Redimensiona un bloque existente, intentando in‑place                                                          |
| `calloc`             | Main  | Reserva memoria e inicializa a cero                                                                            |
| `reallocarray`       | Extra | Redimensiona un bloque existente, intentando in‑place. Con protección de overflow en `nmemb * size`            |
| `aligned_alloc`      | Extra | `alignment` potencia de 2 y múltiplo de `sizeof(void *)`; `size` debe ser múltiplo de `alignment`              |
| `memalign`           | Extra | `alignment` potencia de 2 y múltiplo de `sizeof(void *)`; tamaño arbitrario                                    |
| `posix_memalign`     | Extra | `alignment` potencia de 2 y múltiplo de `sizeof(void *)`; retorna código de error y no toca `*memptr` si falla |
| `malloc_usable_size` | Extra | Devuelve el tamaño útil real del bloque                                                                        |
| `valloc`             | Extra | Reserva memoria alineada a página                                                                              |
| `pvalloc`            | Extra | Reserva memoria alineada a página y redondea el tamaño a página                                                |
| `mallopt`            | Debug | Ajusta parámetros internos de `malloc`                                                                         |
| `show_alloc_mem`     | Debug | Muestra el estado de la memoria                                                                                |
| `show_alloc_mem_ex`  | Debug | Muestra detalles de un puntero (`hexdump`)                                                                     |
| `show_alloc_history` | Debug | Muestra historial de alocaciones y liberaciones                                                                |
|

## 7. Uso de la librería

### Cargar dinámicamente

- Se genera `libft_malloc_$HOSTTYPE.so` y un enlace simbólico `libft_malloc.so` en la carpeta `lib`.
- export `LD_LIBRARY_PATH=path/to/library`.
- export `LD_PRELOAD=library.so`.

### Compilar y enlazar

- Incluir headers: `-I./inc`
- Enlazar con la librería: `-Lpath/to/library -llibrary` (no se pone `lib` ni `.so` del nombre).
- Asegurar búsqueda en runtime: `-Wl,-rpath=path/to/library`
