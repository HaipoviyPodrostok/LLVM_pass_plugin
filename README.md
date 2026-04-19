# LLVM Def-Use Graph pass-plugin

Этот проект реализует пасс-плагин для компилятора LLVM, который выполняет статический анализ промежуточного представления и генерирует граф потока данных с частичным отображением графа потока управления. Также реализована система инструментации для сбора значений переменных в процессе исполнения программы.

## Зависимости
* CMake (версия 3.14)
* Инфраструктура LLVM
* Clang
* Graphviz (для отображения графа)

## Сборка и запуск
Перед запуском необходимо проверить есть ли папки:
`assets/images/`, `assets/dot_files/` и `assets/runtime_values/`).

### 1. Построение графа без инструментаций
```bash
rm -rf ./build/
# Сборка пасс-плагина
cmake -S . -B build && cmake --build build

# Генерация IR (без инструментаций)
clang++ -O0 -S -emit-llvm tests/test.cpp -o build/test.ll

# Запуск пасс-плагина
opt -load-pass-plugin ./build/DefUseGraph.so -passes="def-use-graph" build/test.ll -disable-output

# генерация графа
dot -Tpng assets/dot_files/without_instrumentation.dot -o assets/images/without_instrumentation.png
```

### 2. Построение графа с инструментациями
```bash
rm -rf ./build/

# Сборка пасс-плагина и скрипта для вставки дампа value значений
cmake -S . -B build && cmake --build build

# Сборка тестовой программы с применением пасса
clang++ -fpass-plugin=./build/DefUseGraph.so tests/test.cpp -x c runtime/logger.c -o build/dump_values.x

# Экспорт пути к файлу логера (замените плейсхолдер на свой путь)
export RUNTIME_VALUES_FILE_PATH="<ваш_абсолютный_путь_к_папке_проекта>/assets/runtime_values/runtime_values.txt"

# запуск тестовой программы 
./build/dump_values.x 5 3

# Вставка дампа значений в граф из пункта а (без инструментаций который)
./build/merged_graph.x assets/dot_files/without_instrumentation.dot assets/runtime_values/runtime_values.txt assets/dot_files/with_instrumentation.dot

# генерация графв
dot -Tpng assets/dot_files/with_instrumentation.dot -o assets/images/with_instrumentation.png
```

Запуска и генерацию графа можно автоматизировать скриптами `scripts/run_without_instr.sh` и `scripts/run_with_instr.sh`:
```bash

export RUNTIME_VALUES_FILE_PATH="<ваш_абсолютный_путь_к_папке_проекта>/assets/runtime_values/runtime_values.txt"

# без инструментаций
./scripts/run_without_instr.sh

# с инструментациями
./scripts/run_with_instr.sh 5 3
```
