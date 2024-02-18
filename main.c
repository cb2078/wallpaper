#include <assert.h>
#include <float.h>
#include <math.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

#define CUTOFF	10000
#define ITERATIONS	25100000
#if 1
#define WIDTH	5000
#define HEIGHT	3000
#else
#define WIDTH 5120
#define HEIGHT 2160
#endif

#define SAMPLES 32

#define MAX(x, y)	((x) > (y) ? (x) : (y))
#define MIN(x, y)	((x) < (y) ? (x) : (y))

static double c[6][2];
static double x[ITERATIONS][2];
static double v[ITERATIONS];
static double xe[2];	// for lyapunov exponent
static double x_min[2];
static double x_max[2];
static double v_max;

static double dst(double x0[2], double x1[2])
{
	double dx[2] = {x1[0] - x0[0], x1[1] - x0[1]};
	return sqrt(dx[0] * dx[0] + dx[1] * dx[1]);
}

static void iteration(double x0[2], double x1[2])
{
	double result[2];
	for (int i = 0; i < 2; ++i)
		result[i] =
			c[0][i] +
			c[1][i] * x0[0] +
			c[2][i] * x0[0] * x0[0] +
			c[3][i] * x0[0] * x0[1] +
			c[4][i] * x0[1] * x0[1] +
			c[5][i] * x0[1];
	for (int i = 0; i < 2; ++i)
		x1[i] = result[i];
}

static bool attractor(void)
{
	// initialize parameters
	v_max = 0;
	x_min[0] = x_min[1] = 1e10;
	x_max[0] = x_max[1] = -1e10;
	for (int i = 0; i < 2; ++i)
		for (int j = 0; j < 6; ++j)
			c[j][i] = (double)rand() / RAND_MAX * 4 - 2;
	double d0;
	do {
		for (int i = 0; i < 2; ++i)
			xe[i] = x[0][i] + ((double)rand() / RAND_MAX - 0.5) / 1000;
		d0 = dst(x[0], xe);
	} while (d0 <= 0);

	for (int n = 1; n <= ITERATIONS; ++n) {
		iteration(x[n - 1], x[n]);
		v[n - 1] = dst(x[n], x[n - 1]);
		v_max = MAX(v[n - 1], v_max);
		if (n > CUTOFF)
			for (int i = 0; i < 2; ++i) {
				x_max[i] = x_max[i] > x[n][i] ? x_max[i] : x[n][i];
				x_min[i] = x_min[i] < x[n][i] ? x_min[i] : x[n][i];
			}
		iteration(xe, xe);

		// converge, diverge
		for (int i = 0; i < 2; ++i)
			if (fabs(x[n][i]) > 1e10 || fabs(x[n][i] < 1e-10))
				return false;
		// lyapunov exponent
		if (n == CUTOFF) {
			double d = dst(x[n], xe);
			double lyapunov = log(fabs(d / d0));
			if (lyapunov < 8)
				return false;
		}

// 		if (n % 16 == 0)
// 			printf("%d\n", n);
	}
	return true;
}

// static char srgb(char a)
// {
// 	double L = (float)a / 0xff;
// 	double S = L <= 0.0031308 ? L * 12.92 :
// 		1.055 * pow(L, 1 / 2.44) - 0.055;
// 	return (char)(0xff * S);
// }

static int write_image(char *name)
{
	char *buf = malloc(HEIGHT * WIDTH * sizeof(char) * 3);
	memset(buf, 0x111111, HEIGHT * WIDTH * sizeof(char) * 3);

	for (int n = CUTOFF; n < ITERATIONS; ++n) {
		int i = (int)((HEIGHT - 1) * (x[n][0] - x_min[0]) / (x_max[0] - x_min[0]));
		int j = (int)((WIDTH - 1) * (x[n][1] - x_min[1]) / (x_max[1] - x_min[1])) * 3;
		assert(i >= 0 && i < HEIGHT);
		assert(j >= 0 && j < WIDTH * 3);
		buf[i * WIDTH * 3 + j + 1] = (char)MIN(0x7f, (int)buf[i * WIDTH * 3 + j + 1] + 0x06);
		buf[i * WIDTH * 3 + j + 0] = (char)MIN(0xfe, (int)buf[i * WIDTH * 3 + j + 0] + 0x0b);
		buf[i * WIDTH * 3 + j + 2] = (char)(0xff * (float)v[n] / v_max);
	}

	bool result = stbi_write_png(name, WIDTH, HEIGHT, 3, buf, WIDTH * sizeof(char) * 3);
	if (!result)
		printf("Failed to write %s\n", name);
	else
		printf("%s\n", name);
	return result;
}

int main(void)
{
	srand((unsigned)time(0));

	// initialise
	for (int i = 0; i < 2; ++i) {
		x[0][i] = (double)rand() / RAND_MAX - 0.5;
	}

	for (int i = 0; i < SAMPLES; ++i) {
		// find a chaotic attractor
		while (attractor() == false);

		// write to an image
		char buf[20];
		snprintf(buf, 20, "%d.png", i);
		write_image(buf);
	}

	puts("done");
	return 0;
}
