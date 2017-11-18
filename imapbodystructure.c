/*
 * imapbodystructure.c
 *
 * pretty-print an imap bodystructure
 *
 * imapbodystructure [-i num] [-l num] [-h] [file]
 *	-i num		indent by num characters
 *	-l num		cut lines at num characters
 *	-p 		print the progressive sequence of each part
 *	-c		also print the progressive sequence of combinations
 *	-h		help
 *	file		if omitted or '-', read from stdin
 */

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>

/*
 * output format
 */
int indent = 4;		/* indent at the begin of line: -i num */
int maxline = 0;	/* maximal length of line printed: -l num */
int progressive = -1;	/* print progressive sequences from this nest level */
int combinationsequence = 0;/* print also the sequence number of combinations */

/*
 * parsing and printing status
 */
#define MAXLEVEL 100
struct bodystructureparse {
	int level;		/* nesting level */
	int progressive[MAXLEVEL]; /* progressive number for each level */
	int single;		/* level of nesting where stopped scanning a
				   combination list, or -1 */
	int attribute[MAXLEVEL]; /* reached attributes of a combination */
	int separator;		/* previous character was ')' or ' ' */
	int quote;		/* within quotes "..." */
	int escaped;		/* backslash in quotes, like "...\ */
	int linepos;		/* position in the line */
	int startlist;		/* true for the first character of a list */
	int newline;		/* previous character printed was a newline
				 * (to avoid multiple consecutive newlines) */
};

/*
 * print a regular character
 */
void bodystructureprintchar(FILE *out, char c, struct bodystructureparse *bsp) {
	if (bsp->linepos == maxline && maxline != 0)
		fputs("...", out);
	else if (bsp->linepos < maxline || maxline == 0)
		fputc(c, out);
	bsp->linepos++;
	bsp->newline = 0;
}

/*
 * print a newline and the following indent
 */
void bodystructureprintnewline(FILE *out, struct bodystructureparse *bsp) {
	int i;
	if (bsp->single != -1)
		return;
	if (bsp->newline)
		return;
	fputc('\n', out);
	for (i = 0; i < bsp->level; i++)
		fwrite("                             ", indent, 1, out);
	bsp->linepos = indent * bsp->level;
	bsp->newline = 1;
}

/*
 * print the sequence of progressive numbers
 */
void bodystructureprintprogressive(FILE *out, struct bodystructureparse *bsp) {
	int i;

	if (progressive == -1 || 
	    bsp->single != -1 ||
	    bsp->level < progressive ||
	    bsp->attribute[bsp->level])
		return;

	fprintf(out, "[");
	for (i = progressive; i <= bsp->level; i++)
		fprintf(out, "%s%d",
		        i == progressive ? "" : ".",
		        bsp->progressive[i]);
	fprintf(out, "] ");
}

/*
 * initialize parsing
 */
void bodystructureinit(struct bodystructureparse *bsp) {
	bsp->level = 0;
	bsp->progressive[bsp->level] = 0;
	bsp->single = -1;
	bsp->attribute[bsp->level] = 0;
	bsp->newline = 0;
	bsp->quote = 0;
	bsp->escaped = 0;
	bsp->startlist = 0;
	bsp->linepos = 0;
}

/*
 * new nesting level
 */
void bodystructurenewlevel(struct bodystructureparse *bsp) {
	bsp->level++;
	if (bsp->level >= MAXLEVEL) {
		fprintf(stderr, "too many levels\n");
		exit(EXIT_FAILURE);
	}
	bsp->progressive[bsp->level] = 0;
	bsp->attribute[bsp->level] = 0;
}

/*
 * formatted print of a body structure
 */
void bodystructureprint(FILE *out, char *bodystructure) {
	struct bodystructureparse bsp;
	char c, *pos;

				/* initialize parsing */

	bodystructureinit(&bsp);

				/* process each character */

	for (pos = bodystructure; *pos != '\0'; pos++) {
		c = *pos;

					/* quoted string */

		if (bsp.quote) {
			if (c == '\\') {
				bsp.escaped = 1;
				continue;
			}
			bodystructureprintchar(out, c, &bsp);
			if (c == '"' && ! bsp.escaped)
				bsp.quote = 0;
			bsp.escaped = 0;
			continue;
		}

					/* start of a list */

		if (bsp.startlist) {
			bsp.progressive[bsp.level]++;

			bodystructureprintnewline(out, &bsp);
			if (combinationsequence || c == '"')
				bodystructureprintprogressive(out, &bsp);
			bodystructureprintchar(out, '(', &bsp);

			bodystructurenewlevel(&bsp);
			if (bsp.single == -1 && c == '"')
				bsp.single = bsp.level;
			bodystructureprintnewline(out, &bsp);
		}

				/* character */

		if (c == '(') {
			bsp.startlist = 1;
			continue;
		}
		else
			bsp.startlist = 0;

		if (c == ')') {
			bsp.level--;
			bodystructureprintnewline(out, &bsp);
			bodystructureprintchar(out, c, &bsp);
			if (bsp.level < bsp.single)
				bsp.single = -1;
			bsp.separator = 1;
			continue;
		}

		if (c == '"') {
			bsp.quote = 1;
			bsp.attribute[bsp.level] = 1;
		}

		if (c == ' ') {
			bsp.separator = 1;
			if (bsp.single == -1) {
				bodystructureprintnewline(out, &bsp);
				continue;
			}
		}
		else if (bsp.separator) {
			bsp.separator = 0;
			bsp.progressive[bsp.level]++;
		}

		bodystructureprintchar(out, c, &bsp);
	}
}

/*
 * usage
 */
void usage() {
	printf("format an IMAP bodystructure string\n");
	printf("usage:\n\timapbodystructure [-i num] [-l num] [-p [-c]] ");
	printf("[-h] [file]\n");
	printf("\t\t-i num\tindent by num characters\n");
	printf("\t\t-l num\tcut lines at num characters\n");
	printf("\t\t-p\tprint the progressive sequence of each part\n");
	printf("\t\t-c\talso print the progressive sequence of combinations\n");
	printf("\t\t-h\tthis help\n");
	printf("\t\tfile\tif omitted or '-', read stdin\n");

}

/*
 * main
 */
int main(int argn, char *argv[]) {
	FILE *in, *out;
	char line[5000];
	int opt;

				/* arguments */

	while (-1 != (opt = getopt(argn, argv, "i:l:pch"))) {
		switch(opt) {
		case 'l':
			maxline = atoi(optarg);
			break;
		case 'i':
			indent = atoi(optarg);
			if (indent > 20) {
				printf("maximal indent is 20\n");
				exit(EXIT_FAILURE);
			}
			break;
		case 'p':
			progressive = 2;
			break;
		case 'c':
			combinationsequence = 1;
			break;
		case 'h':
			usage();
			exit(EXIT_SUCCESS);
		default:
			usage();
			exit(EXIT_FAILURE);
		}
	}
	argn -= optind - 1;
	argv += optind - 1;
	if (argn - 1 < 1 || ! strcmp(argv[1], "-"))
		in = stdin;
	else {
		in = fopen(argv[1], "r");
		if (in == NULL) {
			perror(argv[1]);
			exit(EXIT_FAILURE);
		}
	}
	out = stdout;

				/* print each line as a body structure */

	while (fgets(line, 5000, in)) {
		fprintf(out, "============================================\n");
		fprintf(out, "%s", line);
		fprintf(out, "--------------------------------------------\n");

		bodystructureprint(out, line);
	}

	return EXIT_SUCCESS;
}
