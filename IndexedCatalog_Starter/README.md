# Datos del estudiante
Nombre: Edwin Gabriel Portillo García
Número de cuenta: 22511038

# Starter — Laboratorio 2: catálogo indexado y confiable

Este proyecto contiene la infraestructura y las pruebas visibles del Laboratorio 2 de Estructura de Datos II.

Consulte la especificación completa en:

```text
../Lab2_Catalogo_Indexado_Confiable_EstructuraDeDatosII-Q32026.md
```

## Trabajo del estudiante

Modifique `src/catalog.cpp` y complete:

1. `read_record_at`
2. `build_primary_index`
3. `find_offset`
4. `build_composer_index`
5. `find_by_composer`
6. `verify_primary_index`
7. Opcional: `intersect_sorted`

Puede crear funciones auxiliares privadas dentro de ese archivo. Agregue sus pruebas en `tests/student_tests.cpp` y actualice este README. No modifique las interfaces públicas, `tests/tests.cpp`, `CMakeLists.txt` ni los demás archivos provistos.

## Compilar

```bash
cmake -S . -B build
cmake --build build
```

## Ejecutar pruebas

```bash
ctest --test-dir build --output-on-failure
```

O directamente:

```bash
./build/catalog_tests
```

Para ejecutar un ejercicio específico:

```bash
./build/catalog_tests --test-case="E04*"
```

El starter compila desde el inicio, pero las pruebas fallan hasta completar los TODO.

## Generar datos de ejemplo

```bash
./build/catalog_generate data/catalog.psv data/catalog.bin
```

## Usar la aplicación

```bash
./build/lab2_catalog build data/catalog.bin data/catalog.idx
./build/lab2_catalog find data/catalog.bin data/catalog.idx DG18807
./build/lab2_catalog composer data/catalog.bin data/catalog.idx BEETHOVEN
./build/lab2_catalog verify data/catalog.bin data/catalog.idx
```

## Simular corrupción

```bash
./build/catalog_corrupt flip data/catalog.bin data/catalog.corrupt 20
./build/lab2_catalog verify data/catalog.corrupt data/catalog.idx

./build/catalog_corrupt truncate data/catalog.bin data/catalog.truncated 3
./build/lab2_catalog verify data/catalog.truncated data/catalog.idx
```

## Archivos que ya están completos

- `src/binary_io.cpp`: I/O little-endian y utilidades de stream.
- `src/crc32.cpp`: CRC-32/ISO-HDLC.
- `src/catalog_codec.cpp`: codificación y decodificación del payload.
- `src/index_io.cpp`: persistencia del índice primario.
- `src/main.cpp`: interfaz de línea de comandos.
- `tools/`: generación y corrupción controlada de datos.

## Antes de entregar

Actualice este README con:

- Nombre del estudiante.
- Complejidad de las operaciones principales.
- Diferencia entre integridad física y consistencia lógica.
- Descripción de las pruebas adicionales realizadas en `tests/student_tests.cpp`.

No entregue `build/`, ejecutables ni archivos generados `.bin`, `.idx` o `.corrupt`.

## Complejidad de las operaciones principales

### Construcción del índice primario

`build_primary_index` recorre los registros del archivo y construye una entrada por cada registro válido. Después ordena el índice por `label_id`.

- Recorrido: `O(n)`
- Ordenamiento: `O(n log n)`
- Detección de duplicados: `O(n)`

Complejidad total: `O(n log n)`.

### Búsqueda primaria

`find_offset` utiliza búsqueda binaria manual sobre el índice primario ordenado.

Complejidad: `O(log n)`.

La recuperación completa de un registro requiere además un acceso mediante `seek` y la lectura del registro en el archivo.

### Construcción del índice secundario

`build_composer_index` recorre las entradas primarias, obtiene pares `(composer, label_id)`, los ordena y luego los agrupa por compositor.

Complejidad total: `O(n log n)`.

### Búsqueda secundaria

`find_by_composer` utiliza búsqueda binaria sobre el índice secundario ordenado por compositor.

Si existen `c` compositores diferentes, la complejidad es:

`O(log c)`.


## Integridad física y consistencia lógica

### Integridad física

La integridad física se refiere a si los datos almacenados pueden leerse correctamente y si sus bytes coinciden con los datos esperados.

Ejemplos:

- header truncado;
- payload truncado;
- CRC faltante;

### Consistencia lógica

La consistencia lógica se refiere a si las estructuras y referencias del sistema tienen sentido entre sí.

Ejemplos:

- una clave del índice no coincide con la clave del registro;
- claves primarias duplicadas;
- índice desordenado;

Un registro puede tener un CRC válido y aun así presentar una inconsistencia lógica.


## Pruebas adicionales

Se agregaron tres pruebas en `tests/student_tests.cpp`:

1. Se verifica que `read_record_at` retorne `InvalidOffset` y que `record` no tenga valor cuando se intenta leer desde el offset `0` de un archivo vacío.

2. Se verifica que `find_offset` retorne `std::nullopt` cuando la clave buscada no existe en el índice primario.

3. Se verifica que `build_composer_index` descarte una entrada cuando la clave almacenada en el índice no coincide con la clave real del registro, retornando `IndexKeyMismatch` en `skipped`.