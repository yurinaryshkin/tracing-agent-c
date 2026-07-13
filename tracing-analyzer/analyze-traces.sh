#!/bin/bash
# Переходим в директорию, где лежит сам bash-скрипт
cd "$(dirname "$0")"

# НАСТРОЙКА ПУТЕЙ (Прописаны прямо в sh)
MAPPING_CSV="reports/class_jar_mapping.csv"
TRACE_INPUT="../../corax/tracing/" # Можно указать конкретный файл или всю папку
OUTPUT_METHODS="reports/matched_methods.csv"
OUTPUT_JARS="reports/matched_jars.txt"

# Новые переменные для шага фильтрации
REGISTERED_JARS="registered-jars.txt"
OUTPUT_NEW_JARS="reports/new-jars.txt"

echo "=== Запуск анализа файлов трейсинга ==="
# Запуск Java-файла напрямую с передачей аргументов
java src/TraceAnalyzer.java "$MAPPING_CSV" "$TRACE_INPUT" "$OUTPUT_METHODS" "$OUTPUT_JARS"

echo "=== Фильтрация и поиск новых JAR-файлов ==="

# Проверяем, создался ли файл после работы Java и есть ли файл со списком зарегистрированных
if [ ! -f "$OUTPUT_JARS" ]; then
    echo "Ошибка: Файл $OUTPUT_JARS не был создан Java-анализатором."
    exit 1
fi

if [ ! -f "$REGISTERED_JARS" ]; then
    echo "Ошибка: Базовый файл $REGISTERED_JARS не найден в корне директории."
    exit 1
fi

# Очищаем или создаем файл для новых JAR
> "$OUTPUT_NEW_JARS"

# Построчно читаем matched_jars.txt
while IFS= read -r jar || [ -n "$jar" ]; do
    # Игнорируем пустые строки
    [[ -z "$jar" ]] && continue

    # Исключаем свои джарники по признаку SNAPSHOT в версии
    if [[ "$jar" == *"-SNAPSHOT"* ]]; then
        continue
    fi

    # Проверяем, отсутствует ли jar в registered-jars.txt
    if ! grep -qxF "$jar" "$REGISTERED_JARS"; then
        echo "$jar" >> "$OUTPUT_NEW_JARS"
    fi
done < "$OUTPUT_JARS"

echo "Готово! Список новых внешних JAR-файлов сохранен в: $OUTPUT_NEW_JARS"
