#!/bin/bash

# Определяем абсолютный путь к папке, где лежит сам этот скрипт
SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE}" )" && pwd )"

# Базовый корень, где лежат все репозитории (поднимаемся на 2 уровня вверх из tracing-analyzer)
CORAX_BASE_DIR="$( cd "$SCRIPT_DIR/../.." && pwd )"

# Точный путь к репозиторию corax, из которого будут идти все запуски
CORAX_REPO_DIR="$CORAX_BASE_DIR/corax"

# Относительный путь к нативному агенту трассировки
AGENT_PATH="$CORAX_BASE_DIR/tracing-agent-c/agent/libtrace_agent.jnilib"

# Общие аргументы для JVM (флаги инлайнинга обязательны)
JVM_ARGS="-agentpath:$AGENT_PATH -XX:-Inline -Xmixed"

# Папка для логов трассировки внутри репозитория corax
LOGS_DIR="$CORAX_REPO_DIR/tracing"

# Флаг версии Scala из DEVNOTES, обязательный для сборки и тестов Кафки
SCALA_SETTING="-PscalaVersion=2.13"

# ==============================================================================
# БЛОК ОТЛАДКИ (DEBUG INFO)
# ==============================================================================
echo "==============================================================================="
echo "               ОТЛАДОЧНАЯ ИНФОРМАЦИЯ / DEBUG INFO                              "
echo "==============================================================================="
echo "Директория скрипта:       $SCRIPT_DIR"
echo "Корневой репозиторий:     $CORAX_REPO_DIR"
echo "Путь к агенту трассировки:$AGENT_PATH"
echo "Папка для логов (.out):   $LOGS_DIR"
echo "Параметры JVM_ARGS:       $JVM_ARGS"
echo "Флаг Scala:               $SCALA_SETTING"
echo "-------------------------------------------------------------------------------"

if [ ! -f "$AGENT_PATH" ]; then
    echo "[ВНИМАНИЕ] Файл агента '$AGENT_PATH' не найден!"
    echo "           Убедитесь, что агент собран, прежде чем анализировать тесты."
else
    echo "[УСПЕХ] Файл агента успешно обнаружен."
fi

if [ ! -d "$CORAX_REPO_DIR" ]; then
    echo "[ОШИБКА] Основной репозиторий '$CORAX_REPO_DIR' не найден!"
    exit 1
fi
echo "==============================================================================="
echo ""

# Создаем папку для логов, если её нет
mkdir -p "$LOGS_DIR"

# Переходим в корень corax — все команды выполняются строго отсюда
cd "$CORAX_REPO_DIR" || exit 1

# ==============================================================================
# ШАГ 1: ОЧИСТКА И БЫСТРЫЙ ПРОВЕРОЧНЫЙ ПРОГОН (КОМПИЛЯЦИЯ ВСЕГО ПРОЕКТА)
# ==============================================================================
echo "=== ШАГ 1: Очистка проекта и проверка компиляции ==="

if ! ./gradlew clean releaseZip $SCALA_SETTING; then
    echo "[ОШИБКА] Очистка или сборка corax завершилась неудачно."
    exit 1
fi

if ! ./gradlew testClasses $SCALA_SETTING; then
    echo "[ОШИБКА] Компиляция проекта corax завершилась неудачно."
    exit 1
fi

echo "[УСПЕХ] Проект успешно очищен и скомпилирован с правильной версией Scala."
echo "==============================================================================="
echo ""

exit
# ==============================================================================
# ШАГ 2: РЕАЛЬНЫЙ ПРОГОН ФАЗЗИНГ-ТЕСТОВ ИЗ КОРНЯ CORAX
# ==============================================================================
echo "=== ШАГ 2: Запуск реальных фаззинг-тестов ==="

# --- Модули из kafka-fork ---

echo "Запуск фаззинг-тестов для модуля :core (Scala)..."
./gradlew :core:test --rerun-tasks --tests "unit.kafka.server.fuzz.*" $SCALA_SETTING -DtestJvmArgs="$JVM_ARGS" > "$LOGS_DIR/kafka-core-fuzz.out"

echo "Запуск фаззинг-тестов для модуля :connect:basic-auth-extension..."
./gradlew :connect:basic-auth-extension:test --rerun-tasks --tests "org.apache.kafka.connect.rest.basic.auth.extension.fuzzing.*" $SCALA_SETTING -DtestJvmArgs="$JVM_ARGS" > "$LOGS_DIR/connect-basic-auth-fuzz.out"

echo "Запуск фаззинг-тестов для модуля :connect:runtime..."
./gradlew :connect:runtime:test --rerun-tasks --tests "org.apache.kafka.connect.runtime.rest.resources.fuzzing.*" $SCALA_SETTING -DtestJvmArgs="$JVM_ARGS" > "$LOGS_DIR/connect-runtime-fuzz.out"

echo "Запуск конкретных методов в модуле :clients..."
./gradlew :clients:test --rerun-tasks \
  --tests "org.apache.kafka.common.requests.RequestContextTest.fuzzTestParseRequest" \
  --tests "org.apache.kafka.common.requests.RequestHeaderTest.fuzzTestParse" \
  $SCALA_SETTING -DtestJvmArgs="$JVM_ARGS" > "$LOGS_DIR/kafka-clients-fuzz.out"


# --- Родные модули репозитория corax ---

echo "Запуск фаззинг-тестов для модуля :schema-registry:rest-server..."
./gradlew :schema-registry:rest-server:test --rerun-tasks --tests "ru.sbrf.kafka.schemaregistry.rest.fuzzing.*" $SCALA_SETTING -DtestJvmArgs="$JVM_ARGS" > "$LOGS_DIR/rest-server.out"

echo "Запуск фаззинг-тестов для модуля :encrypted-serde..."
./gradlew :encrypted-serde:test --rerun-tasks --tests "ru.sbrf.kafka.serialization.*FuzzTest" $SCALA_SETTING -DtestJvmArgs="$JVM_ARGS" > "$LOGS_DIR/encrypted-serde-fuzz.out"

echo "Запуск конкретного метода в модуле :fingerprint-principal-builder..."
./gradlew :fingerprint-principal-builder:test --rerun-tasks \
  --tests "ru.sbrf.kafka.principal.builder.FingerprintKafkaPrincipalBuilderTest.fuzzTestBuild" \
  $SCALA_SETTING -DtestJvmArgs="$JVM_ARGS" > "$LOGS_DIR/fingerprint-principal-builder-fuzz.out"


echo -e "\n=== Все тесты завершены. Логи сохранены в директорию: ==="
echo "$LOGS_DIR"
