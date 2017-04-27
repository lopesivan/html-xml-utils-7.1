/*
 * Format an HTML source in a consistent manner.
 *
 * Copyright © 1994-2012 World Wide Web Consortium
 * See http://www.w3.org/Consortium/Legal/2002/copyright-software-20021231
 *
 * Created 9 May 1998
 * Bert Bos <bert@w3.org>
 * $Id: hxnormalize.c,v 1.21 2016/10/17 17:37:28 bbos Exp $
 */
#include "config.h"
#include <stdio.h>
#include <ctype.h>
#include <stdlib.h>
#ifdef HAVE_UNISTD_H
#  include <unistd.h>
#endif
#ifdef HAVE_STRING_H
#  include <string.h>
#elif HAVE_STRINGS_H
#  include <strings.h>
#endif
#include <assert.h>
#include <stdbool.h>
#include "export.h"
#include "types.e"
#include "tree.e"
#include "html.e"
#include "scan.e"
#include "textwrap.e"
#include "dict.e"
#include "openurl.e"
#include "errexit.e"

static Tree tree;
static bool do_xml = false;
static bool do_endtag = false;
static bool has_errors = false;
static bool do_doctype = true;
static bool clean_span = false;
static string long_comment = NULL;
static bool do_lang = false;


/* handle_error -- called when a parse error occurred */
void handle_error(void *clientdata, const string s, int lineno)
{
  fprintf(stderr, "%d: %s\n", lineno, s);
  has_errors = true;
}

/* start -- called before the first event is reported */
void* start(void)
{
  tree = create();
  return NULL;
}
  
/* end -- called after the last event is reported */
void end(void *clientdata)
{
  /* skip */
}

/* handle_comment -- called after a comment is parsed */
void handle_comment(void *clientdata, string commenttext)
{
  tree = append_comment(tree, commenttext);
}

/* handle_text -- called after a text chunk is parsed */
void handle_text(void *clientdata, string text)
{
  tree = append_text(tree, text);
}

/* handle_decl -- called after a declaration is parsed */
void handle_decl(void *clientdata, string gi,
		 string fpi, string url)
{
  tree = append_declaration(tree, gi, fpi, url);
}

/* handle_pi -- called after a PI is parsed */
void handle_pi(void *clientdata, string pi_text)
{
  tree = append_procins(tree, pi_text);
}

/* handle_starttag -- called after a start tag is parsed */
void handle_starttag(void *clientdata, string name, pairlist attribs)
{
  tree = html_push(tree, name, attribs);
}

/* handle_emptytag -- called after an empty tag is parsed */
void handle_emptytag(void *clientdata, string name, pairlist attribs)
{
  tree = html_push(tree, name, attribs);
}

/* handle_endtag -- called after an endtag is parsed (name may be "") */
void handle_endtag(void *clientdata, string name)
{
  tree = html_pop(tree, name);
  free(name);
}

/* insert -- insert an attribute into a sorted list of attributes */
static pairlist insert(pairlist x, pairlist list)
{
  if (! list) {					/* Empty list */
    x->next = NULL;
    return x;
  } else if (strcmp(x->name, list->name) <= 0) { /* Insert at head */
    x->next = list;
    return x;
  } else {					/* Insert not at head */
    list->next = insert(x, list->next);
    return list;
  }
}

/* sort_list -- sort a linked list of attributes, return reordered list */
static pairlist sort_list(pairlist list)
{
  /* Insertion sort should be fast enough... */
  if (! list) return NULL;
  else return insert(list, sort_list(list->next));
}

/* next_ambiguous -- check if omitting end changes the meaning */
static bool next_ambiguous(Node *n)
{
  Node *h = n;

  /* Skip text nodes with only white space */
  while (h->sister && h->sister->tp == Text && only_space(h->sister->text))
    h = h->sister;

  if (h->sister == NULL) return false;
  if (h->sister->tp == Text) return true;
  if (h->sister->tp == Comment) return true;
  if (h->sister->tp == Procins) return true;
  if (h->sister->tp == Declaration) return false; /* Should not occur */
  assert(h->sister->tp == Element);		/* Cannot be Root */
  return has_parent(h->sister->name, n->name);
}

/* needs_quotes -- check if the attribute value can be printed unquoted */
static bool needs_quotes(const string s)
{
  int i;
  assert(s);
  if (!s[0]) return true;			/* Empty string */
  for (i = 0; s[i]; i++)
    if (!isalnum(s[i]) && (s[i] != '-') && (s[i] != '.')) return true;
  return false;
}

/* pp -- print the document normalized */
static void pp(Tree n, bool preformatted, bool allow_text,
	       conststring lang)
{
  bool pre, mixed;
  conststring lang2;
  string s;
  pairlist h;
  size_t i, j;
  Tree l;

  switch (n->tp) {
    case Text:
      if (!allow_text) {
	assert(only_space(n->text));
      } else {
	s = n->text;
	i = strlen(s);
	outn(s, i, preformatted);
      }
      break;
    case Comment:
      if (long_comment && strstr(n->text, long_comment) && !preformatted) {
	/* Found a comment that should have an empty line before it */
	outbreak();
	outln(NULL, true);
      }
      out("<!--", true); out(n->text, true);
      if (allow_text || preformatted) out("-->", true);
      else outln("-->", preformatted);
      break;
    case Declaration:
      if (do_doctype) {
	out("<!DOCTYPE ", false);
	out(n->name, false);
	if (n->text) {
	  out(" PUBLIC \"", false);
	  out(n->text, false);
	  out("\"", false);
	}
	if (n->url) {
	  if (!n->text) out(" SYSTEM", false);
	  out(" \"", false);
	  out(n->url, false);
	  out("\"", false);
	} else if (n->text && do_xml) {	/* XML cannot omit the system literal */
	  out(" \"\"", false);
	}
	outln(">", false);
      }
      break;
    case Procins:
      out("<?", false); out(n->text, true);
      if (allow_text || preformatted) out(">", false);
      else outln(">", false);
      break;
    case Element:
      if (clean_span && eq(n->name, "span") && ! n->attribs) {
	/* Omit start and end tags, print just the children. */
	for (l = n->children; l != NULL; l = l->sister)
	  pp(l, preformatted, true, lang);
	break;
      }
      /* Determine language, remove redundant language attribute */
      if (do_lang) {
	if ((lang2 = pairlist_get(n->attribs, "lang")) ||
	    (lang2 = pairlist_get(n->attribs, "xml:lang"))) {
	  if (lang && eq(lang, lang2)) {
	    pairlist_unset(&n->attribs, "lang");
	    pairlist_unset(&n->attribs, "xml:lang");
	  }
	  lang = lang2;
	}
      }
      if (!preformatted && break_before(n->name)) outln(NULL, false);
      out("<", preformatted); out(n->name, preformatted);
      if (break_before(n->name)) inc_indent();
      n->attribs = sort_list(n->attribs);
      for (h = n->attribs; h != NULL; h = h->next) {
	out(" ", false); out(h->name, false);
	if (do_xml) {
	  out("=\"", false);
	  out(h->value ? h->value : h->name, true);
	  outc('"', false);
	} else if (h->value == NULL) {
	  /* The h->name *is* the value (and the attribute name is implicit) */
	} else if (!needs_quotes(h->value)) {
	  out("=", false); /* Omit the quotes */
	  out(h->value, true);
	} else {
	  out("=\"", false);
	  out(h->value, true);
	  outc('"', false);
	}
      }
      if (is_empty(n->name)) {
	assert(n->children == NULL);
	outbreakpoint();
	out(do_xml ? " />" : ">", true);
	if (break_before(n->name)) dec_indent();
	if (!preformatted && break_after(n->name)) outln(NULL, false);

      } else if (do_xml && is_cdata_elt(n->name)) {
	/* Insert <![CDATA[...]]>, but only if input was HTML, not XML */
	if (!n->children) {
	  out(" />", true);
	  if (break_before(n->name)) dec_indent();
	} else {
	  out(">", preformatted);
	  /* TODO: Strictly speaking, if the input is HTML (not XML),
	     then the string "<![CDATA[" in <style> or <script> is to
	     be taken as literal text. In practice, the string
	     "<![CDATA[" is nearly always preceeded by "<!--" or "//"
	     and so this simplistic check will usually work... */
	  assert(n->children->tp == Text);
	  if (!hasprefix(n->children->text, "<![CDATA[")) out("<![CDATA[",true);
	  for (l = n->children; l; l = l->sister) {
	    assert(n->children->tp == Text);
	    out(l->text, true);
	  }
	  if (!hasprefix(n->children->text, "<![CDATA[")) out("]]>", true);
	  if (break_before(n->name)) dec_indent();
	  out("</", preformatted);
	  out(n->name, preformatted);
	  outbreakpoint();
	  out(">", preformatted);
	}
	if (!preformatted && break_after(n->name)) outbreak();

      } else if (!do_xml && is_cdata_elt(n->name) && n->children &&
		 n->children->tp == Text && !n->children->sister &&
		 hasprefix(n->children->text, "<![CDATA[")) {
	/* Remove <![CDATA[...]]>, but only if input was XML, not HTML */
	assert(hasaffix(n->children->text, "]]>"));
	out(">", preformatted);
	s = n->children->text + 9; /* Skip "<![CDATA[" */
	i = strlen(s) - 3;	   /* Omit "]]>" */
	for (j = 0; j < i; j++) outc(s[j], true);
	if (break_before(n->name)) dec_indent();
	out("</", preformatted);
	out(n->name, preformatted);
	outbreakpoint();
	out(">", preformatted);
	if (!preformatted && break_after(n->name)) outbreak();

      } else {
	outbreakpoint();
	out(">", preformatted);
	pre = preformatted || is_pre(n->name);
	mixed = is_mixed(n->name);
	for (l = n->children; l != NULL; l = l->sister)
	  pp(l, pre, mixed, lang);
	if (break_before(n->name)) dec_indent();
	if (do_xml || do_endtag || need_etag(n->name) || next_ambiguous(n)) {
	  out("</", pre); out(n->name, pre);
	  outbreakpoint();
	  out(">", preformatted);
	}
	if (!preformatted && break_after(n->name)) outbreak();
      }
      break;
    default:
      assert(!"Cannot happen");
  }
}

/* prettyprint -- print the tree normalized */
static void prettyprint(Tree t)
{
  Tree h;
  assert(t->tp == Root);
  for (h = t->children; h != NULL; h = h->sister) pp(h, false, false, NULL);
  flush();
}

/* usage -- print usage message and exit */
static void usage(string prog)
{
  fprintf(stderr, "%s version %s\n\
Usage: %s [-e] [-d] [-x] [-L] [-i indent] [-l linelen] [-c commentmagic] [file_or_url]\n",
	  prog, VERSION, prog);
  exit(1);
}

/* main -- main body */
int main(int argc, char *argv[])
{
  int c, status = 200;

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

  while ((c = getopt(argc, argv, "edxi:l:sc:L")) != -1)
    switch (c) {
    case 'e': do_endtag = true; break;
    case 'x': do_xml = true; break;
    case 'd': do_doctype = false; break;
    case 'i': set_indent(atoi(optarg)); break;
    case 'l': set_linelen(atoi(optarg)); break;
    case 's': clean_span = true; break;
    case 'c': long_comment = optarg; break;
    case 'L': do_lang = true; break;
    default: usage(argv[0]);
    }
  if (optind == argc) yyin = stdin;
  else if (optind == argc - 1) yyin = fopenurl(argv[optind], "r", &status);
  else usage(argv[0]);
  if (yyin == NULL) {perror(argv[optind]); exit(2);}
  if (status != 200) errexit("%s : %s\n", argv[optind], http_strerror(status));
  if (yyparse() != 0) {exit(3);}
  tree = get_root(tree);
  prettyprint(tree);
  return has_errors ? 1 : 0;
}
