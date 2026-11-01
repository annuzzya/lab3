#include <stdio.h>

int square(int x) { return x * x; }

int main(void)
{
    int n = 7;
    double r = 2.5;
    printf("square(%d) = %d\n", n, square(n));
    printf("area = %.3f\n", 3.14159 * r * r);
    for (int i = 0; i < 3; i++)
        printf("i=%d%s", i, i < 2 ? ", " : "\n");
    return 0;
}
