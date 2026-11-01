#include <stdio.h>

int memo[46];

int fib(int n)
{
    if (n < 2)
        return n;
    if (memo[n])
        return memo[n];
    memo[n] = fib(n - 1) + fib(n - 2);
    return memo[n];
}

int fib_slow(int n)
{
    return n < 2 ? n : fib_slow(n - 1) + fib_slow(n - 2);
}

int main(void)
{
    int a = 0, b = 1, k = 0;
    while (k < 15) {
        printf("%d ", a);
        int next = a + b;
        a = b;
        b = next;
        k++;
    }
    printf("\n");
    printf("fib(45) = %d\n", fib(45));
    printf("fib_slow(25) = %d\n", fib_slow(25));
    return 0;
}
