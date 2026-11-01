#include <stdio.h>
#include <math.h>

double newton_sqrt(double x)
{
    double guess = x / 2.0;
    int steps = 0;
    while (fabs(guess * guess - x) > 1e-12 && steps < 50) {
        guess = (guess + x / guess) / 2;
        steps++;
    }
    return guess;
}

double leibniz_pi(int terms)
{
    double sum = 0.0;
    double sign = 1.0;
    for (int k = 0; k < terms; k++) {
        sum += sign / (2 * k + 1);
        sign = -sign;
    }
    return 4 * sum;
}

int main(void)
{
    double values[5];
    values[0] = 2;
    values[1] = 10;
    values[2] = 144;
    values[3] = 0.25;
    values[4] = 12345.678;

    for (int i = 0; i < 5; i++) {
        double mine = newton_sqrt(values[i]);
        printf("sqrt(%10.3f) = %.10f  libm: %.10f  diff: %.2e\n",
               values[i], mine, sqrt(values[i]), fabs(mine - sqrt(values[i])));
    }

    printf("pi ~ %.8f (100000 terms)\n", leibniz_pi(100000));
    printf("2^10 = %g, 2^0.5 = %g\n", pow(2, 10), pow(2, 0.5));
    printf("(int)7.9 = %d, (int)-7.9 = %d, 7 / 2 = %d, 7 / 2.0 = %.1f\n",
           (int)7.9, (int)-7.9, 7 / 2, 7 / 2.0);
    return 0;
}
