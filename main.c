#define _USE_MATH_DEFINES
#define M_SQRT3 1.73205080756887729352744634151

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
	MIX,
};

char *colour_map[] = {
	[BW] = "BW",
	[HSV] = "HSV",
	[RGB] = "RGB",
	[MIX] = "MIX",
};

unsigned CUTOFF = 10000;
unsigned ITERATIONS;
#define SAMPLES PREVIEW

#define MAX(x, y)	((x) > (y) ? (x) : (y))
#define MIN(x, y)	((x) < (y) ? (x) : (y))
#define LENGTH(a)	(sizeof(a) / sizeof(a[0]))
#define D 3
#define Dn ((D + 1) * (D + 2) / 2)

#include "cmdline.c"

static double c[Dn][D];
static double x_min[D];
static double x_max[D];
static double v_max[D];
static double u[3][D] = {
	{1, 0},
	{-1.0 / 2, M_SQRT3 / 2},
	{-1.0 / 2, -M_SQRT3 / 2},
};

static double dot(double x[D], double y[D])
{
	double s = 0;
	for (int i = 0; i < D; ++i)
		s += x[i] * y[i];
	return s;
}

static double dst(double x0[D], double x1[D])
{
	double s = 0;
	for (int i = 0; i < D; ++i) {
		double d = x1[i] - x0[i];
		s += d * d;
	}
	return sqrt(s);
}

static void iteration(double y[D])
{
#ifndef D
	double z[2];
	for (int i = 0; i < 2; ++i)
		z[i] =
			c[0][i] +
			c[1][i] * y[0] +
			c[2][i] * y[0] * y[0] +
			c[3][i] * y[0] * y[1] +
			c[4][i] * y[1] * y[1] +
			c[5][i] * y[1];
#else
#define Y(i) (i < D ? y[i] : 1)
	double z[D] = {0, 0};
	for (int i = 0; i < D; ++i) {
		int a = 0;
		for (int j = 0; j < D + 1; ++j)
			for (int k = j; k < D + 1; ++k)
				z[i] += c[a++][i] * Y(j) * Y(k);
	}
#endif
	for (int i = 0; i < D; ++i)
		y[i] = z[i];
}

// find a set of coefficients to generate a strange attractor
static bool attractor(void)
{
	// initialize parameters
	double x[D] = {0};
	double xe[D] = {0};	// for lyapunov exponent
	double d0;
	do {
		for (int i = 0; i < D; ++i)
			xe[i] = x[i] + ((double)rand() / RAND_MAX - 0.5) / 1000;
		d0 = dst(x, xe);
	} while (d0 <= 0);
	double v[D];

	for (int i = 0; i < D; ++i) {
		x_min[i] = 1e10;
		x_max[i] = -1e10;
		v_max[i] = 0;
	}

	double lyapunov = 0;
	for (unsigned n = 0; n < CUTOFF * 2; ++n) {
		double x_last[D];
		for (int i = 0; i < D; ++i)
			x_last[i] = x[i];

		iteration(x);
		iteration(xe);

		// converge, diverge
		for (int i = 0; i < D; ++i)
			if (fabs(x[i]) > 1e10 || fabs(x[i]) < 1e-10)
				return false;
		if (n > CUTOFF) {
			for (int i = 0; i < D; ++i) {
				v[i] = x[i] - x_last[i];
				x_max[i] = MAX(x_max[i], x[i]);
				x_min[i] = MIN(x_min[i], x[i]);
				v_max[i] = MAX(fabs(v[i]), v_max[i]);
			}
			// lyapunov exponent
			double d = dst(x, xe);
			lyapunov += log(fabs(d / d0));
		}
	}
	return lyapunov / CUTOFF > 5;
}

static void random_c(void)
{
	do {
		for (int i = 0; i < D; ++i)
			for (int j = 0; j < Dn; ++j)
				c[j][i] = (double)rand() / RAND_MAX * 4 - 2;
	} while (attractor() == false);
}

static bool set_c(const char buf[256])
{
	bool result = true;
	for (int i = 0; i < D; ++i)
		for (int j = 0; j < Dn; ++j)
			result = result && (bool)sscanf(buf + 7 * (i * Dn + j), "%lf", &c[j][i]);
	return result;
}

static void str_c(char buf[256])
{
	for (int i = 0; i < D; ++i)
		for (int j = 0; j < Dn; ++j)
			sprintf(buf + 7 * (i * Dn + j), "% 1.3f ", c[j][i]);
}

static double srgb(double L)
{
	return L <= 0.0031308 ? L * 12.92
		: 1.055 * pow(L, 1 / 2.44) - 0.055;
}

static void hsv_to_rgb(double H, double S, double V, double rgb[3])
{
	double C = V * S;
	double HH = H / 60;
	double X = C * (1 - fabs(fmod(HH, 2) - 1));
	switch ((int)floor(HH)) {
		case 0:
			rgb[0] = C;
			rgb[1] = X;
			rgb[2] = 0;
			break;
		case 1:
			rgb[0] = X;
			rgb[1] = C;
			rgb[2] = 0;
			break;
		case 2:
			rgb[0] = 0;
			rgb[1] = C;
			rgb[2] = X;
			break;
		case 3:
			rgb[0] = 0;
			rgb[1] = X;
			rgb[2] = C;
			break;
		case 4:
			rgb[0] = X;
			rgb[1] = 0;
			rgb[2] = C;
			break;
		case 5:
			rgb[0] = C;
			rgb[1] = 0;
			rgb[2] = X;
			break;
	}
	double m = V - C;
	for (int k = 0; k < 3; ++k)
		rgb[k] = rgb[k] + m;
}

static void rgb_to_hsv(double r, double g, double b, double *h, double *s, double *v)
{
	double x_max = MAX(r, MAX(g, b));
	double x_min = MIN(r, MIN(g, b));
	double c = x_max - x_min;
	*v = x_max;
	*s = *v == 0 ? 0 : c / *v;
	*h = c == 0 ?  0 :
		*v == r ? 60 * ((g - b) / c) :
		*v == g ? 60 * ((b - r) / c + 2) :
		60 * ((r - g) / c + 4);
	*h = *h < 0 ? 360 + *h : *h;
}

static void render_image(char buf[HEIGHT][WIDTH][3])
{
	memset(buf, 0, sizeof(char) * HEIGHT * WIDTH * 3);
	double (*info)[WIDTH][4] = calloc(1, sizeof(double) * HEIGHT * WIDTH * 4);

	double x[D] = {0};
	for (unsigned n = 0; n < CUTOFF; ++n)
		iteration(x);
	double v[D] = {0};

	double range[2] = {x_max[0] - x_min[0], x_max[1] - x_min[1]};
	int o = range[0] < range[1];
	double x_scale = (WIDTH - 1) / (x_max[o] - x_min[o]);
	double y_scale = (HEIGHT - 1) / (x_max[!o] - x_min[!o]);
	double scale = MIN(x_scale, y_scale) * (1 - BORDER);

#if D == 2 && defined(MANDLEBROT)
	for (int i = 0; i < HEIGHT; ++i)
		for (int j = 0; j < WIDTH; ++j) {
			double x[2];
			x[!o] = (double)i / HEIGHT * 4 - 2;
			x[ o] = (double)j / WIDTH * 4 - 2;
			unsigned n = 0;
			for (; n < 1000; ++n) {
				iteration(x);
				if (fabs(x[0]) > 1e10 || fabs(x[0]) < 1e-10 || fabs(x[1]) > 1e10 || fabs(x[1]) < 1e-10)
					break;
			}
			buf[i][j][0] = buf[i][j][1] = buf[i][j][2] = (char)(0x7 * n);
		}
#endif

	unsigned count = 0;
	for (unsigned n = CUTOFF; n < ITERATIONS; ++n) {
		double x_last[D];
		for (int i = 0; i < D; ++i)
			x_last[i] = x[i];
		iteration(x);
		for (int i = 0; i < D; ++i)
			v[i] = x[i] - x_last[i];

		int i = (int)((HEIGHT - range[!o] * scale) / 2 + (x[!o] - x_min[!o]) * scale);
		int j = (int)((WIDTH  - range[ o] * scale) / 2 + (x[ o] - x_min[ o]) * scale);
		if (i < 0 || i >= HEIGHT) continue;
		if (j < 0 || j >= WIDTH) continue;

		count += info[i][j][0] == 0;
		info[i][j][0] += 1;
		switch (COLOUR) {
			case HSV:
				info[i][j][1] += v[1] / v_max[1];
				info[i][j][2] += v[0] / v_max[0];
				break;
			case MIX:
				for (int k = 0; k < 3; ++k)
					info[i][j][k + 1] += fabs(dot(u[k], v)) / sqrt(dot(v_max, v_max));
				break;
			case RGB:
				info[i][j][1] += MAX(0, v[0] / v_max[0]);
				info[i][j][2] += MAX(0, -v[0] / v_max[0]);
				info[i][j][3] += fabs(v[1]) / v_max[1];
				break;
			case BW:
				break;
		}
	}
	double DENSITY = (double)ITERATIONS / count;

	for (int i = 0; i < HEIGHT; ++i)
		for (int j = 0; j < WIDTH; ++j) {
			switch (COLOUR) {
				case HSV:
				{
					double h = 180 + (atan2(info[i][j][1], info[i][j][2]) * 180 / M_PI);
					double s = 1;
					double v = MIN(1, INTENSITY / DENSITY * info[i][j][0] / 0xff);
					double tmp[3];
					hsv_to_rgb(h, s, v, tmp);
					for (int k = 0; k < 3; ++k)
						buf[i][j][k] = (char)(0xff * srgb(tmp[k]));
				} break;
				default:
					for (int k = 0; k < 3; ++k) {
						double a = MIN(0xff, info[i][j][COLOUR == BW ? 0 : k + 1] * INTENSITY / DENSITY);
						a /= 0xff;
						a = srgb(a);
						buf[i][j][k] = (char)(0xff * a);
					}
					if (COLOUR == BW)
						for (int k = 0; k < 3; ++k)
							buf[i][j][k] = 0xff - buf[i][j][k];
					break;
			}
		}

	free(info);
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

static int write_samples(char name[], char (*params_array)[256], int samples)
{
	int n = (int)ceil(sqrt((double)samples));
	int w = WIDTH * n;
	int h = HEIGHT * n;
	char *buf = (char *)malloc(sizeof(char) * w * h * 3);
#define BUF_AT(i, j, k) buf[(i) * w * 3 + (j) * 3 + (k)]
	memset(buf, 0x7f, sizeof(char) * w * h * 3);

	char txt[256];
	snprintf(txt, 256, "images/%s.txt", name);
	FILE *f = fopen(txt, "w");

	static char (*tmp)[WIDTH][3] = NULL;
	if (tmp == NULL)
		tmp = malloc(sizeof(char) * HEIGHT * WIDTH * 3);
	for (int s = 0; s < samples; ++s) {
		printf("%3d/%d\n", 1 + s, samples);
		int i = s / n;
		int j = s % n;

		set_c(params_array[s]);
		fprintf(f, "%s # %d\n", params_array[s], 1 + s);

		attractor();
		render_image(tmp);
		for (int k = 0; k < HEIGHT; ++k)
			memcpy(&BUF_AT(i * HEIGHT + k, j * WIDTH, 0), &tmp[k][0][0], sizeof(char) * WIDTH * 3);
	}
	fclose(f);

	char png[256];
	snprintf(png, 256, "images/%s.png", name);
	int result = write_image(png, w, h, buf);
	free(buf);
	return result;
}

static void write_attractor(char *name)
{
	static char (*buf)[WIDTH][3] = NULL;
	if (buf == NULL)
		buf = malloc(sizeof(char) * HEIGHT * WIDTH * 3);
	attractor();
	render_image(buf);
	write_image(name, WIDTH, HEIGHT, buf);
}

static void sample_attractor(int samples)
{
	char (*samples_array)[256] = (char (*)[256])malloc(sizeof(char) * samples * 256);

	for (int s = 0; s < samples; ++s) {
		random_c();
		str_c(samples_array[s]);
	}

	write_samples("samples", samples_array, samples);
	free(samples_array);
}

static void write_video(const char *params, double *cn, double start, double end, int frames)
{
	set_c(params);
	double range = end - start;
	double dt = range / frames;
	*cn += start;

	for (int frame = 0; frame < frames; ++frame) {
		attractor();	// TODO I shouldn't be doing this
		char name[256];
		snprintf(name, 256, "video/%d.png", frame);
		printf("%3d%%\t", frame * 100 / frames);
		write_attractor(name);
		*cn += dt;
	}
}

static void video_preview(const char *params, double *cn, double start, double end, int samples)
{
	char (*samples_array)[256] = (char (*)[256])malloc(sizeof(char) * samples * 256);
	set_c(params);
	double range = end - start;
	double dt = range / (double)samples;
	*cn += start;

	for (int s = 0; s < samples; ++s) {
		str_c(samples_array[s]);
		*cn += dt;
	}

	write_samples("preview", samples_array, samples);
}

static void video_params(double **cn, double *start, double *end)
{
	int i = D * rand() / RAND_MAX;
	int j = Dn * rand() / RAND_MAX;
	*cn = &c[j][i];

	static double step = 1e-2;
	double tmp = c[j][i];
	double dt = 0;
	if (start) {
		do {
			dt += step;
			c[j][i] = tmp - dt;
		} while (attractor());
		*start = -dt;
	}

	if (end) {
		c[j][i] = tmp;
		dt = 0;
		do {
			dt += step;
			c[j][i] = tmp + dt;
		} while (attractor());
		*end = dt;
	}
}

int main(int argc, char **argv)
{
	srand(time(NULL));

	// print help if no args
	if (argc <= 1) {
		help();
		exit(0);
	}

	// get the mode
	unsigned mode;
	if (0 == strcmp("video", argv[1]))
		mode = VIDEO;
	else if (0 == strcmp("image", argv[1]))
		mode = IMAGE;
	else {
		fprintf(stderr, "unknown mode: %s, expected \"image\" or \"video\"\n", argv[1]);
		exit(1);
	}

	// parse and set the options
	assert(argc % 2 == 0);
	for (int i = 2; i < argc;) {
		parse_option(mode, argv[i], argv[i + 1]);
		i += 2;
	}
	ITERATIONS = WIDTH * HEIGHT * QUALITY;

	// print the configuration
	print_values(mode);

	switch (mode) {
		case IMAGE:
			if (SAMPLES) {
				sample_attractor(SAMPLES);
			} else if (PARAMS) {
				FILE *f = fopen(PARAMS, "r");
				if (f == NULL) {
					fprintf(stderr, "option error: --params could not open \"%s\"\n", PARAMS);
					exit(1);
				}
				char buf[256];
				for (int i = 0; fgets(buf, 256, f); ++i) {
					int result = set_c(buf);
					if (!result) {
						fprintf(stderr, "parse error when reading \"%s\", line %d:\n", PARAMS, i);
						fprintf(stderr, "%s", buf);
						fprintf(stderr, "%s should be a %s\n", PARAMS, options[OP_PARAMS].doc);
						exit(1);
					}
					char name[256];
					snprintf(name, 256, "images/%d.png", i);
					write_attractor(name);
				}
			} else {;
				random_c();
				char buf[256];
				str_c(buf);
				char name[256];
				snprintf(name, 256, "images/single.png");
				write_attractor(name);
			}
			break;
		case VIDEO:
			if (SAMPLES > 0) {
				double *cn;
				video_params(&cn, is_set(OP_START) ? NULL : &START, is_set(OP_END) ? NULL : &END);
				random_c();
				char buf[256];
				str_c(buf);
				video_preview(buf, cn, START, END, SAMPLES);
			} else {
				fprintf(stderr, "video rendering not supported\n");
				exit(1);
			}
			break;
	}
}
