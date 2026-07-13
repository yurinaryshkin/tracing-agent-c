#!/bin/bash
# Переходим в директорию, где лежит сам bash-скрипт
cd "$(dirname "$0")"

# НАСТРОЙКА ПУТЕЙ (Прописаны прямо в sh)
RELEASE_ZIP="../../corax/build/distributions/kafka-dist.zip"
OUTPUT_CSV="reports/class_jar_mapping.csv"

echo "=== Запуск генерации маппинга классов ==="
# Запуск Java-файла напрямую с передачей аргументов
java src/ReleaseMappingGenerator.java "$RELEASE_ZIP" "$OUTPUT_CSV"
