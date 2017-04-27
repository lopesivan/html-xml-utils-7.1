/*
 * Copy input (well-formed XML) without any elements that match a given selector
 *
 * To do: Put common routines with hxselect in separate module.
 *
 * To do: Only look for a LANG attribute if the input is XHTML.
 *
 * Part of HTML-XML-utils, see:
 * http://www.w3.org/Tools/HTML-XML-utils/
 *
 * Copyright © 2012 World Wide Web Consortium
 * See http://www.w3.org/Consortium/Legal/copyright-software
 *
 * Author: Bert Bos <bert@w3.org>
 * Created: 21 Oct 2012
 */
#include "config.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <ctype.h>
#include <stdbool.h>
#ifdef HAVE_STRING_H
# include <string.h>
#elif HAVE_STRINGS_H
# include <strings.h>
#endif
#include "types.e"
#include "tree.e"
#include "selector.e"
#include "heap.e"
#include "errexit.e"
#include "html.e"
#include "scan.e"


static Selector selector;			/* The selector to match */
static string language = "";			/* Initial language */
static bool case_insensitive = false;	/* How to match elems/attrs */


/* get_language -- return the (inherited) human language of an element */
static conststring get_language(const Node *n)
{
  conststring s;

  assert(n);
  if (n->tp == Root) return language; /* Language from -l option */
  assert(n->tp == Element);
  if ((s = get_attrib(n, "xml:lang"))) return s;
  if ((s = get_attrib(n, "lang"))) return s;
  return get_language(n->parent);
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
      if (!lang_match(get_language(n), q->s)) return false;
      break;
    default:
      assert(!"Cannot happen");
    }
  }
  return true;
}


/* matches -- check if an element t matches the selector s */
static bool matches(const Tree t, const Selector s)
{
  Tree h;

  assert(s);
  if (! t) return false;
  if (! simple_match(t, s)) return false;
  if (! s->context) return true;
  switch (s->combinator) {
  case Descendant:
    for (h = t->parent; h && !matches(h, s->context); h = h->parent);
    return h != NULL;
  case Child:
    return matches(t->parent, s->context);
  case Adjacent:
    return matches(t->sister, s->context);
  case Sibling:
    for (h = t->sister; h && !matches(h, s->context); h = h->sister);
    return h != NULL;
  default:
    assert(!"Cannot happen");
    return false;
  }
}


/* print_tree -- print the tree below t, omitting elements that match the selector */
static void print_tree(Tree t)
{
  pairlist p;

  if (!t) return;
  switch (t->tp) {
  case Element:
    if (!matches(t, selector)) {
      printf("<%s", t->name);
      for (p = t->attribs; p; p = p->next) printf(" %s=\"%s\"", p->name, p->value);
      if (!t->children) {
	printf("/>");
      } else {
	printf(">");
	print_tree(t->children);
	printf("</%s>", t->name);
      }
    }
    print_tree(t->sister);
    break;
  case Text:
    printf("%s", t->text);
    print_tree(t->sister);
    break;
  case Comment:
    printf("<!--%s-->", t->text);
    print_tree(t->sister);
    break;
  case Declaration:
    printf("<!DOCTYPE %s", t->name);
    if (t->text && t->url) printf(" PUBLIC \"%s\" \"%s\"", t->text, t->url);
    else if (t->text) printf(" PUBLIC \"%s\"", t->text);
    else if (t->url) printf(" SYSTEM \"%s\"", t->url);
    printf(">");
    print_tree(t->sister);
    break;
  case Procins:
    printf("<?%s>", t->text);
    print_tree(t->sister);
    break;
  case Root:
    print_tree(t->children);

    break;
  default: assert(!"Cannot happen!");
  }
}


/*********************** Parser callback API ***********************/

/* handle_error -- called when a parse error occurred */
void handle_error(void *clientdata, const string s, int lineno)
{
  fprintf(stderr, "%d: %s\n", lineno, s);
}


/* start -- called before the first event is reported */
void* handle_start(void)
{
  Tree *tp;
  new(tp);
  *tp = create();		/* Create an empty tree */
  return tp;
}


/* end -- called after the last event is reported */
void handle_end(void *clientdata)
{
  Tree *t = (Tree*)clientdata;
  print_tree(get_root(*t)); /* Print tree, filtering out unwanted elements */
  tree_delete(*t);
}


/* handle_comment -- called after a comment is parsed */
void handle_comment(void *clientdata, const string commenttext)
{
  Tree *t = (Tree*)clientdata;
  *t = append_comment(*t, commenttext);
}


/* handle_text -- called after a text chunk is parsed */
void handle_text(void *clientdata, const string text)
{
  Tree *t = (Tree*)clientdata;
  *t = tree_append_text(*t, text);
}


/* handle_decl -- called after a declaration is parsed */
void handle_decl(void *clientdata, const string gi, const string fpi, const string url)
{
  Tree *t = (Tree*)clientdata;
  *t = append_declaration(*t, gi, fpi, url);
}


/* handle_pi -- called after a Processing Instruction is parsed */
void handle_pi(void *clientdata, const string pi_text)
{
  Tree *t = (Tree*)clientdata;
  *t = append_procins(*t, pi_text);
}


/* handle_starttag -- called after a start tag is parsed */
void handle_starttag(void *clientdata, const string name, pairlist attribs)
{
  Tree *t = (Tree*)clientdata;
  *t = tree_push(*t, name, attribs);
}


/* handle_emptytag -- called after an empty tag is parsed */
void handle_emptytag(void *clientdata, const string name, pairlist attribs)
{
  Tree *t = (Tree*)clientdata;
  *t = tree_push(*t, name, attribs);
  *t = tree_pop(*t, name);
}


/* handle_endtag -- called after an endtag is parsed (name may be "") */
void handle_endtag(void *clientdata, const string name)
{
  Tree *t = (Tree*)clientdata;
  *t = tree_pop(*t, name);
}


/* handle_endincl -- called at the end of an included file */
/* void handle_endincl(void *clientdata) */

/********************* End of parser callbacks *********************/


/* usage -- print usage message and exit */
static void usage(const conststring progname)
{
  errexit("Usage: %s [-v] [-l language] [-i] selector\n", progname);
}


int main(int argc, char *argv[])
{
  string s;
  int c;

  /* Command line options */
  while ((c = getopt(argc, argv, "il:v")) != -1) {
    switch (c) {
    case 'l': language = optarg; break;
    case 'i': case_insensitive = true;  break;
    case 'v': printf("Version: %s %s\n", PACKAGE, VERSION); return 0;
    case '?': usage(argv[0]); break;
    default: assert(!"Cannot happen");
    }
  }

  /* Bind the parser callback routines to our handlers */
  set_error_handler(handle_error);
  set_start_handler(handle_start);
  set_end_handler(handle_end);
  set_comment_handler(handle_comment);
  set_text_handler(handle_text);
  set_decl_handler(handle_decl);
  set_pi_handler(handle_pi);
  set_starttag_handler(handle_starttag);
  set_emptytag_handler(handle_emptytag);
  set_endtag_handler(handle_endtag);

  /* Parse the selector */
  if (optind >= argc) usage(argv[0]);		/* Need at least 1 arg */
  for (s = newstring(argv[optind++]); optind < argc; optind++)
    strapp(&s, " ", argv[optind], NULL);
  selector = parse_selector(s);

  /* Parse the input, build a tree, filter the tree */
  yyin = stdin;
  if (yyparse() != 0) exit(3);
  return 0;
}
