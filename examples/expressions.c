#include <stdio.h>

int g = 3 * (2 + 1) - 4 / 2;
double h = 1.5e2;
int arr[5];
double darr[3];

int twice(double x);

double average(int a[], int n)
{
    int sum = 0;
    for (int i = 0; i < n; i++)
        sum += a[i];
    return (double)sum / n;
}

int main(void)
{
    int a = 'A', b = 0x1F, c = -17;
    printf("%d %d %d %c\n", a, b, c % 5, a + 2);
    printf("%d %.1f %d\n", g, h, twice(2.75));

    arr[0] = 7;
    arr[1] = arr[0] * 3;
    arr[2] += 2.9;
    arr[3] = 10;
    arr[3] /= 3;
    arr[4] = -9;
    arr[4] %= 4;
    darr[0] = 1;
    darr[0] /= 4;
    darr[1] = arr[1] / 2;
    darr[2] = arr[1] / 2.0;
    for (int i = 0; i < 5; i++)
        printf("%d ", arr[i]);
    printf("| %.3f %.3f %.3f\n", darr[0], darr[1], darr[2]);
    printf("average = %.4f\n", average(arr, 5));

    int x = 5;
    x++;
    int y = (x > 3) + (x < 3) * 10 + !x;
    int z = x > 2 ? x < 10 ? 1 : 2 : 3;
    double q = 7 / 2 + 7 / 2.0;
    printf("%d %d %d %.2f %d\n", x, y, z, q, (int)(q * 10) % 7);

    int i = 0, s = 0;
    while (1) {
        i++;
        if (i % 2)
            continue;
        s += i;
        if (i >= 10)
            break;
    }
    printf("%d %d %+.3e %g %g\n", i, s, 12345.6789, 0.0001234, 1e21);
    printf("%5.1f|%-6d|%06.2f|%x|%X|%o\n", 3.14159, 42, -3.5, 255, 48879, 8);
    return 0;
}

int twice(double x)
{
    return x * 2;
}
