#include <stdio.h>

int sieve[1001];

int main(void)
{
    int limit = 1000;
    int count = 0;

    for (int i = 2; i * i <= limit; i++) {
        if (sieve[i])
            continue;
        for (int j = i * i; j <= limit; j += i)
            sieve[j] = 1;
    }

    for (int i = 2; i <= limit; i++) {
        if (sieve[i])
            continue;
        printf("%5d", i);
        if (++count % 12 == 0)
            printf("\n");
        if (i > 500)
            break;
    }
    printf("\n%d primes printed\n", count);
    return 0;
}
