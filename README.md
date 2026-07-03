# Tracing C Agent

A native JVM agent written in C that prints method signatures with full parameter and return type information for executed methods using the JVMTI (Java Virtual Machine Tool Interface).

## Features

- Prints full method signatures: `Lpackage/Class;.method(Ljava/lang/String;I)Ljava/lang/Object;`
- Controlled tracing via `AgentCommand.startTrace()` / `AgentCommand.endTrace()`
- Configurable package filtering via whitelist/blacklist to exclude noise (JDK internals, etc.)
- Native shared library (`.jnilib` on macOS, `.so` on Linux)
- Supports both static attach (`-agentpath`) and dynamic attach (`jcmd`)

## Building

### macOS

```bash
clang -shared -fPIC -I"$JAVA_HOME/include" -I"$JAVA_HOME/include/darwin" -o libtrace_agent.jnilib trace_agent.c
```

Output: `libtrace_agent.jnilib`

### Linux

```bash
gcc -shared -fPIC -I"$JAVA_HOME/include" -I"$JAVA_HOME/include/linux" -o libtrace_agent.so trace_agent.c
```

Output: `libtrace_agent.so`

### Windows (MinGW)

```bash
gcc -shared -fPIC -I"%JAVA_HOME%\include" -I"%JAVA_HOME%\include\win32" -o trace_agent.dll trace_agent.c
```

Output: `trace_agent.dll`

## Usage

### Static Attach (Recommended)

```bash
java -cp <your-classpath> -agentpath:path/to/libtrace_agent.jnilib <YourMainClass>
```

### Example

```bash
java -cp src/main/java -agentpath:libtrace_agent.jnilib Main2
```

### Output

```
[TRACE_AGENT] Loaded and enabled
[INFO] can_generate_method_entry_events = 1
Hello from hello()
[TRACE] LMain2;.hello()V
Hello from hello()
[TRACE] LMain2;.hello()V
Hello from hello()
```

## Controlling Tracing

Use `AgentCommand.startTrace()` and `AgentCommand.endTrace()` to control when tracing is active:

```java
public class Main2 {
    public static void main(String[] args) {
        hello();                          // Not traced (tracing disabled)
        AgentCommand.startTrace();
        hello();                          // Traced
        AgentCommand.endTrace();
        hello();                          // Not traced (tracing disabled)
    }
    
    static void hello() {
        System.out.println("Hello from hello()");
    }
}
```

The agent looks for calls to `ru/sbrf/AgentCommand.startTrace()` and `ru/sbrf/AgentCommand.endTrace()` to toggle the trace flag.

## Method Signature Format

The agent prints method signatures in JVM descriptor format:

- `LClassName;.methodName()V` - no parameters, returns void
- `LClassName;.methodName(Ljava/lang/String;)V` - one String parameter, returns void
- `LClassName;.methodName(I)Ljava/lang/String;` - one int parameter, returns String
- `LClassName;.methodName(Ljava/util/List;Ljava/lang/Object;)Z` - List and Object parameters, returns boolean

See [Java Type Signatures](https://docs.oracle.com/javase/specs/jvms/se17/html/jvms-4.html#jvms-4.3) for the full format.

## Package Filtering

Edit `WHITELIST_PREFIXES` and `BLACKLIST_PREFIXES` in `trace_agent.c` to customize which packages are traced:

```c
// Whitelist: only trace these packages (uncomment/add as needed)
static const char* WHITELIST_PREFIXES[] = {
    "net/jpountz/",
    "com/azure/",
    "com/fasterxml/jackson/",
    "org/apache/commons/text/",
    "org/springframework",
    // Add your packages here
};

// Blacklist: never trace these packages
static const char* BLACKLIST_PREFIXES[] = {
    "java/",
    "javax/",
    "jdk/",
    "sun/",
    "com/sun/",
    "com/oracle/",
    "apple/",
    "org/gradle",
};
```

**Note:** Use `/` separators (JVM internal format), not `.`. The whitelist is currently commented out; the agent uses blacklist mode by default (traces everything except blacklisted packages).

## Architecture

The agent uses:
- **JVMTI (Java Virtual Machine Tool Interface)** to intercept method entry events
- **MethodEntry callback** to capture every method invocation
- **Capability negotiation** to request `can_generate_method_entry_events`
- **Thread-local state** via JVMTI for tracing enable/disable flag
- **Memory management** via JVMTI allocation/deallocation for strings

### Entry Points

- `Agent_OnLoad()` - Called when agent is loaded via `-agentpath` (static attach)
- `Agent_OnAttach()` - Called when agent is loaded dynamically via `jcmd ... JVMTI.agent_load`
- `Agent_OnUnload()` - Called when agent is unloaded

### Callback Flow

1. JVM invokes `cbMethodEntry()` on every method entry
2. Agent retrieves method name, signature, and declaring class via JVMTI
3. Agent checks whitelist/blacklist filters in `should_trace()`
4. Agent checks for `startTrace()`/`endTrace()` calls to toggle tracing
5. If tracing is enabled and class passes filters, prints `[TRACE]` line
6. JVMTI-allocated memory is deallocated before callback returns

## Files

- `trace_agent.c` - Main agent implementation with JVMTI callbacks
- `README.txt` - Basic readme (Russian)
- `README.md` - This comprehensive readme

## Comparison with Java Agent

| Feature | Java Agent | C Agent |
|---------|-----------|---------|
| **Instrumentation** | ASM bytecode weaving | JVMTI method entry events |
| **Build Output** | Fat JAR with ASM bundled | Native shared library |
| **Performance** | Moderate (bytecode modification) | Lower overhead (native callbacks) |
| **Portability** | Platform-independent JAR | Platform-specific binary |
| **Filtering** | Package exclusion only | Whitelist + blacklist modes |
| **Dynamic Attach** | No | Yes (via `Agent_OnAttach`) |

## Troubleshooting

### "Unable to create JVMTI env"

Ensure `JAVA_HOME` is set correctly and points to a valid JDK installation (not just JRE).

### No trace output

1. Verify the agent loaded: look for `[TRACE_AGENT] Loaded and enabled`
2. Check if tracing is disabled by default (calls to `startTrace()` may be needed)
3. Verify your classes are not in the blacklist

### Build fails with missing headers

Ensure you're using the correct include paths for your platform:
- macOS: `-I"$JAVA_HOME/include" -I"$JAVA_HOME/include/darwin"`
- Linux: `-I"$JAVA_HOME/include" -I"$JAVA_HOME/include/linux"`
- Windows: `-I"%JAVA_HOME%\include" -I"%JAVA_HOME%\include\win32"`

## License

Same license as the Java agent project.
