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

#define MAX(x, y)	((x) > (y) ? (x) : (y))
#define MIN(x, y)	((x) < (y) ? (x) : (y))
#define LENGTH(a)	(sizeof(a) / sizeof(a[0]))

#include "platform.h"
#include "cmdline.h"
#include "gradient.h"

struct work_queue_info {
	volatile int next_entry;
	volatile int entry_count;
	volatile int back;
};

static void run_jobs(thread_callback callback, thread_arg arg)
{
	// create threads
	thread_handle *threads = malloc(sizeof(thread_handle) * THREADS);
	for (int i = 0; i < THREADS; ++i)
		threads[i] = create_thread(callback, arg);

	// wait for the work to be done
	wait_for_multiple_threads(threads, THREADS);

	// close the thread handless
	for (int i = 0; i < THREADS; ++i)
		close_thread(threads[i]);
	free(threads);
}

long long unsigned CUTOFF = 10000, ITERATIONS;

typedef double coef[8][2];
typedef double vec[2];

struct config {
	coef c;
	vec x_min, x_max, v_max;
	enum colour_type colour;
};

static vec u[3] = {
	{1, 0},
	{-1.0 / 2, M_SQRT3 / 2},
	{-1.0 / 2, -M_SQRT3 / 2},
};

static double dot(vec x, vec y)
{
	double s = 0;
	for (int i = 0; i < 2; ++i)
		s += x[i] * y[i];
	return s;
}

static double dst(vec x0, vec x1)
{
	double s = 0;
	for (int i = 0; i < 2; ++i) {
		double d = x1[i] - x0[i];
		s += d * d;
	}
	return sqrt(s);
}

static double mag(vec x)
{
	double d = dot(x, x);
	return sqrt(d);
}

static double triangle(double x)
{
	return 1 - 4 * fabs(x - floor(x + 0.5));
}

static double saw(double x)
{
	return 1 - 2 * (x - floor(x));
}

static void iteration(coef c, vec y)
{
#if 1
	vec z;
	for (int i = 0; i < 2; ++i)
		switch (TYPE) {
			case AT_POLY:
				z[i] =
					c[0][i] +
					c[1][i] * y[0] +
					c[2][i] * y[0] * y[0] +
					c[3][i] * y[0] * y[1] +
					c[4][i] * y[1] * y[1] +
					c[5][i] * y[1];
				break;
			case AT_TRIG:
				z[i] =
					c[0][i] * sin(c[1][i] * y[1]) +
					c[2][i] * cos(c[3][i] * y[0]) +
					c[4][i] * sin(c[5][i] * y[0]) +
					c[6][i] * cos(c[7][i] * y[1]);
				break;
			case AT_SAW:
				z[i] =
					c[0][i] * saw(c[1][i] * y[1]) +
					c[2][i] * saw(c[3][i] * y[0] + 0.5) +
					c[4][i] * saw(c[5][i] * y[0]) +
					c[6][i] * saw(c[7][i] * y[1] + 0.5);
				break;
			case AT_TRI:
				z[i] =
					c[0][i] * triangle(c[1][i] * y[1]) +
					c[2][i] * triangle(c[3][i] * y[0] + 0.5) +
					c[4][i] * triangle(c[5][i] * y[0]) +
					c[6][i] * triangle(c[7][i] * y[1] + 0.5);
				break;
		}
#else
#define Y(i) (i < 2 ? y[i] : 1)
	vec z = {0, 0};
	for (int i = 0; i < 2; ++i) {
		int a = 0;
		for (int j = 0; j < 2 + 1; ++j)
			for (int k = j; k < 2 + 1; ++k)
				z[i] += c[a++][i] * Y(j) * Y(k);
	}
#endif
	for (int i = 0; i < 2; ++i)
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

	for (int i = 0; i < 2; ++i) {
		conf->x_min[i] = 1e10;
		conf->x_max[i] = -1e10;
		conf->v_max[i] = 0;
	}

	double lyapunov = 0;
	for (unsigned n = 0; n < CUTOFF * 2; ++n) {
		vec x_last;
		for (int i = 0; i < 2; ++i)
			x_last[i] = x[i];

		iteration(conf->c, x);
		iteration(conf->c, xe);

		// converge, diverge
		for (int i = 0; i < 2; ++i)
			if (fabs(x[i]) > 1e10 || fabs(x[i]) < 1e-10)
				return false;
		if (n > CUTOFF) {
			for (int i = 0; i < 2; ++i) {
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
	printf("\rfinding attractor parameters");
	conf->colour = COLOUR;
	do {
		for (int i = 0; i < 2; ++i)
			for (int j = 0; j < CN; ++j)
				conf->c[j][i] = (double)rand() / RAND_MAX * 4 - 2;
	} while (attractor(conf) == false);
}

static bool set_config(struct config *conf, const char params[256])
{
	conf->colour = COLOUR;
	bool result = true;
	for (int i = 0; i < 2; ++i)
		for (int j = 0; j < CN; ++j)
			result &= (bool)sscanf(params + 7 * (i * CN + j), "%lf", &conf->c[j][i]);
	attractor(conf);
	return result;
}

static void str_c(coef c, char params[256])
{
	for (int i = 0; i < 2; ++i)
		for (int j = 0; j < CN; ++j)
			sprintf(params + 7 * (i * CN + j), "% 1.3f ", c[j][i]);
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

static void hsv_to_hsl(double sv, double v, double *sl, double *l)
{
	*l = v * (1 - sv / 2);
	*sl = *l == 0 || *l == 1 ? 0 : (v - *l) / MIN(*l, 1 - *l);
}

static void hsl_to_hsv(double sl, double l, double *sv, double *v)
{
	*v = l + sl * MIN(l, 1 - l);
	*sv = *v == 0 ? 0 : 2 * (1 - l / *v);
}

static void hsl_to_rgb(double h, double s, double l, double rgb[3])
{
	double sv, v;
	hsl_to_hsv(s, l, &sv, &v);
	hsv_to_rgb(h, sv, v, rgb);
}

static void rgb_to_hsl(double rgb[3], double *h, double *s, double *l)
{
	double sv, v;
	rgb_to_hsv(rgb[0], rgb[1], rgb[2], h, &sv, &v);
	hsv_to_hsl(sv, v, s, l);
}

static void rgb256_to_rgb1(unsigned char rgb256[3], double rgb1[3])
{
	for (int k = 0; k < 3; ++k)
		rgb1[k] = (double)rgb256[k] / 0xff;
}

static void rgb1_to_rgb256(double rgb1[3], unsigned char rgb256[3])
{
	for (int k = 0; k < 3; ++k)
		rgb256[k] = (unsigned char)(rgb1[k] * 0xff);
}

static void inv(double rgb[3])
{
	for (int k = 0; k < 3; ++k)
		rgb[k] = 1 - rgb[k];
}

static void set_brightness(double desired, double rgb1[3], double r[3])
{
#if 0
	double h, s, l;
	rgb_to_hsl(rgb1, &h, &s, &l);
	hsl_to_rgb(h, s, desired, r);
#else
	double dot = rgb1[0] * rgb1[0] + rgb1[1] * rgb1[1] + rgb1[2] * rgb1[2];
	double mag = sqrt(dot);
	if (mag == 0)
		memcpy(r, rgb1, sizeof(double) * 3);
	else
		for (int k = 0; k < 3; ++k) {
			r[k] = r[k] / mag * sqrt(pow(desired, 2) *3);
			if (r[k] > 1)
				r[k] = 1;
		}
#endif
}

static void lerp(double result[3], double left[3], double right[3], double t)
{
	for (int k = 0; k < 3; ++k)
		result[k] = (1 -t ) * left[k] + t * right[k];
}

#define BUF(i, j, k) buf[(i) * WIDTH * 3 + (j) * 3 + (k)]
#define BIG_BUF(i, j, k) big_buf[(i) * D_WIDTH * 3 + (j) * 3 + (k)]
#define INFO(i, j, k) info[(i) * D_WIDTH * 4 + (j) * 4 + (k)]

static void render_image(struct config *conf, unsigned char *buf)
{
	unsigned char bg = LIGHT ? 0xff : 0;
	int D_WIDTH = WIDTH * DOWNSCALE, D_HEIGHT = HEIGHT * DOWNSCALE;
	unsigned char *big_buf = DOWNSCALE > 1 ?
		malloc(sizeof(char) * D_HEIGHT * D_WIDTH * 3) :
		buf;
	memset(big_buf, bg, sizeof(char) * D_HEIGHT * D_WIDTH * 3);
	double *info = calloc(1, sizeof(double) * D_HEIGHT * D_WIDTH * 4);

	vec x = {0};
	for (unsigned n = 0; n < CUTOFF; ++n)
		iteration(conf->c, x);

	double range[2] = {conf->x_max[0] - conf->x_min[0], conf->x_max[1] - conf->x_min[1]};
	int o = range[0] < range[1];
	double x_scale = (D_WIDTH - 1) / (conf->x_max[o] - conf->x_min[o]) * (1 - BORDER);
	double y_scale = (D_HEIGHT - 1) / (conf->x_max[!o] - conf->x_min[!o]) * (1 - BORDER);
	if (!STRETCH) {
		if (x_scale > y_scale)
			x_scale = y_scale;
		else
			y_scale = x_scale;
	}

	unsigned count = 0;
	for (long long unsigned n = CUTOFF; n < ITERATIONS; ++n) {
		vec x_last;
		vec v;
		for (int i = 0; i < 2; ++i)
			x_last[i] = x[i];
		iteration(conf->c, x);
		for (int i = 0; i < 2; ++i)
			v[i] = x[i] - x_last[i];

		int i = (int)((D_HEIGHT - range[!o] * y_scale) / 2 + (x[!o] - conf->x_min[!o]) * y_scale);
		int j = (int)((D_WIDTH  - range[ o] * x_scale) / 2 + (x[ o] - conf->x_min[ o]) * x_scale);
		if (i < 0 || i >= D_HEIGHT) continue;
		if (j < 0 || j >= D_WIDTH) continue;

		count += INFO(i, j, 0) == 0;
		INFO(i, j, 0) += 1;
		switch (conf->colour) {
			case HSV:
			case HSL:
				vec w = {v[1] / conf->v_max[1], v[0] / conf->v_max[0]};
				double m = mag(w);
				INFO(i, j, 1) += w[1] / m;
				INFO(i, j, 2) += w[0] / m;
				break;
			case MIX:
				for (int k = 0; k < 3; ++k)
					INFO(i, j, k + 1) += fabs(dot(u[k], v)) / sqrt(dot(conf->v_max, conf->v_max));
				break;
			case RGB:
				INFO(i, j, 1) += MAX(0, v[0] / conf->v_max[0]);
				INFO(i, j, 2 + LIGHT) += MAX(0, -v[0] / conf->v_max[0]);
				INFO(i, j, 3 - LIGHT) += fabs(v[1]) / conf->v_max[1];
				break;
			default:
				break;
		}
	}
	double DENSITY = (double)ITERATIONS / count;

	for (int i = 0; i < D_HEIGHT; ++i)
		for (int j = 0; j < D_WIDTH; ++j) {
			if (INFO(i, j, 0) == 0)
				continue;
			double v = INTENSITY / DENSITY * INFO(i, j, 0) / 0xff;
			v = MIN(1, conf->colour == HSV ? v * 2 : v);
			v = sqrt(v);
			switch (conf->colour) {
				case HSV:
				case HSL:
				{
					double h = 180 + atan2(INFO(i, j, 1), INFO(i, j, 2)) * 180 / M_PI;
					double s = mag((double *)&INFO(i, j, 1)) / INFO(i, j, 0);
					double rgb[3];
					(conf->colour == HSV ? hsv_to_rgb : hsl_to_rgb)(h, s, v, rgb);
					if (LIGHT) inv(rgb);
					rgb1_to_rgb256(rgb, &BIG_BUF(i, j, 0));
					break;
				}
				case BW:
				{
					for (int k = 0; k < 3; ++k)
						BIG_BUF(i, j, k) = (char)((LIGHT ? 1 - v : v) * 0xff);
					break;
				}
				case MIX:
				case RGB:
				{
					double rgb[3];
					for (int k = 0; k < 3; ++k)
						rgb[k] = MIN(1, INFO(i, j, k + 1) * INTENSITY / DENSITY/ 0xff);
					set_brightness(v, rgb, rgb);
					if (LIGHT) inv(rgb);
					rgb1_to_rgb256(rgb, &BIG_BUF(i, j, 0));
					break;
				}
				// gradient map
				default:
				{
					double g = (LIGHT ? 1 - v : v) * (GN - 1);
					double t = fmod(g, 1);
					int l = (int)floor(g), r = (int)ceil(g);
					double rgb[3];
					lerp(rgb, gradients[conf->colour][l], gradients[conf->colour][r], t);
					for (int k = 0; k < 3; ++k)
						BIG_BUF(i, j, k) = (char)rgb[k];
					break;
				}
			}
		}

	if (DOWNSCALE >  1) {
		stbir_resize_uint8_srgb((unsigned char *)big_buf, D_WIDTH, D_HEIGHT, D_WIDTH * sizeof(char) * 3,
		                        (unsigned char *)buf, WIDTH, HEIGHT, WIDTH * sizeof(char) * 3,
		                        STBIR_RGB);
		free(big_buf);
	}
	free(info);

#if 0
	// write debug gradient map
	for (int j = 0; j < MIN(600, D_HEIGHT); ++j) {
		double v = (double)j / 600 * (GN - 1);
		double t = fmod(v, 1);
		int l = (int)floor(v);
		int r = (int)ceil(v);
		for (int i = 0; i < 20; ++i)
			for (int k = 0; k < 3; ++k)
				BUF(i, j, k) = (char)((1 - t) * gradients[conf->colour][l][k] + t * gradients[conf->colour][r][k]);

	}
#endif
}

static void progress(int i, int max, time_t elapsed)
{
	double speed = (double)i / elapsed;
	double eta = (max - i) / speed;
	printf("\r%d/%d, %.3f fps, eta %.3fs   ", i, max, speed, eta);
}

static int write_image(char *name, int width, int height, void *buf)
{
	int result = stbi_write_png(name, width, height, 3, buf, width * sizeof(char) * 3);
	if (!result)
		printf("failed to write %s\n", name);
	else
		printf("wrote %s\n", name);
	return result;
}

struct write_samples_arg {
	struct work_queue_info thread_info;
	struct config *config_array;
	int n;
	int w, h;
	char *buf;
	time_t start;
};


static void write_samples_callback(void *arg_)
{
	struct write_samples_arg *arg = (struct write_samples_arg *)arg_;
	unsigned char *buf = malloc(sizeof(char) * HEIGHT * WIDTH * 3);

	while (arg->thread_info.next_entry < arg->thread_info.entry_count) {
		int s = interlocked_increment((long *)&arg->thread_info.next_entry) - 1;
		if (s >= arg->thread_info.next_entry)
			break;

		int i = s / arg->n;
		int j = s % arg->n;
		render_image(&arg->config_array[s], buf);
		for (int k = 0; k < HEIGHT; ++k)
#define SBUF(i, j, k) arg->buf[(i) * arg->w * 3 + (j) * 3 + (k)]
			memcpy(&SBUF(i * HEIGHT + k, j * WIDTH, 0), &BUF(k, 0, 0), sizeof(char) * WIDTH * 3);

		int b = interlocked_increment((long *)&arg->thread_info.back);
		progress(b, arg->thread_info.entry_count, time(NULL) - arg->start);
	}

	free(buf);
}

static int write_samples(char name[], struct config *config_array, int samples)
{
	int n = (int)ceil(sqrt((double)samples));
	int w = WIDTH * n;
	int h = HEIGHT * n;
	char *buf = (char *)malloc(sizeof(char) * w * h * 3);
	memset(buf, 0x7f, sizeof(char) * w * h * 3);

	char txt[256];
	snprintf(txt, 256, "%s%s.txt", OUT_DIR, name);
	FILE *f = fopen(txt, "w");
	for (int s = 0; s < samples; ++s) {
		char params[256];
		str_c(config_array[s].c, params);
		fprintf(f, "%s # %d\n", params, 1 + s);
	}
	fclose(f);

	struct write_samples_arg arg = {0};
	arg.thread_info.entry_count = samples;
	arg.config_array = config_array;
	arg.n = n;
	arg.w = w;
	arg.h = h;
	arg.buf = buf;
	arg.start = time(NULL);
	run_jobs(write_samples_callback, (void *)&arg);
	putchar('\n');

	char png[256];
	snprintf(png, 256, "%s%s.png", OUT_DIR, name);
	int result = write_image(png, w, h, buf);
	free(buf);
	return result;
}

static void write_attractor(char *name, struct config *conf)
{
	unsigned char *buf = malloc(sizeof(char) * HEIGHT * WIDTH * 3);
	render_image(conf, buf);
	write_image(name, WIDTH, HEIGHT, buf);
	free(buf);
}

struct write_attractors_arg {
	struct work_queue_info thread_info;
	struct config *config_array;
};

static void write_attractors_callback(void *arg_)
{
	struct write_attractors_arg *arg = (struct write_attractors_arg *)arg_;
	unsigned char *buf = malloc(sizeof(char) * HEIGHT * WIDTH * 3);

	while (arg->thread_info.next_entry < arg->thread_info.entry_count) {
		int s = interlocked_increment((long *)&arg->thread_info.next_entry) - 1;
		if (s >= arg->thread_info.next_entry)
			break;

		char name[256];
		snprintf(name, 256, "%s%d.png", OUT_DIR, s);
		render_image(&arg->config_array[s], buf);
		write_image(name, WIDTH, HEIGHT, buf);
	}

	free(buf);
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
	putchar('\n');

	write_samples("samples", config_array, samples);
	free(config_array);
}

static void set_video_params(const char *params, struct config *config_array, int frames)
{
	double range = END - START;
	double dt = range / frames;

	vec x_max;
	vec x_min;
	for (int i = 0; i < 2; ++i) {
		x_min[i] = 1e10;
		x_max[i] = -1e10;
	}

	set_config(&config_array[0], params);
	config_array[0].c[CJ][CI] += START;
	for (int i = 1; i < frames; ++i) {
		memcpy(&config_array[i], &config_array[0], sizeof(struct config));
		config_array[i].c[CJ][CI] += dt * i;
		attractor(&config_array[i]);

		for (int j = 0; j < 2; ++j) {
			x_max[j] = MAX(x_max[j], config_array[i].x_max[j]);
			x_min[j] = MIN(x_min[j], config_array[i].x_min[j]);
		}
	}

	for (int i = 0; i < frames; ++i)
		for (int j = 0; j < 2; ++j) {
			config_array[i].x_max[j] = x_max[j];
			config_array[i].x_min[j] = x_min[j];
		}
}

static void video_preview(const char *params, int samples)
{
	struct config *config_array = malloc(sizeof(struct config) * samples);
	set_video_params(params, config_array, samples);
	write_samples("preview", config_array, samples);
	free(config_array);
}

struct write_video_arg {
	struct work_queue_info thread_info;
	struct config *config_array;
	time_t start;
	FILE *pipe;
	event_handle event;
};

static void write_video_callback(void *arg_)
{
	struct write_video_arg *arg = (struct write_video_arg *)arg_;
	unsigned char *buf = malloc(sizeof(char) * HEIGHT * WIDTH * 3);

	while (arg->thread_info.next_entry < arg->thread_info.entry_count) {
		int s = interlocked_increment((long *)&arg->thread_info.next_entry) - 1;
		if (s >= arg->thread_info.next_entry)
			break;

		render_image(&arg->config_array[s], buf);

		// wait until it's out to to write the image
		while (s != arg->thread_info.back)
			wait_for_event(arg->event);
		reset_event(arg->event);
		fwrite(buf, sizeof(char) * HEIGHT * WIDTH * 3, 1, arg->pipe);
		++arg->thread_info.back;
		progress(arg->thread_info.back, arg->thread_info.entry_count, time(NULL) - arg->start);

		// notify other threads when we're done
		set_event(arg->event);
	}

	free(buf);
}

static void write_video(const char *params, int frames)
{
	struct config *config_array = malloc(sizeof(struct config) * frames);
	set_video_params(params, config_array, frames);

	char *LOSSY_OPTS = "-crf 17 -pix_fmt yuv420p";
	char *LOSSLESS_OPTS = "-qp 0 -preset veryslow";
	char buf[256];
	snprintf(buf, 256,
	         "ffmpeg -loglevel error -y -f rawvideo -pix_fmt rgb24 -s %dx%d -r %d -i - "
	         " -c:v libx264 %s %sout.mp4", WIDTH, HEIGHT, FPS, LOSSLESS ? LOSSLESS_OPTS : LOSSY_OPTS, OUT_DIR);
#ifdef _WIN64
	char *mode = "wb";
#else
	char *mode = "w";
#endif
	FILE *pipe = popen(buf, mode);

	struct write_video_arg arg = {0};
	arg.thread_info.entry_count = frames;
	arg.config_array = config_array;
	arg.pipe = pipe;
	arg.start = time(NULL);
	arg.event = create_event();
	run_jobs(write_video_callback, (void *)&arg);
	putchar('\n');
	printf("rendered in %lld seconds\n", (long long)(time(NULL) - arg.start));

	pclose(pipe);
	printf("wrote %sout.mp4", OUT_DIR);

	// write a thumbnail
	struct config conf;
	set_config(&conf, params);
	char name[256];
	snprintf(name, 256, "%sthumbnail.png", OUT_DIR);
	write_attractor(name, &conf);

	free(config_array);
}

static void video_params(coef c)
{
	puts("finding video parameters");

	if (!is_set(OP_COEFFICIENT)) {
		set(OP_COEFFICIENT);
		CI = rand() % 2;
		CJ = rand() % CN;
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

static void load_config(struct config config_array[256], int *count)
{
	FILE *f = fopen(PARAMS, "r");
	if (f == NULL) {
		fprintf(stderr, "option error: -params could not open \"%s\"\n", PARAMS);
		exit(1);
	}

	if (*count == 0)  {
		while (!feof(f))
			*count += fgetc(f) == '\n';
		rewind(f);
	}

	char buf[256];
	for (int i = 0; i < *count && fgets(buf, 256, f); ++i) {
		int result = set_config(&config_array[i], buf);
		if (!result) {
			fprintf(stderr, "parse error when reading \"%s\", line %d:\n", PARAMS, i);
			fprintf(stderr, "%s", buf);
			fprintf(stderr, "%s should be a %s\n", PARAMS, options[OP_PARAMS].doc);
			exit(1);
		}
	}
}

int main(int argc, char **argv)
{
	for (int g = 0; g < NUM_GRADIENTS; ++g)
		for (int i = 0; i < GN; ++i) {
			double tmp[3];
			for (int k = 0; k < 3; ++k)
				tmp[k] = gradients[g][i][k] / 0xff;
			set_brightness((double)i / (GN - 1), tmp, tmp);
			for (int k = 0; k < 3; ++k)
				gradients[g][i][k] = tmp[k] * 0xff;
		}

	srand((unsigned int)time(NULL));

	// print help if no args
	if (argc <= 1) {
		help();
		exit(0);
	}

	// set number of threads
	if (!is_set(OP_THREADS)) {
		THREADS = platform_thread_count() - 1;
		options[OP_THREADS].set = true;
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
	for (int i = 2; i < argc; i += 2)
		parse_option(mode, argv[i], argv[i + 1]);
	ITERATIONS = WIDTH * HEIGHT * (unsigned long long)QUALITY;

	switch (mode) {
		case IMAGE:
			if (PREVIEW && PARAMS) {
				// TODO conflig resolution
				fprintf(stderr, "option error: -params is not compatible with -preview\n");
				exit(1);
			} else if (PREVIEW) {
				sample_attractor(PREVIEW);
			} else if (COLOUR_PREVIEW) {
				if (is_set(OP_COLOUR)) {
					// TODO conflig resolution
					fprintf(stderr, "option error: -colour_preview is not compatible with -preview\n");
					exit(1);
				}

				struct config config_array[COLOUR_COUNT];
				int count = 1;
				if (PARAMS)
					load_config(config_array, &count);
				else
					random_config(&config_array[0]), putchar('\n');
				for (int i = 1; i < COLOUR_COUNT; ++i) {
					memcpy(&config_array[i], &config_array[0], sizeof(struct config));
					config_array[i].colour = (enum colour_type)i;
				}
				config_array[0].colour = (enum colour_type)0;

				write_samples("colour_preview", config_array, COLOUR_COUNT);
			} else if (PARAMS) {
				struct config config_array[256];
				int count = 0;
				load_config(config_array, &count);
				write_attractors(config_array, count);
			} else {
				struct config conf;
				random_config(&conf); putchar('\n');
				char buf[256];
				str_c(conf.c, buf);
				char name[256];
				snprintf(name, 256, "%ssingle.png", OUT_DIR);
				write_attractor(name, &conf);
			}
			break;
		case VIDEO:
			struct config conf;
			int count = 1;
			if (PARAMS)
				load_config(&conf, &count);
			else
				random_config(&conf), putchar('\n');
			video_params(conf.c);
			char params[256];
			str_c(conf.c, params);
			if (PREVIEW)
				video_preview(params, PREVIEW);
			else
				write_video(params, DURATION * FPS);
			break;
	}

	// print the final configuration
	print_values(mode);
}
