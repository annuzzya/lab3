#include <stdio.h>

int depth(int n)
{
    return depth(n + 1) + 1;
}

int main(void)
{
    int data[4];
    int zero = 0;
    for (int i = 0; i <= 4; i++) {
        data[i] = i * i;
        printf("data[%d] = %d\n", i, data[i]);
    }
    printf("%d\n", 10 / zero);
    return depth(0);
}
