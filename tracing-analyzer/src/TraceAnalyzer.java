import java.io.BufferedReader;
import java.io.BufferedWriter;
import java.io.File;
import java.io.FileReader;
import java.io.FileWriter;
import java.io.IOException;
import java.util.HashMap;
import java.util.Map;
import java.util.Set;
import java.util.TreeMap;
import java.util.TreeSet;
import java.util.regex.Matcher;
import java.util.regex.Pattern;

public class TraceAnalyzer {

    // Регулярное выражение для разбора строки TRACE лога
    private static final Pattern TRACE_PATTERN = Pattern.compile("^\\[TRACE\\]\\s+L([^;]+);\\.(.+)");

    public static void main(String[] args) {
        if (args.length < 4) {
            System.err.println("Ошибка: Недостаточно аргументов!");
            System.err.println("Нужно: <маппинг_csv> <вход_трейсы> <выход_методы_csv> <выход_джарники_txt>");
            return;
        }

        File mappingFile = new File(args[0]);
        File traceInput = new File(args[1]);
        File outputMethodsFile = new File(args[2]);
        File outputJarsFile = new File(args[3]);

        if (!mappingFile.exists() || !traceInput.exists()) {
            System.err.println("Ошибка: Проверьте существование файла маппинга и входных файлов трейсинга!");
            return;
        }

        try {
            System.out.println("Загрузка маппинга из файла...");
            Map<String, String> classToJarMap = loadMapping(mappingFile);
            System.out.println("Загружено классов: " + classToJarMap.size());

            Map<String, String> methodToJarResult = new TreeMap<>();
            Set<String> uniqueJarsResult = new TreeSet<>();
            int totalLinesProcessed = 0;

            System.out.println("Обработка файлов трейсинга...");
            if (traceInput.isDirectory()) {
                File[] files = traceInput.listFiles();
                if (files != null) {
                    for (File file : files) {
                        if (file.isFile()) {
                            System.out.println(file.getAbsolutePath());
                            totalLinesProcessed += processTraceFile(file, classToJarMap, methodToJarResult, uniqueJarsResult);
                        }
                    }
                }
            } else {
                totalLinesProcessed += processTraceFile(traceInput, classToJarMap, methodToJarResult, uniqueJarsResult);
            }
            System.out.println("Обработано строк лога: " + totalLinesProcessed);

            // ЗАПИСЬ С ЭКРАНИРОВАНИЕМ ДЛЯ EXCEL
            try (BufferedWriter writer = new BufferedWriter(new FileWriter(outputMethodsFile))) {
                writer.write("class_with_method;jar\n");
                for (Map.Entry<String, String> entry : methodToJarResult.entrySet()) {
                    String classWithMethod = entry.getKey();
                    String jarName = entry.getValue();

                    // Экранируем кавычки внутри строки, если они вдруг там есть (стандарт CSV)
                    String escapedClassWithMethod = classWithMethod.replace("\"", "\"\"");

                    // Оборачиваем значение в кавычки, чтобы Excel игнорировал точки с запятой внутри метода
                    writer.write("\"" + escapedClassWithMethod + "\";" + jarName + "\n");
                }
            }

            try (BufferedWriter writer = new BufferedWriter(new FileWriter(outputJarsFile))) {
                for (String jar : uniqueJarsResult) {
                    writer.write(jar + "\n");
                }
            }

            System.out.println("\nАнализ успешно завершен!");
            System.out.println("Результаты сохранены в:\n - " + outputMethodsFile.getAbsolutePath() + "\n - " + outputJarsFile.getAbsolutePath());

        } catch (IOException e) {
            System.err.println("Произошла ошибка при вводе-выводе данных:");
            e.printStackTrace();
        }
    }

    private static Map<String, String> loadMapping(File mappingFile) throws IOException {
        Map<String, String> map = new HashMap<>();
        try (BufferedReader reader = new BufferedReader(new FileReader(mappingFile))) {
            String line = reader.readLine();
            if (line != null && !line.startsWith("class;")) {
                parseAndAddToMap(line, map);
            }
            while ((line = reader.readLine()) != null) {
                parseAndAddToMap(line, map);
            }
        }
        return map;
    }

    private static void parseAndAddToMap(String line, Map<String, String> map) {
        String[] parts = line.split(";");
        if (parts.length >= 2) {
            // Убираем возможные кавычки из файла-маппинга, если они там были
            String className = parts[0].replace("\"", "").trim();
            String jarName = parts[1].replace("\"", "").trim();
            map.put(className, jarName);
        }
    }

    private static int processTraceFile(File file, Map<String, String> classToJarMap,
                                        Map<String, String> methodToJarResult, Set<String> uniqueJarsResult) throws IOException {
        int linesCount = 0;
        try (BufferedReader reader = new BufferedReader(new FileReader(file))) {
            String line;
            while ((line = reader.readLine()) != null) {
                linesCount++;
                Matcher matcher = TRACE_PATTERN.matcher(line);
                if (matcher.find()) {
                    String className = matcher.group(1);
                    String methodSignature = matcher.group(2);

                    if (className.contains("$$Lambda$")) {
                        continue;
                    }

                    String jarName = classToJarMap.get(className);
                    if (jarName != null) {
                        String classWithMethod = className + "." + methodSignature;
                        methodToJarResult.put(classWithMethod, jarName);
                        uniqueJarsResult.add(jarName);
                    }
                }
            }
        }
        return linesCount;
    }
}
