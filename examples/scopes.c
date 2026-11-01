#include <stdio.h>

int counter = 100;
int calls;

int tick(void)
{
    calls++;
    return ++counter;
}

int main(void)
{
    int x = 1;
    printf("outer x = %d, global counter = %d\n", x, counter);
    {
        int x = 2;
        int counter = 5;
        printf("inner x = %d, local counter = %d\n", x, counter);
    }
    printf("outer x again = %d\n", x);

    int a = 10;
    a += 5;
    a -= 3;
    a *= 4;
    a /= 5;
    a %= 7;
    printf("a = %d\n", a);

    int i = 5;
    int pre = ++i;
    int post = i++;
    printf("pre = %d, post = %d, i = %d\n", pre, post, i);

    printf("-7 / 2 = %d, -7 %% 2 = %d, 7 %% -2 = %d\n", -7 / 2, -7 % 2, 7 % -2);

    if (0 && tick())
        printf("unreachable\n");
    if (1 || tick())
        printf("short-circuit: calls = %d\n", calls);
    int t = tick();
    printf("tick() = %d, calls = %d\n", t, calls);

    int n = 0;
    do {
        n += 3;
    } while (n < 10);
    printf("do-while stopped at n = %d\n", n);

    int sign = n > 10 ? 1 : n < 10 ? -1 : 0;
    printf("sign = %d, !sign = %d, !!n = %d\n", sign, !sign, !!n);

    double d = 1;
    d /= 3;
    int truncated = 0;
    truncated += 2.9;
    printf("d = %.4f, truncated = %d\n", d, truncated);
    return 0;
}
