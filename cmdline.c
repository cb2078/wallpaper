enum option_mode {
	IMAGE = 1,
	VIDEO = 2,
};

struct option {
	char *name;
	enum option_mode mode;
	char type;
	union {
		int d;
		float f;
		char *c;
		char *s;
	} val;
	char *doc;
};

struct option options[] = {
	{ .name = "border", .type = 'f', .val.f = 0.05, },
	{ .name = "coefficient", .mode = VIDEO, .type = 's', .doc = "coefficient to change during the video, must have regex \"[xy][0-5]\"", },
	{ .name = "colour", .type = 'c', .doc = "how to colour the attractor"},
	{ .name = "duration", .mode = VIDEO, .type = 'd', .val.d = 40, .doc = "duration in seconds"},
	{ .name = "end", .mode = VIDEO, .type = 'f', .doc = "end value for coefficient," },
	{ .name = "fps", .mode = VIDEO, .type = 'd', .val.d = 24, },
	{ .name = "height", .type = 'd', .val.d = 720, },
	{ .name = "intensity", .type = 'f', .val.f = 50, .doc = "how bright the iterations make the pixel"},
	{ .name = "params", .type = 's', .doc = "file containing lines of 12 space separated floats"},
	{ .name = "preview", .type = 'd', .doc = "show grid of some thumbnails" },
	{ .name = "quality", .type = 'd', .val.d = 25, .doc = "how many iterations to do per pixel" },
	{ .name = "start", .mode = VIDEO, .type = 'f', .doc = "start value for coefficient," },
	{ .name = "width", .type = 'd', .val.d = 1280, },
};

static char *type_str(char t)
{
	static char *names[] = {
		['d'] = "<int>",
		['f'] = "<float>",
		['c'] = "(BW|WB|HSV|RGB)",
		['s'] = "<string>",
	};
	return names[t];
}

static void help_mode(char *name, enum option_mode mode)
{
	int c = printf("  %s ", name);
	for (int i = 0; i < LENGTH(options); ++i) {
		if (options[i].mode != mode)
			continue;
		if (c > 80) {
			printf("\n");
			c = printf("    ");
		}
		c += printf("[--%s %s] ", options[i].name, type_str(options[i].type));
	}
	printf("[options]\n");
}

static void help(void)
{
	int indent = 0;
	for (int i = 0; i < LENGTH(options); ++i) {
		char buf[256];
		int c = snprintf(buf, 256, "  --%s %s", options[i].name, type_str(options[i].type));
		indent = MAX(c, indent);
	}
	printf("%d\n", indent);

	printf("usage\n");
	help_mode("image", IMAGE);
	help_mode("video", VIDEO);
	printf("\noptions\n");
	for (int i = 0; i < LENGTH(options); ++i) {
		int c = printf("  --%s %s", options[i].name, type_str(options[i].type));
		printf("%*s  %s\n", indent - c, "", options[i].doc ? options[i].doc : "");
	}
}
