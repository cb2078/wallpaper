enum option_mode {
	IMAGE = 1,
	VIDEO = 2,
};

enum option_name {
	OP_BORDER,
	OP_COEFFICIENT,
	OP_COLOUR,
	OP_DURATION,
	OP_END,
	OP_FPS,
	OP_HEIGHT,
	OP_INTENSITY,
	OP_PARAMS,
	OP_PREVIEW,
	OP_QUALITY,
	OP_START,
	OP_WIDTH,
};

struct option {
	char *str;
	enum option_mode mode;
	char type;
	union {
		int d;
		float f;
		char *c;
		char *s;
	} val;
	char *doc;
	enum option_name conflicts;
};

struct option options[] = {
	[OP_BORDER] = {
		.str = "border",
		.type = 'f',
		.val.f = 0.05f,
	},
	[OP_COEFFICIENT] = {
		.str = "coefficient",
		.mode = VIDEO,
		.type = 's',
		.doc = "coefficient to change during the video, must have regex \"[xy][0-5]\"",
	},
	[OP_COLOUR] = {
		.str = "colour",
		.type = 'c',
		.doc = "how to colour the attractor",
	},
	[OP_DURATION] = {
		.str = "duration",
		.mode = VIDEO,
		.type = 'd',
		.val.d = 40,
		.doc = "duration in seconds",
		.conflicts = OP_PREVIEW,
	},
	[OP_END] = {
		.str = "end",
		.mode = VIDEO,
		.type = 'f',
		.doc = "end value for coefficient",
	},
	[OP_FPS] = {
		.str = "fps",
		.mode = VIDEO,
		.type = 'd',
		.val.d = 24,
		.conflicts = OP_PREVIEW,
	},
	[OP_HEIGHT] = {
		.str = "height",
		.type = 'd',
		.val.d = 720,
	},
	[OP_INTENSITY] = {
		.str = "intensity",
		.type = 'f',
		.val.f = 50,
		.doc = "how bright the iterations make each pixel",
	},
	[OP_PARAMS] = {
		.str = "params",
		.type = 's',
		.doc = "file containing lines of 12 space separated floats",
	},
	[OP_PREVIEW] = {
		.str = "preview",
		.type = 'd',
		.doc = "show grid of some thumbnails",
	},
	[OP_QUALITY] = {
		.str = "quality",
		.type = 'd',
		.val.d = 25,
		.doc = "how many iterations to do per pixel",
	},
	[OP_START] = {
		.str = "start",
		.mode = VIDEO,
		.type = 'f',
		.doc = "start value for coefficient",
	},
	[OP_WIDTH] = {
		.str = "width",
		.type = 'd',
		.val.d = 1280,
	},
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
		c += printf("[--%s %s] ", options[i].str, type_str(options[i].type));
	}
	printf("[options]\n");
}

static bool has_conflicts(struct option *o)
{
	return o->conflicts != 0;
}

static bool has_doc(struct option *o)
{
	return o->doc != NULL;
}

static bool has_default(struct option *o)
{
	switch (o->type) {
		case 'd':
			return 0 != o->val.d;
		case 'f':
			return 0 != o->val.f;
		case 'c':
			return NULL != o->val.c;
		case 's':
			return NULL != o->val.s;
	}
	exit(1);
}

static void val_str(struct option *o, char buf[256])
{
	switch (o->type) {
		case 'd':
			snprintf(buf, 256, "%d", o->val.d);
			return;
		case 'f':
			snprintf(buf, 256, "%.2f", o->val.f);
			return;
		case 'c':
			snprintf(buf, 256, "%s", o->val.c);
			return;
		case 's':
			snprintf(buf, 256, "%s", o->val.s);
			return;
	}
	exit(1);
}

static void help(void)
{
	int indent = 0;
	for (int i = 0; i < LENGTH(options); ++i) {
		char buf[256];
		int c = snprintf(buf, 256, "  --%s %s", options[i].str, type_str(options[i].type));
		indent = MAX(c, indent);
	}

	printf("usage\n");
	help_mode("attractor image", IMAGE);
	help_mode("attractor video", VIDEO);
	printf("\noptions\n");
	for (int i = 0; i < LENGTH(options); ++i) {
		int c = printf("  --%s %s", options[i].str, type_str(options[i].type));
		printf("%*s  %s", indent - c, "", has_doc(&options[i]) ? options[i].doc : "");
		if (has_default(&options[i])) {
			if (has_doc(&options[i]))
				printf(", ");
			char buf[256];
			val_str(&options[i], buf);
			printf("default: %s", buf);
		}
		if (has_conflicts(&options[i])) {
			if (has_default(&options[i]) || has_doc(&options[i]))
				printf(", ");
			printf("conficts with \"--%s\"", options[options[i].conflicts].str);
		}
		putchar('\n');
	}
}

static int parse_flag(char *flag)
{
	if (strlen(flag) < 2 || flag[0] != '-' && flag[1] != '-')
		goto err;

	for (int i = 0; i < LENGTH(options); ++i)
		if (0 == strcmp(options[i].str, flag + 2))
			return i;

err:
	fprintf(stderr, "unknown flag: %s\n", flag);
	exit(1);
}

static enum colour_type parse_colour(char *s)
{
	static struct {
		enum colour_type key;
		char *val;
	} table[] = {
		{BW, "BW"},
		{HSV, "HSV"},
		{RGB, "RGB"},
	};

	for (int i = 0; i < LENGTH(table); ++i)
		if (0 == strcmp(table[i].val, s))
			return table[i].key;

	fprintf(stderr, "value error: --colour expected %s, got %s\n", type_str('c'), s);
	exit(1);
}

static void nyi(char *flag)
{
	fprintf(stderr, "%s not yet implemented\n", flag);
	exit(1);
}

static void parse_option(int mode, char *flag, char *val)
{
		int o = parse_flag(flag);

		// check mode is valid
		if (options[o].mode && options[o].mode != mode) {
			fprintf(stderr, "%s not allowed in %s mode\n", flag, mode == VIDEO ? "video" : "image");
			exit(1);
		}

		char *format = NULL;
		switch (options[o].type) {
			case 'd':
				format = "%d";
				break;
			case 'f':
				format = "%lf";
				break;
			case 'c':
			case 's':
				format = "%s";
				break;
			default:
				exit(1);
				break;
		}

		// set the option
		switch (o) {
			// TODO sscanf error
#define CASE(x) case OP_##x: sscanf(val, format, &x); break
			CASE(BORDER);
			CASE(DURATION);
			CASE(END);
			CASE(FPS);
			CASE(HEIGHT);
			CASE(INTENSITY);
			CASE(PREVIEW);
			CASE(QUALITY);
			CASE(START);
			CASE(WIDTH);
#undef CASE
			case OP_COLOUR:
				COLOUR = parse_colour(val);
				break;
			case OP_COEFFICIENT:
			{
				char c;
				int d;
				int result = sscanf(val, "%c%1d", &c, &d);
				if (result < 0) {
					fprintf(stderr, "%s expected regex \"[xy][0-5]\", got %s\n", flag, val);
					exit(1);
				}
				CI = c == 'x' ? 0 : 1;
				CJ = d;
			} break;
			case OP_PARAMS:
				nyi(flag);
				break;
		}
}
