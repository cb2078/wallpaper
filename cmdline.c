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

enum option_type {
	TY_INT,
	TY_DOUBLE,
	TY_ENUM,
	TY_STRING,
};

struct option {
	char *str;
	enum option_mode mode;
	enum option_type type;
	union {
		int d;
		double f;
		enum colour_type c;
		char *s;
	} val;
	char *doc;
	enum option_name conflicts;
	bool set;
};

struct option options[] = {
	[OP_BORDER] = {
		.str = "border",
		.type = TY_DOUBLE,
		.val.f = 0.05f,
	},
	[OP_COEFFICIENT] = {
		.str = "coefficient",
		.mode = VIDEO,
		.type = TY_STRING,
		.doc = "coefficient to change during the video, must have regex \"[xy][0-5]\"",
	},
	[OP_COLOUR] = {
		.str = "colour",
		.type = TY_ENUM,
		.doc = "how to colour the attractor",
	},
	[OP_DURATION] = {
		.str = "duration",
		.mode = VIDEO,
		.type = TY_INT,
		.val.d = 40,
		.doc = "duration in seconds",
		.conflicts = OP_PREVIEW,
	},
	[OP_END] = {
		.str = "end",
		.mode = VIDEO,
		.type = TY_DOUBLE,
		.doc = "end value for coefficient",
	},
	[OP_FPS] = {
		.str = "fps",
		.mode = VIDEO,
		.type = TY_INT,
		.val.d = 24,
		.conflicts = OP_PREVIEW,
	},
	[OP_HEIGHT] = {
		.str = "height",
		.type = TY_INT,
		.val.d = 720,
	},
	[OP_INTENSITY] = {
		.str = "intensity",
		.type = TY_DOUBLE,
		.val.f = 50,
		.doc = "how bright the iterations make each pixel",
	},
	[OP_PARAMS] = {
		.str = "params",
		.type = TY_STRING,
		.doc = "file containing lines of 12 space separated floats",
		.conflicts = OP_PREVIEW,
	},
	[OP_PREVIEW] = {
		.str = "preview",
		.type = TY_INT,
		.doc = "show grid of some thumbnails",
	},
	[OP_QUALITY] = {
		.str = "quality",
		.type = TY_INT,
		.val.d = 25,
		.doc = "how many iterations to do per pixel",
	},
	[OP_START] = {
		.str = "start",
		.mode = VIDEO,
		.type = TY_DOUBLE,
		.doc = "start value for coefficient",
	},
	[OP_WIDTH] = {
		.str = "width",
		.type = TY_INT,
		.val.d = 1280,
	},
};

#define BORDER      options[OP_BORDER].val.f
int CI, CJ;
#define COLOUR      options[OP_COLOUR].val.c
#define DURATION    options[OP_DURATION].val.d
#define END         options[OP_END].val.f
#define FPS         options[OP_FPS].val.d
#define HEIGHT      options[OP_HEIGHT].val.d
#define INTENSITY   options[OP_INTENSITY].val.f
#define PARAMS      options[OP_PARAMS].val.s
#define PREVIEW     options[OP_PREVIEW].val.d
#define QUALITY     options[OP_QUALITY].val.d
#define START       options[OP_START].val.f
#define WIDTH       options[OP_WIDTH].val.d

static char *type_str(enum option_type type)
{
	switch (type) {
		case TY_INT:
			return "<int>";
		case TY_DOUBLE:
			return "<float>";
		case TY_ENUM:
			return  "(BW|WB|HSV|RGB|MIX)";
		case TY_STRING:
			return "<string>";
	}
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
		case TY_INT:
			return 0 != o->val.d;
		case TY_DOUBLE:
			return 0 != o->val.f;
		case TY_ENUM:
			return true;
		case TY_STRING:
			return NULL != o->val.s;
	}
	exit(1);
}

static void val_str(struct option *o, char buf[256])
{
	switch (o->type) {
		case TY_INT:
			snprintf(buf, 256, "%d", o->val.d);
			return;
		case TY_DOUBLE:
			snprintf(buf, 256, "%.2f", o->val.f);
			return;
		case TY_ENUM:
			strncpy(buf, colour_map[o->val.c], 256);
			return;
		case TY_STRING:
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

static void option_error(char *flag, char *expected, char *val)
{
	fprintf(stderr, "option error: %s expected %s, got %s\n", flag, expected, val);
	exit(1);
}

static void option_type_error(char *flag, char type, char *val)
{
	option_error(flag, type_str(type), val);
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
			case TY_INT:
				format = "%d";
				break;
			case TY_DOUBLE:
				format = "%lf";
				break;
			case TY_ENUM:
			case TY_STRING:
				break;
		}
		// set the option
		switch (o) {
#define CASE(x) \
	case OP_##x: \
	{ \
		int result = sscanf(val, format, &x); \
		if (result < 1) \
			option_type_error(flag, options[o].type, val); \
	} break;
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
			{
				int i;
				for (i = 0; i < LENGTH(colour_map); ++i)
					if (0 == strcmp(colour_map[i], val))
						break;
				if (i == LENGTH(colour_map))
					option_type_error(flag, options[o].type, val);
				COLOUR = (enum colour_type)i;
			} break;
			case OP_COEFFICIENT:
			{
				char c;
				int d;
				int result = sscanf(val, "%c%d", &c, &d);
				if (result < 2 || (c != 'x' && c != 'y') || (d < 0 || d >= 6))
					option_error(flag, "regex [xy][0-5]", val);
				CI = c == 'x' ? 0 : 1;
				CJ = d;
			} break;
			case OP_PARAMS:
				PARAMS = val;
				break;
		}

		// finally mark the option as set
		options[o].set = true;
}

static void print_values(int mode)
{
	printf("using options");
	for (int i = 0; i < LENGTH(options); ++i) {
		if (!options[i].set && !has_default(&options[i]))
			continue;
		if (options[i].mode && options[i].mode != mode)
			continue;
		char buf[256];
		val_str(&options[i], buf);
		printf(" --%s %s", options[i].str, buf);
	}
	putchar('\n');
}

static bool is_set(enum option_name o)
{
	return options[o].set;
}
