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

typedef double coef[Dn][D];
typedef double vec[D];

struct config {
	coef c;
	vec x_min, x_max, v_max;
};

static vec u[3] = {
	{1, 0},
	{-1.0 / 2, M_SQRT3 / 2},
	{-1.0 / 2, -M_SQRT3 / 2},
};

static double dot(vec x, vec y)
{
	double s = 0;
	for (int i = 0; i < D; ++i)
		s += x[i] * y[i];
	return s;
}

static double dst(vec x0, vec x1)
{
	double s = 0;
	for (int i = 0; i < D; ++i) {
		double d = x1[i] - x0[i];
		s += d * d;
	}
	return sqrt(s);
}

static void iteration(coef c, vec y)
{
#ifndef D
	vec z[2];
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
	vec z = {0, 0};
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
static bool attractor(struct config *conf)
{
	// initialize parameters
	vec x = {0};
	vec xe = {0};	// for lyapunov exponent
	double d0;
	do {
		for (int i = 0; i < D; ++i)
			xe[i] = x[i] + ((double)rand() / RAND_MAX - 0.5) / 1000;
		d0 = dst(x, xe);
	} while (d0 <= 0);
	vec v;

	for (int i = 0; i < D; ++i) {
		conf->x_min[i] = 1e10;
		conf->x_max[i] = -1e10;
		conf->v_max[i] = 0;
	}

	double lyapunov = 0;
	for (unsigned n = 0; n < CUTOFF * 2; ++n) {
		vec x_last;
		for (int i = 0; i < D; ++i)
			x_last[i] = x[i];

		iteration(conf->c, x);
		iteration(conf->c, xe);

		// converge, diverge
		for (int i = 0; i < D; ++i)
			if (fabs(x[i]) > 1e10 || fabs(x[i]) < 1e-10)
				return false;
		if (n > CUTOFF) {
			for (int i = 0; i < D; ++i) {
				v[i] = x[i] - x_last[i];
				conf->x_max[i] = MAX(conf->x_max[i], x[i]);
				conf->x_min[i] = MIN(conf->x_min[i], x[i]);
				conf->v_max[i] = MAX(fabs(v[i]), conf->v_max[i]);
			}
			// lyapunov exponent
			double d = dst(x, xe);
			lyapunov += log(fabs(d / d0));
		}
	}
	return lyapunov / CUTOFF > 5;
}

static bool is_valid(coef c)
{
	struct config tmp;
	memcpy(tmp.c, c, sizeof(coef));
	return attractor(&tmp);
}

static void random_config(struct config *conf)
{
	do {
		for (int i = 0; i < D; ++i)
			for (int j = 0; j < Dn; ++j)
				conf->c[j][i] = (double)rand() / RAND_MAX * 4 - 2;
	} while (attractor(conf) == false);
}

static bool set_config(struct config *conf, const char params[256])
{
	bool result = true;
	for (int i = 0; i < D; ++i)
		for (int j = 0; j < Dn; ++j)
			result &= (bool)sscanf(params + 7 * (i * Dn + j), "%lf", &conf->c[j][i]);
	result &= attractor(conf);
	return result;
}

static void str_c(coef c, char params[256])
{
	for (int i = 0; i < D; ++i)
		for (int j = 0; j < Dn; ++j)
			sprintf(params + 7 * (i * Dn + j), "% 1.3f ", c[j][i]);
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

static void render_image(struct config *conf, char buf[HEIGHT][WIDTH][3])
{
	memset(buf, 0, sizeof(char) * HEIGHT * WIDTH * 3);
	double (*info)[WIDTH][4] = calloc(1, sizeof(double) * HEIGHT * WIDTH * 4);

	vec x = {0};
	for (unsigned n = 0; n < CUTOFF; ++n)
		iteration(conf->c, x);
	vec v = {0};

	double range[2] = {conf->x_max[0] - conf->x_min[0], conf->x_max[1] - conf->x_min[1]};
	int o = range[0] < range[1];
	double x_scale = (WIDTH - 1) / (conf->x_max[o] - conf->x_min[o]);
	double y_scale = (HEIGHT - 1) / (conf->x_max[!o] - conf->x_min[!o]);
	double scale = MIN(x_scale, y_scale) * (1 - BORDER);

#if 0
	for (int i = 0; i < HEIGHT; ++i)
		for (int j = 0; j < WIDTH; ++j) {
			vec x[2];
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
		vec x_last;
		for (int i = 0; i < D; ++i)
			x_last[i] = x[i];
		iteration(conf->c, x);
		for (int i = 0; i < D; ++i)
			v[i] = x[i] - x_last[i];

		int i = (int)((HEIGHT - range[!o] * scale) / 2 + (x[!o] - conf->x_min[!o]) * scale);
		int j = (int)((WIDTH  - range[ o] * scale) / 2 + (x[ o] - conf->x_min[ o]) * scale);
		if (i < 0 || i >= HEIGHT) continue;
		if (j < 0 || j >= WIDTH) continue;

		count += info[i][j][0] == 0;
		info[i][j][0] += 1;
		switch (COLOUR) {
			case HSV:
				info[i][j][1] += v[1] / conf->v_max[1];
				info[i][j][2] += v[0] / conf->v_max[0];
				break;
			case MIX:
				for (int k = 0; k < 3; ++k)
					info[i][j][k + 1] += fabs(dot(u[k], v)) / sqrt(dot(conf->v_max, conf->v_max));
				break;
			case RGB:
				info[i][j][1] += MAX(0, v[0] / conf->v_max[0]);
				info[i][j][2] += MAX(0, -v[0] / conf->v_max[0]);
				info[i][j][3] += fabs(v[1]) / conf->v_max[1];
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

static int write_samples(char name[], struct config *config_array, int samples)
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

	char (*tmp)[WIDTH][3] = malloc(sizeof(char) * HEIGHT * WIDTH * 3);
	for (int s = 0; s < samples; ++s) {
		printf("%3d/%d\n", 1 + s, samples);
		int i = s / n;
		int j = s % n;

		char name[256];
		str_c(config_array[s].c, name);
		fprintf(f, "%s # %d\n", name, 1 + s);

		render_image(&config_array[s], tmp);
		for (int k = 0; k < HEIGHT; ++k)
			memcpy(&BUF_AT(i * HEIGHT + k, j * WIDTH, 0), &tmp[k][0][0], sizeof(char) * WIDTH * 3);
	}
	fclose(f);

	char png[256];
	snprintf(png, 256, "images/%s.png", name);
	int result = write_image(png, w, h, buf);
	free(buf);
	free(tmp);
	return result;
}

static void write_attractor(char *name, struct config *conf)
{
	char (*buf)[WIDTH][3] = malloc(sizeof(char) * HEIGHT * WIDTH * 3);
	render_image(conf, buf);
	write_image(name, WIDTH, HEIGHT, buf);
	free(buf);
}

static void sample_attractor(int samples)
{
	struct config *config_array = (struct config *)malloc(sizeof(struct config) * samples);

	for (int s = 0; s < samples; ++s)
		random_config(&config_array[s]);

	write_samples("samples", config_array, samples);
	free(config_array);
}

static void write_video(const char *params, int ci, int cj, double start, double end, int frames)
{
	struct config conf;
	set_config(&conf, params);
	double range = end - start;
	double dt = range / frames;
	conf.c[cj][ci] += start;

	for (int frame = 0; frame < frames; ++frame) {
		char name[256];
		snprintf(name, 256, "video/%d.png", frame);
		printf("%3d%%\t", frame * 100 / frames);
		write_attractor(name, &conf);
		conf.c[cj][ci] += dt;
	}
}

static void video_preview(const char *params, int ci, int cj, double start, double end, int samples)
{
	struct config *config_array = (struct config *)malloc(sizeof(struct config) * samples);

	set_config(&config_array[0], params);
	double range = end - start;
	double dt = range / (double)samples;
	config_array[0].c[cj][ci] += start;
	for (int s = 1; s < samples; ++s) {
		memcpy(&config_array[s], &config_array[0], sizeof(struct config));
		config_array[s].c[cj][ci] += dt * s;
	}

	write_samples("preview", config_array, samples);
	free(config_array);
}

static void video_params(coef c, int *ci, int *cj, double *start, double *end)
{
	*ci = D * rand() / RAND_MAX;
	*cj = Dn * rand() / RAND_MAX;

	static const double step = 1e-2;
	double tmp = c[*cj][*ci];
	if (start) {
		double dt = 0;
		do {
			dt += step;
			c[*cj][*ci] = tmp - dt;
		} while (is_valid(c));
		*start = -dt;
		c[*cj][*ci] = tmp;
	}
	if (end) {
		double dt = 0;
		do {
			dt += step;
			c[*cj][*ci] = tmp + dt;
		} while (is_valid(c));
		*end = dt;
		c[*cj][*ci] = tmp;
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
					struct config conf;
					int result = set_config(&conf, buf);
					if (!result) {
						fprintf(stderr, "parse error when reading \"%s\", line %d:\n", PARAMS, i);
						fprintf(stderr, "%s", buf);
						fprintf(stderr, "%s should be a %s\n", PARAMS, options[OP_PARAMS].doc);
						exit(1);
					}
					char name[256];
					snprintf(name, 256, "images/%d.png", i);
					write_attractor(name, &conf);
				}
			} else {;
				struct config conf;
				random_config(&conf);
				char buf[256];
				str_c(conf.c, buf);
				char name[256];
				snprintf(name, 256, "images/single.png");
				write_attractor(name, &conf);
			}
			break;
		case VIDEO:
			if (SAMPLES > 0) {
				struct config conf;
				random_config(&conf);
				int ci, cj;
				video_params(conf.c, &ci, &cj, is_set(OP_START) ? NULL : &START, is_set(OP_END) ? NULL : &END);
				char params[256];
				str_c(conf.c, params);
				video_preview(params, ci, cj, START, END, SAMPLES);
			} else {
				fprintf(stderr, "video rendering not supported\n");
				exit(1);
			}
			break;
	}
}
