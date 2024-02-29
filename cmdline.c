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
	{ .name = "colour", .type = 'c', },
	{ .name = "duration", .mode = VIDEO, .type = 'd', .val.d = 40, },
	{ .name = "end", .mode = VIDEO, .type = 'f', .doc = "end value for coefficient," },
	{ .name = "fps", .mode = VIDEO, .type = 'd', .val.d = 24, },
	{ .name = "height", .type = 'd', .val.d = 720, },
	{ .name = "intensity", .type = 'f', .val.f = 50, .doc = "how bright the iterations make the pixel"},
	{ .name = "params", .type = 's', .doc = "file containing a set of 12 parameters on each line,\n"
	                                        "parameters separated floats formatted with \"% 1.3f\""},
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
	printf("usage\n");
	help_mode("image", IMAGE);
	help_mode("video", VIDEO);
	printf("\noptions\n");
	// TODO indentation
	for (int i = 0; i < LENGTH(options); ++i)
		printf("  --%s %s\t%s\n", options[i].name, type_str(options[i].type), options[i].doc ? options[i].doc : "");
}
