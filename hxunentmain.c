/* unent -- expand HTML entities
 *
 * Author: Bert Bos
 * Created: 10 Aug 2008
 */

#include "config.h"
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <ctype.h>
#include "export.h"
#include "unent.e"

static int leave_builtin = 0;	/* Leave standard entities untouched */
static int fix_ampersands = 0;	/* Replace lone and unrecognized & by &amp; */

/* append_utf8 -- append the UTF-8 sequence for code n */
static void append_utf8(const int n)
{
  if (n <= 0x7F) {
    putchar(n);
  } else if (n <= 0x7FF) {
    putchar(0xC0 | (n >> 6));
    putchar(0x80 | (n & 0x3F));
  } else if (n <= 0xFFFF) {
    putchar(0xE0 | (n >> 12));
    putchar(0x80 | ((n >> 6) & 0x3F));
    putchar(0x80 | (n & 0x3F));
  } else if (n <= 0x1FFFFF) {
    putchar(0xF0 | (n >> 18));
    putchar(0x80 | ((n >> 12) & 0x3F));
    putchar(0x80 | ((n >> 6) & 0x3F));
    putchar(0x80 | (n & 0x3F));
  } else if (n <= 0x3FFFFFF) {
    putchar(0xF0 | (n >> 24));
    putchar(0x80 | ((n >> 18) & 0x3F));
    putchar(0x80 | ((n >> 12) & 0x3F));
    putchar(0x80 | ((n >> 6) & 0x3F));
    putchar(0x80 | (n & 0x3F));
  } else {
    putchar(0xF0 | (n >> 30));
    putchar(0x80 | ((n >> 24) & 0x3F));
    putchar(0x80 | ((n >> 18) & 0x3F));
    putchar(0x80 | ((n >> 12) & 0x3F));
    putchar(0x80 | ((n >> 6) & 0x3F));
    putchar(0x80 | (n & 0x3F));
  }
}

/* expand -- print string, expanding entities to UTF-8 sequences */
static void expand(const char *s)
{
  const struct _Entity *e;
  int i, n;

  for (i = 0; s[i];) {
    if (s[i] != '&') {				/* Literal character */
      putchar(s[i++]);
    } else if (s[i+1] != '#') {			/* Named entity, eg. &eacute */
      for (i++, n = 0; isalnum(s[i+n]); n++) ;
      if (! (e = lookup_entity(s + i, n))) {	/* Unknown entity */
	if (fix_ampersands) fputs("&amp;", stdout); else putchar('&');
      } else if (leave_builtin && (e->code == 38 || e->code == 39
 	  || e->code == 60 || e->code == 62 || e->code == 34)) { /* Keep it */
	putchar('&');
	for (; isalnum(s[i]); i++) putchar(s[i]);
	if (s[i] != ';') putchar(';');		/* Make sure the ; is there */
      } else {					/* Expand to UTF-8 */
	append_utf8(e->code);
	i += n;
	if (s[i] == ';') i++;
      }
    } else if (s[i+2] != 'x') {			/* Decimal entity, eg. &#70 */
      for (n = 0, i += 2; isdigit(s[i]); i++) n = 10 * n + s[i] - '0';
      if (leave_builtin && (n == 38 || n == 60 || n == 62 || n == 34))
        printf("&#%d;", n);
      else
        append_utf8(n);
      if (s[i] == ';') i++;
    } else {					/* Hex entity, eg. &#x5F */
      for (n = 0, i += 3; isxdigit(s[i]); i++)
	if (isdigit(s[i])) n = 16 * n + s[i] - '0';
	else n = 16 * n + toupper(s[i]) - 'A' + 10;
      if (leave_builtin && (n == 38 || n == 60 || n == 62 || n == 34))
        printf("&#x%x;", n);
      else
        append_utf8(n);
      if (s[i] == ';') i++;
    }
  }
  /* SGML says also that a record-end (i.e., an end-of-line) may be
   * used instead of a semicolon to end an entity reference. But the
   * record-end is not suppressed in HTML and such an entity reference
   * is invalid in XML, so we don't implement that rule here. Instead,
   * the end-of-line is treated as any other character (other than
   * semicolon) and left in the document.
   */
}

static void usage(const char *prog)
#if __GNUC__ > 2 || __GNUC__ == 2 && __GNUC_MINOR__ >= 5
  __attribute__((__noreturn__))
#endif
;

/* usage -- print usage message and exit */
static void usage(const char *prog)
{
  fprintf(stderr, "Version %s\nUsage: %s [-b] [-f] [file]\n", VERSION, prog);
  exit(2);
}

/* main -- read input, expand entities, write out again */
int main(int argc, char *argv[])
{
  char buf[4096];
  FILE *infile;
  int c;

  while ((c = getopt(argc, argv, "bf")) != -1)
    switch (c) {
    case 'b': leave_builtin = 1; break;
    case 'f': fix_ampersands = 1; break;
    default: usage(argv[0]);
    }
  if (optind == argc) infile = stdin;
  else if (optind == argc - 1) infile = fopen(argv[optind], "r");
  else usage(argv[0]);
  if (infile == NULL) {perror(argv[optind]); exit(1);}

  while (fgets(buf, sizeof(buf), infile)) expand(buf);

  fclose(infile);
  return 0;
}
