Трассирующий агент для JVM на основе JVMTI интерфейса

Агент позволяет логгировать в stdout все вызовы методов JVM. Для фильтрации таких методов имеется whitelist и blacklist.

Сборка (MacOS):
`clang -shared -fPIC -I"$JAVA_HOME/include" -I"$JAVA_HOME/include/darwin" -o libtrace_agent.jnilib trace_agent.c`
На выходе: libtrace_agent.jnilib

Использование (тестовый код находится в репозитории tracing-agent-java):
`java -cp src/main/java -agentpath:path/to/libtrace_agent.jnilib Main2`