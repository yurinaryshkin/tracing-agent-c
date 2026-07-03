#include <jvmti.h>
#include <stdio.h>
#include <string.h>

// Global jvmtiEnv pointer (initialized in Agent_OnLoad)
static jvmtiEnv *jvmti = NULL;

// Forward declaration (needed before Agent_OnLoad)
static void JNICALL cbMethodEntry(jvmtiEnv *jvmti_env, JNIEnv *env, jthread thread, jmethodID method);

#ifdef __cplusplus
extern "C" {
#endif
// Called on -agentpath load (static attach)
__attribute__((visibility("default")))
JNIEXPORT jint JNICALL
Agent_OnLoad(JavaVM *vm, char *options, void *reserved) {
    // return init_agent(vm, options, reserved);
    jvmtiError err;
    // Get JVMTI env
    if ((*vm)->GetEnv(vm, (void **)&jvmti, JVMTI_VERSION_1_0) != JNI_OK) {
        fprintf(stderr, "ERROR: Unable to create JVMTI env\n");
        return JNI_ERR;
    }
    // Request capabilities (only once)
    jvmtiCapabilities caps;
    memset(&caps, 0, sizeof(caps));
    err = (*jvmti)->GetCapabilities(jvmti, &caps);
    if (err == JVMTI_ERROR_NONE) {
        printf("[INFO] can_generate_method_entry_events = %d\n", caps.can_generate_method_entry_events);
    }
    caps.can_generate_method_entry_events = 1;
    err = (*jvmti)->AddCapabilities(jvmti, &caps);
    if (err != JVMTI_ERROR_NONE) {
        fprintf(stderr, "ERROR: AddCapabilities failed: %d\n", err);
        return JNI_ERR;
    }
    // printf("[TRACE_AGENT] can_generate_method_entry_events capability enabled\n");

    jvmtiEventCallbacks callbacks;
    memset(&callbacks, 0, sizeof(callbacks));
    callbacks.MethodEntry = cbMethodEntry;
    err = (*jvmti)->SetEventCallbacks(jvmti, &callbacks, (jint)sizeof(callbacks));
    if (err != JVMTI_ERROR_NONE) {
        fprintf(stderr, "ERROR: SetEventCallbacks failed: %d\n", err);
        // free(visited_methods);
        // visited_methods = NULL;
        return JNI_ERR;
    }
    // Enable event
    err = (*jvmti)->SetEventNotificationMode(jvmti, JVMTI_ENABLE,
                                             JVMTI_EVENT_METHOD_ENTRY, NULL);
    if (err != JVMTI_ERROR_NONE) {
        fprintf(stderr, "ERROR: Enable MethodEntry failed: %d\n", err);
        // free(visited_methods);
        // visited_methods = NULL;
        return JNI_ERR;
    }
    printf("[TRACE_AGENT] Loaded and enabled\n");

    return JNI_OK;
}
// Called on jcmd ... JVMTI.agent_load (dynamic attach)
__attribute__((visibility("default")))
JNIEXPORT jint JNICALL
Agent_OnAttach(JavaVM *vm, char *options, void *reserved) {
    // return init_agent(vm, options, reserved);
    jvmtiError err;
    // Get JVMTI env
    if ((*vm)->GetEnv(vm, (void **)&jvmti, JVMTI_VERSION_1_0) != JNI_OK) {
        fprintf(stderr, "ERROR: Unable to create JVMTI env\n");
        return JNI_ERR;
    }

    jvmtiEventCallbacks callbacks;
    memset(&callbacks, 0, sizeof(callbacks));
    callbacks.MethodEntry = cbMethodEntry;
    err = (*jvmti)->SetEventCallbacks(jvmti, &callbacks, (jint)sizeof(callbacks));
    if (err != JVMTI_ERROR_NONE) {
        fprintf(stderr, "ERROR: SetEventCallbacks failed: %d\n", err);
        // free(visited_methods);
        // visited_methods = NULL;
        return JNI_ERR;
    }
    // Enable event
    err = (*jvmti)->SetEventNotificationMode(jvmti, JVMTI_ENABLE,
                                             JVMTI_EVENT_METHOD_ENTRY, NULL);
    if (err != JVMTI_ERROR_NONE) {
        fprintf(stderr, "ERROR: Enable MethodEntry failed: %d\n", err);
        // free(visited_methods);
        // visited_methods = NULL;
        return JNI_ERR;
    }
    printf("[TRACE_AGENT] Loaded and enabled\n");
    return JNI_OK;
}
__attribute__((visibility("default")))
JNIEXPORT void JNICALL
Agent_OnUnload(JavaVM *vm) {
    printf("[TRACE_AGENT] Unloaded\n");
}

#ifdef __cplusplus
}
#endif

// ✅ Whitelist: only these packages (must match JVM internal format: "pkg/subpkg/")
// Edit this list to add/remove packages you want to trace
 static const char* WHITELIST_PREFIXES[] = {
     // "ru/sbrf",
     // "ru/sbrf/kafka/schemaregistry",
     "net/jpountz/",
     "com/azure/",
     "com/fasterxml/classmate",
     "org/apache/commons/text/",
     "org/hibernate/validator",
     "com/fasterxml/jackson/",
     // "com/fasterxml/jackson/datatype/jsr310",
     // "com/fasterxml/jackson/module/paramnames",
     "org/everit/json/schema/",
     "com/github/erosb/",
     "com/github/luben/zstd",
     "autovalue/shaded/",
     "com/google/gson",
     "com/google/common/",
     "com/google/protobuf/",
     "com/nimbusds/jose/shaded/gson",
     "com/squareup/wire/schema",
     "org/apache/commons/codec",
     "org/apache/commons/io",
     "io/debezium",
     "io/micrometer/core",
     "jakarta/activation",
     "jakarta/validation",
     "jakarta/xml/bind",
     "org/jspecify/annotations",
     "com/yammer/metrics",
     "com/codahale/metrics",
     "io/micrometer/common",
     "io/micrometer/observation",
     "io/netty/buffer",
     "io/netty/handler/codec",
     "io/netty/util",
     "io/netty/handler",
     "io/netty/resolver",
     "io/netty/channel",
     "io/netty/bootstrap",
     "io/netty/channel/epoll",
     "io/netty/channel/unix",
     "reactor/core",
     "reactor/netty",
     "io/prometheus/metrics",
     "javax/ws/rs",
     "org/apache/avro",
     "org/apache/commons/compress",
     "org/apache/hc/client5",
     "org/apache/catalina",
     "org/apache/tomcat",
     "org/eclipse/jetty",
     "org/glassfish/jersey",
     "kotlin/reflect",
     "org/json",
     "org/msgpack",
     "org/rocksdb",
     "org/yaml/snakeyaml",
     "org/springframework",
     // "org/springframework/kafka",
     // "org/springframework/messaging",
     // "org/springframework/retry",
     // "org/springframework/retry",
     // "org/springframework/transaction",
     // "org/springframework/web/servlet",
     "org/springdoc/core",
     "org/springdoc/scalar",
     "org/springdoc/ui",
     "org/springdoc/webmvc/core",
     "org/springdoc/webmvc/api",
     "org/springdoc/webmvc/ui",
     "io/swagger/v3/core",
     "io/swagger/v3/oas",
     "org/xerial/snappy",
     "software/amazon/awssdk",
     "org/apache/velocity",
     "org/webjars",
     "org/apache/zookeeper",
     "Main2"
 };
static const char* BLACKLIST_PREFIXES[] = {
    "java/",
    "javax/",
    "jdk/",
    "sun/",
    "com/sun/",
    "com/oracle/",
    "apple/",
    "org/gradle", //build system
    "com/esotericsoftware/kryo/io/", //gradle sub-dependency
    "com/code_intelligence/jazzer/" //jazzer test framework
};
static int TRACE = 1;
static const char* FLAG_CLASS = "ru/sbrf/AgentCommand;";
static const char* FLAG_START_TRACE = "startTrace";
static const char* FLAG_END_TRACE = "endTrace";
static const size_t NUM_WHITELIST = sizeof(WHITELIST_PREFIXES) / sizeof(WHITELIST_PREFIXES[0]);
static const size_t NUM_BLACKLIST = sizeof(BLACKLIST_PREFIXES) / sizeof(WHITELIST_PREFIXES[0]);
// Returns 1 if class should be traced, 0 otherwise
static int should_trace(const char* class_sig, const char* method_name) {
    if (class_sig == NULL || class_sig[0] != 'L') {
        return 0; // Not a class signature → skip
    }
    const char* name = class_sig + 1; // skip 'L'
    size_t len = strlen(name);
    if (len == 0) return 0;
    // Strip trailing ';'
    if (name[len - 1] == ';') len--;
    
    if (strcmp(FLAG_CLASS, name) == 0) {
        if (strcmp(FLAG_START_TRACE, method_name) == 0) {
            TRACE = 1;
            return 1;
        }
        if (strcmp(FLAG_END_TRACE, method_name) == 0) {
            TRACE = 0;
            return 1;
        }
    }

    if (!TRACE) {
        return 0; // skip
    }

    // // Check if starts with ANY whitelist prefix
    // for (size_t i = 0; i < NUM_WHITELIST; i++) {
    //     const char* prefix = WHITELIST_PREFIXES[i];
    //     size_t plen = strlen(prefix);
    //     if (len >= plen && strncmp(name, prefix, plen) == 0) {
    //         return 1; // matches whitelist → trace
    //     }
    // }
    for (size_t i = 0; i < NUM_BLACKLIST; i++) {
        const char* prefix = BLACKLIST_PREFIXES[i];
        size_t plen = strlen(prefix);
        if (len >= plen && strncmp(name, prefix, plen) == 0) {
            return 0;
        }
    }
    return 1; // skip
}

static void JNICALL
cbMethodEntry(jvmtiEnv *jvmti_env, JNIEnv *env, jthread thread, jmethodID method) {
    // Step 1: Get method name & sig
    char *method_name = NULL;
    char *method_sig = NULL;
    if ((*jvmti_env)->GetMethodName(jvmti_env, method, &method_name, &method_sig, NULL) != JVMTI_ERROR_NONE) {
        return;
    }
    if (!TRACE && strcmp(FLAG_START_TRACE, method_name) != 0 && strcmp(FLAG_END_TRACE, method_name) != 0) {
        if (method_name) (*jvmti_env)->Deallocate(jvmti_env, (unsigned char *)method_name);
        if (method_sig)  (*jvmti_env)->Deallocate(jvmti_env, (unsigned char *)method_sig);
        return;
    }
    // Step 2: Get declaring class
    jclass method_class = NULL;
    if ((*jvmti_env)->GetMethodDeclaringClass(jvmti_env, method, &method_class) != JVMTI_ERROR_NONE) {
        if (method_name) (*jvmti_env)->Deallocate(jvmti_env, (unsigned char *)method_name);
        if (method_sig)  (*jvmti_env)->Deallocate(jvmti_env, (unsigned char *)method_sig);
        return;
    }
    // Step 3: Get class signature
    char *class_name = NULL;
    if ((*jvmti_env)->GetClassSignature(jvmti_env, method_class, &class_name, NULL) != JVMTI_ERROR_NONE) {
        if (method_class) {
            // We cannot DeleteLocalRef in JVMTI 1.0, but JVM will clean it up after return.
            // Still, to avoid potential local ref exhaustion in extreme cases,
            // we could push/pop frame — but for simplicity, we omit and trust JVM.
        }
        if (method_name)  (*jvmti_env)->Deallocate(jvmti_env, (unsigned char *)method_name);
        if (method_sig)   (*jvmti_env)->Deallocate(jvmti_env, (unsigned char *)method_sig);
        return;
    }
    // Step 4: Whitelist filter
    if (!should_trace(class_name, method_name)) {
        if (method_class) {
            // JVM will clean up local refs (no DeleteLocalRef needed in JVMTI 1.0)
        }
        if (class_name)   (*jvmti_env)->Deallocate(jvmti_env, (unsigned char *)class_name);
        if (method_name)  (*jvmti_env)->Deallocate(jvmti_env, (unsigned char *)method_name);
        if (method_sig)   (*jvmti_env)->Deallocate(jvmti_env, (unsigned char *)method_sig);
        return;
    }
    // Step 5: Skip synthetic methods (optional)
    jboolean is_synthetic = JNI_FALSE;
    if ((*jvmti_env)->IsMethodSynthetic(jvmti_env, method, &is_synthetic) == JVMTI_ERROR_NONE && is_synthetic) {
        if (class_name)   (*jvmti_env)->Deallocate(jvmti_env, (unsigned char *)class_name);
        if (method_name)  (*jvmti_env)->Deallocate(jvmti_env, (unsigned char *)method_name);
        if (method_sig)   (*jvmti_env)->Deallocate(jvmti_env, (unsigned char *)method_sig);
        return;
    }
    // Step 6: Print
    printf("[TRACE] %s.%s%s\n", class_name, method_name, method_sig);
    // Step 7: Clean up JVMTI strings (NOT local refs!)
    if (class_name)   (*jvmti_env)->Deallocate(jvmti_env, (unsigned char *)class_name);
    if (method_name)  (*jvmti_env)->Deallocate(jvmti_env, (unsigned char *)method_name);
    if (method_sig)   (*jvmti_env)->Deallocate(jvmti_env, (unsigned char *)method_sig);
    // method_class is a local ref — JVM cleans it automatically after callback returns.
}