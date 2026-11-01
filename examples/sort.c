#include <stdio.h>

int seed = 2024;

int next_random(void)
{
    seed = (seed * 75 + 74) % 65537;
    return seed % 1000;
}

void print_array(int a[], int n)
{
    for (int i = 0; i < n; i++)
        printf("%4d%s", a[i], i % 10 == 9 ? "\n" : "");
    if (n % 10)
        printf("\n");
}

void swap(int a[], int i, int j)
{
    int t = a[i];
    a[i] = a[j];
    a[j] = t;
}

int partition(int a[], int lo, int hi)
{
    int pivot = a[hi];
    int i = lo - 1;
    for (int j = lo; j < hi; j++)
        if (a[j] <= pivot)
            swap(a, ++i, j);
    swap(a, i + 1, hi);
    return i + 1;
}

void quicksort(int a[], int lo, int hi)
{
    if (lo >= hi)
        return;
    int p = partition(a, lo, hi);
    quicksort(a, lo, p - 1);
    quicksort(a, p + 1, hi);
}

void bubble_sort(int a[], int n)
{
    int swapped;
    do {
        swapped = 0;
        for (int i = 1; i < n; i++) {
            if (a[i - 1] > a[i]) {
                swap(a, i - 1, i);
                swapped = 1;
            }
        }
        n--;
    } while (swapped);
}

int is_sorted(int a[], int n)
{
    for (int i = 1; i < n; i++)
        if (a[i - 1] > a[i])
            return 0;
    return 1;
}

int main(void)
{
    int a[30];
    int b[30];
    int n = 30;

    for (int i = 0; i < n; i++) {
        a[i] = next_random();
        b[i] = a[i];
    }

    printf("input:\n");
    print_array(a, n);

    quicksort(a, 0, n - 1);
    bubble_sort(b, n);

    printf("quicksort:\n");
    print_array(a, n);
    printf("sorted: %s, bubble sort agrees: %s\n",
           is_sorted(a, n) ? "yes" : "no",
           is_sorted(b, n) && a[0] == b[0] && a[n - 1] == b[n - 1] ? "yes" : "no");
    return 0;
}
