struct _Entity {char *name; unsigned int code;};
extern const struct _Entity *lookup_entity (const char *str, unsigned int len);
