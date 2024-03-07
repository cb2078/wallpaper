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

char *colour_map[] = {
	[BW] = "BW",
	[HSV] = "HSV",
	[RGB] = "RGB",
};

unsigned CUTOFF = 10000;
unsigned ITERATIONS;
#define SAMPLES PREVIEW

#define MAX(x, y)	((x) > (y) ? (x) : (y))
#define MIN(x, y)	((x) < (y) ? (x) : (y))
#define LENGTH(a)	(sizeof(a) / sizeof(a[0]))

#include "cmdline.c"

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
			if (fabs(x[i]) > 1e10 || fabs(x[i]) < 1e-10)
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

static void random_c(void)
{
	do {
		for (int i = 0; i < 2; ++i)
			for (int j = 0; j < 6; ++j)
				c[j][i] = (double)rand() / RAND_MAX * 4 - 2;
	} while (attractor() == false);
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
	double (*info)[WIDTH][4] = calloc(1, sizeof(double) * HEIGHT * WIDTH * 4);

	double x[2] = {0, 0};
	for (unsigned n = 0; n < CUTOFF; ++n)
		iteration(x);
	double v[2] = {0, 0};

	double range[2] = {x_max[0] - x_min[0], x_max[1] - x_min[1]};
	int o = range[0] < range[1];
	double x_scale = (WIDTH - 1) / (x_max[o] - x_min[o]);
	double y_scale = (HEIGHT - 1) / (x_max[!o] - x_min[!o]);
	double scale = MIN(x_scale, y_scale) * (1 - BORDER);
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
		fprintf(f, "%3d\t%s\n", s + 1, params_array[s]);

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
	int i = 2 * rand() / RAND_MAX;
	int j = 6 * rand() / RAND_MAX;
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
			// draw samples
			if (SAMPLES > 0)
				sample_attractor(SAMPLES);
			else {
				random_c();
				char buf[256];
				str_c(buf);
				char name[256];
				snprintf(name, 256, "images/%s.png", buf);
				write_attractor(name);
			}
			break;
		case VIDEO:
			// make a preview
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
