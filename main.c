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
#define STB_IMAGE_RESIZE_IMPLEMENTATION
#include "stb_image_resize2.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

unsigned THREAD_COUNT;

struct work_queue_info {
	volatile int next_entry;
	volatile int entry_count;
	volatile int back;
};

static void run_jobs(DWORD WINAPI (*callback)(void *), void *arg)
{
	// create threads
	HANDLE threads[THREAD_COUNT];
	for (int i = 0; i < THREAD_COUNT; ++i)
		threads[i] = CreateThread(0, 0, callback, arg, 0, 0);

	// wait for the work to be done
	WaitForMultipleObjects(THREAD_COUNT, threads, true, INFINITE);

	// close the thread handless
	for (int i = 0; i < THREAD_COUNT; ++i)
		CloseHandle(threads[i]);
}

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
#define D 2
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
	vec xe = {1e-3};	// for lyapunov exponent
	double d0 = dst(x, xe);
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
	attractor(conf);
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
	unsigned D_WIDTH = WIDTH * DOWNSCALE, D_HEIGHT = HEIGHT * DOWNSCALE;
	char (*big_buf)[WIDTH * DOWNSCALE][3] = NULL;
	if (DOWNSCALE > 1)
		big_buf = calloc(1, sizeof(char) * D_HEIGHT * D_WIDTH * 3);
	else
		big_buf = buf;
	memset(big_buf, 0, sizeof(char) * D_HEIGHT * D_WIDTH * 3);
	double (*info)[D_WIDTH][4] = calloc(1, sizeof(double) * D_HEIGHT * D_WIDTH * 4);

	vec x = {0};
	for (unsigned n = 0; n < CUTOFF; ++n)
		iteration(conf->c, x);
	vec v = {0};

	double range[2] = {conf->x_max[0] - conf->x_min[0], conf->x_max[1] - conf->x_min[1]};
	int o = range[0] < range[1];
	double x_scale = (D_WIDTH - 1) / (conf->x_max[o] - conf->x_min[o]);
	double y_scale = (D_HEIGHT - 1) / (conf->x_max[!o] - conf->x_min[!o]);
	double scale = MIN(x_scale, y_scale) * (1 - BORDER);

#if 0
	for (int i = 0; i < D_HEIGHT; ++i)
		for (int j = 0; j < D_WIDTH; ++j) {
			vec x[2];
			x[!o] = (double)i / D_HEIGHT * 4 - 2;
			x[ o] = (double)j / D_WIDTH * 4 - 2;
			unsigned n = 0;
			for (; n < 1000; ++n) {
				iteration(x);
				if (fabs(x[0]) > 1e10 || fabs(x[0]) < 1e-10 || fabs(x[1]) > 1e10 || fabs(x[1]) < 1e-10)
					break;
			}
			big_buf[i][j][0] = big_buf[i][j][1] = big_buf[i][j][2] = (char)(0x7 * n);
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

		int i = (int)((D_HEIGHT - range[!o] * scale) / 2 + (x[!o] - conf->x_min[!o]) * scale);
		int j = (int)((D_WIDTH  - range[ o] * scale) / 2 + (x[ o] - conf->x_min[ o]) * scale);
		if (i < 0 || i >= D_HEIGHT) continue;
		if (j < 0 || j >= D_WIDTH) continue;

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

	for (int i = 0; i < D_HEIGHT; ++i)
		for (int j = 0; j < D_WIDTH; ++j) {
			switch (COLOUR) {
				case HSV:
				{
					double h = 180 + (atan2(info[i][j][1], info[i][j][2]) * 180 / M_PI);
					double s = 1;
					double v = MIN(1, INTENSITY / DENSITY * info[i][j][0] / 0xff);
					double tmp[3];
					hsv_to_rgb(h, s, v, tmp);
					for (int k = 0; k < 3; ++k)
						big_buf[i][j][k] = (char)(0xff * srgb(tmp[k]));
				} break;
				default:
					for (int k = 0; k < 3; ++k) {
						double a = MIN(0xff, info[i][j][COLOUR == BW ? 0 : k + 1] * INTENSITY / DENSITY);
						a /= 0xff;
						a = srgb(a);
						big_buf[i][j][k] = (char)(0xff * a);
					}
					if (COLOUR == BW)
						for (int k = 0; k < 3; ++k)
							big_buf[i][j][k] = 0xff - big_buf[i][j][k];
					break;
			}
		}

	if (DOWNSCALE >  1) {
		stbir_resize_uint8_srgb((unsigned char *)big_buf, D_WIDTH, D_HEIGHT, D_WIDTH * sizeof(char) * 3,
		                        (unsigned char *)buf, WIDTH, HEIGHT, WIDTH * sizeof(char) * 3,
		                        STBIR_RGB);
		free(big_buf);
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

struct write_samples_arg {
	struct work_queue_info thread_info;
	struct config *config_array;
	int n;
	int w, h;
	char *buf;
};

static DWORD WINAPI write_samples_callback(void *arg_)
{
	struct write_samples_arg *arg = (struct write_samples_arg *)arg_;
	char (*img)[WIDTH][3] = malloc(sizeof(char) * HEIGHT * WIDTH * 3);
#define BUF_AT(i, j, k) arg->buf[(i) * arg->w * 3 + (j) * 3 + (k)]

	while (arg->thread_info.next_entry < arg->thread_info.entry_count) {
		int s = InterlockedIncrement((long *)&arg->thread_info.next_entry) - 1;
		if (s >= arg->thread_info.next_entry)
			break;

		int i = s / arg->n;
		int j = s % arg->n;
		render_image(&arg->config_array[s], img);
		for (int k = 0; k < HEIGHT; ++k)
			memcpy(&BUF_AT(i * HEIGHT + k, j * WIDTH, 0), &img[k][0][0], sizeof(char) * WIDTH * 3);
		printf("%3d\n", 1 + s);
	}

	free(img);
	return 0;
}

static int write_samples(char name[], struct config *config_array, int samples)
{
	int n = (int)ceil(sqrt((double)samples));
	int w = WIDTH * n;
	int h = HEIGHT * n;
	char *buf = (char *)malloc(sizeof(char) * w * h * 3);
	memset(buf, 0x7f, sizeof(char) * w * h * 3);

	char txt[256];
	snprintf(txt, 256, "images/%s.txt", name);
	FILE *f = fopen(txt, "w");
	// TODO do this in the thread function
	for (int s = 0; s < samples; ++s) {
		char name[256];
		str_c(config_array[s].c, name);
		fprintf(f, "%s # %d\n", name, 1 + s);
	}
	fclose(f);

	struct write_samples_arg arg = {0};
	arg.thread_info.entry_count = samples;
	arg.config_array = config_array;
	arg.n = n;
	arg.w = w;
	arg.h = h;
	arg.buf = buf;
	run_jobs(write_samples_callback, (void *)&arg);

	char png[256];
	snprintf(png, 256, "images/%s.png", name);
	int result = write_image(png, w, h, buf);
	free(buf);
	return result;
}

static void write_attractor(char *name, struct config *conf)
{
	char (*buf)[WIDTH][3] = malloc(sizeof(char) * HEIGHT * WIDTH * 3);
	render_image(conf, buf);
	write_image(name, WIDTH, HEIGHT, buf);
	free(buf);
}

struct write_attractors_arg {
	struct work_queue_info thread_info;
	struct config *config_array;
};

static DWORD WINAPI write_attractors_callback(void *arg_)
{
	struct write_attractors_arg *arg = (struct write_attractors_arg *)arg_;
	char (*buf)[WIDTH][3] = malloc(sizeof(char) * HEIGHT * WIDTH * 3);

	while (arg->thread_info.next_entry < arg->thread_info.entry_count) {
		int s = InterlockedIncrement((long *)&arg->thread_info.next_entry) - 1;
		if (s >= arg->thread_info.next_entry)
			break;

		char name[256];
		snprintf(name, 256, "images/%d.png", s);
		render_image(&arg->config_array[s], buf);
		write_image(name, WIDTH, HEIGHT, buf);
	}

	free(buf);
	return 0;
}

static void write_attractors(struct config *config_array, int count)
{
	struct write_attractors_arg arg = {0};
	arg.thread_info.entry_count = count;
	arg.config_array = config_array;
	run_jobs(write_attractors_callback, (void *)&arg);
}

static void sample_attractor(int samples)
{
	struct config *config_array = (struct config *)malloc(sizeof(struct config) * samples);

	for (int s = 0; s < samples; ++s)
		random_config(&config_array[s]);

	write_samples("samples", config_array, samples);
	free(config_array);
}

static void set_video_params(const char *params, struct config *config_array, int frames)
{
	double range = END - START;
	double dt = range / frames;

	vec x_max;
	vec x_min;
	for (int i = 0; i < D; ++i) {
		x_min[i] = 1e10;
		x_max[i] = -1e10;
	}

	set_config(&config_array[0], params);
	config_array[0].c[CJ][CI] += START;
	for (int i = 1; i < frames; ++i) {
		memcpy(&config_array[i], &config_array[0], sizeof(struct config));
		config_array[i].c[CJ][CI] += dt * i;
		attractor(&config_array[i]);

		for (int j = 0; j < D; ++j) {
			x_max[j] = MAX(x_max[j], config_array[i].x_max[j]);
			x_min[j] = MIN(x_min[j], config_array[i].x_min[j]);
		}
	}

	for (int i = 0; i < frames; ++i)
		for (int j = 0; j < D; ++j) {
			config_array[i].x_max[j] = x_max[j];
			config_array[i].x_min[j] = x_min[j];
		}
}

static void video_preview(const char *params, int samples)
{
	struct config config_array[samples];
	set_video_params(params, config_array, samples);
	write_samples("preview", config_array, samples);
}

struct write_video_arg {
	struct work_queue_info thread_info;
	struct config *config_array;
	FILE *pipe;
};

static DWORD WINAPI write_video_callback(void *arg_)
{
	struct write_video_arg *arg = (struct write_video_arg *)arg_;
	char (*buf)[WIDTH][3] = malloc(sizeof(char) * HEIGHT * WIDTH * 3);

	while (arg->thread_info.next_entry < arg->thread_info.entry_count) {
		int s = InterlockedIncrement((long *)&arg->thread_info.next_entry) - 1;
		if (s >= arg->thread_info.next_entry)
			break;

		render_image(&arg->config_array[s], buf);

		// wait until it's out to to write the image
		int undesired = -1;
		while (s != arg->thread_info.back)
			WaitOnAddress(&arg->thread_info.back, &undesired, sizeof(int), INFINITE);
		fwrite(buf, sizeof(char) * HEIGHT * WIDTH * 3, 1, arg->pipe);
		++arg->thread_info.back;
		// notify other threads when we're done
		WakeByAddressAll((void *)&arg->thread_info.back);
	}

	free(buf);
	return 0;
}

static void write_video(const char *params, int frames)
{
	struct config config_array[frames];
	set_video_params(params, config_array, frames);
	char buf[256];
	snprintf(buf, 256,
	         "ffmpeg.exe -y -f rawvideo -pix_fmt rgb24 -s %dx%d -r %d -i - "
	         "-c:v libx264 -preset ultrafast -qp 0 images/out.mp4", WIDTH, HEIGHT, FPS);
	FILE *pipe = _popen(buf, "wb");

	struct write_video_arg arg = {0};
	arg.thread_info.entry_count = frames;
	arg.config_array = config_array;
	arg.pipe = pipe;
	run_jobs(write_video_callback, (void *)&arg);

	_pclose(pipe);
}

static void video_params(coef c)
{
	if (!is_set(OP_COEFFICIENT)) {
		set(OP_COEFFICIENT);
		CI = rand() % D;
		CJ = rand() % Dn;
	}

	static const double step = 1e-2;
	double tmp = c[CJ][CI];
	if (!is_set(OP_START)) {
		set(OP_START);
		double dt = 0;
		do {
			dt += step;
			c[CJ][CI] = tmp - dt;
		} while (is_valid(c));
		START = -dt;
		c[CJ][CI] = tmp;
	}
	if (!is_set(OP_END)) {
		set(OP_END);
		double dt = 0;
		do {
			dt += step;
			c[CJ][CI] = tmp + dt;
		} while (is_valid(c));
		END = dt;
		c[CJ][CI] = tmp;
	}
}

int main(int argc, char **argv)
{
	srand(time(NULL));

	// thread count
	{
		SYSTEM_INFO sysinfo;
		GetSystemInfo(&sysinfo);
		THREAD_COUNT = sysinfo.dwNumberOfProcessors - 1;
	}

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

				int count = 0;
				while (!feof(f))
					count += fgetc(f) == '\n';
				rewind(f);

				struct config config_array[count];
				char buf[256];
				for (int i = 0; fgets(buf, 256, f); ++i) {
					int result = set_config(&config_array[i], buf);
					if (!result) {
						fprintf(stderr, "parse error when reading \"%s\", line %d:\n", PARAMS, i);
						fprintf(stderr, "%s", buf);
						fprintf(stderr, "%s should be a %s\n", PARAMS, options[OP_PARAMS].doc);
						exit(1);
					}
				}

				write_attractors(config_array, count);
			} else {
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
			struct config conf;
			if (PARAMS) {
				FILE *f = fopen(PARAMS, "r");
				if (f == NULL) {
					fprintf(stderr, "option error: --params could not open \"%s\"\n", PARAMS);
					exit(1);
				}

				char buf[256];
				fgets(buf, 256, f);
				int result = set_config(&conf, buf);
				if (!result) {
					fprintf(stderr, "parse error when reading \"%s\", line %d:\n", PARAMS, 0);
					fprintf(stderr, "%s", buf);
					fprintf(stderr, "%s should be a %s\n", PARAMS, options[OP_PARAMS].doc);
					exit(1);
				}
			} else {
				random_config(&conf);
			}

			video_params(conf.c);
			char params[256];
			str_c(conf.c, params);
			if (SAMPLES)
				video_preview(params, SAMPLES);
			else
				write_video(params, DURATION * FPS);
			break;
	}

	// print the final configuration
	print_values(mode);
}
