extern void set_indent(int n);
extern void set_linelen(int n);
extern void flush();
extern void outc(char c, 
                        _Bool 
                             preformatted);
extern void out(string s, 
                         _Bool 
                              preformatted);
extern void outn(string s, size_t n, 
                                    _Bool 
                                         preformatted);
extern void outln(char *s, 
                          _Bool 
                               preformatted);
extern void outbreak(void);
extern void outbreakpoint(void);
extern void inc_indent(void);
extern void dec_indent(void);
