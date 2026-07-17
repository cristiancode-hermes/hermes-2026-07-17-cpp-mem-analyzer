# Memory Analyzer — cpp-mem-analyzer

Analizador de memoria para Linux en C++23. Lee estadísticas de `/proc/meminfo` y memoria por proceso desde `/proc/[pid]/status`, proporcionando análisis detallado de uso de memoria del sistema.

## Características

- **Resumen de memoria**: lectura completa de `/proc/meminfo` con tabla formateada
- **Top procesos por RSS**: lista los procesos que más memoria consumen
- **Monitor en tiempo real**: refresco periódico con barra de presión de memoria
- **Salida JSON**: integrable con scripts y herramientas externas
- **Vista compacta**: resumen de una línea para integración en paneles
- **Presión de memoria**: indicador 0-100% basado en memoria disponible

## Tecnologías

- **C++23** con `std::expected`, `std::println`, `std::format`
- **CLI11** — parsing de argumentos CLI moderno
- **nlohmann/json** — serialización JSON
- **spdlog** — logging estructurado
- **Catch2** — tests unitarios + integración + benchmarks

## Build

```bash
# Requisitos: cmake 3.28+, gcc 14+ o clang 18+, ninja o make
# Debian/Ubuntu:
sudo apt install cmake g++-14 pkg-config ninja-build

# Configurar y compilar
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build

# O usando presets
cmake --preset debug
cmake --build --preset debug
```

## Uso

```bash
# Resumen completo de memoria
./build/debug/mem-analyzer summary

# Top 10 procesos por uso de memoria
./build/debug/mem-analyzer top

# Top 20 procesos
./build/debug/mem-analyzer top -n 20

# Monitor en tiempo real (cada 2s, 30 iteraciones)
./build/debug/mem-analyzer monitor

# Monitor cada 5s, 10 iteraciones
./build/debug/mem-analyzer monitor -i 5 -t 10

# Salida JSON para scripting
./build/debug/mem-analyzer json

# Vista compacta (una línea)
./build/debug/mem-analyzer compact
```

### Ejemplo de salida

```
┌────────────────────────────────────────────┐
│         Memory Analyzer — Summary          │
├────────────────────────────────────────────┤
│ Total:       15.60 GB        100.0% │
│ Available:   10.20 GB         65.4% │
│ Used:         5.40 GB         34.6% │
│ Buffers:     78.12 MB                  │
│ Cached:       2.50 GB                  │
├────────────────────────────────────────────┤
│ Swap:         2.00 GB        100.0% │
│ Swap Free:    1.95 GB         97.5% │
│ Swap Used:   51.20 MB          2.5% │
├────────────────────────────────────────────┤
│ Active:      3.20 GB    │ Anon:  2.80 GB  │
│ Inactive:    1.50 GB    │ Mapped: 800 MB  │
│ Dirty:      12.00 MB    │ Slab:  450 MB  │
└────────────────────────────────────────────┘
```

## Tests

```bash
cmake --build --preset debug && ctest --preset debug
```

## Docker

```bash
docker build -t mem-analyzer .
docker run --rm -v /proc:/proc:ro mem-analyzer summary
```

## Estructura del proyecto

```
2026-07-17-cpp-mem-analyzer/
├── CMakeLists.txt          # Build principal (C++23, CMake 3.28+)
├── CMakePresets.json       # v9, debug (asan+ubsan), release (LTO)
├── Dockerfile              # multi-stage gcc:14 → debian slim
├── .gitignore
├── README.md
├── src/
│   ├── main.cpp            # Entry point + CLI
│   └── collector.cpp       # /proc collectors
├── include/
│   └── mem-analyzer/
│       └── collector.hpp   # Headers públicos
├── tests/
│   ├── CMakeLists.txt
│   ├── test_main.cpp       # Unit + benchmarks
│   └── test_collector.cpp  # Integration tests
└── docs/
    └── architecture.md
```
