#!/bin/bash
set -e

# --- НАСТРОЙКА НАГРУЗКИ (Одна общая переменная) ---
N=32
# -----------------------------------------------------

echo "=== [1/3] Сборка нативного Си-агента ==="
cd ../agent
clang -shared -fPIC -I"$JAVA_HOME/include" -I"$JAVA_HOME/include/darwin" -o libtrace_agent.jnilib src/trace_agent.c
cd ../test-app

echo "=== [2/3] Компиляция Java-теста ==="
mkdir -p target
javac -d target src/ru/sbrf/kafka/benchmark/*.java

echo "=== [3/3] Внешний замер времени (утилитой time) ==="

echo "1. Чистый запуск Java (БЕЗ агента) для N=$N:"
time java -cp target ru.sbrf.kafka.benchmark.BenchmarkApp $N

echo "--------------------------------------------------"

echo "2. Запуск Java С оригинальным агентом для N=$N:"
# Трейс пишется в файл, а результат Фибоначчи допишется в конец этого файла
time java -cp target -agentpath:../agent/libtrace_agent.jnilib ru.sbrf.kafka.benchmark.BenchmarkApp $N > trace_output.log

echo "--------------------------------------------------"
echo "Завершено! Лог вызовов и результат сохранены в test-app/trace_output.log"
echo "Количество строк в логе (вызовов методов):"
wc -l trace_output.log
