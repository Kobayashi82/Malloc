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

- `TINY`: hasta 128 bytes con capacidad para 128 alocaciones.
- `SMALL`: hasta 2048 bytes con capacidad para 128 alocaciones.
- `LARGE`: mayor de 2048 bytes usando un `mmap` dedicado.

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

## 8. Script de evaluación

Vale, malloc es implementar el malloc de toda la vida en C
Los requisitos del proyecto son muy basicos. Hacer malloc, realloc y free usando 3 tipos de zonas o heaps, pequeña, mediana y grande.

Las zonas son un requisito del proyecto que realmente no funciona asi en el malloc nativo. No se porque lo piden, pero bueno.
Para asignar memoria se usa mmap y munmap para liberarla. Esta memoria se asigna por numeros de páginas, que son normalmente 4096 bytes

Lo que hace malloc realmente es crear un espacio de memoria e ir ampliandolo poco a poco segun se necesite. En caso de asignaciones grandes, crea un espacio aparte
especificamente para esa asignacion.

Cuando se solicita memoria, primero asigno una arena, las arenas son los contenedores de las zonas. Una arena puede tener varios hilos accediendo a ella, pero si se satura mucho
se crea otra arena. El sistema lo que hace es cuando un hilo hace la primera asignacion se intenta acceder a la primera arena con un mutex y try_lock que bloquea el mutex solo si esta libre.
Si no lo estuviera, prueba con el resto de arenas. Si al final no encuentra ninguna disponible, crea una arena nueva y se le asigna.

Cuando se le asigna una arena, se usa una variable especial que es unica por hilo y apunta a la arena asignada

Una vez tenemos la arena, se determina el tipo de zona que necesita. Para zonas pequeñas o tiny cuando se pide hasta 128 bytes, mediana o small cuando se pide hasta 2048 bytes y si es mayor, se crea una
grande o large con mmap especialmente para esa asignacion.

Aunque en realidad no es del todo asi, ya que tiene que entrar el encabezado tambien, que puede ser 8 o 16 bytes dependiendo de la arquitectura. Así que habria que hacer 128 - sizeof(encabezado), que por
lo generar es 128 - 16. Siendo 112 bytes como maximo para la zona pequeña.

Cuando se busca una zona libre se determina el porcentaje de uso para evitar usar zonas que puedan ser liberadas pronto. Si no se encuentra ninguna zona, se crea una nueva.

Otra cosa a tener en cuenta es que hay que alinear la memoria. Esto es que el puntero devuelto al usuario este alineado a cierto valor. Por ejemplo, la alineacion mas comun es 16 bytes, asi que un puntero debe ser divisible por 16 para que se considere alineado.

Por suerte el encabezado es de ese tamaño, asi que normalmente no hay que hacer nada especial. Pero hay funciones que pueden alinear a otros valores y en estos caso hay que jugar un poco con los chunks o trocos de memoria dentro de una zona para que esté alineado correctamente.

Un chunk es una porcion de una zona. Por ejemplo, la zona pequeña puede contener 128 chunks de 128 bytes. Aunque realmente si se crean chunks mas pequeños, pueden caber mas.
Un chunk esta formado por un encabezado y el espacio de usuario que empieza en el puntero devuelto. El tamaño del chunk tambien está alineado.

Dentro del encabezado hay 2 valores, tamaño o size y numero mágico o magic.
El tamaño indica la longitud del espacio de usuario y el numero magico se usa para determinar si hay corrupcion en la memoria. Esto no es infalible, se puede corromper el tamaño y no el numero magico, por ejemplo.
Dentro del tamaño, se usan los ultimos bits para guardar unos flags del chunk, como si el chunk anterior esta en uso, el tipo de zona, si es el top chunk o si se ha hecho un mmap independiente
El flag que determina si el chunk anterior esta en uso es importante para hacer la defragmentacion o coalescing. Esto es unir varios chunks libres en uno solo. Cuando el chunk anterior esta libre, se guarda justo antes del encabezado el tamaño del chunk anterior, dentro del espacio del usuario. Así se puede saber donde empieza el chunk anterior.

El uso del espacio de usuario se usa mas veces. Por ejemplo, para guardar un puntero al siguiente chunk libre del mismo tamaño. Esto se usa con los bins.

Los bins son como cajas donde se guardan listas de chunks libres por rango de tamaño. Así cuando se necesita hacer una asignacion, se busca en los bins y se devuelve el primero que se encuentre.
En caso de no haber ninguno, entonces se corta un chunk del top chunk. El top chunk es el ultimo chunk, cuando se crea una zona, todo es top chunk, un chunk del tamaño de la zona.
Por supuesto, si no hay espacio en el top chunk, se crea una zona nueva.

Cuando se libera un puntero, se marca como libre el chunk, se hace coalescing y se añade al bin correspondiente.

Como habiamos visto con el magic, hay comprobacion de errores y corrupcion. Esto se hace tanto en asignaciones como en liberaciones. Al asignar, cuando se busca en los bins, se comprueban los magic para determinar que no hay corrupcion, al hacer coalescing tambien y al liberar un puntero. Al liberar se mira si magic es correcto, si lo es, se cambia por el valor de poison, que es como magic, pero se usa
para chunks libres. Si intentamos liberar un puntero y su valor magic es igual a poison, estonces es que estamos liberando un puntero ya liberado.
Tambien se verifica alineacion, que pertenece a una asignacion nuestra, que no esta en medio de un chunk, etc.

La informacion de las arenas y zonas se guardan en una alocacion interna. Esto lo hago asi porque cuando elimino una zona, necesito saber que esa zona no existe ya, pero existio. Esto es para evitar double free, por ejemplo.

Cada arena tiene un mmap independente para guarda esa informacion y despues de los datos de la arena pongo un encabezado especial que indica cuantas zonas hay en ese mmap y un puntero a la siguiete zona interna en caso de que hubiera mas. Despues de ese encabezado esta la lista de zonas con sus valores.
Esto permite evitar el uso de encabezados de zonas en cada zona.

Ademas de la asignacion y liberacion, tambien hay variables de entorno que modifican el comportamiento de malloc. Estas permiten ajustar el comportamiento de la creacion de arenas, activar un modo debug y de logging y determinar el comportamiento ante un fallo como puntero incorrecto, double free, etc.

Aparte de las funciones obligatorias como malloc, realloc y free, he implementado otras muchas mas.

El problema es que con esas 3 probraba mi malloc con otro programas y me llegaban punteros que no habia asignado yo y a veces incluso crasheaban los programas.
Esto es porque hay mas funciones de asignacion de memoria. Al final decidi implementarlas tambien. Muchas eran sencillas, pero otras, que requerian alineacion especifica si que fueron mas complicadas.

Tambien hay algunas funciones que no estan relacionadas con asignacion y liberacion, como malloc_usable_size(), que devuelve el espacio real disponible para un puntero.
Como ya dije, los tamaños se alinean tambien. Asi que si un usuario hace malloc(25), realmente se esta asignando 25 + sizeof(encabezado) y se alinea. Esto equivaldria a 25 + 16 = 41, pero 41 no es multiplo de 16, asi que se sube hasta el multiplo, que seria 48.

Tenemos entonces que el usuario a pedio 25 bytes, pero realmente puede usar hasta 32 bytes. Que es esto lo que devolveria malloc_usable_size().

Tambien esta implementado mallopt() que sirve para modificar el comportamiento de malloc. Esto es igual que hacerlo con las variables de entorno, pero se puede hacer a nivel de codigo.
Eso si, una vez hecha la primera asignacion ya no se puede modificar la configuracion, ni con variable de entorno ni con mallopt().

Y finalmente estan las funciones de debug, que muestran el estado de las arenas y zonas, informacion especifica de un puntero y el historial de asignaciones/liberaciones de malloc, aunque este ultimo solo si la opcion de logging esta activa.

Por ultimo, para usar la libreria, se puede hacer cargandola dinamicamente. Asi sobreescribe al malloc nativo y lo usan todos los programas.
O enlazandola en la compilacion de un programa.

Es importante saber que la carga dinamica no funciona en todos los casos, si un programa necesita permisos de administrador, usará el malloc nativo ignorando nuestra libreria.

Otra cosa que me he dejado es que cuando se usa malloc con hilos y se hacen forks, cosa no recomendada, he implementado un sistema que evita deadlock. Lo que hace es bloquear todos los mutex antes del fork y luego en el padre y en el hijo desbloquearlos.
Esto evita que el hijo inicie con un mutex bloqueado y sin opcion de desbloquearlo.
