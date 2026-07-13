import java.io.BufferedWriter;
import java.io.File;
import java.io.FileWriter;
import java.io.IOException;
import java.nio.file.Files;
import java.util.Enumeration;
import java.util.Map;
import java.util.TreeMap;
import java.util.zip.ZipEntry;
import java.util.zip.ZipFile;

public class ReleaseMappingGenerator {

    public static void main(String[] args) {
        if (args.length < 2) {
            System.err.println("Ошибка: Не переданы аргументы! Нужно: <путь_к_zip> <путь_к_выходному_csv>");
            return;
        }

        File releaseZip = new File(args[0]);
        File outputFile = new File(args[1]);
        File tempUnzipDir = new File("build/tmp-mapping-unzip"); // внутренний темп

        // TreeMap автоматически сортирует классы по алфавиту
        Map<String, String> classToJarMap = new TreeMap<>();

        try {
            // 1. Распаковка основного архива
            unzip(releaseZip, tempUnzipDir);

            // 2. Рекурсивный сбор данных в память с проверкой на дубликаты
            scanAndMap(tempUnzipDir, classToJarMap);

            outputFile.getParentFile().mkdirs();
            // 3. Запись уникальных и отсортированных данных в CSV
            try (BufferedWriter writer = new BufferedWriter(new FileWriter(outputFile))) {
                writer.write("class;jar\n");

                for (Map.Entry<String, String> entry : classToJarMap.entrySet()) {
                    writer.write(entry.getKey() + ";" + entry.getValue() + "\n");
                }
            }

            System.out.println("Маппинг успешно сохранен в: " + outputFile.getAbsolutePath());
        } catch (IOException e) {
            e.printStackTrace();
        } finally {
            // 4. Очистка временной папки
            deleteDirectory(tempUnzipDir);
        }
    }

    private static void scanAndMap(File dir, Map<String, String> classToJarMap) throws IOException {
        File[] files = dir.listFiles();
        if (files == null) return;

        // Обработка случая с Fat JAR (как в вашем плагине)
        if (files.length == 1 && files[0].getName().endsWith(".jar")) {
            String fatJarPath = files[0].getAbsolutePath();
            File fatJarAsDir = new File(fatJarPath.substring(0, fatJarPath.length() - 4));
            unzip(files[0], fatJarAsDir);
            scanAndMap(fatJarAsDir, classToJarMap);
            return;
        }

        for (File file : files) {
            if (file.isDirectory()) {
                scanAndMap(file, classToJarMap);
            } else if (file.getName().endsWith(".jar")) {
                extractClassesFromJar(file, classToJarMap);
            }
        }
    }

    private static void extractClassesFromJar(File jarFile, Map<String, String> classToJarMap) throws IOException {
        String currentJarName = jarFile.getName();

        try (ZipFile zipFile = new ZipFile(jarFile)) {
            Enumeration<? extends ZipEntry> entries = zipFile.entries();
            while (entries.hasMoreElements()) {
                ZipEntry entry = entries.nextElement();
                String name = entry.getName();

                if (name.endsWith(".class") && !name.endsWith("module-info.class")) {
                    // Преобразуем путь "org/apache/log4j/PatternLayout.class" в "org/apache/log4j/PatternLayout"
                    String className = name.substring(0, name.length() - 6);

                    // Проверяем, существует ли уже маппинг для этого класса
                    if (classToJarMap.containsKey(className)) {
                        String existingJarName = classToJarMap.get(className);

                        // Если имена JAR-файлов не совпадают — кидаем ошибку
                        if (!existingJarName.equals(currentJarName)) {
                            throw new IllegalStateException(String.format(
                                    "Duplicate class found! Class '%s' is present in both '%s' and '%s'",
                                    className, existingJarName, currentJarName
                            ));
                        }
                    } else {
                        // Если класса еще нет, просто добавляем его
                        classToJarMap.put(className, currentJarName);
                    }
                }
            }
        }
    }

    private static void unzip(File srcFile, File destDir) throws IOException {
        deleteDirectory(destDir);
        try (ZipFile zipFile = new ZipFile(srcFile)) {
            Enumeration<? extends ZipEntry> entries = zipFile.entries();
            while (entries.hasMoreElements()) {
                ZipEntry entry = entries.nextElement();
                File file = new File(destDir, entry.getName());
                if (!entry.isDirectory() && entry.getName().endsWith(".jar")) {
                    file.getParentFile().mkdirs();
                    Files.copy(zipFile.getInputStream(entry), file.toPath());
                }
            }
        }
    }

    private static void deleteDirectory(File dir) {
        if (dir.exists()) {
            File[] files = dir.listFiles();
            if (files != null) {
                for (File file : files) {
                    if (file.isDirectory()) {
                        deleteDirectory(file);
                    } else {
                        file.delete();
                    }
                }
            }
            dir.delete();
        }
    }
}
