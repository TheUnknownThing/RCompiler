typedef unsigned int size_t;

extern int printf(const char *pattern, ...);
extern int scanf(const char *pattern, ...);
extern void *malloc(size_t n);
extern void *memcpy(void *dest, const void *src, size_t n);
extern void *memset(void *dest, int ch, size_t n);
extern void exit(int status);

void _Function_prelude_print(char *str) { printf("%s", str); }

void _Function_prelude_println(char *str) { printf("%s\n", str); }

void _Function_prelude_printInt(int n) { printf("%d", n); }

void _Function_prelude_printlnInt(int n) { printf("%d\n", n); }

char *_Function_prelude_getString(void) {
  char *buffer = (char *)malloc(256);
  scanf("%s", buffer);
  return buffer;
}

int _Function_prelude_getInt(void) {
  int n;
  scanf("%d", &n);
  return n;
}

void *_Function_prelude_builtin_memset(void *dest, int ch, size_t n) {
  return memset(dest, ch, n);
}

void *_Function_prelude_builtin_memcpy(void *dest, const void *src, size_t n) {
  return memcpy(dest, src, n);
}

void _Function_prelude_exit(int status) {
  //   exit(status);
  (void)status;
}
