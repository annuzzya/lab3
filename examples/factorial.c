#include <stdio.h>

int factorial(int n);

int factorial_iter(int n)
{
    int result = 1;
    while (n > 1)
        result *= n--;
    return result;
}

int main(void)
{
    for (int i = 0; i <= 12; i++)
        printf("%2d! = %9d  %9d\n", i, factorial(i), factorial_iter(i));
    return 0;
}

int factorial(int n)
{
    if (n <= 1)
        return 1;
    return n * factorial(n - 1);
}
