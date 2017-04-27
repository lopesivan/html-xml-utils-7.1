typedef enum {
  RootSel, NthChild, NthOfType, FirstChild, FirstOfType, Lang
} PseudoType;
typedef struct _PseudoCond {
  PseudoType type;
  int a, b;
  string s;
  struct _PseudoCond *next;
} PseudoCond;
typedef enum {
  Exists, Equals, Includes, StartsWith, EndsWidth, Contains, LangMatch,
  HasClass, HasID
} Operator;
typedef struct _AttribCond {
  Operator op;
  string name;
  string value;
  struct _AttribCond *next;
} AttribCond;
typedef enum {
  Descendant, Child, Adjacent, Sibling
} Combinator;
typedef struct _SimpleSelector {
  string name;
  AttribCond *attribs;
  PseudoCond *pseudos;
  Combinator combinator;
  struct _SimpleSelector *context;
} SimpleSelector, *Selector;
extern string selector_to_string(const Selector selector);
extern Selector parse_selector(const string selector);
