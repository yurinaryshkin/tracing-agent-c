#include <jvmti.h>
#include <jni.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <stdatomic.h>

// --- Настройка блэклиста ---
static const char* BLACKLIST_PREFIXES[] = {
    "java/",
    "javax/",
    "jdk/",
    "sun/",
    "com/sun/",
    "com/oracle/",
    "apple/",
    "org/gradle",
    "worker/org/gradle",
    "com/esotericsoftware/kryo/io/",
    "com/code_intelligence/jazzer/"
};

static const size_t NUM_BLACKLIST = sizeof(BLACKLIST_PREFIXES) / sizeof(BLACKLIST_PREFIXES[0]);

// --- Оптимизированная структура кэша на цепочках ---
#define CACHE_SIZE 1048576
#define CACHE_MASK (CACHE_SIZE - 1)

typedef struct MethodNode {
    jmethodID method;
    _Atomic(struct MethodNode*) next; // Атомарный указатель для быстрого пути
} MethodNode;

// Глобальный массив атомарных указателей на головы цепочек
static _Atomic(MethodNode*) method_cache[CACHE_SIZE];

// Глобальный мьютекс для медленного пути
static pthread_mutex_t cache_mutex = PTHREAD_MUTEX_INITIALIZER;

static inline size_t hash_method(jmethodID method) {
    return (((size_t)method) >> 3) & CACHE_MASK;
}

// Функция проверки по блэклисту
static int should_trace(const char* class_sig, const char* method_name) {
    if (class_sig == NULL || class_sig[0] != 'L') {
        return 0;
    }
    const char* name = class_sig + 1;
    size_t len = strlen(name);
    if (len == 0) return 0;
    if (name[len - 1] == ';') len--;

    for (size_t i = 0; i < NUM_BLACKLIST; i++) {
        size_t plen = strlen(BLACKLIST_PREFIXES[i]);
        if (len >= plen && strncmp(name, BLACKLIST_PREFIXES[i], plen) == 0) {
            return 0;
        }
    }
    return 1;
}

// --- Высокопроизводительный колбэк на вход в метод ---
static void JNICALL cbMethodEntry(jvmtiEnv *jvmti_env, JNIEnv *env, jthread thread, jmethodID method) {
    size_t bucket = hash_method(method);

    // ------------------------------------------------------------------
    // ШАГ 1: Быстрый путь (Чтение БЕЗ блокировок)
    // ------------------------------------------------------------------
    MethodNode* current = atomic_load_explicit(&method_cache[bucket], memory_order_acquire);

    while (current != NULL) {
        if (current->method == method) {
            return; // Мгновенный возврат в JVM без аллокаций и локов
        }
        current = atomic_load_explicit(&current->next, memory_order_acquire);
    }

    // ------------------------------------------------------------------
    // ШАГ 2: Медленный путь (Синхронизация через один goto-выход)
    // ------------------------------------------------------------------
    char *method_name = NULL;
    char *method_sig = NULL;
    char *class_name = NULL;
    int need_unlock = 0;

    // ВРЕМЕННО ОТКЛЮЧАЕМ событие для ТЕКУЩЕГО ПОТОКА, чтобы избежать дедлока при fprintf/malloc
    (*jvmti_env)->SetEventNotificationMode(jvmti_env, JVMTI_DISABLE, JVMTI_EVENT_METHOD_ENTRY, thread);

    pthread_mutex_lock(&cache_mutex);
    need_unlock = 1;

    // Double-Check: перепроверяем цепочку внутри мьютекса
    current = method_cache[bucket];
    while (current != NULL) {
        if (current->method == method) {
            goto cleanup;
        }
        current = current->next;
    }

    // Запрашиваем метаданные метода из JVM
    if ((*jvmti_env)->GetMethodName(jvmti_env, method, &method_name, &method_sig, NULL) != JVMTI_ERROR_NONE) {
        goto cleanup;
    }

    jclass method_class = NULL;
    if ((*jvmti_env)->GetMethodDeclaringClass(jvmti_env, method, &method_class) != JVMTI_ERROR_NONE) {
        goto cleanup;
    }

    if ((*jvmti_env)->GetClassSignature(jvmti_env, method_class, &class_name, NULL) != JVMTI_ERROR_NONE) {
        goto cleanup;
    }

    // Фильтрация по блэклисту и синтетичности + Вывод в stdout (Объединенное условие)
    jboolean is_synthetic = JNI_FALSE;
    if (should_trace(class_name, method_name) &&
        ((*jvmti_env)->IsMethodSynthetic(jvmti_env, method, &is_synthetic) != JVMTI_ERROR_NONE || !is_synthetic))
    {
        fprintf(stdout, "[TRACE] %s.%s%s\n", class_name, method_name, method_sig);
        fflush(stdout);
    }

    // Создаем новый узел и атомарно вставляем его в НАЧАЛО цепочки кэша
    MethodNode* new_node = (MethodNode*)malloc(sizeof(MethodNode));
    if (new_node != NULL) {
        new_node->method = method;
        new_node->next = method_cache[bucket];
        atomic_store_explicit(&method_cache[bucket], new_node, memory_order_release);
    }

cleanup:
    // Освобождаем ресурсы JVMTI
    if (class_name)  (*jvmti_env)->Deallocate(jvmti_env, (unsigned char *)class_name);
    if (method_name) (*jvmti_env)->Deallocate(jvmti_env, (unsigned char *)method_name);
    if (method_sig)  (*jvmti_env)->Deallocate(jvmti_env, (unsigned char *)method_sig);

    // Отпускаем мьютекс, если он был захвачен
    if (need_unlock) {
        pthread_mutex_unlock(&cache_mutex);
    }

    // ВКЛЮЧАЕМ событие обратно для текущего потока перед выходом в JVM
    (*jvmti_env)->SetEventNotificationMode(jvmti_env, JVMTI_ENABLE, JVMTI_EVENT_METHOD_ENTRY, thread);
}

// --- Колбэк при закрытии виртуальной машины (Вывод статистики) ---
static void JNICALL cbVMDeath(jvmtiEnv *jvmti_env, JNIEnv *env) {
    size_t total_buckets = CACHE_SIZE;
    size_t active_buckets = 0;
    size_t total_methods = 0;
    size_t max_chain_length = 0;

    for (size_t i = 0; i < total_buckets; i++) {
        MethodNode* current = method_cache[i];
        if (current != NULL) {
            active_buckets++;
            size_t current_chain_len = 0;

            while (current != NULL) {
                total_methods++;
                current_chain_len++;
                current = current->next;
            }

            if (current_chain_len > max_chain_length) {
                max_chain_length = current_chain_len;
            }
        }
    }

    double load_factor = (double)total_methods / total_buckets;

    fprintf(stderr, "\n=== [TRACE AGENT STATISTICS] ===\n");
    fprintf(stderr, "Размер массива кэша (CACHE_SIZE): %zu\n", total_buckets);
    fprintf(stderr, "Всего уникальных методов в кэше: %zu\n", total_methods);
    fprintf(stderr, "Занятых ячеек массива (Buckets): %zu\n", active_buckets);
    fprintf(stderr, "Максимальная длина цепочки коллизий: %zu\n", max_chain_length);
    fprintf(stderr, "Коэффициент заполнения (Load Factor): %.4f\n", load_factor);
    fprintf(stderr, "================================\n\n");
    fflush(stderr);
}

// --- Инициализация агента ---
JNIEXPORT jint JNICALL Agent_OnLoad(JavaVM *vm, char *options, void *reserved) {
    memset(method_cache, 0, sizeof(method_cache));

    jvmtiEnv *jvmti = NULL;
    if ((*vm)->GetEnv(vm, (void **)&jvmti, JVMTI_VERSION_1_0) != JNI_OK) {
        return JNI_ERR;
    }

    jvmtiCapabilities caps;
    memset(&caps, 0, sizeof(caps));
    caps.can_generate_method_entry_events = 1;
    (*jvmti)->AddCapabilities(jvmti, &caps);

    jvmtiEventCallbacks callbacks;
    memset(&callbacks, 0, sizeof(callbacks));
    callbacks.MethodEntry = &cbMethodEntry;
    callbacks.VMDeath = &cbVMDeath;

    (*jvmti)->SetEventCallbacks(jvmti, &callbacks, sizeof(callbacks));
    (*jvmti)->SetEventNotificationMode(jvmti, JVMTI_ENABLE, JVMTI_EVENT_METHOD_ENTRY, NULL);
    (*jvmti)->SetEventNotificationMode(jvmti, JVMTI_ENABLE, JVMTI_EVENT_VM_DEATH, NULL);

    return JVMTI_ERROR_NONE;
}
