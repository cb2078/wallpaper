#define _USE_MATH_DEFINES

#include <assert.h>
#include <float.h>
#include <math.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

enum colour_type {
	BW,
	HSV,
	RGB,
};

const unsigned CUTOFF = 10000;
const int WIDTH = 800;
const int HEIGHT = 600;
const unsigned QUALITY = 25;
const unsigned ITERATIONS = WIDTH * HEIGHT * QUALITY;
const double INTENSITY = 50;
const double BOARDER = 0.05;
const unsigned SAMPLES = 50;
const enum colour_type COLOUR = BW;
const char *PARAMS[] = {
	" 0.275 -0.072 -1.355  1.905 -1.437  1.268  1.145  1.450 -1.011 -1.781  1.364 -1.696",
	" 0.434  1.831 -0.900 -1.835  0.580 -0.190  0.788  1.578 -1.139  1.579 -0.846 -1.067",
	" 0.770 -0.988  0.482  0.740  0.200  0.414  0.081  1.878 -1.079  0.013 -0.486  0.369",
	" 0.728  1.898 -1.476 -0.422 -0.405  0.088  0.749 -0.969  1.334 -0.853  1.028 -1.044",
	" 0.900 -1.751  1.179 -0.456  0.735  1.060  0.126  1.608 -1.290 -0.014  0.733 -0.210",
};

#define MAX(x, y)	((x) > (y) ? (x) : (y))
#define MIN(x, y)	((x) < (y) ? (x) : (y))
#define LENGTH(a)	(sizeof(a) / sizeof(a[0]))

static double c[6][2];
static double x_min[2];
static double x_max[2];
static double v_max[2];

static double dst(double x0[2], double x1[2])
{
	double dx[2] = {x1[0] - x0[0], x1[1] - x0[1]};
	return sqrt(dx[0] * dx[0] + dx[1] * dx[1]);
}

static void iteration(double y[2])
{
	double z[2];
	for (int i = 0; i < 2; ++i)
		z[i] =
			c[0][i] +
			c[1][i] * y[0] +
			c[2][i] * y[0] * y[0] +
			c[3][i] * y[0] * y[1] +
			c[4][i] * y[1] * y[1] +
			c[5][i] * y[1];
	for (int i = 0; i < 2; ++i)
		y[i] = z[i];
}

static void random_c(void)
{
	for (int i = 0; i < 2; ++i)
		for (int j = 0; j < 6; ++j)
			c[j][i] = (double)rand() / RAND_MAX * 4 - 2;
}

static int set_c(const char buf[256])
{
	int result = 1;
	for (int i = 0; i < 2; ++i)
		for (int j = 0; j < 6; ++j)
			result = result && sscanf(buf + 7 * (i * 6 + j), "%lf", &c[j][i]);
	return result;
}

static void str_c(char buf[256])
{
	for (int i = 0; i < 2; ++i)
		for (int j = 0; j < 6; ++j)
			sprintf(buf + 7 * (i * 6 + j), "% 1.3f ", c[j][i]);
}

// find a set of coefficients to generate a strange attractor
static bool attractor(void)
{
	// initialize parameters
	double x[2] = {0, 0};
	double xe[2] = {0, 0};	// for lyapunov exponent
	double d0;
	do {
		for (int i = 0; i < 2; ++i)
			xe[i] = x[i] + ((double)rand() / RAND_MAX - 0.5) / 1000;
		d0 = dst(x, xe);
	} while (d0 <= 0);
	double v[2];

	x_min[0] = x_min[1] = 1e10;
	x_max[0] = x_max[1] = -1e10;
	v_max[0] = v_max[1] = 0;

	double lyapunov = 0;
	for (unsigned n = 0; n < CUTOFF * 2; ++n) {
		double x_last[2];
		for (int i = 0; i < 2; ++i)
			x_last[i] = x[i];

		iteration(x);
		iteration(xe);

		// converge, diverge
		for (int i = 0; i < 2; ++i)
			if (fabs(x[i]) > 1e10 || fabs(x[i] < 1e-10))
				return false;
		if (n > CUTOFF)
			for (int i = 0; i < 2; ++i) {
				v[i] = x[i] - x_last[i];
				x_max[i] = MAX(x_max[i], x[i]);
				x_min[i] = MIN(x_min[i], x[i]);
				v_max[i] = MAX(v[i], v_max[i]);
			}
			// lyapunov exponent
			double d = dst(x, xe);
			lyapunov += log(fabs(d / d0));
	}
	return lyapunov / CUTOFF > 10;
}

static char srgb(unsigned char a)
{
	double L = (double)a / 0xff;
 	double S = L <= 0.0031308 ? L * 12.92 :
		1.055 * pow(L, 1 / 2.44) - 0.055;
	return (unsigned char)(0xff * MIN(1, MAX(0, S)));
}

static void HSV_to_RGB(double H, double S, double V, char RGB[3])
{
	double C = V * S;
	double HH = H / 60;
	double X = C * (1 - fabs(fmod(HH, 2) - 1));
	double RGB1[3];
	switch ((int)floor(HH)) {
		case 0:
			RGB1[0] = C;
			RGB1[1] = X;
			RGB1[2] = 0;
			break;
		case 1:
			RGB1[0] = X;
			RGB1[1] = C;
			RGB1[2] = 0;
			break;
		case 2:
			RGB1[0] = 0;
			RGB1[1] = C;
			RGB1[2] = X;
			break;
		case 3:
			RGB1[0] = 0;
			RGB1[1] = X;
			RGB1[2] = C;
			break;
		case 4:
			RGB1[0] = X;
			RGB1[1] = 0;
			RGB1[2] = C;
			break;
		case 5:
			RGB1[0] = C;
			RGB1[1] = 0;
			RGB1[2] = X;
			break;
	}
	double m = V - C;
	for (int k = 0; k < 3; ++k)
		RGB[k] = (char)((RGB1[k] + m) * 0xff);
}

static void render_image(char buf[HEIGHT][WIDTH][3])
{
	memset(buf, 0, sizeof(char) * HEIGHT * WIDTH * 3);
	static double info[HEIGHT][WIDTH][4];
	memset(info, 0, sizeof(double) * HEIGHT * WIDTH * 4);

	double x[2] = {0, 0};
	for (unsigned n = 0; n < CUTOFF; ++n)
		iteration(x);
	double v[2] = {0, 0};

	double range[2] = {x_max[0] - x_min[0], x_max[1] - x_min[1]};
	int o = range[0] < range[1];
	double x_scale = (WIDTH - 1) / (x_max[o] - x_min[o]);
	double y_scale = (HEIGHT - 1) / (x_max[!o] - x_min[!o]);
	double scale = MIN(x_scale, y_scale) * (1 - BOARDER);
	unsigned count = 0;
	for (unsigned n = CUTOFF; n < ITERATIONS; ++n) {
		double x_last[2];
		for (int i = 0; i < 2; ++i)
			x_last[i] = x[i];
		iteration(x);
		for (int i = 0; i < 2; ++i)
			v[i] = x[i] - x_last[i];

		int i = (int)((HEIGHT - range[!o] * scale) / 2 + (x[!o] - x_min[!o]) * scale);
		int j = (int)((WIDTH  - range[ o] * scale) / 2 + (x[ o] - x_min[ o]) * scale);
		if (i < 0 || i >= HEIGHT) continue;
		if (j < 0 || j >= WIDTH) continue;

		count += info[i][j][0] == 0;
		info[i][j][0] += 1;
		if (COLOUR == HSV) {
			info[i][j][1] += v[1] / v_max[1];
			info[i][j][2] += v[0] / v_max[0];
		} else {
			info[i][j][1] += MAX(0, v[0] / v_max[0]);
			info[i][j][2] += MAX(0, -v[0] / v_max[0]);
			info[i][j][3] += fabs(v[1]);
		}
	}
	double DENSITY = (double)ITERATIONS / count;

	for (int i = 0; i < HEIGHT; ++i)
		for (int j = 0; j < WIDTH; ++j) {
			if (COLOUR == HSV) {
				double H = 180 + (atan2(info[i][j][1], info[i][j][2]) * 180 / M_PI);
				double S = 1;
				double V = MIN(1, INTENSITY / DENSITY * info[i][j][0] / 0xff);
				HSV_to_RGB(H, S, V, buf[i][j]);
			} else {
				if (info[i][j][0])
					for (int k = 0; k < 3; ++k)
						buf[i][j][k] = (char)MIN(0xff, info[i][j][COLOUR == BW ? 0 : k + 1] * INTENSITY / DENSITY);
			}
			for (int k = 0; k < 3; ++k) {
				buf[i][j][k] = srgb(buf[i][j][k]);
				if (COLOUR == BW)
					buf[i][j][k] = 0xff - buf[i][j][k];
			}
		}
}

static int write_image(char *name, int width, int height, void *buf)
{
	int result = stbi_write_png(name, width, height, 3, buf, width * sizeof(char) * 3);
	if (!result)
		printf("Failed to write %s\n", name);
	else
		printf("%s\n", name);
	return result;
}

static void write_attractor(char *name)
{
	static char buf[HEIGHT][WIDTH][3];
	render_image(buf);
	write_image(name, WIDTH, HEIGHT, buf);
}

static void write_video(const char *params)
{
	set_c(params);
	double start = -10e-2;
	double end = 4e-2;
	double range = end - start;
	int frames = 1300;
	int N = frames / (1 * 1);
	double dt = range / N;

	int i = 0, j = 3;
	c[j][i] += start;

	int frame = 0;
	for (int n = 0; n < N; ++n) {
		attractor();	// TODO I shouldn't be doing this
		char name[256];
		snprintf(name, 256, "video/%d.png", frame++);
		printf("%3d%%\t", frame * 100 / frames);
		write_attractor(name);
		c[j][i] += dt;
	}
}

static void video_preview(const char *params, double *cn, double start, double end, int slides)
{
	int n = (int)ceil(sqrt((double)slides)); // width of the square showing each of the slides
	int w = WIDTH * n;
	int h = HEIGHT * n;
	char *buf = (char *)malloc(sizeof(char) * w * h * 3);
#define BUF_AT(i, j, k)	buf[(i) * w * 3 + (j) * 3 + (k)]
	memset(buf, 0x7f, sizeof(char) * w * h * 3);

	set_c(params);
	double range = end - start;
	double dt = range / (double)slides;
	*cn += start;

	for (int s = 0; s < slides; ++s) {
		printf("%2d/%d\n", 1 + s, slides);
		int i = s / n;
		int j = s % n;
		static char tmp[HEIGHT][WIDTH][3];
		attractor();
		render_image(tmp);
		for (int k = 0; k < HEIGHT; ++k)
			memcpy(&BUF_AT(i * HEIGHT + k, j * WIDTH, 0), &tmp[k][0][0], sizeof(char) * WIDTH * 3);
		*cn += dt;
	}

	write_image("images/preview.png", w, h, buf);
}

static void video_params(double **cn, double *start, double *end)
{
	int i = 2 * rand() / RAND_MAX;
	int j = 6 * rand() / RAND_MAX;
	*cn = &c[j][i];
	static double step = 1e-2;

	double tmp = c[j][i];
	double dt = 0;
	do {
		dt += step;
		c[j][i] = tmp - dt;
	} while (attractor());
	*start = -dt;

	c[j][i] = tmp;
	dt = 0;
	do {
		dt += step;
		c[j][i] = tmp + dt;
	} while (attractor());
	*end = dt;
}

int main(void)
{
	srand((unsigned)time(0));

#if 1
	do {
		random_c();
	} while (!attractor());
	char params[256];
	str_c(params);

	set_c(params);
	double *cn, start, end;
	video_params(&cn, &start, &end);
	video_preview(params, cn, start, end, 32);

	set_c(params);
	attractor();
	write_attractor("images/test.png");
#else
	if (true)
		for (int n = 0; n < SAMPLES; ++n) {
			// find a chaotic attractor
			do
				random_c();
			while (attractor() == false);

			// write to an image
			char params[256];
			str_c(params);
			char name[256];
			snprintf(name, 256, "images/%s.png", params);
			printf("%2d/%d ", 1 + n, SAMPLES);
			write_attractor(name);
		}
	else
		for (int n = 0; n < LENGTH(PARAMS); ++n) {
			int result = set_c(PARAMS[n]);
			if (!result) {
				puts("Failed to load c");
				continue;
			}
			attractor();
			char name[256];
			snprintf(name, 256, "images/%c.png", 'a' + char(n));
			printf("%2d/%d ", 1 + n, (int)LENGTH(PARAMS));
			write_attractor(name);
		}
#endif
	puts("done");
	return 0;
}
