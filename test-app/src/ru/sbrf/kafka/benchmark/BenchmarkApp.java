package ru.sbrf.kafka.benchmark;

public class BenchmarkApp {
    public static void main(String[] args) {
        int n = Integer.parseInt(args[0]);

        AgentCommand.startTrace();
        long result = fib(n);
        AgentCommand.endTrace();

        System.out.println("Результат: Фибоначчи(" + n + ") = " + result);
    }

    private static long fib(int n) {
        if (n <= 1) return n;
        return fib(n - 1) + fib(n - 2);
    }
}