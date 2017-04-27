[FORKED]

~~~ sh
    $ ./autogen.sh
    $ ./configure
    $ make
    $ bash local-install-with-ln-s.sh
    $ hxselect
    $ which hxselect
    $ bash local-rm-links.sh
    $ hxselect
~~~

In this directory:

html-xml-utils-*.tar.gz
    A number of simple utilities for manipulating HTML and XML files.
    See INSTALL for generic installation instructions.
    Get the source at: http://www.w3.org/Tools/HTML-XML-utils/

htmlutils-*.tar.gz
    Old versions (before version 0.1)


Note 1: Your package manager may have a precompiled copy
already. There are versions in Debian, Ubuntu, Macports and others. In
that case you need to download from here only if you want a different
version or want to hack on the source.

Note 2: the names changed in version 5.0: most programs got an "hx"
prefix. Please, uninstall any version < 5.0 before installing a
version >= 5.0


cexport (1)          - create headerfile of exported declarations from a C file
hxaddid (1)          - add ID's to selected elements
hxcite (1)           - replace bibliographic references by hyperlinks
hxcite-mkbib (1)     - expand references and create bibliography
hxcopy (1)           - copy an HTML file while preserving relative links
hxcount (1)          - count elements and attributes in HTML or XML files
hxextract (1)        - extract selected elements
hxclean (1)          - apply heuristics to correct an HTML file
hxprune (1)          - remove marked elements from an HTML file
hxincl (1)           - expand included HTML or XML files
hxindex (1)          - create an alphabetically sorted index
hxmkbib (1)          - create bibliography from a template
hxmultitoc (1)       - create a table of contents for a set of HTML files
hxname2id            - move some ID= or NAME= from A elements to their parents
hxnormalize (1)      - pretty-print an HTML file
hxnum (1)            - number section headings in an HTML file
hxpipe (1)           - convert XML to a format easier to parse with Perl or AWK
hxprintlinks (1)     - number links & add table of URLs at end of an HTML file
hxremove (1)         - remove selected elements from an XML file
hxtabletrans (1)     - transpose an HTML or XHTML table
hxtoc (1)            - insert a table of contents in an HTML file
hxuncdata (1)        - replace CDATA sections by character entities
hxunent (1)          - replace HTML predefined character entities to UTF-8
hxunpipe (1)         - convert output of pipe back to XML format
hxunxmlns (1)        - replace "global names" by XML Namespace prefixes
hxwls (1)            - list links in an HTML file
hxxmlns (1)          - replace XML Namespace prefixes by "global names"
asc2xml, xml2asc (1) - convert between UTF8 and &#nnn; entities
hxref (1)            - generate cross-references
hxselect (1)         - extract elements that match a (CSS) selector



This package is configured with automake/autoconf. Generic
instructions are in the file INSTALL. Here are some specific problems
that may arise:

1) Error when running lex:

      lex   scan.l && mv lex.yy.c scan.c
      "scan.l":line 2: Error: missing translation value

   The scan.l file uses features of flex that do not exist in lex.
   However, it is not necessary to run lex, since the file scan.c is
   provided in the package. Just do a "touch scan.c" to make sure
   "make" will not try to generate it anew.

2) Warning about "libidn not found":

  Without libidn2 or libidn, hxwls will not be able to translate
  Internationalized Domain Names to ASCII (option -a). You can install
  either libidn2 or libidn.

  If you install them in a non-standard location, use --with-libidn2
  or --with-libidn when invoking ./configure. E.g., if you install
  libidn from MacPorts on Mac OS X, run:

      ./configure --with-libidn=/opt/local


$Date: 2016/04/14 00:42:15 $
