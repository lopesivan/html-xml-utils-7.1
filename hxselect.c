/*
 * select -- extract elements matching a selector
 *
 * Assumes that class selectors (".foo") refer to an attribute called
 * "class".
 *
 * Assumes that ID selectors ("#foo") refer to an attribute called
 * "id".
 *
 * Options:
 *
 * -l language
 *
 *     Sets the default language, in case the root element doesn't
 *     have an xml: lang attribute. Example: -l en. Default: none.
 *
 * -s separator
 *
 *     A string to print after each match. Accepts C-like escapes.
 *     Example: -s '\n\n' to print an empty line after each match.
 *     Default: empty string.
 *
 * -i
 *
 *     Match case-insensitively. Useful for HTML and some other
 *     SGML-based languages.
 *
 * -c
 *
 *     Print content only. Without -c, the start and end tag of the
 *     matched element are printed as well; with -c only the contents
 *     of the matched element are printed.
 *
 * Part of HTML-XML-utils, see:
 * http://www.w3.org/Tools/HTML-XML-utils/
 *
 * Copyright © 2001-2012 World Wide Web Consortium
 * See http://www.w3.org/Consortium/Legal/copyright-software
 *
 * Author: Bert Bos <bert@w3.org>
 * Created: 5 Jul 2001
 * Version: $Id: hxselect.c,v 1.7 2013-06-30 20:43:31 bbos Exp $
 *
 **/
#include "config.h"
#include <assert.h>
#include <ctype.h>
#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include <unistd.h>
#include <stdbool.h>
#if STDC_HEADERS
# include <string.h>
#else
# ifndef HAVE_STRCHR
#  define strchr index
#  define strrchr rindex
# endif
# ifndef HAVE_STRSTR
#  include "strstr.e"
# endif
#endif
#include "export.h"
#include "types.e"
#include "heap.e"
#include "html.e"
#include "scan.e"
#include "errexit.e"
#include "selector.e"


typedef struct _Node {
  string name;
  pairlist attribs;
  conststring language;				/* Shared, don't free() this! */
  struct _Node *child;				/* Youngest(!) child */
  struct _Node *sister;				/* Older(!) sister */
  struct _Node *parent;
} Node, *Tree;

static Tree tree = NULL;			/* Current elt in tree */
static Selector selector;			/* The selector to match */
static int copying = 0;				/* >0 means: inside a match */
static string language = "";			/* Initial language */
static bool content_only = false;		/* Omit start/end tag */
static string separator = "";			/* Printed between matches */
static bool case_insensitive = false;	/* How to match elems/attrs */


/* push -- add an element to the tree, make tree point to that element */
static void push(const string name, const pairlist attribs)
{
  pairlist p;
  Node *h;

  new(h);
  h->name = name;
  h->attribs = attribs;
  for (p = attribs; p && !eq(p->name, "xml:lang"); p = p->next) ;
  if (p) h->language = p->value;		/* Explicit */
  else if (tree) h->language = tree->language;	/* Inherit */
  else h->language = language;			/* Initial */
  h->child = NULL;
  h->parent = tree;
  h->sister = tree ? tree->child : NULL;
  if (tree) tree->child = h;
  tree = h;
}

/* pop -- make tree point to parent of current node */
static void pop(const string name)
{
  if (!tree || !tree->name || !eq(tree->name, name))
    errexit("Input is not well-formed. (Maybe try normalize?)\n");
  tree = tree->parent;
}

/* same -- compare two names, case-(in)sensitively, depending */
static bool same(const string a, const string b)
{
  return case_insensitive ? strcasecmp(a, b) == 0 : eq(a, b);
}

/* nr_sisters -- return # of elder sisters of a node */
static int nr_sisters(const Node *n)
{
  if (!n->sister) return 0;
  else return 1 + nr_sisters(n->sister);
}

/* nr_typed_sisters -- return # of elder sisters of type t */
static int nr_typed_sisters(const Node *n, const Node *t)
{
  if (!n->sister)
    return 0;
  else if (!same(n->sister->name, t->name))
    return nr_typed_sisters(n->sister, t);
  else
    return 1 + nr_typed_sisters(n->sister, t);
}

/* get_attr -- return the value of the named attribute, or NULL */
static string get_attr(const Node *n, const string name)
{
  pairlist p;

  for (p = n->attribs; p && !same(p->name, name); p = p->next) ;
  return p ? p->value : NULL;
}

/* includes -- check for word in the space-separated words of line */
static bool includes(const string line, const string word)
{
  int i = 0, n = strlen(word);

  /* What should happen if word is the empty string? */
  /* To do: compare with contains() in class.c, keep the best */
  while (line[i]) {
    if (case_insensitive) {
      if (!strncasecmp(line+i, word, n) && (!line[i+n] || isspace(line[i+n])))
	return true;
    } else {
      if (!strncmp(line+i, word, n) && (!line[i+n] || isspace(line[i+n])))
	return true;
    }
    do i++; while (line[i] && !isspace(line[i]));
    while (isspace(line[i])) i++;
  }
  return false;
}

/* starts_with -- check if line starts with prefix */
static bool starts_with(const string line, const string prefix)
{
  return case_insensitive
    ? strncasecmp(line, prefix, strlen(prefix)) == 0
    : strncmp(line, prefix, strlen(prefix)) == 0;
}

/* ends_with -- check if line ends with suffix */
static bool ends_with(const string line, const string suffix)
{
  int n1 = strlen(line), n2 = strlen(suffix);
  return n1 >= n2 && eq(line + n1 - n2, suffix);
}

/* contains -- check if line contains s */
static bool contains(const string line, const string s)
{
  return strstr(line, s) != NULL;
}

/* lang_match -- check if language specific is subset of general */
static bool lang_match(const conststring specific, const conststring general)
{
  int n = strlen(general);
  return !strncasecmp(specific, general, n)
    && (specific[n] == '-' || !specific[n]);
}

/* simple_match -- check if a node matches a simple selector */
static bool simple_match(const Node *n, const SimpleSelector *s)
{
  AttribCond *p;
  PseudoCond *q;
  string h;
  int i;

  /* Match the type selector */
  if (s->name && !same(s->name, n->name)) return false;

  /* Match the attribute selectors, including class and ID */
  for (p = s->attribs; p; p = p->next) {
    if (!(h = get_attr(n, (p->op == HasClass) ? (string)"class"
		       : (p->op == HasID) ? (string) "id" : p->name)))
      return false;
    switch (p->op) {
    case Exists: break;
    case Equals:
    case HasID: if (!eq(p->value, h)) return false; break;
    case Includes:
    case HasClass: if (!includes(h, p->value)) return false; break;
    case StartsWith: if (!starts_with(h, p->value)) return false; break;
    case EndsWidth: if (!ends_with(h, p->value)) return false; break;
    case Contains: if (!contains(h, p->value)) return false; break;
    case LangMatch: if (!lang_match(h, p->value)) return false; break;
    default: assert(!"Cannot happen");
    }
  }

  /* Match the pseudo-classes */
  for (q = s->pseudos; q; q = q->next) {
    switch (q->type) {
    case RootSel:
      if (n->parent) return false;
      break;
    case NthChild:
      i = nr_sisters(n) + 1;
      if (q->a == 0) {if (i != q->b) return false;}
      else {if ((i-q->b)/q->a < 0 || (i-q->b)%q->a != 0) return false;}
      break;
    case NthOfType:
      i = nr_typed_sisters(n, n) + 1;
      if (q->a == 0) {if (i != q->b) return false;}
      else {if ((i-q->b)/q->a < 0 || (i-q->b)%q->a != 0) return false;}
      break;
    case FirstChild:
      if (n->sister) return false;
      break;
    case FirstOfType:
      if (nr_typed_sisters(n, n) != 0) return false;
      break;
    case Lang:
      if (!lang_match(n->language, q->s)) return false;
      break;
    default:
      assert(!"Cannot happen");
    }
  }
  return true;
}

/* matches_sel -- check if node matches selector (recursively) */
static bool matches_sel(const Tree t, const Selector s)
{
  Tree h;

  if (!simple_match(t, s)) return false;
  if (!s->context) return true;
  switch (s->combinator) {
  case Descendant:
    for (h = t->parent; h && !matches_sel(h, s->context); h = h->parent);
    return h != NULL;
  case Child:
    return t->parent && matches_sel(t->parent, s->context);
  case Adjacent:
    return t->sister && matches_sel(t->sister, s->context);
  case Sibling:
    for (h = t->sister; h && !matches_sel(h, s->context); h = h->sister);
    return h != NULL;
  default:
    assert(!"Cannot happen");
    return false;
  }
}

/* matches -- check if current node matches the selector */
static bool matches(void)
{
  return matches_sel(tree, selector);
}

/* printtag -- print a start tag or an XML-style empty tag */
static void printtag(const string name, const pairlist attribs,
		     bool slash)
{
  pairlist p;

  printf("<%s", name);
  for (p = attribs; p; p = p->next) {
    printf(" %s=\"%s\"", p->name, p->value);
  }
  if (slash) putchar('/');
  putchar('>');
}

/* printsep -- print the separator string, interpret escapes */
static void printsep(const string separator)
{
  string s = separator;
  int c;

  while (*s) {
    if (*s != '\\') putchar(*(s++));
    else if ('0' <= *(++s) && *s <= '7') {
      c = *s - '0';
      if ('0' <= *(++s) && *s <= '7') {
	c = 8 * c + *s - '0';
	if ('0' <= *(++s) && *s <= '7') c = 8 * c + *s - '0';
      }
      putchar(c); s++;
    } else
      switch (*s) {
      case '\0': putchar('\\'); break;
      case 'n': putchar('\n'); s++; break;
      case 't': putchar('\t'); s++; break;
      case 'r': putchar('\r'); s++; break;
      case 'f': putchar('\f'); s++; break;
      default: putchar(*(s++)); break;
      }
  }
}


/* handle_error -- called when a parse error occurred */
static void handle_error(void *clientdata, const string s, int lineno)
{
  fprintf(stderr, "%d: %s\n", lineno, s);
}

/* start -- called before the first event is reported */
static void* start(void)
{
  return NULL;
}
  
/* end -- called after the last event is reported */
static void end(void *clientdata)
{
}

/* handle_comment -- called after a comment is parsed */
static void handle_comment(void *clientdata, const string commenttext)
{
  free(commenttext);
}

/* handle_text -- called after a text chunk is parsed */
static void handle_text(void *clientdata, const string text)
{
  if (copying) printf("%s", text);
  free(text);
}

/* handle_declaration -- called after a declaration is parsed */
static void handle_decl(void *clientdata, const string gi, const string fpi,
			const string url)
{
  free(gi);
  free(fpi);
  free(url);
}

/* handle_proc_instr -- called after a PI is parsed */
static void handle_pi(void *clientdata, const string pi_text)
{
  free(pi_text);
}

/* handle_starttag -- called after a start tag is parsed */
static void handle_starttag(void *clientdata, const string name,
			    pairlist attribs)
{
  assert(copying >= 0);
  push(name, attribs);				/* Add to tree */
  if (copying || matches()) copying++;		/* Level of copying */
  if (copying > 1 || (copying == 1 && !content_only))
    printtag(name, attribs, false);		/* Print a start tag */
}

/* handle_emptytag -- called after an empty tag is parsed */
static void handle_emptytag(void *clientdata, const string name,
			    pairlist attribs)
{
  assert(copying >= 0);
  push(name, attribs);				/* Add to tree */
  if (copying || matches()) copying++;		/* Level of copying */
  if (copying > 1 || (copying == 1 && !content_only))
    printtag(name, attribs, true);		/* Print a start tag */
  if (copying == 1) printsep(separator);	/* Separate the matches */
  if (copying) copying--;
  pop(name);					/* Remove from tree again */
}

/* handle_endtag -- called after an endtag is parsed (name may be "") */
static void handle_endtag(void *clientdata, const string name)
{
  assert(copying >= 0);
  if (copying > 1 || (copying == 1 && !content_only)) printf("</%s>", name);
  if (copying == 1) printsep(separator);	/* Separate the matches */
  if (copying) copying--;
  pop(name);
  free(name);
}

/* usage -- print usage message and exit */
static void usage(const string name)
{
  errexit("Usage: %s [-v] [-i] [-c] [-l language] [-s separator] selector\n",
	  name);
}


int main(int argc, char *argv[])
{
  string s;
  int c;

  /* Bind the parser callback routines to our handlers */
  set_error_handler(handle_error);
  set_start_handler(start);
  set_end_handler(end);
  set_comment_handler(handle_comment);
  set_text_handler(handle_text);
  set_decl_handler(handle_decl);
  set_pi_handler(handle_pi);
  set_starttag_handler(handle_starttag);
  set_emptytag_handler(handle_emptytag);
  set_endtag_handler(handle_endtag);

  /* Command line options */
  while ((c = getopt(argc, argv, "icl:s:v")) != -1) {
    switch (c) {
    case 'c': content_only = true; break;
    case 'l': language = optarg; break;
    case 's': separator = optarg; break;
    case 'i': case_insensitive = true;  break;
    case 'v': printf("Version: %s %s\n", PACKAGE, VERSION); return 0;
    case '?': usage(argv[0]); break;
    default: assert(!"Cannot happen");
    }
  }

  /* Parse the selector */
  if (optind >= argc) usage(argv[0]);		/* Need at least 1 arg */
  for (s = newstring(argv[optind++]); optind < argc; optind++)
    strapp(&s, " ", argv[optind], NULL);
  selector = parse_selector(s);

  /* Walk the tree */
  yyin = stdin;
  if (yyparse() != 0) exit(3);
  return 0;
}
