/* Lisp Badge - uLisp 3.3a
   David Johnson-Davies - www.technoblogy.com - 10th July 2020

   Licensed under the MIT license: https://opensource.org/licenses/MIT
*/

// Lisp Library
const char LispLibrary[] PROGMEM = "";

// Compile options

#define checkoverflow
// #define resetautorun
#define printfreespace
// #define serialmonitor
// #define printgcs
// #define sdcardsupport
// #define lisplibrary

// Includes

// #include "LispLibrary.h"
#include <avr/sleep.h>
#include <setjmp.h>
#include <SPI.h>
#include <limits.h>
#include <EEPROM.h>

#if defined(sdcardsupport)
#include <SD.h>
#define SDSIZE 172
#else
#define SDSIZE 0
#endif

// C Macros

#define nil                NULL
#define car(x)             (((object *) (x))->car)
#define cdr(x)             (((object *) (x))->cdr)

#define first(x)           (((object *) (x))->car)
#define second(x)          (car(cdr(x)))
#define cddr(x)            (cdr(cdr(x)))
#define third(x)           (car(cdr(cdr(x))))

#define push(x, y)         ((y) = cons((x),(y)))
#define pop(y)             ((y) = cdr(y))

#define integerp(x)        ((x) != NULL && (x)->type == NUMBER)
#define floatp(x)          ((x) != NULL && (x)->type == FLOAT)
#define symbolp(x)         ((x) != NULL && (x)->type == SYMBOL)
#define stringp(x)         ((x) != NULL && (x)->type == STRING)
#define characterp(x)      ((x) != NULL && (x)->type == CHARACTER)
#define streamp(x)         ((x) != NULL && (x)->type == STREAM)

#define mark(x)            (car(x) = (object *)(((uintptr_t)(car(x))) | MARKBIT))
#define unmark(x)          (car(x) = (object *)(((uintptr_t)(car(x))) & ~MARKBIT))
#define marked(x)          ((((uintptr_t)(car(x))) & MARKBIT) != 0)
#define MARKBIT            1

#define setflag(x)         (Flags = Flags | 1<<(x))
#define clrflag(x)         (Flags = Flags & ~(1<<(x)))
#define tstflag(x)         (Flags & 1<<(x))

// Constants

const int TRACEMAX = 3; // Number of traced functions
enum type { ZZERO=0, SYMBOL=2, NUMBER=4, STREAM=6, CHARACTER=8, FLOAT=10, STRING=12, PAIR=14 };  // STRING and PAIR must be last
enum token { UNUSED, BRA, KET, QUO, DOT };
enum stream { SERIALSTREAM, I2CSTREAM, SPISTREAM, SDSTREAM };

// Stream names used by printobject
const char serialstream[] PROGMEM = "serial";
const char i2cstream[] PROGMEM = "i2c";
const char spistream[] PROGMEM = "spi";
const char sdstream[] PROGMEM = "sd";
PGM_P const streamname[] PROGMEM = {serialstream, i2cstream, spistream, sdstream};

enum function { NIL, TEE, NOTHING, OPTIONAL, AMPREST, LAMBDA, LET, LETSTAR, CLOSURE, SPECIAL_FORMS, QUOTE,
DEFUN, DEFVAR, SETQ, LOOP, RETURN, PUSH, POP, INCF, DECF, SETF, DOLIST, DOTIMES, TRACE, UNTRACE,
FORMILLIS, WITHSERIAL, WITHI2C, WITHSPI, WITHSDCARD, TAIL_FORMS, PROGN, IF, COND, WHEN, UNLESS, CASE, AND,
OR, FUNCTIONS, NOT, NULLFN, CONS, ATOM, LISTP, CONSP, SYMBOLP, BOUNDP, SETFN, STREAMP, EQ, CAR, FIRST,
CDR, REST, CAAR, CADR, SECOND, CDAR, CDDR, CAAAR, CAADR, CADAR, CADDR, THIRD, CDAAR, CDADR, CDDAR, CDDDR,
LENGTH, LIST, REVERSE, NTH, ASSOC, MEMBER, APPLY, FUNCALL, APPEND, MAPC, MAPCAR, MAPCAN, ADD, SUBTRACT,
MULTIPLY, DIVIDE, TRUNCATE, MOD, ONEPLUS, ONEMINUS, ABS, RANDOM, MAXFN, MINFN, NOTEQ, NUMEQ, LESS, LESSEQ,
GREATER, GREATEREQ, PLUSP, MINUSP, ZEROP, ODDP, EVENP, INTEGERP, NUMBERP, CHAR, CHARCODE, CODECHAR,
CHARACTERP, STRINGP, STRINGEQ, STRINGLESS, STRINGGREATER, SORT, STRINGFN, CONCATENATE, SUBSEQ,
READFROMSTRING, PRINCTOSTRING, PRIN1TOSTRING, LOGAND, LOGIOR, LOGXOR, LOGNOT, ASH, LOGBITP, EVAL, GLOBALS,
LOCALS, MAKUNBOUND, BREAK, READ, PRIN1, PRINT, PRINC, TERPRI, READBYTE, READLINE, WRITEBYTE, WRITESTRING,
WRITELINE, RESTARTI2C, GC, ROOM, SAVEIMAGE, LOADIMAGE, CLS, PINMODE, DIGITALREAD, DIGITALWRITE,
ANALOGREAD, ANALOGWRITE, DELAY, MILLIS, SLEEP, NOTE, EDIT, PPRINT, PPRINTALL, FORMAT, REQUIRE,
LISTLIBRARY, PLOT, PLOT3D, GLYPHPIXEL, PLOTPIXEL, FILLSCREEN, ENDFUNCTIONS };

// Typedefs

typedef unsigned int symbol_t;

typedef struct sobject {
  union {
    struct {
      sobject *car;
      sobject *cdr;
    };
    struct {
      unsigned int type;
      union {
        symbol_t name;
        int integer;
        int chars; // For strings
      };
    };
  };
} object;

typedef object *(*fn_ptr_type)(object *, object *);

typedef struct {
  const char *string;
  fn_ptr_type fptr;
  uint8_t minmax;
} tbl_entry_t;

typedef int (*gfun_t)();
typedef void (*pfun_t)(char);
typedef int BitOrder;
typedef int PinMode;

// Workspace - sizes in bytes
#define WORDALIGNED __attribute__((aligned (2)))
#define BUFFERSIZE 18

#if defined(__AVR_ATmega1284P__)
  #define WORKSPACESIZE 2816-SDSIZE       /* Objects (4*bytes) */
  #define EEPROMSIZE 4096                 /* Bytes */
  #define SYMBOLTABLESIZE 512             /* Bytes */
  #define STACKDIFF 320
#endif

object Workspace[WORKSPACESIZE] WORDALIGNED;
char SymbolTable[SYMBOLTABLESIZE];
#define SDCARD_SS_PIN 10

// Global variables

jmp_buf exception;
unsigned int Freespace = 0;
object *Freelist;
char *SymbolTop = SymbolTable;
unsigned int I2CCount;
unsigned int TraceFn[TRACEMAX];
unsigned int TraceDepth[TRACEMAX];

object *GlobalEnv;
object *GCStack = NULL;
object *GlobalString;
int GlobalStringIndex = 0;
uint8_t PrintCount = 0;
uint8_t BreakLevel = 0;
char LastChar = 0;
char LastPrint = 0;

// Flags
enum flag { PRINTREADABLY, RETURNFLAG, ESCAPE, EXITEDITOR, LIBRARYLOADED, NOESC };
volatile char Flags = 0b00001; // PRINTREADABLY set by default

// Forward references
object *tee;
object *tf_progn (object *form, object *env);
object *eval (object *form, object *env);
object *read (gfun_t gfun);
void repl (object *env);
void printobject (object *form, pfun_t pfun);
char *lookupbuiltin (symbol_t name);
intptr_t lookupfn (symbol_t name);
int builtin (char* n);
void pfstring (PGM_P s, pfun_t pfun);

// Error handling

void errorsub (symbol_t fname, PGM_P string) {
  pfl(pserial); pfstring(PSTR("Error: "), pserial);
  if (fname) {
    pserial('\'');
    pstring(symbolname(fname), pserial);
    pfstring(PSTR("' "), pserial);
  }
  pfstring(string, pserial);
}

void error (symbol_t fname, PGM_P string, object *symbol) {
  errorsub(fname, string);
  pfstring(PSTR(": "), pserial); printobject(symbol, pserial);
  pln(pserial);
  GCStack = NULL;
  longjmp(exception, 1);
}

void error2 (symbol_t fname, PGM_P string) {
  errorsub(fname, string);
  pln(pserial);
  GCStack = NULL;
  longjmp(exception, 1);
}

// Save space as these are used multiple times
const char notanumber[] PROGMEM = "argument is not a number";
const char notastring[] PROGMEM = "argument is not a string";
const char notalist[] PROGMEM = "argument is not a list";
const char notasymbol[] PROGMEM = "argument is not a symbol";
const char notproper[] PROGMEM = "argument is not a proper list";
const char toomanyargs[] PROGMEM = "too many arguments";
const char toofewargs[] PROGMEM = "too few arguments";
const char noargument[] PROGMEM = "missing argument";
const char nostream[] PROGMEM = "missing stream argument";
const char overflow[] PROGMEM = "arithmetic overflow";
const char invalidarg[] PROGMEM = "invalid argument";
const char invalidpin[] PROGMEM = "invalid pin";
const char resultproper[] PROGMEM = "result is not a proper list";
const char oddargs[] PROGMEM = "odd number of arguments";

// Set up workspace

void initworkspace () {
  Freelist = NULL;
  for (int i=WORKSPACESIZE-1; i>=0; i--) {
    object *obj = &Workspace[i];
    car(obj) = NULL;
    cdr(obj) = Freelist;
    Freelist = obj;
    Freespace++;
  }
}

object *myalloc () {
  if (Freespace == 0) error2(0, PSTR("no room"));
  object *temp = Freelist;
  Freelist = cdr(Freelist);
  Freespace--;
  return temp;
}

void myfree (object *obj) {
  car(obj) = NULL;
  cdr(obj) = Freelist;
  Freelist = obj;
  Freespace++;
}

// Make each type of object

object *number (int n) {
  object *ptr = myalloc();
  ptr->type = NUMBER;
  ptr->integer = n;
  return ptr;
}

object *character (char c) {
  object *ptr = myalloc();
  ptr->type = CHARACTER;
  ptr->chars = c;
  return ptr;
}

object *cons (object *arg1, object *arg2) {
  object *ptr = myalloc();
  ptr->car = arg1;
  ptr->cdr = arg2;
  return ptr;
}

object *symbol (symbol_t name) {
  object *ptr = myalloc();
  ptr->type = SYMBOL;
  ptr->name = name;
  return ptr;
}

object *newsymbol (symbol_t name) {
  for (int i=WORKSPACESIZE-1; i>=0; i--) {
    object *obj = &Workspace[i];
    if (symbolp(obj) && obj->name == name) return obj;
  }
  return symbol(name);
}

object *stream (unsigned char streamtype, unsigned char address) {
  object *ptr = myalloc();
  ptr->type = STREAM;
  ptr->integer = streamtype<<8 | address;
  return ptr;
}

// Garbage collection

void markobject (object *obj) {
  MARK:
  if (obj == NULL) return;
  if (marked(obj)) return;

  object* arg = car(obj);
  unsigned int type = obj->type;
  mark(obj);

  if (type >= PAIR || type == ZZERO) { // cons
    markobject(arg);
    obj = cdr(obj);
    goto MARK;
  }

  if (type == STRING) {
    obj = cdr(obj);
    while (obj != NULL) {
      arg = car(obj);
      mark(obj);
      obj = arg;
    }
  }
}

void sweep () {
  Freelist = NULL;
  Freespace = 0;
  for (int i=WORKSPACESIZE-1; i>=0; i--) {
    object *obj = &Workspace[i];
    if (!marked(obj)) myfree(obj); else unmark(obj);
  }
}

void gc (object *form, object *env) {
  #if defined(printgcs)
  int start = Freespace;
  #endif
  markobject(tee);
  markobject(GlobalEnv);
  markobject(GCStack);
  markobject(form);
  markobject(env);
  sweep();
  #if defined(printgcs)
  pfl(pserial); pserial('{'); pint(Freespace - start, pserial); pserial('}');
  #endif
}

// Compact image

void movepointer (object *from, object *to) {
  for (int i=0; i<WORKSPACESIZE; i++) {
    object *obj = &Workspace[i];
    unsigned int type = (obj->type) & ~MARKBIT;
    if (marked(obj) && (type >= STRING || type==ZZERO)) {
      if (car(obj) == (object *)((uintptr_t)from | MARKBIT))
        car(obj) = (object *)((uintptr_t)to | MARKBIT);
      if (cdr(obj) == from) cdr(obj) = to;
    }
  }
  // Fix strings
  for (int i=0; i<WORKSPACESIZE; i++) {
    object *obj = &Workspace[i];
    if (marked(obj) && ((obj->type) & ~MARKBIT) == STRING) {
      obj = cdr(obj);
      while (obj != NULL) {
        if (cdr(obj) == to) cdr(obj) = from;
        obj = (object *)((uintptr_t)(car(obj)) & ~MARKBIT);
      }
    }
  }
}

uintptr_t compactimage (object **arg) {
  markobject(tee);
  markobject(GlobalEnv);
  markobject(GCStack);
  object *firstfree = Workspace;
  while (marked(firstfree)) firstfree++;
  object *obj = &Workspace[WORKSPACESIZE-1];
  while (firstfree < obj) {
    if (marked(obj)) {
      car(firstfree) = car(obj);
      cdr(firstfree) = cdr(obj);
      unmark(obj);
      movepointer(obj, firstfree);
      if (GlobalEnv == obj) GlobalEnv = firstfree;
      if (GCStack == obj) GCStack = firstfree;
      if (*arg == obj) *arg = firstfree;
      while (marked(firstfree)) firstfree++;
    }
    obj--;
  }
  sweep();
  return firstfree - Workspace;
}

// Make SD card filename

char *MakeFilename (object *arg) {
  char *buffer = SymbolTop;
  int max = maxbuffer(buffer);
  int i = 0;
  do {
    char c = nthchar(arg, i);
    if (c == '\0') break;
    buffer[i++] = c;
  } while (i<max);
  buffer[i] = '\0';
  return buffer;
}

// Save-image and load-image

#if defined(sdcardsupport)
void SDWriteInt (File file, int data) {
  file.write(data & 0xFF); file.write(data>>8 & 0xFF);
}
#else
void EEPROMWriteInt (unsigned int *addr, int data) {
  EEPROM.write((*addr)++, data & 0xFF); EEPROM.write((*addr)++, data>>8 & 0xFF);
}
#endif

unsigned int saveimage (object *arg) {
  unsigned int imagesize = compactimage(&arg);
#if defined(sdcardsupport)
  SD.begin(SDCARD_SS_PIN);
  File file;
  if (stringp(arg)) {
    file = SD.open(MakeFilename(arg), O_RDWR | O_CREAT | O_TRUNC);
    arg = NULL;
  } else if (arg == NULL || listp(arg)) file = SD.open("/ULISP.IMG", O_RDWR | O_CREAT | O_TRUNC);
  else error(SAVEIMAGE, invalidarg, arg);
  if (!file) error2(SAVEIMAGE, PSTR("problem saving to SD card"));
  SDWriteInt(file, (uintptr_t)arg);
  SDWriteInt(file, imagesize);
  SDWriteInt(file, (uintptr_t)GlobalEnv);
  SDWriteInt(file, (uintptr_t)GCStack);
  #if SYMBOLTABLESIZE > BUFFERSIZE
  SDWriteInt(file, (uintptr_t)SymbolTop);
  for (int i=0; i<SYMBOLTABLESIZE; i++) file.write(SymbolTable[i]);
  #endif
  for (unsigned int i=0; i<imagesize; i++) {
    object *obj = &Workspace[i];
    SDWriteInt(file, (uintptr_t)car(obj));
    SDWriteInt(file, (uintptr_t)cdr(obj));
  }
  file.close();
  return imagesize;
#else
  if (!(arg == NULL || listp(arg))) error(SAVEIMAGE, invalidarg, arg);
  int bytesneeded = imagesize*4 + SYMBOLTABLESIZE + 10;
  if (bytesneeded > EEPROMSIZE) error(SAVEIMAGE, PSTR("image size too large"), number(imagesize));
  unsigned int addr = 0;
  EEPROMWriteInt(&addr, (unsigned int)arg);
  EEPROMWriteInt(&addr, imagesize);
  EEPROMWriteInt(&addr, (unsigned int)GlobalEnv);
  EEPROMWriteInt(&addr, (unsigned int)GCStack);
  #if SYMBOLTABLESIZE > BUFFERSIZE
  EEPROMWriteInt(&addr, (unsigned int)SymbolTop);
  for (int i=0; i<SYMBOLTABLESIZE; i++) EEPROM.write(addr++, SymbolTable[i]);
  #endif
  for (unsigned int i=0; i<imagesize; i++) {
    object *obj = &Workspace[i];
    EEPROMWriteInt(&addr, (uintptr_t)car(obj));
    EEPROMWriteInt(&addr, (uintptr_t)cdr(obj));
  }
  return imagesize;
#endif
}

#if defined(sdcardsupport)
int SDReadInt (File file) {
  uint8_t b0 = file.read(); uint8_t b1 = file.read();
  return b0 | b1<<8;
}
#else
int EEPROMReadInt (unsigned int *addr) {
  uint8_t b0 = EEPROM.read((*addr)++); uint8_t b1 = EEPROM.read((*addr)++);
  return b0 | b1<<8;
}
#endif

unsigned int loadimage (object *arg) {
#if defined(sdcardsupport)
  SD.begin(SDCARD_SS_PIN);
  File file;
  if (stringp(arg)) file = SD.open(MakeFilename(arg));
  else if (arg == NULL) file = SD.open("/ULISP.IMG");
  else error(LOADIMAGE, invalidarg, arg);
  if (!file) error2(LOADIMAGE, PSTR("problem loading from SD card"));
  SDReadInt(file);
  int imagesize = SDReadInt(file);
  GlobalEnv = (object *)SDReadInt(file);
  GCStack = (object *)SDReadInt(file);
  #if SYMBOLTABLESIZE > BUFFERSIZE
  SymbolTop = (char *)SDReadInt(file);
  for (int i=0; i<SYMBOLTABLESIZE; i++) SymbolTable[i] = file.read();
  #endif
  for (int i=0; i<imagesize; i++) {
    object *obj = &Workspace[i];
    car(obj) = (object *)SDReadInt(file);
    cdr(obj) = (object *)SDReadInt(file);
  }
  file.close();
  gc(NULL, NULL);
  return imagesize;
#else
  unsigned int addr = 0;
  EEPROMReadInt(&addr); // Skip eval address
  unsigned int imagesize = EEPROMReadInt(&addr);
  if (imagesize == 0 || imagesize == 0xFFFF) error2(LOADIMAGE, PSTR("no saved image"));
  GlobalEnv = (object *)EEPROMReadInt(&addr);
  GCStack = (object *)EEPROMReadInt(&addr);
  #if SYMBOLTABLESIZE > BUFFERSIZE
  SymbolTop = (char *)EEPROMReadInt(&addr);
  for (int i=0; i<SYMBOLTABLESIZE; i++) SymbolTable[i] = EEPROM.read(addr++);
  #endif
  for (int i=0; i<imagesize; i++) {
    object *obj = &Workspace[i];
    car(obj) = (object *)EEPROMReadInt(&addr);
    cdr(obj) = (object *)EEPROMReadInt(&addr);
  }
  gc(NULL, NULL);
  return imagesize;
#endif
}

void autorunimage () {
#if defined(sdcardsupport)
  SD.begin(SDCARD_SS_PIN);
  File file = SD.open("ULISP.IMG");
  if (!file) error2(0, PSTR("problem autorunning from SD card"));
  object *autorun = (object *)SDReadInt(file);
  file.close();
  if (autorun != NULL) {
    loadimage(NULL);
    apply(0, autorun, NULL, NULL);
  }
#else
  unsigned int addr = 0;
  object *autorun = (object *)EEPROMReadInt(&addr);
  if (autorun != NULL && (unsigned int)autorun != 0xFFFF) {
    loadimage(nil);
    apply(0, autorun, NULL, NULL);
  }
#endif
}

// Tracing

bool tracing (symbol_t name) {
  int i = 0;
  while (i < TRACEMAX) {
    if (TraceFn[i] == name) return i+1;
    i++;
  }
  return 0;
}

void trace (symbol_t name) {
  if (tracing(name)) error(TRACE, PSTR("already being traced"), symbol(name));
  int i = 0;
  while (i < TRACEMAX) {
    if (TraceFn[i] == 0) { TraceFn[i] = name; TraceDepth[i] = 0; return; }
    i++;
  }
  error2(TRACE, PSTR("already tracing 3 functions"));
}

void untrace (symbol_t name) {
  int i = 0;
  while (i < TRACEMAX) {
    if (TraceFn[i] == name) { TraceFn[i] = 0; return; }
    i++;
  }
  error(UNTRACE, PSTR("not tracing"), symbol(name));
}

// Helper functions

bool consp (object *x) {
  if (x == NULL) return false;
  unsigned int type = x->type;
  return type >= PAIR || type == ZZERO;
}

bool atom (object *x) {
  if (x == NULL) return true;
  unsigned int type = x->type;
  return type < PAIR && type != ZZERO;
}

bool listp (object *x) {
  if (x == NULL) return true;
  unsigned int type = x->type;
  return type >= PAIR || type == ZZERO;
}

bool improperp (object *x) {
  if (x == NULL) return false;
  unsigned int type = x->type;
  return type < PAIR && type != ZZERO;
}

object *quote (object *arg) {
  return cons(symbol(QUOTE), cons(arg,NULL));
}

// Radix 40 encoding

#define MAXSYMBOL 64000

int toradix40 (char ch) {
  if (ch == 0) return 0;
  if (ch >= '0' && ch <= '9') return ch-'0'+30;
  if (ch == '$') return 27; if (ch == '*') return 28; if (ch == '-') return 29;
  ch = ch | 0x20;
  if (ch >= 'a' && ch <= 'z') return ch-'a'+1;
  return -1; // Invalid
}

int fromradix40 (int n) {
  if (n >= 1 && n <= 26) return 'a'+n-1;
  if (n == 27) return '$'; if (n == 28) return '*'; if (n == 29) return '-';
  if (n >= 30 && n <= 39) return '0'+n-30;
  return 0;
}

int pack40 (char *buffer) {
  return (((toradix40(buffer[0]) * 40) + toradix40(buffer[1])) * 40 + toradix40(buffer[2]));
}

bool valid40 (char *buffer) {
 return (toradix40(buffer[0]) >= 0 && toradix40(buffer[1]) >= 0 && toradix40(buffer[2]) >= 0);
}

char *symbolname (symbol_t x) {
  if (x < ENDFUNCTIONS) return lookupbuiltin(x);
  else if (x >= MAXSYMBOL) return lookupsymbol(x);
  char *buffer = SymbolTop;
  buffer[3] = '\0';
  for (int n=2; n>=0; n--) {
    buffer[n] = fromradix40(x % 40);
    x = x / 40;
  }
  return buffer;
}

int digitvalue (char d) {
  if (d>='0' && d<='9') return d-'0';
  d = d | 0x20;
  if (d>='a' && d<='f') return d-'a'+10;
  return 16;
}

int checkinteger (symbol_t name, object *obj) {
  if (!integerp(obj)) error(name, notanumber, obj);
  return obj->integer;
}

int checkchar (symbol_t name, object *obj) {
  if (!characterp(obj)) error(name, PSTR("argument is not a character"), obj);
  return obj->chars;
}

int isstream (object *obj){
  if (!streamp(obj)) error(0, PSTR("not a stream"), obj);
  return obj->integer;
}

int issymbol (object *obj, symbol_t n) {
  return symbolp(obj) && obj->name == n;
}

void checkargs (symbol_t name, object *args) {
  int nargs = listlength(name, args);
  if (name >= ENDFUNCTIONS) error(0, PSTR("not valid here"), symbol(name));
  checkminmax(name, nargs);
}

int eq (object *arg1, object *arg2) {
  if (arg1 == arg2) return true;  // Same object
  if ((arg1 == nil) || (arg2 == nil)) return false;  // Not both values
  if (arg1->cdr != arg2->cdr) return false;  // Different values
  if (symbolp(arg1) && symbolp(arg2)) return true;  // Same symbol
  if (integerp(arg1) && integerp(arg2)) return true;  // Same integer
  if (characterp(arg1) && characterp(arg2)) return true;  // Same character
  return false;
}

int listlength (symbol_t name, object *list) {
  int length = 0;
  while (list != NULL) {
    if (improperp(list)) error2(name, notproper);
    list = cdr(list);
    length++;
  }
  return length;
}

// Association lists

object *assoc (object *key, object *list) {
  while (list != NULL) {
    if (improperp(list)) error(ASSOC, notproper, list);
    object *pair = first(list);
    if (!listp(pair)) error(ASSOC, PSTR("element is not a list"), pair);
    if (pair != NULL && eq(key,car(pair))) return pair;
    list = cdr(list);
  }
  return nil;
}

object *delassoc (object *key, object **alist) {
  object *list = *alist;
  object *prev = NULL;
  while (list != NULL) {
    object *pair = first(list);
    if (eq(key,car(pair))) {
      if (prev == NULL) *alist = cdr(list);
      else cdr(prev) = cdr(list);
      return key;
    }
    prev = list;
    list = cdr(list);
  }
  return nil;
}

// String utilities

void indent (uint8_t spaces, char ch, pfun_t pfun) {
  for (uint8_t i=0; i<spaces; i++) pfun(ch);
}

object *startstring (symbol_t name) {
  object *string = myalloc();
  string->type = STRING;
  GlobalString = NULL;
  GlobalStringIndex = 0;
  return string;
}

void buildstring (char ch, int *chars, object **head) {
  static object* tail;
  static uint8_t shift;
  if (*chars == 0) {
    shift = (sizeof(int)-1)*8;
    *chars = ch<<shift;
    object *cell = myalloc();
    if (*head == NULL) *head = cell; else tail->car = cell;
    cell->car = NULL;
    cell->chars = *chars;
    tail = cell;
  } else {
    shift = shift - 8;
    *chars = *chars | ch<<shift;
    tail->chars = *chars;
    if (shift == 0) *chars = 0;
  }
}

object *readstring (char delim, gfun_t gfun) {
  object *obj = myalloc();
  obj->type = STRING;
  int ch = gfun();
  if (ch == -1) return nil;
  object *head = NULL;
  int chars = 0;
  while ((ch != delim) && (ch != -1)) {
    if (ch == '\\') ch = gfun();
    buildstring(ch, &chars, &head);
    ch = gfun();
  }
  obj->cdr = head;
  return obj;
}

int stringlength (object *form) {
  int length = 0;
  form = cdr(form);
  while (form != NULL) {
    int chars = form->chars;
    for (int i=(sizeof(int)-1)*8; i>=0; i=i-8) {
      if (chars>>i & 0xFF) length++;
    }
    form = car(form);
  }
  return length;
}

char nthchar (object *string, int n) {
  object *arg = cdr(string);
  int top;
  if (sizeof(int) == 4) { top = n>>2; n = 3 - (n&3); }
  else { top = n>>1; n = 1 - (n&1); }
  for (int i=0; i<top; i++) {
    if (arg == NULL) return 0;
    arg = car(arg);
  }
  if (arg == NULL) return 0;
  return (arg->chars)>>(n*8) & 0xFF;
}

int gstr () {
  if (LastChar) {
    char temp = LastChar;
    LastChar = 0;
    return temp;
  }
  char c = nthchar(GlobalString, GlobalStringIndex++);
  if (c != 0) return c;
  return '\n'; // -1?
}

void pstr (char c) {
  buildstring(c, &GlobalStringIndex, &GlobalString);
}

// Lookup variable in environment

object *value (symbol_t n, object *env) {
  while (env != NULL) {
    object *pair = car(env);
    if (pair != NULL && car(pair)->name == n) return pair;
    env = cdr(env);
  }
  return nil;
}

bool boundp (object *var, object *env) {
  symbol_t varname = var->name;
  if (value(varname, env) != NULL) return true;
  if (value(varname, GlobalEnv) != NULL) return true;
  return false;
}

object *findvalue (object *var, object *env) {
  symbol_t varname = var->name;
  object *pair = value(varname, env);
  if (pair == NULL) pair = value(varname, GlobalEnv);
  if (pair == NULL) error(0, PSTR("unknown variable"), var);
  return pair;
}

// Handling closures

object *closure (int tc, symbol_t name, object *state, object *function, object *args, object **env) {
  int trace = 0;
  if (name) trace = tracing(name);
  if (trace) {
    indent(TraceDepth[trace-1]<<1, ' ', pserial);
    pint(TraceDepth[trace-1]++, pserial);
    pserial(':'); pserial(' '); pserial('('); pstring(symbolname(name), pserial);
  }
  object *params = first(function);
  function = cdr(function);
  // Dropframe
  if (tc) {
    if (*env != NULL && car(*env) == NULL) {
      pop(*env);
      while (*env != NULL && car(*env) != NULL) pop(*env);
    } else push(nil, *env);
  }
  // Push state
  while (state != NULL) {
    object *pair = first(state);
    push(pair, *env);
    state = cdr(state);
  }
  // Add arguments to environment
  bool optional = false;
  while (params != NULL) {
    object *value;
    object *var = first(params);
    if (symbolp(var) && var->name == OPTIONAL) optional = true;
    else {
      if (consp(var)) {
        if (!optional) error(name, PSTR("invalid default value"), var);
        if (args == NULL) value = eval(second(var), *env);
        else { value = first(args); args = cdr(args); }
        var = first(var);
        if (!symbolp(var)) error(name, PSTR("illegal optional parameter"), var);
      } else if (!symbolp(var)) {
        error2(name, PSTR("illegal parameter"));
      } else if (var->name == AMPREST) {
        params = cdr(params);
        var = first(params);
        value = args;
        args = NULL;
      } else {
        if (args == NULL) {
          if (optional) value = nil;
          else error2(name, toofewargs);
        } else { value = first(args); args = cdr(args); }
      }
      push(cons(var,value), *env);
      if (trace) { pserial(' '); printobject(value, pserial); }
    }
    params = cdr(params);
  }
  if (args != NULL) error2(name, toomanyargs);
  if (trace) { pserial(')'); pln(pserial); }
  // Do an implicit progn
  if (tc) push(nil, *env);
  return tf_progn(function, *env);
}

object *apply (symbol_t name, object *function, object *args, object *env) {
  if (symbolp(function)) {
    symbol_t fname = function->name;
    checkargs(fname, args);
    return ((fn_ptr_type)lookupfn(fname))(args, env);
  }
  if (consp(function) && issymbol(car(function), LAMBDA)) {
    function = cdr(function);
    object *result = closure(0, 0, NULL, function, args, &env);
    return eval(result, env);
  }
  if (consp(function) && issymbol(car(function), CLOSURE)) {
    function = cdr(function);
    object *result = closure(0, 0, car(function), cdr(function), args, &env);
    return eval(result, env);
  }
  error(name, PSTR("illegal function"), function);
  return NULL;
}

// In-place operations

object **place (symbol_t name, object *args, object *env) {
  if (atom(args)) return &cdr(findvalue(args, env));
  object* function = first(args);
  if (symbolp(function)) {
    symbol_t fname = function->name;
    if (fname == CAR || fname == FIRST) {
      object *value = eval(second(args), env);
      if (!listp(value)) error(name, PSTR("can't take car"), value);
      return &car(value);
    }
    if (fname == CDR || fname == REST) {
      object *value = eval(second(args), env);
      if (!listp(value)) error(name, PSTR("can't take cdr"), value);
      return &cdr(value);
    }
    if (fname == NTH) {
      int index = checkinteger(NTH, eval(second(args), env));
      object *list = eval(third(args), env);
      if (atom(list)) error(name, PSTR("second argument to nth is not a list"), list);
      while (index > 0) {
        list = cdr(list);
        if (list == NULL) error2(name, PSTR("index to nth is out of range"));
        index--;
      }
      return &car(list);
    }
  }
  error2(name, PSTR("illegal place"));
  return nil;
}

// Checked car and cdr

object *carx (object *arg) {
  if (!listp(arg)) error(0, PSTR("can't take car"), arg);
  if (arg == nil) return nil;
  return car(arg);
}

object *cdrx (object *arg) {
  if (!listp(arg)) error(0, PSTR("can't take cdr"), arg);
  if (arg == nil) return nil;
  return cdr(arg);
}

// I2C interface

uint8_t const TWI_SDA_PIN = 17;
uint8_t const TWI_SCL_PIN = 16;

uint32_t const F_TWI = 400000L;  // Hardware I2C clock in Hz
uint8_t const TWSR_MTX_DATA_ACK = 0x28;
uint8_t const TWSR_MTX_ADR_ACK = 0x18;
uint8_t const TWSR_MRX_ADR_ACK = 0x40;
uint8_t const TWSR_START = 0x08;
uint8_t const TWSR_REP_START = 0x10;
uint8_t const I2C_READ = 1;
uint8_t const I2C_WRITE = 0;

void I2Cinit(bool enablePullup) {
  TWSR = 0;                        // no prescaler
  TWBR = (F_CPU/F_TWI - 16)/2;     // set bit rate factor
  if (enablePullup) {
    digitalWrite(TWI_SDA_PIN, HIGH);
    digitalWrite(TWI_SCL_PIN, HIGH);
  }
}

int I2Cread() {
  if (I2CCount != 0) I2CCount--;
  TWCR = 1<<TWINT | 1<<TWEN | ((I2CCount == 0) ? 0 : (1<<TWEA));
  while (!(TWCR & 1<<TWINT));
  return TWDR;
}

bool I2Cwrite(uint8_t data) {
  TWDR = data;
  TWCR = 1<<TWINT | 1 << TWEN;
  while (!(TWCR & 1<<TWINT));
  return (TWSR & 0xF8) == TWSR_MTX_DATA_ACK;
}

bool I2Cstart(uint8_t address, uint8_t read) {
  uint8_t addressRW = address<<1 | read;
  TWCR = 1<<TWINT | 1<<TWSTA | 1<<TWEN;    // Send START condition
  while (!(TWCR & 1<<TWINT));
  if ((TWSR & 0xF8) != TWSR_START && (TWSR & 0xF8) != TWSR_REP_START) return false;
  TWDR = addressRW;  // send device address and direction
  TWCR = 1<<TWINT | 1<<TWEN;
  while (!(TWCR & 1<<TWINT));
  if (addressRW & I2C_READ) return (TWSR & 0xF8) == TWSR_MRX_ADR_ACK;
  else return (TWSR & 0xF8) == TWSR_MTX_ADR_ACK;
}

bool I2Crestart (uint8_t address, uint8_t read) {
  return I2Cstart(address, read);
}

void I2Cstop (uint8_t read) {
  (void) read;
  TWCR = 1<<TWINT | 1<<TWEN | 1<<TWSTO;
  while (TWCR & 1<<TWSTO); // wait until stop and bus released
}

// Streams

inline int spiread () { return SPI.transfer(0); }
inline int serial1read () { while (!Serial1.available()) testescape(); return Serial1.read(); }
#if defined(sdcardsupport)
File SDpfile, SDgfile;
inline int SDread () {
  if (LastChar) { 
    char temp = LastChar;
    LastChar = 0;
    return temp;
  }
  return SDgfile.read();
}
#endif

void serialbegin (int address, int baud) {
  if (address == 1) Serial1.begin((long)baud*100);
  else error(WITHSERIAL, PSTR("port not supported"), number(address));
}

void serialend (int address) {
  if (address == 1) {Serial1.flush(); Serial1.end(); }
}

gfun_t gstreamfun (object *args) {
  int streamtype = SERIALSTREAM;
  int address = 0;
  gfun_t gfun = gserial;
  if (args != NULL) {
    int stream = isstream(first(args));
    streamtype = stream>>8; address = stream & 0xFF;
  }
  if (streamtype == I2CSTREAM) gfun = (gfun_t)I2Cread;
  else if (streamtype == SPISTREAM) gfun = spiread;
  else if (streamtype == SERIALSTREAM) {
    if (address == 0) gfun = gserial;
    else if (address == 1) gfun = serial1read;
  }
  #if defined(sdcardsupport)
  else if (streamtype == SDSTREAM) gfun = (gfun_t)SDread;
  #endif
  else error2(0, PSTR("Unknown stream type"));
  return gfun;
}

inline void spiwrite (char c) { SPI.transfer(c); }
inline void serial1write (char c) { Serial1.write(c); }
#if defined(sdcardsupport)
inline void SDwrite (char c) { SDpfile.write(c); }
#endif

pfun_t pstreamfun (object *args) {
  int streamtype = SERIALSTREAM;
  int address = 0;
  pfun_t pfun = pserial;
  if (args != NULL && first(args) != NULL) {
    int stream = isstream(first(args));
    streamtype = stream>>8; address = stream & 0xFF;
  }
  if (streamtype == I2CSTREAM) pfun = (pfun_t)I2Cwrite;
  else if (streamtype == SPISTREAM) pfun = spiwrite;
  else if (streamtype == SERIALSTREAM) {
    if (address == 0) pfun = pserial;
    else if (address == 1) pfun = serial1write;
  }
  #if defined(sdcardsupport)
  else if (streamtype == SDSTREAM) pfun = (pfun_t)SDwrite;
  #endif
  else error2(0, PSTR("unknown stream type"));
  return pfun;
}

// Check pins

void checkanalogread (int pin) {
  if (!(pin>=0 && pin<=7)) error(ANALOGREAD, invalidpin, number(pin));
}

void checkanalogwrite (int pin) {
  if (!(pin==3 || pin==4 || pin==6 || pin==7 || (pin>=12 && pin<=15))) error(ANALOGWRITE, invalidpin, number(pin));
}

// Note

const int scale[] PROGMEM = {4186,4435,4699,4978,5274,5588,5920,6272,6645,7040,7459,7902};

void playnote (int pin, int note, int octave) {
  if (pin == 4) {
    int prescaler = 8 - octave - note/12;
    if (prescaler<0 || prescaler>8) error(NOTE, PSTR("octave out of range"), number(prescaler));
    tone(pin, pgm_read_word(&scale[note%12])>>prescaler);
  } else error(NOTE, PSTR("invalid pin"), number(pin));
}

void nonote (int pin) {
  noTone(pin);
}

// Sleep

// Interrupt vector for sleep watchdog
ISR(WDT_vect) {
  WDTCSR |= 1<<WDIE;
}

void initsleep () {
  set_sleep_mode(SLEEP_MODE_PWR_DOWN);
}

void sleep (int secs) {
  // Set up Watchdog timer for 1 Hz interrupt
  WDTCSR = 1<<WDCE | 1<<WDE;
  WDTCSR = 1<<WDIE | 6<<WDP0;     // 1 sec interrupt
  delay(100);  // Give serial time to settle
  // Disable ADC and timer 0
  ADCSRA = ADCSRA & ~(1<<ADEN);
  PRR0 = PRR0 | 1<<PRTIM0;
  while (secs > 0) {
    sleep_enable();
    sleep_cpu();
    secs--;
  }
  WDTCSR = 1<<WDCE | 1<<WDE;     // Disable watchdog
  WDTCSR = 0;
  // Enable ADC and timer 0
  ADCSRA = ADCSRA | 1<<ADEN;
  PRR0 = PRR0 & ~(1<<PRTIM0);
}

// Prettyprint

const int PPINDENT = 2;
const int PPWIDTH = 42;

void pcount (char c) {
  if (c == '\n') PrintCount++;
  PrintCount++;
}

uint8_t atomwidth (object *obj) {
  PrintCount = 0;
  printobject(obj, pcount);
  return PrintCount;
}

uint8_t hexwidth (object *obj) {
  PrintCount = 0;
  pinthex(obj->integer, pcount);
  return PrintCount;
}

boolean quoted (object *obj) {
  return (consp(obj) && car(obj) != NULL && car(obj)->name == QUOTE && consp(cdr(obj)) && cddr(obj) == NULL);
}

int subwidth (object *obj, int w) {
  if (atom(obj)) return w - atomwidth(obj);
  if (quoted(obj)) return subwidthlist(car(cdr(obj)), w - 1);
  return subwidthlist(obj, w - 1);
}

int subwidthlist (object *form, int w) {
  while (form != NULL && w >= 0) {
    if (atom(form)) return w - (2 + atomwidth(form));
    w = subwidth(car(form), w - 1);
    form = cdr(form);
  }
  return w;
}

void superprint (object *form, int lm, pfun_t pfun) {
  if (atom(form)) {
    if (symbolp(form) && form->name == NOTHING) pstring(symbolname(form->name), pfun);
    else printobject(form, pfun);
  }
  else if (quoted(form)) { pfun('\''); superprint(car(cdr(form)), lm + 1, pfun); }
  else if (subwidth(form, PPWIDTH - lm) >= 0) supersub(form, lm + PPINDENT, 0, pfun);
  else supersub(form, lm + PPINDENT, 1, pfun);
}

const int ppspecials = 15;
const uint8_t ppspecial[ppspecials] PROGMEM = 
  { DOTIMES, DOLIST, IF, SETQ, TEE, LET, LETSTAR, LAMBDA, WHEN, UNLESS, WITHI2C, WITHSERIAL, WITHSPI, WITHSDCARD, FORMILLIS };

void supersub (object *form, int lm, int super, pfun_t pfun) {
  int special = 0, separate = 1;
  object *arg = car(form);
  if (symbolp(arg)) {
    int name = arg->name;
    if (name == DEFUN) special = 2;
    else for (int i=0; i<ppspecials; i++) {
      #if defined(__AVR_ATmega4809__) || defined(ARDUINO_AVR_ATmega4809)
      if (name == ppspecial[i]) { special = 1; break; }    
      #else
      if (name == pgm_read_byte(&ppspecial[i])) { special = 1; break; }
      #endif   
    } 
  }
  while (form != NULL) {
    if (atom(form)) { pfstring(PSTR(" . "), pfun); printobject(form, pfun); pfun(')'); return; }
    else if (separate) { pfun('('); separate = 0; }
    else if (special) { pfun(' '); special--; }
    else if (!super) pfun(' ');
    else { pln(pfun); indent(lm, ' ', pfun); }
    superprint(car(form), lm, pfun);
    form = cdr(form);
  }
  pfun(')'); return;
}

// Special forms

object *sp_quote (object *args, object *env) {
  (void) env;
  checkargs(QUOTE, args);
  return first(args);
}

object *sp_defun (object *args, object *env) {
  (void) env;
  checkargs(DEFUN, args);
  object *var = first(args);
  if (!symbolp(var)) error(DEFUN, notasymbol, var);
  object *val = cons(symbol(LAMBDA), cdr(args));
  object *pair = value(var->name,GlobalEnv);
  if (pair != NULL) cdr(pair) = val;
  else push(cons(var, val), GlobalEnv);
  return var;
}

object *sp_defvar (object *args, object *env) {
  checkargs(DEFVAR, args);
  object *var = first(args);
  if (!symbolp(var)) error(DEFVAR, notasymbol, var);
  object *val = NULL;
  args = cdr(args);
  if (args != NULL) { setflag(NOESC); val = eval(first(args), env); clrflag(NOESC); }
  object *pair = value(var->name, GlobalEnv);
  if (pair != NULL) cdr(pair) = val;
  else push(cons(var, val), GlobalEnv);
  return var;
}

object *sp_setq (object *args, object *env) {
  object *arg = nil;
  while (args != NULL) {
    if (cdr(args) == NULL) error2(SETQ, oddargs);
    object *pair = findvalue(first(args), env);
    arg = eval(second(args), env);
    cdr(pair) = arg;
    args = cddr(args);
  }
  return arg;
}

object *sp_loop (object *args, object *env) {
  object *start = args;
  for (;;) {
    args = start;
    while (args != NULL) {
      object *result = eval(car(args),env);
      if (tstflag(RETURNFLAG)) {
        clrflag(RETURNFLAG);
        return result;
      }
      args = cdr(args);
    }
  }
}

object *sp_return (object *args, object *env) {
  object *result = eval(tf_progn(args,env), env);
  setflag(RETURNFLAG);
  return result;
}

object *sp_push (object *args, object *env) {
  checkargs(PUSH, args); 
  object *item = eval(first(args), env);
  object **loc = place(PUSH, second(args), env);
  push(item, *loc);
  return *loc;
}

object *sp_pop (object *args, object *env) {
  checkargs(POP, args); 
  object **loc = place(POP, first(args), env);
  object *result = car(*loc);
  pop(*loc);
  return result;
}

// Accessors

object *incfdecf (symbol_t name, object *args, int increment, object *env) {
  checkargs(name, args); 
  object **loc = place(name, first(args), env);
  int result = checkinteger(name, *loc);
  args = cdr(args);
  if (args != NULL) increment = checkinteger(name, eval(first(args), env)) * increment;
  #if defined(checkoverflow)
  if (increment < 1) { if (INT_MIN - increment > result) error2(name, overflow); }
  else { if (INT_MAX - increment < result) error2(name, overflow); }
  #endif
  result = result + increment;
  *loc = number(result);
  return *loc;
}

object *sp_incf (object *args, object *env) {
  incfdecf(INCF, args, 1, env);
}

object *sp_decf (object *args, object *env) {
  incfdecf(DECF, args, -1, env);
}

object *sp_setf (object *args, object *env) {
  object *arg = nil;
  while (args != NULL) {
    if (cdr(args) == NULL) error2(SETF, oddargs);
    object **loc = place(SETF, first(args), env);
    arg = eval(second(args), env);
    *loc = arg;
    args = cddr(args);
  }
  return arg;
}

// Other special forms

object *sp_dolist (object *args, object *env) {
  if (args == NULL) error2(DOLIST, noargument);
  object *params = first(args);
  object *var = first(params);
  object *list = eval(second(params), env);
  push(list, GCStack); // Don't GC the list
  object *pair = cons(var,nil);
  push(pair,env);
  params = cdr(cdr(params));
  args = cdr(args);
  while (list != NULL) {
    if (improperp(list)) error(DOLIST, notproper, list);
    cdr(pair) = first(list);
    object *forms = args;
    while (forms != NULL) {
      object *result = eval(car(forms), env);
      if (tstflag(RETURNFLAG)) {
        clrflag(RETURNFLAG);
        pop(GCStack);
        return result;
      }
      forms = cdr(forms);
    }
    list = cdr(list);
  }
  cdr(pair) = nil;
  pop(GCStack);
  if (params == NULL) return nil;
  return eval(car(params), env);
}

object *sp_dotimes (object *args, object *env) {
  if (args == NULL) error2(DOTIMES, noargument);
  object *params = first(args);
  object *var = first(params);
  int count = checkinteger(DOTIMES, eval(second(params), env));
  int index = 0;
  params = cdr(cdr(params));
  object *pair = cons(var,number(0));
  push(pair,env);
  args = cdr(args);
  while (index < count) {
    cdr(pair) = number(index);
    object *forms = args;
    while (forms != NULL) {
      object *result = eval(car(forms), env);
      if (tstflag(RETURNFLAG)) {
        clrflag(RETURNFLAG);
        return result;
      }
      forms = cdr(forms);
    }
    index++;
  }
  cdr(pair) = number(index);
  if (params == NULL) return nil;
  return eval(car(params), env);
}

object *sp_trace (object *args, object *env) {
  (void) env;
  while (args != NULL) {
      trace(first(args)->name);
      args = cdr(args);
  }
  int i = 0;
  while (i < TRACEMAX) {
    if (TraceFn[i] != 0) args = cons(symbol(TraceFn[i]), args);
    i++;
  }
  return args;
}

object *sp_untrace (object *args, object *env) {
  (void) env;
  if (args == NULL) {
    int i = 0;
    while (i < TRACEMAX) {
      if (TraceFn[i] != 0) args = cons(symbol(TraceFn[i]), args);
      TraceFn[i] = 0;
      i++;
    }
  } else {
    while (args != NULL) {
      untrace(first(args)->name);
      args = cdr(args);
    }
  }
  return args;
}

object *sp_formillis (object *args, object *env) {
  object *param = first(args);
  unsigned long start = millis();
  unsigned long now, total = 0;
  if (param != NULL) total = checkinteger(FORMILLIS, eval(first(param), env));
  eval(tf_progn(cdr(args),env), env);
  do {
    now = millis() - start;
    testescape();
  } while (now < total);
  if (now <= INT_MAX) return number(now);
  return nil;
}

object *sp_withserial (object *args, object *env) {
  object *params = first(args);
  if (params == NULL) error2(WITHSERIAL, nostream);
  object *var = first(params);
  int address = checkinteger(WITHSERIAL, eval(second(params), env));
  params = cddr(params);
  int baud = 96;
  if (params != NULL) baud = checkinteger(WITHSERIAL, eval(first(params), env));
  object *pair = cons(var, stream(SERIALSTREAM, address));
  push(pair,env);
  serialbegin(address, baud);
  object *forms = cdr(args);
  object *result = eval(tf_progn(forms,env), env);
  serialend(address);
  return result;
}

object *sp_withi2c (object *args, object *env) {
  object *params = first(args);
  if (params == NULL) error2(WITHI2C, nostream);
  object *var = first(params);
  int address = checkinteger(WITHI2C, eval(second(params), env));
  params = cddr(params);
  int read = 0; // Write
  I2CCount = 0;
  if (params != NULL) {
    object *rw = eval(first(params), env);
    if (integerp(rw)) I2CCount = rw->integer;
    read = (rw != NULL);
  }
  I2Cinit(1); // Pullups
  object *pair = cons(var, (I2Cstart(address, read)) ? stream(I2CSTREAM, address) : nil);
  push(pair,env);
  object *forms = cdr(args);
  object *result = eval(tf_progn(forms,env), env);
  I2Cstop(read);
  return result;
}

object *sp_withspi (object *args, object *env) {
  object *params = first(args);
  if (params == NULL) error2(WITHSPI, nostream);
  object *var = first(params);
  params = cdr(params);
  if (params == NULL) error2(WITHSPI, nostream);
  int pin = checkinteger(WITHSPI, eval(car(params), env));
  pinMode(pin, OUTPUT);
  digitalWrite(pin, HIGH);
  params = cdr(params);
  int clock = 4000, mode = SPI_MODE0; // Defaults
  BitOrder bitorder = MSBFIRST;
  if (params != NULL) {
    clock = checkinteger(WITHSPI, eval(car(params), env));
    params = cdr(params);
    if (params != NULL) {
      bitorder = (checkinteger(WITHSPI, eval(car(params), env)) == 0) ? LSBFIRST : MSBFIRST;
      params = cdr(params);
      if (params != NULL) {
        int modeval = checkinteger(WITHSPI, eval(car(params), env));
        mode = (modeval == 3) ? SPI_MODE3 : (modeval == 2) ? SPI_MODE2 : (modeval == 1) ? SPI_MODE1 : SPI_MODE0;
      }
    }
  }
  object *pair = cons(var, stream(SPISTREAM, pin));
  push(pair,env);
  SPI.begin();
  SPI.beginTransaction(SPISettings(((unsigned long)clock * 1000), bitorder, mode));
  digitalWrite(pin, LOW);
  object *forms = cdr(args);
  object *result = eval(tf_progn(forms,env), env);
  digitalWrite(pin, HIGH);
  SPI.endTransaction();
  return result;
}

object *sp_withsdcard (object *args, object *env) {
#if defined(sdcardsupport)
  object *params = first(args);
  if (params == NULL) error2(WITHSDCARD, nostream);
  object *var = first(params);
  object *filename = eval(second(params), env);
  params = cddr(params);
  SD.begin(SDCARD_SS_PIN);
  int mode = 0;
  if (params != NULL && first(params) != NULL) mode = checkinteger(WITHSDCARD, first(params));
  int oflag = O_READ;
  if (mode == 1) oflag = O_RDWR | O_CREAT | O_APPEND; else if (mode == 2) oflag = O_RDWR | O_CREAT | O_TRUNC;
  if (mode >= 1) {
    SDpfile = SD.open(MakeFilename(filename), oflag);
    if (!SDpfile) error2(WITHSDCARD, PSTR("problem writing to SD card"));
  } else {
    SDgfile = SD.open(MakeFilename(filename), oflag);
    if (!SDgfile) error2(WITHSDCARD, PSTR("problem reading from SD card"));
  }
  object *pair = cons(var, stream(SDSTREAM, 1));
  push(pair,env);
  object *forms = cdr(args);
  object *result = eval(tf_progn(forms,env), env);
  if (mode >= 1) SDpfile.close(); else SDgfile.close();
  return result;
#else
  (void) args, (void) env;
  error2(WITHSDCARD, PSTR("not supported"));
  return nil;
#endif
}

// Tail-recursive forms

object *tf_progn (object *args, object *env) {
  if (args == NULL) return nil;
  object *more = cdr(args);
  while (more != NULL) {
    object *result = eval(car(args),env);
    if (tstflag(RETURNFLAG)) return result;
    args = more;
    more = cdr(args);
  }
  return car(args);
}

object *tf_if (object *args, object *env) {
  if (args == NULL || cdr(args) == NULL) error2(IF, PSTR("missing argument(s)"));
  if (eval(first(args), env) != nil) return second(args);
  args = cddr(args);
  return (args != NULL) ? first(args) : nil;
}

object *tf_cond (object *args, object *env) {
  while (args != NULL) {
    object *clause = first(args);
    if (!consp(clause)) error(COND, PSTR("illegal clause"), clause);
    object *test = eval(first(clause), env);
    object *forms = cdr(clause);
    if (test != nil) {
      if (forms == NULL) return quote(test); else return tf_progn(forms, env);
    }
    args = cdr(args);
  }
  return nil;
}

object *tf_when (object *args, object *env) {
  if (args == NULL) error2(WHEN, noargument);
  if (eval(first(args), env) != nil) return tf_progn(cdr(args),env);
  else return nil;
}

object *tf_unless (object *args, object *env) {
  if (args == NULL) error2(UNLESS, noargument);
  if (eval(first(args), env) != nil) return nil;
  else return tf_progn(cdr(args),env);
}

object *tf_case (object *args, object *env) {
  object *test = eval(first(args), env);
  args = cdr(args);
  while (args != NULL) {
    object *clause = first(args);
    if (!consp(clause)) error(CASE, PSTR("illegal clause"), clause);
    object *key = car(clause);
    object *forms = cdr(clause);
    if (consp(key)) {
      while (key != NULL) {
        if (eq(test,car(key))) return tf_progn(forms, env);
        key = cdr(key);
      }
    } else if (eq(test,key) || eq(key,tee)) return tf_progn(forms, env);
    args = cdr(args);
  }
  return nil;
}

object *tf_and (object *args, object *env) {
  if (args == NULL) return tee;
  object *more = cdr(args);
  while (more != NULL) {
    if (eval(car(args), env) == NULL) return nil;
    args = more;
    more = cdr(args);
  }
  return car(args);
}

object *tf_or (object *args, object *env) {
  while (args != NULL) {
    if (eval(car(args), env) != NULL) return car(args);
    args = cdr(args);
  }
  return nil;
}

// Core functions

object *fn_not (object *args, object *env) {
  (void) env;
  return (first(args) == nil) ? tee : nil;
}

object *fn_cons (object *args, object *env) {
  (void) env;
  return cons(first(args), second(args));
}

object *fn_atom (object *args, object *env) {
  (void) env;
  return atom(first(args)) ? tee : nil;
}

object *fn_listp (object *args, object *env) {
  (void) env;
  return listp(first(args)) ? tee : nil;
}

object *fn_consp (object *args, object *env) {
  (void) env;
  return consp(first(args)) ? tee : nil;
}

object *fn_symbolp (object *args, object *env) {
  (void) env;
  object *arg = first(args);
  return symbolp(arg) ? tee : nil;
}

object *fn_boundp (object *args, object *env) {
  (void) env;
  object *var = first(args);
  if (!symbolp(var)) error(BOUNDP, notasymbol, var);
  return boundp(var, env) ? tee : nil;
}

object *fn_setfn (object *args, object *env) {
  object *arg = nil;
  while (args != NULL) {
    if (cdr(args) == NULL) error2(SETFN, oddargs);
    object *pair = findvalue(first(args), env);
    arg = second(args);
    cdr(pair) = arg;
    args = cddr(args);
  }
  return arg;
}

object *fn_streamp (object *args, object *env) {
  (void) env;
  object *arg = first(args);
  return streamp(arg) ? tee : nil;
}

object *fn_eq (object *args, object *env) {
  (void) env;
  return eq(first(args), second(args)) ? tee : nil;
}

// List functions

object *fn_car (object *args, object *env) {
  (void) env;
  return carx(first(args));
}

object *fn_cdr (object *args, object *env) {
  (void) env;
  return cdrx(first(args));
}

object *fn_caar (object *args, object *env) {
  (void) env;
  return carx(carx(first(args)));
}

object *fn_cadr (object *args, object *env) {
  (void) env;
  return carx(cdrx(first(args)));
}

object *fn_cdar (object *args, object *env) {
  (void) env;
  return cdrx(carx(first(args)));
}

object *fn_cddr (object *args, object *env) {
  (void) env;
  return cdrx(cdrx(first(args)));
}

object *fn_caaar (object *args, object *env) {
  (void) env;
  return carx(carx(carx(first(args))));
}

object *fn_caadr (object *args, object *env) {
  (void) env;
  return carx(carx(cdrx(first(args))));
}

object *fn_cadar (object *args, object *env) {
  (void) env;
  return carx(cdrx(carx(first(args))));
}

object *fn_caddr (object *args, object *env) {
  (void) env;
  return carx(cdrx(cdrx(first(args))));
}

object *fn_cdaar (object *args, object *env) {
  (void) env;
  return cdrx(carx(carx(first(args))));
}

object *fn_cdadr (object *args, object *env) {
  (void) env;
  return cdrx(carx(cdrx(first(args))));
}

object *fn_cddar (object *args, object *env) {
  (void) env;
  return cdrx(cdrx(carx(first(args))));
}

object *fn_cdddr (object *args, object *env) {
  (void) env;
  return cdrx(cdrx(cdrx(first(args))));
}

object *fn_length (object *args, object *env) {
  (void) env;
  object *arg = first(args);
  if (listp(arg)) return number(listlength(LENGTH, arg));
  if (!stringp(arg)) error(LENGTH, PSTR("argument is not a list or string"), arg);
  return number(stringlength(arg));
}

object *fn_list (object *args, object *env) {
  (void) env;
  return args;
}

object *fn_reverse (object *args, object *env) {
  (void) env;
  object *list = first(args);
  object *result = NULL;
  while (list != NULL) {
    if (improperp(list)) error(REVERSE, notproper, list);
    push(first(list),result);
    list = cdr(list);
  }
  return result;
}

object *fn_nth (object *args, object *env) {
  (void) env;
  int n = checkinteger(NTH, first(args));
  object *list = second(args);
  while (list != NULL) {
    if (improperp(list)) error(NTH, notproper, list);
    if (n == 0) return car(list);
    list = cdr(list);
    n--;
  }
  return nil;
}

object *fn_assoc (object *args, object *env) {
  (void) env;
  object *key = first(args);
  object *list = second(args);
  return assoc(key,list);
}

object *fn_member (object *args, object *env) {
  (void) env;
  object *item = first(args);
  object *list = second(args);
  while (list != NULL) {
    if (improperp(list)) error(MEMBER, notproper, list);
    if (eq(item,car(list))) return list;
    list = cdr(list);
  }
  return nil;
}

object *fn_apply (object *args, object *env) {
  object *previous = NULL;
  object *last = args;
  while (cdr(last) != NULL) {
    previous = last;
    last = cdr(last);
  }
  object *arg = car(last);
  if (!listp(arg)) error(APPLY, notalist, arg);
  cdr(previous) = arg;
  return apply(APPLY, first(args), cdr(args), env);
}

object *fn_funcall (object *args, object *env) {
  return apply(FUNCALL, first(args), cdr(args), env);
}

object *fn_append (object *args, object *env) {
  (void) env;
  object *head = NULL;
  object *tail;
  while (args != NULL) {
    object *list = first(args);
    if (!listp(list)) error(APPEND, notalist, list);
    while (consp(list)) {
      object *obj = cons(car(list), cdr(list));
      if (head == NULL) head = obj;
      else cdr(tail) = obj;
      tail = obj;
      list = cdr(list);
      if (cdr(args) != NULL && improperp(list)) error(APPEND, notproper, first(args));
    }
    args = cdr(args);
  }
  return head;
}

object *fn_mapc (object *args, object *env) {
  object *function = first(args);
  args = cdr(args);
  object *result = first(args);
  object *params = cons(NULL, NULL);
  push(params,GCStack);
  // Make parameters
  while (true) {
    object *tailp = params;
    object *lists = args;
    while (lists != NULL) {
      object *list = car(lists);
      if (list == NULL) {
         pop(GCStack);
         return result;
      }
      if (improperp(list)) error(MAPC, notproper, list);
      object *obj = cons(first(list),NULL);
      car(lists) = cdr(list);
      cdr(tailp) = obj; tailp = obj;
      lists = cdr(lists);
    }
    apply(MAPC, function, cdr(params), env);
  }
}

object *fn_mapcar (object *args, object *env) {
  object *function = first(args);
  args = cdr(args);
  object *params = cons(NULL, NULL);
  push(params,GCStack);
  object *head = cons(NULL, NULL);
  push(head,GCStack);
  object *tail = head;
  // Make parameters
  while (true) {
    object *tailp = params;
    object *lists = args;
    while (lists != NULL) {
      object *list = car(lists);
      if (list == NULL) {
         pop(GCStack);
         pop(GCStack);
         return cdr(head);
      }
      if (improperp(list)) error(MAPCAR, notproper, list);
      object *obj = cons(first(list),NULL);
      car(lists) = cdr(list);
      cdr(tailp) = obj; tailp = obj;
      lists = cdr(lists);
    }
    object *result = apply(MAPCAR, function, cdr(params), env);
    object *obj = cons(result,NULL);
    cdr(tail) = obj; tail = obj;
  }
}

object *fn_mapcan (object *args, object *env) {
  object *function = first(args);
  args = cdr(args);
  object *params = cons(NULL, NULL);
  push(params,GCStack);
  object *head = cons(NULL, NULL);
  push(head,GCStack);
  object *tail = head;
  // Make parameters
  while (true) {
    object *tailp = params;
    object *lists = args;
    while (lists != NULL) {
      object *list = car(lists);
      if (list == NULL) {
         pop(GCStack);
         pop(GCStack);
         return cdr(head);
      }
      if (improperp(list)) error(MAPCAN, notproper, list);
      object *obj = cons(first(list),NULL);
      car(lists) = cdr(list);
      cdr(tailp) = obj; tailp = obj;
      lists = cdr(lists);
    }
    object *result = apply(MAPCAN, function, cdr(params), env);
    while (consp(result)) {
      cdr(tail) = result; tail = result;
      result = cdr(result);
    }
    if (result != NULL) error(MAPCAN, resultproper, result);
  }
}

// Arithmetic functions

object *fn_add (object *args, object *env) {
  (void) env;
  int result = 0;
  while (args != NULL) {
    int temp = checkinteger(ADD, car(args));
    #if defined(checkoverflow)
    if (temp < 1) { if (INT_MIN - temp > result) error2(ADD, overflow); }
    else { if (INT_MAX - temp < result) error2(ADD, overflow); }
    #endif
    result = result + temp;
    args = cdr(args);
  }
  return number(result);
}

object *fn_subtract (object *args, object *env) {
  (void) env;
  int result = checkinteger(SUBTRACT, car(args));
  args = cdr(args);
  if (args == NULL) {
    #if defined(checkoverflow)
    if (result == INT_MIN) error2(SUBTRACT, overflow);
    #endif
    return number(-result);
  }
  while (args != NULL) {
    int temp = checkinteger(SUBTRACT, car(args));
    #if defined(checkoverflow)
    if (temp < 1) { if (INT_MAX + temp < result) error2(SUBTRACT, overflow); }
    else { if (INT_MIN + temp > result) error2(SUBTRACT, overflow); }
    #endif
    result = result - temp;
    args = cdr(args);
  }
  return number(result);
}

object *fn_multiply (object *args, object *env) {
  (void) env;
  int result = 1;
  while (args != NULL){
    #if defined(checkoverflow)
    signed long temp = (signed long) result * checkinteger(MULTIPLY, car(args));
    if ((temp > INT_MAX) || (temp < INT_MIN)) error2(MULTIPLY, overflow);
    result = temp;
    #else
    result = result * checkinteger(MULTIPLY, car(args));
    #endif
    args = cdr(args);
  }
  return number(result);
}

object *fn_divide (object *args, object *env) {
  (void) env;
  int result = checkinteger(DIVIDE, first(args));
  args = cdr(args);
  while (args != NULL) {
    int arg = checkinteger(DIVIDE, car(args));
    if (arg == 0) error2(DIVIDE, PSTR("division by zero"));
    #if defined(checkoverflow)
    if ((result == INT_MIN) && (arg == -1)) error2(DIVIDE, overflow);
    #endif
    result = result / arg;
    args = cdr(args);
  }
  return number(result);
}

object *fn_mod (object *args, object *env) {
  (void) env;
  int arg1 = checkinteger(MOD, first(args));
  int arg2 = checkinteger(MOD, second(args));
  if (arg2 == 0) error2(MOD, PSTR("division by zero"));
  int r = arg1 % arg2;
  if ((arg1<0) != (arg2<0)) r = r + arg2;
  return number(r);
}

object *fn_oneplus (object *args, object *env) {
  (void) env;
  int result = checkinteger(ONEPLUS, first(args));
  #if defined(checkoverflow)
  if (result == INT_MAX) error2(ONEPLUS, overflow);
  #endif
  return number(result + 1);
}

object *fn_oneminus (object *args, object *env) {
  (void) env;
  int result = checkinteger(ONEMINUS, first(args));
  #if defined(checkoverflow)
  if (result == INT_MIN) error2(ONEMINUS, overflow);
  #endif
  return number(result - 1);
}

object *fn_abs (object *args, object *env) {
  (void) env;
  int result = checkinteger(ABS, first(args));
  #if defined(checkoverflow)
  if (result == INT_MIN) error2(ABS, overflow);
  #endif
  return number(abs(result));
}

object *fn_random (object *args, object *env) {
  (void) env;
  int arg = checkinteger(RANDOM, first(args));
  return number(random(arg));
}

object *fn_maxfn (object *args, object *env) {
  (void) env;
  int result = checkinteger(MAXFN, first(args));
  args = cdr(args);
  while (args != NULL) {
    int next = checkinteger(MAXFN, car(args));
    if (next > result) result = next;
    args = cdr(args);
  }
  return number(result);
}

object *fn_minfn (object *args, object *env) {
  (void) env;
  int result = checkinteger(MINFN, first(args));
  args = cdr(args);
  while (args != NULL) {
    int next = checkinteger(MINFN, car(args));
    if (next < result) result = next;
    args = cdr(args);
  }
  return number(result);
}

// Arithmetic comparisons

object *fn_noteq (object *args, object *env) {
  (void) env;
  while (args != NULL) {   
    object *nargs = args;
    int arg1 = checkinteger(NOTEQ, first(nargs));
    nargs = cdr(nargs);
    while (nargs != NULL) {
       int arg2 = checkinteger(NOTEQ, first(nargs));
       if (arg1 == arg2) return nil;
       nargs = cdr(nargs);
    }
    args = cdr(args);
  }
  return tee;
}

object *fn_numeq (object *args, object *env) {
  (void) env;
  int arg1 = checkinteger(NUMEQ, first(args));
  args = cdr(args);
  while (args != NULL) {
    int arg2 = checkinteger(NUMEQ, first(args));
    if (!(arg1 == arg2)) return nil;
    arg1 = arg2;
    args = cdr(args);
  }
  return tee;
}

object *fn_less (object *args, object *env) {
  (void) env;
  int arg1 = checkinteger(LESS, first(args));
  args = cdr(args);
  while (args != NULL) {
    int arg2 = checkinteger(LESS, first(args));
    if (!(arg1 < arg2)) return nil;
    arg1 = arg2;
    args = cdr(args);
  }
  return tee;
}

object *fn_lesseq (object *args, object *env) {
  (void) env;
  int arg1 = checkinteger(LESSEQ, first(args));
  args = cdr(args);
  while (args != NULL) {
    int arg2 = checkinteger(LESSEQ, first(args));
    if (!(arg1 <= arg2)) return nil;
    arg1 = arg2;
    args = cdr(args);
  }
  return tee;
}

object *fn_greater (object *args, object *env) {
  (void) env;
  int arg1 = checkinteger(GREATER, first(args));
  args = cdr(args);
  while (args != NULL) {
    int arg2 = checkinteger(GREATER, first(args));
    if (!(arg1 > arg2)) return nil;
    arg1 = arg2;
    args = cdr(args);
  }
  return tee;
}

object *fn_greatereq (object *args, object *env) {
  (void) env;
  int arg1 = checkinteger(GREATEREQ, first(args));
  args = cdr(args);
  while (args != NULL) {
    int arg2 = checkinteger(GREATEREQ, first(args));
    if (!(arg1 >= arg2)) return nil;
    arg1 = arg2;
    args = cdr(args);
  }
  return tee;
}

object *fn_plusp (object *args, object *env) {
  (void) env;
  int arg = checkinteger(PLUSP, first(args));
  if (arg > 0) return tee;
  else return nil;
}

object *fn_minusp (object *args, object *env) {
  (void) env;
  int arg = checkinteger(MINUSP, first(args));
  if (arg < 0) return tee;
  else return nil;
}

object *fn_zerop (object *args, object *env) {
  (void) env;
  int arg = checkinteger(ZEROP, first(args));
  return (arg == 0) ? tee : nil;
}

object *fn_oddp (object *args, object *env) {
  (void) env;
  int arg = checkinteger(ODDP, first(args));
  return ((arg & 1) == 1) ? tee : nil;
}

object *fn_evenp (object *args, object *env) {
  (void) env;
  int arg = checkinteger(EVENP, first(args));
  return ((arg & 1) == 0) ? tee : nil;
}

// Number functions

object *fn_integerp (object *args, object *env) {
  (void) env;
  return integerp(first(args)) ? tee : nil;
}

// Characters

object *fn_char (object *args, object *env) {
  (void) env;
  object *arg = first(args);
  if (!stringp(arg)) error(CHAR, notastring, arg);
  char c = nthchar(arg, checkinteger(CHAR, second(args)));
  if (c == 0) error2(CHAR, PSTR("index out of range"));
  return character(c);
}

object *fn_charcode (object *args, object *env) {
  (void) env;
  return number(checkchar(CHARCODE, first(args)));
}

object *fn_codechar (object *args, object *env) {
  (void) env;
  return character(checkinteger(CODECHAR, first(args)));
}

object *fn_characterp (object *args, object *env) {
  (void) env;
  return characterp(first(args)) ? tee : nil;
}

// Strings

object *fn_stringp (object *args, object *env) {
  (void) env;
  return stringp(first(args)) ? tee : nil;
}

bool stringcompare (symbol_t name, object *args, bool lt, bool gt, bool eq) {
  object *arg1 = first(args); if (!stringp(arg1)) error(name, notastring, arg1);
  object *arg2 = second(args); if (!stringp(arg2)) error(name, notastring, arg2);
  arg1 = cdr(arg1);
  arg2 = cdr(arg2);
  while ((arg1 != NULL) || (arg2 != NULL)) {
    if (arg1 == NULL) return lt;
    if (arg2 == NULL) return gt;
    if (arg1->chars < arg2->chars) return lt;
    if (arg1->chars > arg2->chars) return gt;
    arg1 = car(arg1);
    arg2 = car(arg2);
  }
  return eq;
}

object *fn_stringeq (object *args, object *env) {
  (void) env;
  return stringcompare(STRINGEQ, args, false, false, true) ? tee : nil;
}

object *fn_stringless (object *args, object *env) {
  (void) env;
  return stringcompare(STRINGLESS, args, true, false, false) ? tee : nil;
}

object *fn_stringgreater (object *args, object *env) {
  (void) env;
  return stringcompare(STRINGGREATER, args, false, true, false) ? tee : nil;
}

object *fn_sort (object *args, object *env) {
  if (first(args) == NULL) return nil;
  object *list = cons(nil,first(args));
  push(list,GCStack);
  object *predicate = second(args);
  object *compare = cons(NULL, cons(NULL, NULL));
  push(compare,GCStack);
  object *ptr = cdr(list);
  while (cdr(ptr) != NULL) {
    object *go = list;
    while (go != ptr) {
      car(compare) = car(cdr(ptr));
      car(cdr(compare)) = car(cdr(go));
      if (apply(SORT, predicate, compare, env)) break;
      go = cdr(go);
    }
    if (go != ptr) {
      object *obj = cdr(ptr);
      cdr(ptr) = cdr(obj);
      cdr(obj) = cdr(go);
      cdr(go) = obj;
    } else ptr = cdr(ptr);
  }
  pop(GCStack); pop(GCStack);
  return cdr(list);
}

object *fn_stringfn (object *args, object *env) {
  (void) env;
  object *arg = first(args);
  int type = arg->type;
  if (type == STRING) return arg;
  object *obj = myalloc();
  obj->type = STRING;
  if (type == CHARACTER) {
    object *cell = myalloc();
    cell->car = NULL;
    uint8_t shift = (sizeof(int)-1)*8;
    cell->chars = (arg->chars)<<shift;
    obj->cdr = cell;
  } else if (type == SYMBOL) {
    char *s = symbolname(arg->name);
    char ch = *s++;
    object *head = NULL;
    int chars = 0;
    while (ch) {
      if (ch == '\\') ch = *s++;
      buildstring(ch, &chars, &head);
      ch = *s++;
    }
    obj->cdr = head;
  } else error(STRINGFN, PSTR("can't convert to string"), arg);
  return obj;
}

object *fn_concatenate (object *args, object *env) {
  (void) env;
  object *arg = first(args);
  symbol_t name = arg->name;
  if (name != STRINGFN) error2(CONCATENATE, PSTR("only supports strings"));
  args = cdr(args);
  object *result = myalloc();
  result->type = STRING;
  object *head = NULL;
  int chars = 0;
  while (args != NULL) {
    object *obj = first(args);
    if (!stringp(obj)) error(CONCATENATE, notastring, obj);
    obj = cdr(obj);
    while (obj != NULL) {
      int quad = obj->chars;
      while (quad != 0) {
         char ch = quad>>((sizeof(int)-1)*8) & 0xFF;
         buildstring(ch, &chars, &head);
         quad = quad<<8;
      }
      obj = car(obj);
    }
    args = cdr(args);
  }
  result->cdr = head;
  return result;
}

object *fn_subseq (object *args, object *env) {
  (void) env;
  object *arg = first(args);
  if (!stringp(arg)) error(SUBSEQ, notastring, arg);
  int start = checkinteger(SUBSEQ, second(args));
  int end;
  args = cddr(args);
  if (args != NULL) end = checkinteger(SUBSEQ, car(args)); else end = stringlength(arg);
  object *result = myalloc();
  result->type = STRING;
  object *head = NULL;
  int chars = 0;
  for (int i=start; i<end; i++) {
    char ch = nthchar(arg, i);
    if (ch == 0) error2(SUBSEQ, PSTR("index out of range"));
    buildstring(ch, &chars, &head);
  }
  result->cdr = head;
  return result;
}

object *fn_readfromstring (object *args, object *env) {
  (void) env;
  object *arg = first(args);
  if (!stringp(arg)) error(READFROMSTRING, notastring, arg);
  GlobalString = arg;
  GlobalStringIndex = 0;
  return read(gstr);
}

object *fn_princtostring (object *args, object *env) {
  (void) env;
  object *arg = first(args);
  object *obj = startstring(PRINCTOSTRING);
  prin1object(arg, pstr);
  obj->cdr = GlobalString;
  return obj;
}

object *fn_prin1tostring (object *args, object *env) {
  (void) env;
  object *arg = first(args);
  object *obj = startstring(PRIN1TOSTRING);
  printobject(arg, pstr);
  obj->cdr = GlobalString;
  return obj;
}

// Bitwise operators

object *fn_logand (object *args, object *env) {
  (void) env;
  int result = -1;
  while (args != NULL) {
    result = result & checkinteger(LOGAND, first(args));
    args = cdr(args);
  }
  return number(result);
}

object *fn_logior (object *args, object *env) {
  (void) env;
  int result = 0;
  while (args != NULL) {
    result = result | checkinteger(LOGIOR, first(args));
    args = cdr(args);
  }
  return number(result);
}

object *fn_logxor (object *args, object *env) {
  (void) env;
  int result = 0;
  while (args != NULL) {
    result = result ^ checkinteger(LOGXOR, first(args));
    args = cdr(args);
  }
  return number(result);
}

object *fn_lognot (object *args, object *env) {
  (void) env;
  int result = checkinteger(LOGNOT, car(args));
  return number(~result);
}

object *fn_ash (object *args, object *env) {
  (void) env;
  int value = checkinteger(ASH, first(args));
  int count = checkinteger(ASH, second(args));
  if (count >= 0) return number(value << count);
  else return number(value >> abs(count));
}

object *fn_logbitp (object *args, object *env) {
  (void) env;
  int index = checkinteger(LOGBITP, first(args));
  int value = checkinteger(LOGBITP, second(args));
  return (bitRead(value, index) == 1) ? tee : nil;
}

// System functions

object *fn_eval (object *args, object *env) {
  return eval(first(args), env);
}

object *fn_globals (object *args, object *env) {
  (void) args;
  if (GlobalEnv == NULL) return nil;
  return fn_mapcar(cons(symbol(CAR),cons(GlobalEnv,nil)), env);
}

object *fn_locals (object *args, object *env) {
  (void) args;
  return env;
}

object *fn_makunbound (object *args, object *env) {
  (void) env;
  object *var = first(args);
  if (!symbolp(var)) error(MAKUNBOUND, notasymbol, var);
  delassoc(var, &GlobalEnv);
  return var;
}

object *fn_break (object *args, object *env) {
  (void) args;
  pfstring(PSTR("\nBreak!\n"), pserial);
  BreakLevel++;
  repl(env);
  BreakLevel--;
  return nil;
}

object *fn_read (object *args, object *env) {
  (void) env;
  gfun_t gfun = gstreamfun(args);
  return read(gfun);
}

object *fn_prin1 (object *args, object *env) {
  (void) env;
  object *obj = first(args);
  pfun_t pfun = pstreamfun(cdr(args));
  printobject(obj, pfun);
  return obj;
}

object *fn_print (object *args, object *env) {
  (void) env;
  object *obj = first(args);
  pfun_t pfun = pstreamfun(cdr(args));
  pln(pfun);
  printobject(obj, pfun);
  pfun(' ');
  return obj;
}

object *fn_princ (object *args, object *env) {
  (void) env;
  object *obj = first(args);
  pfun_t pfun = pstreamfun(cdr(args));
  prin1object(obj, pfun);
  return obj;
}

object *fn_terpri (object *args, object *env) {
  (void) env;
  pfun_t pfun = pstreamfun(args);
  pln(pfun);
  return nil;
}

object *fn_readbyte (object *args, object *env) {
  (void) env;
  gfun_t gfun = gstreamfun(args);
  int c = gfun();
  return (c == -1) ? nil : number(c);
}

object *fn_readline (object *args, object *env) {
  (void) env;
  gfun_t gfun = gstreamfun(args);
  return readstring('\n', gfun);
}

object *fn_writebyte (object *args, object *env) {
  (void) env;
  int value = checkinteger(WRITEBYTE, first(args));
  pfun_t pfun = pstreamfun(cdr(args));
  (pfun)(value);
  return nil;
}

object *fn_writestring (object *args, object *env) {
  (void) env;
  object *obj = first(args);
  pfun_t pfun = pstreamfun(cdr(args));
  char temp = Flags;
  clrflag(PRINTREADABLY);
  printstring(obj, pfun);
  Flags = temp;
  return nil;
}

object *fn_writeline (object *args, object *env) {
  (void) env;
  object *obj = first(args);
  pfun_t pfun = pstreamfun(cdr(args));
  char temp = Flags;
  clrflag(PRINTREADABLY);
  printstring(obj, pfun);
  pln(pfun);
  Flags = temp;
  return nil;
}

object *fn_restarti2c (object *args, object *env) {
  (void) env;
  int stream = first(args)->integer;
  args = cdr(args);
  int read = 0; // Write
  I2CCount = 0;
  if (args != NULL) {
    object *rw = first(args);
    if (integerp(rw)) I2CCount = rw->integer;
    read = (rw != NULL);
  }
  int address = stream & 0xFF;
  if (stream>>8 != I2CSTREAM) error2(RESTARTI2C, PSTR("not an i2c stream"));
  return I2Crestart(address, read) ? tee : nil;
}

object *fn_gc (object *obj, object *env) {
  int initial = Freespace;
  unsigned long start = micros();
  gc(obj, env);
  unsigned long elapsed = micros() - start;
  pfstring(PSTR("Space: "), pserial);
  pint(Freespace - initial, pserial);
  pfstring(PSTR(" bytes, Time: "), pserial);
  pint(elapsed, pserial);
  pfstring(PSTR(" us\n"), pserial);
  return nil;
}

object *fn_room (object *args, object *env) {
  (void) args, (void) env;
  return number(Freespace);
}

object *fn_saveimage (object *args, object *env) {
  if (args != NULL) args = eval(first(args), env);
  return number(saveimage(args));
}

object *fn_loadimage (object *args, object *env) {
  (void) env;
  if (args != NULL) args = first(args);
  return number(loadimage(args));
}

object *fn_cls (object *args, object *env) {
  (void) args, (void) env;
  pserial(12);
  return nil;
}

// Arduino procedures

object *fn_pinmode (object *args, object *env) {
  (void) env;
  int pin = checkinteger(PINMODE, first(args));
  PinMode pm = INPUT;
  object *mode = second(args);
  if (integerp(mode)) {
    int nmode = mode->integer;
    if (nmode == 1) pm = OUTPUT; else if (nmode == 2) pm = INPUT_PULLUP;
    #if defined(INPUT_PULLDOWN)
    else if (nmode == 4) pm = INPUT_PULLDOWN;
    #endif
  } else if (mode != nil) pm = OUTPUT;
  pinMode(pin, pm);
  return nil;
}

object *fn_digitalread (object *args, object *env) {
  (void) env;
  int pin = checkinteger(DIGITALREAD, first(args));
  if (digitalRead(pin) != 0) return tee; else return nil;
}

object *fn_digitalwrite (object *args, object *env) {
  (void) env;
  int pin = checkinteger(DIGITALWRITE, first(args));
  object *mode = second(args);
  if (integerp(mode)) digitalWrite(pin, mode->integer ? HIGH : LOW);
  else digitalWrite(pin, (mode != nil) ? HIGH : LOW);
  return mode;
}

object *fn_analogread (object *args, object *env) {
  (void) env;
  int pin = checkinteger(ANALOGREAD, first(args));
  checkanalogread(pin);
  return number(analogRead(pin));
}

object *fn_analogwrite (object *args, object *env) {
  (void) env;
  int pin = checkinteger(ANALOGWRITE, first(args));
  checkanalogwrite(pin);
  object *value = second(args);
  analogWrite(pin, checkinteger(ANALOGWRITE, value));
  return value;
}

object *fn_delay (object *args, object *env) {
  (void) env;
  object *arg1 = first(args);
  delay(checkinteger(DELAY, arg1));
  return arg1;
}

object *fn_millis (object *args, object *env) {
  (void) args, (void) env;
  return number(millis());
}

object *fn_sleep (object *args, object *env) {
  (void) env;
  object *arg1 = first(args);
  sleep(checkinteger(SLEEP, arg1));
  return arg1;
}

object *fn_note (object *args, object *env) {
  (void) env;
  static uint8_t pin = 255;
  if (args != NULL) {
    pin = checkinteger(NOTE, first(args));
    int note = 0;
    if (cddr(args) != NULL) note = checkinteger(NOTE, second(args));
    int octave = 0;
    if (cddr(args) != NULL) octave = checkinteger(NOTE, third(args));
    playnote(pin, note, octave);
  } else nonote(pin);
  return nil;
}

// Tree Editor

object *fn_edit (object *args, object *env) {
  object *fun = first(args);
  object *pair = findvalue(fun, env);
  clrflag(EXITEDITOR);
  object *arg = edit(eval(fun, env));
  cdr(pair) = arg;
  return arg;
}

object *edit (object *fun) {
  while (1) {
    if (tstflag(EXITEDITOR)) return fun;
    char c = gserial();
    if (c == 'q') setflag(EXITEDITOR);
    else if (c == 'b') return fun;
    else if (c == 'r') fun = read(gserial);
    else if (c == '\n') { pfl(pserial); superprint(fun, 0, pserial); pln(pserial); }
    else if (c == 'c') fun = cons(read(gserial), fun);
    else if (atom(fun)) pserial('!');
    else if (c == 'd') fun = cons(car(fun), edit(cdr(fun)));
    else if (c == 'a') fun = cons(edit(car(fun)), cdr(fun));
    else if (c == 'x') fun = cdr(fun);
    else pserial('?');
  }
}

// Pretty printer

object *fn_pprint (object *args, object *env) {
  (void) env;
  object *obj = first(args);
  pfun_t pfun = pstreamfun(cdr(args));
  pln(pfun);
  superprint(obj, 0, pfun);
  return symbol(NOTHING);
}

object *fn_pprintall (object *args, object *env) {
  (void) env;
  pfun_t pfun = pstreamfun(args);
  object *globals = GlobalEnv;
  while (globals != NULL) {
    object *pair = first(globals);
    object *var = car(pair);
    object *val = cdr(pair);
    pln(pfun);
    if (consp(val) && symbolp(car(val)) && car(val)->name == LAMBDA) {
      superprint(cons(symbol(DEFUN), cons(var, cdr(val))), 0, pfun);
    } else {
      superprint(cons(symbol(DEFVAR), cons(var, cons(quote(val), NULL))), 0, pserial);
    }
    pln(pfun);
    testescape();
    globals = cdr(globals);
  }
  return symbol(NOTHING);
}

// Format

void formaterr (object *formatstr, PGM_P string, uint8_t p) {
  pln(pserial); indent(4, ' ', pserial); printstring(formatstr, pserial); pln(pserial);
  indent(p+5, ' ', pserial); pserial('^');
  errorsub(FORMAT, string);
  pln(pserial);
  GCStack = NULL;
  longjmp(exception, 1);
}

object *fn_format (object *args, object *env) {
  (void) env;
  pfun_t pfun = pserial;
  object *output = first(args);
  object *obj;
  if (output == nil) { obj = startstring(FORMAT); pfun = pstr; }
  else if (output != tee) pfun = pstreamfun(args);
  object *formatstr = second(args);
  if (!stringp(formatstr)) error(FORMAT, notastring, formatstr);
  object *save = NULL;
  args = cddr(args);
  int len = stringlength(formatstr);
  uint8_t n = 0, width = 0, w, bra = 0;
  char pad = ' ';
  bool tilde = false, mute = false, comma, quote;
  while (n < len) {
    char ch = nthchar(formatstr, n);
    char ch2 = ch & ~0x20; // force to upper case
    if (tilde) {
     if (ch == '}') {
        if (save == NULL) formaterr(formatstr, PSTR("no matching ~{"), n);
        if (args == NULL) { args = cdr(save); save = NULL; } else n = bra; 
        mute = false; tilde = false;
      }      
      else if (!mute) {
        if (comma && quote) { pad = ch; comma = false, quote = false; }
        else if (ch == '\'') {
          if (comma) quote = true; 
          else formaterr(formatstr, PSTR("quote not valid"), n);
        }
        else if (ch == '~') { pfun('~'); tilde = false; }
        else if (ch >= '0' && ch <= '9') width = width*10 + ch - '0';
        else if (ch == ',') comma = true;
        else if (ch == '%') { pln(pfun); tilde = false; }
        else if (ch == '&') { pfl(pfun); tilde = false; }
        else if (ch == '^') {
          if (save != NULL && args == NULL) mute = true;
          tilde = false;
        }
        else if (ch == '{') {
          if (save != NULL) formaterr(formatstr, PSTR("can't nest ~{"), n);
          if (args == NULL) formaterr(formatstr, noargument, n);
          if (!listp(first(args))) formaterr(formatstr, notalist, n);
          save = args; args = first(args); bra = n; tilde = false;
        }
        else if (ch2 == 'A' || ch2 == 'S' || ch2 == 'D' || ch2 == 'G' || ch2 == 'X') {
          if (args == NULL) formaterr(formatstr, noargument, n);
          object *arg = first(args); args = cdr(args);
          uint8_t aw = atomwidth(arg);
          if (width < aw) w = 0; else w = width-aw;
          tilde = false;
          if (ch2 == 'A') { prin1object(arg, pfun); indent(w, pad, pfun); }
          else if (ch2 == 'S') { printobject(arg, pfun); indent(w, pad, pfun); }
          else if (ch2 == 'D' || ch2 == 'G') { indent(w, pad, pfun); prin1object(arg, pfun); }
          else if (ch2 == 'X' && integerp(arg)) {
            uint8_t hw = hexwidth(arg); if (width < hw) w = 0; else w = width-hw;
            indent(w, pad, pfun); pinthex(arg->integer, pfun);
          } else if (ch2 == 'X') { indent(w, pad, pfun); prin1object(arg, pfun); }
          tilde = false;
        } else formaterr(formatstr, PSTR("invalid directive"), n);
      }
    } else {
      if (ch == '~') { tilde = true; pad = ' '; width = 0; comma = false; quote = false; }
      else if (!mute) pfun(ch);
    }
    n++;
  }
  if (output == nil) { obj->cdr = GlobalString; return obj; }
  else return nil;
}

// LispLibrary

object *fn_require (object *args, object *env) {
  object *arg = first(args);
  object *globals = GlobalEnv;
  if (!symbolp(arg)) error(REQUIRE, notasymbol, arg);
  while (globals != NULL) {
    object *pair = first(globals);
    object *var = car(pair);
    if (symbolp(var) && var == arg) return nil;
    globals = cdr(globals);
  }
  GlobalStringIndex = 0;
  object *line = read(glibrary);
  while (line != NULL) {
    // Is this the definition we want
    int fname = first(line)->name;
    if ((fname == DEFUN || fname == DEFVAR) && symbolp(second(line)) && second(line)->name == arg->name) {
      eval(line, env);
      return tee;
    }
    line = read(glibrary);
  }
  return nil;
}

object *fn_listlibrary (object *args, object *env) {
  (void) args, (void) env;
  GlobalStringIndex = 0;
  object *line = read(glibrary);
  while (line != NULL) {
    int fname = first(line)->name;
    if (fname == DEFUN || fname == DEFVAR) {
      pstring(symbolname(second(line)->name), pserial); pserial(' ');
    }
    line = read(glibrary);
  }
  return symbol(NOTHING);
}

// Lisp Badge plotting

void plotsub (uint8_t x, uint8_t y, uint8_t n, int ys[5]) {
  if (y>=0 && y<64) {
    uint8_t grey = 0x0F-n*3;
    uint8_t blob = grey;
    if ((x&1) == 0) { blob = grey<<4; ys[n] = y; }
    else {
      for (int i=0; i<5; i++) {
        if (y == ys[i]) blob = (0x0F-i*3)<<4 | grey;
      }
    }
    PlotByte(x>>1, y, blob);
  }
}

object *fn_plot (object *args, object *env) {
  int ys[5] = {-1, -1, -1, -1, -1};
  int xaxis = -1, yaxis = -1;
  delay(20);
  ClearDisplay(0); // Clear display
  if (args != NULL && integerp(first(args))) { xaxis = checkinteger(PLOT, first(args)); args = cdr(args); }
  if (args != NULL && integerp(first(args))) { yaxis = checkinteger(PLOT, first(args)); args = cdr(args); }
  int nargs = min(listlength(PLOT, args),4);
  for (int x=0; x<256; x++) {
    object *rest = args;
    for (int n=0; n<nargs; n++) {
      object *function = first(rest);
      int y = checkinteger(PLOT, apply(PLOT, function, cons(number(x), NULL), env));
      plotsub(x, y, n+1, ys);
      rest = cdr(rest);
    }
    plotsub(x, yaxis, 0, ys);
    if (x == xaxis) for (int y=0; y<64; y++) plotsub(x, y, 0, ys);
    if ((x&1) != 0) for (int i=0; i<5; i++) ys[i] = -1;
  }
  while (!tstflag(ESCAPE)); clrflag(ESCAPE);
  return symbol(NOTHING);
}

object *fn_plot3d (object *args, object *env) {
  int xaxis = -1, yaxis = -1;
  uint8_t blob;
  delay(20);
  ClearDisplay(0); // Clear display
  if (args != NULL && integerp(first(args))) { xaxis = checkinteger(PLOT3D, first(args)); args = cdr(args); }
  if (args != NULL && integerp(first(args))) { yaxis = checkinteger(PLOT3D, first(args)); args = cdr(args); }
  if (args != NULL) {
    object *function = first(args);
    for (int y=0; y<64; y++) {
      for (int x=0; x<256; x++) {
        int z = checkinteger(PLOT3D, apply(PLOT3D, function, cons(number(x), cons(number(y), NULL)), env));
        if (x == xaxis || y == yaxis) z = 0xF;
        if ((x&1) == 0) blob = z<<4; else blob = blob | (z&0xF);
        PlotByte(x>>1, y, blob);
      }
    }
  }
  while (!tstflag(ESCAPE)); clrflag(ESCAPE);
  return symbol(NOTHING);
}

extern const uint8_t CharMap[96][6] PROGMEM;

object *fn_glyphpixel (object *args, object *env) {
  (void) env;
  uint8_t c = 0, x = 6, y = 8;
  c = checkchar(GLYPHPIXEL, first(args));
  x = checkinteger(GLYPHPIXEL, second(args));
  y = checkinteger(GLYPHPIXEL, third(args));
  if (x > 5 || y > 7) return number(0);
  return pgm_read_byte(&CharMap[(c & 0x7f) - 32][x]) & 1 << (7 - y) ? number(15) : number(0);
}

object *fn_plotpixel (object *args, object *env) {
  (void) env;
  int x = checkinteger(PLOTPIXEL, first(args));
  int y = checkinteger(PLOTPIXEL, second(args));
  args = cddr(args);
  uint8_t grey = 0xff;
  if (args != NULL) grey = checkinteger(PLOTPIXEL, first(args));
  PlotByte(x, y, grey);
  return nil;
}

object *fn_fillscreen (object *args, object *env) {
  (void) env;
  uint8_t grey = 0;
  if (args != NULL) grey = checkinteger(FILLSCREEN, first(args));
  ClearDisplay(grey);
  return nil;
}

// Insert your own function definitions here

// Built-in procedure names - stored in PROGMEM

const char string0[] PROGMEM = "nil";
const char string1[] PROGMEM = "t";
const char string2[] PROGMEM = "nothing";
const char string3[] PROGMEM = "&optional";
const char string4[] PROGMEM = "&rest";
const char string5[] PROGMEM = "lambda";
const char string6[] PROGMEM = "let";
const char string7[] PROGMEM = "let*";
const char string8[] PROGMEM = "closure";
const char string9[] PROGMEM = "";
const char string10[] PROGMEM = "quote";
const char string11[] PROGMEM = "defun";
const char string12[] PROGMEM = "defvar";
const char string13[] PROGMEM = "setq";
const char string14[] PROGMEM = "loop";
const char string15[] PROGMEM = "return";
const char string16[] PROGMEM = "push";
const char string17[] PROGMEM = "pop";
const char string18[] PROGMEM = "incf";
const char string19[] PROGMEM = "decf";
const char string20[] PROGMEM = "setf";
const char string21[] PROGMEM = "dolist";
const char string22[] PROGMEM = "dotimes";
const char string23[] PROGMEM = "trace";
const char string24[] PROGMEM = "untrace";
const char string25[] PROGMEM = "for-millis";
const char string26[] PROGMEM = "with-serial";
const char string27[] PROGMEM = "with-i2c";
const char string28[] PROGMEM = "with-spi";
const char string29[] PROGMEM = "with-sd-card";
const char string30[] PROGMEM = "";
const char string31[] PROGMEM = "progn";
const char string32[] PROGMEM = "if";
const char string33[] PROGMEM = "cond";
const char string34[] PROGMEM = "when";
const char string35[] PROGMEM = "unless";
const char string36[] PROGMEM = "case";
const char string37[] PROGMEM = "and";
const char string38[] PROGMEM = "or";
const char string39[] PROGMEM = "";
const char string40[] PROGMEM = "not";
const char string41[] PROGMEM = "null";
const char string42[] PROGMEM = "cons";
const char string43[] PROGMEM = "atom";
const char string44[] PROGMEM = "listp";
const char string45[] PROGMEM = "consp";
const char string46[] PROGMEM = "symbolp";
const char string47[] PROGMEM = "boundp";
const char string48[] PROGMEM = "set";
const char string49[] PROGMEM = "streamp";
const char string50[] PROGMEM = "eq";
const char string51[] PROGMEM = "car";
const char string52[] PROGMEM = "first";
const char string53[] PROGMEM = "cdr";
const char string54[] PROGMEM = "rest";
const char string55[] PROGMEM = "caar";
const char string56[] PROGMEM = "cadr";
const char string57[] PROGMEM = "second";
const char string58[] PROGMEM = "cdar";
const char string59[] PROGMEM = "cddr";
const char string60[] PROGMEM = "caaar";
const char string61[] PROGMEM = "caadr";
const char string62[] PROGMEM = "cadar";
const char string63[] PROGMEM = "caddr";
const char string64[] PROGMEM = "third";
const char string65[] PROGMEM = "cdaar";
const char string66[] PROGMEM = "cdadr";
const char string67[] PROGMEM = "cddar";
const char string68[] PROGMEM = "cdddr";
const char string69[] PROGMEM = "length";
const char string70[] PROGMEM = "list";
const char string71[] PROGMEM = "reverse";
const char string72[] PROGMEM = "nth";
const char string73[] PROGMEM = "assoc";
const char string74[] PROGMEM = "member";
const char string75[] PROGMEM = "apply";
const char string76[] PROGMEM = "funcall";
const char string77[] PROGMEM = "append";
const char string78[] PROGMEM = "mapc";
const char string79[] PROGMEM = "mapcar";
const char string80[] PROGMEM = "mapcan";
const char string81[] PROGMEM = "+";
const char string82[] PROGMEM = "-";
const char string83[] PROGMEM = "*";
const char string84[] PROGMEM = "/";
const char string85[] PROGMEM = "truncate";
const char string86[] PROGMEM = "mod";
const char string87[] PROGMEM = "1+";
const char string88[] PROGMEM = "1-";
const char string89[] PROGMEM = "abs";
const char string90[] PROGMEM = "random";
const char string91[] PROGMEM = "max";
const char string92[] PROGMEM = "min";
const char string93[] PROGMEM = "/=";
const char string94[] PROGMEM = "=";
const char string95[] PROGMEM = "<";
const char string96[] PROGMEM = "<=";
const char string97[] PROGMEM = ">";
const char string98[] PROGMEM = ">=";
const char string99[] PROGMEM = "plusp";
const char string100[] PROGMEM = "minusp";
const char string101[] PROGMEM = "zerop";
const char string102[] PROGMEM = "oddp";
const char string103[] PROGMEM = "evenp";
const char string104[] PROGMEM = "integerp";
const char string105[] PROGMEM = "numberp";
const char string106[] PROGMEM = "char";
const char string107[] PROGMEM = "char-code";
const char string108[] PROGMEM = "code-char";
const char string109[] PROGMEM = "characterp";
const char string110[] PROGMEM = "stringp";
const char string111[] PROGMEM = "string=";
const char string112[] PROGMEM = "string<";
const char string113[] PROGMEM = "string>";
const char string114[] PROGMEM = "sort";
const char string115[] PROGMEM = "string";
const char string116[] PROGMEM = "concatenate";
const char string117[] PROGMEM = "subseq";
const char string118[] PROGMEM = "read-from-string";
const char string119[] PROGMEM = "princ-to-string";
const char string120[] PROGMEM = "prin1-to-string";
const char string121[] PROGMEM = "logand";
const char string122[] PROGMEM = "logior";
const char string123[] PROGMEM = "logxor";
const char string124[] PROGMEM = "lognot";
const char string125[] PROGMEM = "ash";
const char string126[] PROGMEM = "logbitp";
const char string127[] PROGMEM = "eval";
const char string128[] PROGMEM = "globals";
const char string129[] PROGMEM = "locals";
const char string130[] PROGMEM = "makunbound";
const char string131[] PROGMEM = "break";
const char string132[] PROGMEM = "read";
const char string133[] PROGMEM = "prin1";
const char string134[] PROGMEM = "print";
const char string135[] PROGMEM = "princ";
const char string136[] PROGMEM = "terpri";
const char string137[] PROGMEM = "read-byte";
const char string138[] PROGMEM = "read-line";
const char string139[] PROGMEM = "write-byte";
const char string140[] PROGMEM = "write-string";
const char string141[] PROGMEM = "write-line";
const char string142[] PROGMEM = "restart-i2c";
const char string143[] PROGMEM = "gc";
const char string144[] PROGMEM = "room";
const char string145[] PROGMEM = "save-image";
const char string146[] PROGMEM = "load-image";
const char string147[] PROGMEM = "cls";
const char string148[] PROGMEM = "pinmode";
const char string149[] PROGMEM = "digitalread";
const char string150[] PROGMEM = "digitalwrite";
const char string151[] PROGMEM = "analogread";
const char string152[] PROGMEM = "analogwrite";
const char string153[] PROGMEM = "delay";
const char string154[] PROGMEM = "millis";
const char string155[] PROGMEM = "sleep";
const char string156[] PROGMEM = "note";
const char string157[] PROGMEM = "edit";
const char string158[] PROGMEM = "pprint";
const char string159[] PROGMEM = "pprintall";
const char string160[] PROGMEM = "format";
const char string161[] PROGMEM = "require";
const char string162[] PROGMEM = "list-library";
const char string163[] PROGMEM = "plot";
const char string164[] PROGMEM = "plot3d";
const char string165[] PROGMEM = "glyph-pixel";
const char string166[] PROGMEM = "plot-pixel";
const char string167[] PROGMEM = "fill-screen";

// Third parameter is no. of arguments; 1st hex digit is min, 2nd hex digit is max, 0xF is unlimited
const tbl_entry_t lookup_table[] PROGMEM = {
  { string0, NULL, 0x00 },
  { string1, NULL, 0x00 },
  { string2, NULL, 0x00 },
  { string3, NULL, 0x00 },
  { string4, NULL, 0x00 },
  { string5, NULL, 0x0F },
  { string6, NULL, 0x0F },
  { string7, NULL, 0x0F },
  { string8, NULL, 0x0F },
  { string9, NULL, 0x00 },
  { string10, sp_quote, 0x11 },
  { string11, sp_defun, 0x2F },
  { string12, sp_defvar, 0x12 },
  { string13, sp_setq, 0x2F },
  { string14, sp_loop, 0x0F },
  { string15, sp_return, 0x0F },
  { string16, sp_push, 0x22 },
  { string17, sp_pop, 0x11 },
  { string18, sp_incf, 0x12 },
  { string19, sp_decf, 0x12 },
  { string20, sp_setf, 0x2F },
  { string21, sp_dolist, 0x1F },
  { string22, sp_dotimes, 0x1F },
  { string23, sp_trace, 0x01 },
  { string24, sp_untrace, 0x01 },
  { string25, sp_formillis, 0x1F },
  { string26, sp_withserial, 0x1F },
  { string27, sp_withi2c, 0x1F },
  { string28, sp_withspi, 0x1F },
  { string29, sp_withsdcard, 0x2F },
  { string30, NULL, 0x00 },
  { string31, tf_progn, 0x0F },
  { string32, tf_if, 0x23 },
  { string33, tf_cond, 0x0F },
  { string34, tf_when, 0x1F },
  { string35, tf_unless, 0x1F },
  { string36, tf_case, 0x1F },
  { string37, tf_and, 0x0F },
  { string38, tf_or, 0x0F },
  { string39, NULL, 0x00 },
  { string40, fn_not, 0x11 },
  { string41, fn_not, 0x11 },
  { string42, fn_cons, 0x22 },
  { string43, fn_atom, 0x11 },
  { string44, fn_listp, 0x11 },
  { string45, fn_consp, 0x11 },
  { string46, fn_symbolp, 0x11 },
  { string47, fn_boundp, 0x11 },
  { string48, fn_setfn, 0x2F },
  { string49, fn_streamp, 0x11 },
  { string50, fn_eq, 0x22 },
  { string51, fn_car, 0x11 },
  { string52, fn_car, 0x11 },
  { string53, fn_cdr, 0x11 },
  { string54, fn_cdr, 0x11 },
  { string55, fn_caar, 0x11 },
  { string56, fn_cadr, 0x11 },
  { string57, fn_cadr, 0x11 },
  { string58, fn_cdar, 0x11 },
  { string59, fn_cddr, 0x11 },
  { string60, fn_caaar, 0x11 },
  { string61, fn_caadr, 0x11 },
  { string62, fn_cadar, 0x11 },
  { string63, fn_caddr, 0x11 },
  { string64, fn_caddr, 0x11 },
  { string65, fn_cdaar, 0x11 },
  { string66, fn_cdadr, 0x11 },
  { string67, fn_cddar, 0x11 },
  { string68, fn_cdddr, 0x11 },
  { string69, fn_length, 0x11 },
  { string70, fn_list, 0x0F },
  { string71, fn_reverse, 0x11 },
  { string72, fn_nth, 0x22 },
  { string73, fn_assoc, 0x22 },
  { string74, fn_member, 0x22 },
  { string75, fn_apply, 0x2F },
  { string76, fn_funcall, 0x1F },
  { string77, fn_append, 0x0F },
  { string78, fn_mapc, 0x2F },
  { string79, fn_mapcar, 0x2F },
  { string80, fn_mapcan, 0x2F },
  { string81, fn_add, 0x0F },
  { string82, fn_subtract, 0x1F },
  { string83, fn_multiply, 0x0F },
  { string84, fn_divide, 0x2F },
  { string85, fn_divide, 0x12 },
  { string86, fn_mod, 0x22 },
  { string87, fn_oneplus, 0x11 },
  { string88, fn_oneminus, 0x11 },
  { string89, fn_abs, 0x11 },
  { string90, fn_random, 0x11 },
  { string91, fn_maxfn, 0x1F },
  { string92, fn_minfn, 0x1F },
  { string93, fn_noteq, 0x1F },
  { string94, fn_numeq, 0x1F },
  { string95, fn_less, 0x1F },
  { string96, fn_lesseq, 0x1F },
  { string97, fn_greater, 0x1F },
  { string98, fn_greatereq, 0x1F },
  { string99, fn_plusp, 0x11 },
  { string100, fn_minusp, 0x11 },
  { string101, fn_zerop, 0x11 },
  { string102, fn_oddp, 0x11 },
  { string103, fn_evenp, 0x11 },
  { string104, fn_integerp, 0x11 },
  { string105, fn_integerp, 0x11 },
  { string106, fn_char, 0x22 },
  { string107, fn_charcode, 0x11 },
  { string108, fn_codechar, 0x11 },
  { string109, fn_characterp, 0x11 },
  { string110, fn_stringp, 0x11 },
  { string111, fn_stringeq, 0x22 },
  { string112, fn_stringless, 0x22 },
  { string113, fn_stringgreater, 0x22 },
  { string114, fn_sort, 0x22 },
  { string115, fn_stringfn, 0x11 },
  { string116, fn_concatenate, 0x1F },
  { string117, fn_subseq, 0x23 },
  { string118, fn_readfromstring, 0x11 },
  { string119, fn_princtostring, 0x11 },
  { string120, fn_prin1tostring, 0x11 },
  { string121, fn_logand, 0x0F },
  { string122, fn_logior, 0x0F },
  { string123, fn_logxor, 0x0F },
  { string124, fn_lognot, 0x11 },
  { string125, fn_ash, 0x22 },
  { string126, fn_logbitp, 0x22 },
  { string127, fn_eval, 0x11 },
  { string128, fn_globals, 0x00 },
  { string129, fn_locals, 0x00 },
  { string130, fn_makunbound, 0x11 },
  { string131, fn_break, 0x00 },
  { string132, fn_read, 0x01 },
  { string133, fn_prin1, 0x12 },
  { string134, fn_print, 0x12 },
  { string135, fn_princ, 0x12 },
  { string136, fn_terpri, 0x01 },
  { string137, fn_readbyte, 0x02 },
  { string138, fn_readline, 0x01 },
  { string139, fn_writebyte, 0x12 },
  { string140, fn_writestring, 0x12 },
  { string141, fn_writeline, 0x12 },
  { string142, fn_restarti2c, 0x12 },
  { string143, fn_gc, 0x00 },
  { string144, fn_room, 0x00 },
  { string145, fn_saveimage, 0x01 },
  { string146, fn_loadimage, 0x01 },
  { string147, fn_cls, 0x00 },
  { string148, fn_pinmode, 0x22 },
  { string149, fn_digitalread, 0x11 },
  { string150, fn_digitalwrite, 0x22 },
  { string151, fn_analogread, 0x11 },
  { string152, fn_analogwrite, 0x22 },
  { string153, fn_delay, 0x11 },
  { string154, fn_millis, 0x00 },
  { string155, fn_sleep, 0x11 },
  { string156, fn_note, 0x03 },
  { string157, fn_edit, 0x11 },
  { string158, fn_pprint, 0x12 },
  { string159, fn_pprintall, 0x01 },
  { string160, fn_format, 0x2F },
  { string161, fn_require, 0x11 },
  { string162, fn_listlibrary, 0x00 },
  { string163, fn_plot, 0x06 },
  { string164, fn_plot3d, 0x03 },
  { string165, fn_glyphpixel, 0x33 },
  { string166, fn_plotpixel, 0x23 },
  { string167, fn_fillscreen, 0x01 },
};

// Table lookup functions

int builtin (char* n) {
  int entry = 0;
  while (entry < ENDFUNCTIONS) {
    if (strcasecmp_P(n, (char*)pgm_read_word(&lookup_table[entry].string)) == 0)
      return entry;
    entry++;
  }
  return ENDFUNCTIONS;
}

int longsymbol (char *buffer) {
  char *p = SymbolTable;
  int i = 0;
  while (strcasecmp(p, buffer) != 0) {p = p + strlen(p) + 1; i++; }
  if (p == buffer) {
    // Add to symbol table?
    char *newtop = SymbolTop + strlen(p) + 1;
    if (SYMBOLTABLESIZE - (newtop - SymbolTable) < BUFFERSIZE) error2(0, PSTR("no room for long symbols"));
    SymbolTop = newtop;
  }
  if (i > 1535) error2(0, PSTR("too many long symbols"));
  return i + MAXSYMBOL; // First number unused by radix40
}

intptr_t lookupfn (symbol_t name) {
  return pgm_read_word(&lookup_table[name].fptr);
}

void checkminmax (symbol_t name, int nargs) {
  uint8_t minmax = pgm_read_byte(&lookup_table[name].minmax);
  if (nargs<(minmax >> 4)) error2(name, toofewargs);
  if ((minmax & 0x0f) != 0x0f && nargs>(minmax & 0x0f)) error2(name, toomanyargs);
}

char *lookupbuiltin (symbol_t name) {
  char *buffer = SymbolTop;
  strcpy_P(buffer, (char *)(pgm_read_word(&lookup_table[name].string)));
  return buffer;
}

char *lookupsymbol (symbol_t name) {
  char *p = SymbolTable;
  int i = name - MAXSYMBOL;
  while (i > 0 && p < SymbolTop) {p = p + strlen(p) + 1; i--; }
  if (p == SymbolTop) return NULL; else return p;
}

void deletesymbol (symbol_t name) {
  char *p = lookupsymbol(name);
  if (p == NULL) return;
  char *q = p + strlen(p) + 1;
  *p = '\0'; p++;
  while (q < SymbolTop) *(p++) = *(q++);
  SymbolTop = p;
}

void testescape () {
#if defined serialmonitor
  if (Serial.read() == '~') error2(0, PSTR("Escape!"));
#endif
}

// Main evaluator

extern char __bss_end[];

object *eval (object *form, object *env) {
  uint8_t sp[0];
  int TC=0;
  EVAL:
  // Enough space?
  // Serial.println((uint16_t)sp - (uint16_t)__bss_end);
  if ((uint16_t)sp - (uint16_t)__bss_end < STACKDIFF) error2(0, PSTR("Stack overflow"));
  if (Freespace <= (WORKSPACESIZE)>>4) gc(form, env);
  // Escape
  if (tstflag(ESCAPE)) { clrflag(ESCAPE); error2(0, PSTR("Escape!"));}
  #if defined (serialmonitor)
  if (!tstflag(NOESC)) testescape();
  #endif

  if (form == NULL) return nil;

  if (form->type >= NUMBER && form->type <= STRING) return form;

  if (symbolp(form)) {
    symbol_t name = form->name;
    object *pair = value(name, env);
    if (pair != NULL) return cdr(pair);
    pair = value(name, GlobalEnv);
    if (pair != NULL) return cdr(pair);
    else if (name <= ENDFUNCTIONS) return form;
    error(0, PSTR("undefined"), form);
  }

  // It's a list
  object *function = car(form);
  object *args = cdr(form);

  if (function == NULL) error(0, PSTR("illegal function"), nil);
  if (!listp(args)) error(0, PSTR("can't evaluate a dotted pair"), args);

  // List starts with a symbol?
  if (symbolp(function)) {
    symbol_t name = function->name;

    if ((name == LET) || (name == LETSTAR)) {
      int TCstart = TC;
      object *assigns = first(args);
      if (!listp(assigns)) error(name, notalist, assigns);
      object *forms = cdr(args);
      object *newenv = env;
      push(newenv, GCStack);
      while (assigns != NULL) {
        object *assign = car(assigns);
        if (!consp(assign)) push(cons(assign,nil), newenv);
        else if (cdr(assign) == NULL) push(cons(first(assign),nil), newenv);
        else push(cons(first(assign),eval(second(assign),env)), newenv);
        car(GCStack) = newenv;
        if (name == LETSTAR) env = newenv;
        assigns = cdr(assigns);
      }
      env = newenv;
      pop(GCStack);
      form = tf_progn(forms,env);
      TC = TCstart;
      goto EVAL;
    }

    if (name == LAMBDA) {
      if (env == NULL) return form;
      object *envcopy = NULL;
      while (env != NULL) {
        object *pair = first(env);
        if (pair != NULL) push(pair, envcopy);
        env = cdr(env);
      }
      return cons(symbol(CLOSURE), cons(envcopy,args));
    }

    if ((name > SPECIAL_FORMS) && (name < TAIL_FORMS)) {
      return ((fn_ptr_type)lookupfn(name))(args, env);
    }

    if ((name > TAIL_FORMS) && (name < FUNCTIONS)) {
      form = ((fn_ptr_type)lookupfn(name))(args, env);
      TC = 1;
      goto EVAL;
    }

    if (name < SPECIAL_FORMS) error2((uintptr_t)function, PSTR("can't be used as a function"));
  }

  // Evaluate the parameters - result in head
  object *fname = car(form);
  int TCstart = TC;
  object *head = cons(eval(fname, env), NULL);
  push(head, GCStack); // Don't GC the result list
  object *tail = head;
  form = cdr(form);
  int nargs = 0;

  while (form != NULL){
    object *obj = cons(eval(car(form),env),NULL);
    cdr(tail) = obj;
    tail = obj;
    form = cdr(form);
    nargs++;
  }

  function = car(head);
  args = cdr(head);

  if (symbolp(function)) {
    symbol_t name = function->name;
    if (name >= ENDFUNCTIONS) error(0, PSTR("not valid here"), fname);
    checkminmax(name, nargs);
    object *result = ((fn_ptr_type)lookupfn(name))(args, env);
    pop(GCStack);
    return result;
  }

  if (consp(function)) {

    if (issymbol(car(function), LAMBDA)) {
      form = closure(TCstart, fname->name, NULL, cdr(function), args, &env);
      pop(GCStack);
      int trace = tracing(fname->name);
      if (trace) {
        object *result = eval(form, env);
        indent((--(TraceDepth[trace-1]))<<1, ' ', pserial);
        pint(TraceDepth[trace-1], pserial);
        pserial(':'); pserial(' ');
        printobject(fname, pserial); pfstring(PSTR(" returned "), pserial);
        printobject(result, pserial); pln(pserial);
        return result;
      } else {
        TC = 1;
        goto EVAL;
      }
    }

    if (issymbol(car(function), CLOSURE)) {
      function = cdr(function);
      form = closure(TCstart, fname->name, car(function), cdr(function), args, &env);
      pop(GCStack);
      TC = 1;
      goto EVAL;
    }

  }
  error(0, PSTR("illegal function"), fname); return nil;
}

// Print functions

inline int maxbuffer (char *buffer) {
  return SYMBOLTABLESIZE-(buffer-SymbolTable)-1;
}

void pserial (char c) {
  LastPrint = c;
  Display(c);
  #if defined (serialmonitor)
  if (c == '\n') Serial.write('\r');
  Serial.write(c);
  #endif
}

const char ControlCodes[] PROGMEM = "Null\0SOH\0STX\0ETX\0EOT\0ENQ\0ACK\0Bell\0Backspace\0Tab\0Newline\0VT\0"
"Page\0Return\0SO\0SI\0DLE\0DC1\0DC2\0DC3\0DC4\0NAK\0SYN\0ETB\0CAN\0EM\0SUB\0Escape\0FS\0GS\0RS\0US\0Space\0";

void pcharacter (char c, pfun_t pfun) {
  if (!tstflag(PRINTREADABLY)) pfun(c);
  else {
    pfun('#'); pfun('\\');
    if (c > 32) pfun(c);
    else {
      PGM_P p = ControlCodes;
      while (c > 0) {p = p + strlen_P(p) + 1; c--; }
      pfstring(p, pfun);
    }
  }
}

void pstring (char *s, pfun_t pfun) {
  while (*s) pfun(*s++);
}

void printstring (object *form, pfun_t pfun) {
  if (tstflag(PRINTREADABLY)) pfun('"');
  form = cdr(form);
  while (form != NULL) {
    int chars = form->chars;
    for (int i=(sizeof(int)-1)*8; i>=0; i=i-8) {
      char ch = chars>>i & 0xFF;
      if (tstflag(PRINTREADABLY) && (ch == '"' || ch == '\\')) pfun('\\');
      if (ch) pfun(ch);
    }
    form = car(form);
  }
  if (tstflag(PRINTREADABLY)) pfun('"');
}

void pfstring (PGM_P s, pfun_t pfun) {
  intptr_t p = (intptr_t)s;
  while (1) {
    char c = pgm_read_byte(p++);
    if (c == 0) return;
    pfun(c);
  }
}

void pint (int i, pfun_t pfun) {
  int lead = 0;
  #if INT_MAX == 32767
  int p = 10000;
  #else
  int p = 1000000000;
  #endif
  if (i<0) pfun('-');
  for (int d=p; d>0; d=d/10) {
    int j = i/d;
    if (j!=0 || lead || d==1) { pfun(abs(j)+'0'); lead=1;}
    i = i - j*d;
  }
}

void pinthex (uint16_t i, pfun_t pfun) {
  int lead = 0;
  uint16_t p = 0x1000;
  for (uint16_t d=p; d>0; d=d/16) {
    uint16_t j = i/d;
    if (j!=0 || lead || d==1) { pfun((j<10) ? j+'0' : j+'W'); lead=1;}  
    i = i - j*d;
  }
}

inline void pln (pfun_t pfun) {
  pfun('\n');
}

void pfl (pfun_t pfun) {
  if (LastPrint != '\n') pfun('\n');
}

void plist (object *form, pfun_t pfun) {
  pfun('(');
  printobject(car(form), pfun);
  form = cdr(form);
  while (form != NULL && listp(form)) {
    pfun(' ');
    printobject(car(form), pfun);
    form = cdr(form);
  }
  if (form != NULL) {
    pfstring(PSTR(" . "), pfun);
    printobject(form, pfun);
  }
  pfun(')');
}

void pstream (object *form, pfun_t pfun) {
  pfun('<');
  pfstring(streamname[(form->integer)>>8], pfun);
  pfstring(PSTR("-stream "), pfun);
  pint(form->integer & 0xFF, pfun);
  pfun('>');
}

void printobject (object *form, pfun_t pfun) {
  if (form == NULL) pfstring(PSTR("nil"), pfun);
  else if (listp(form) && issymbol(car(form), CLOSURE)) pfstring(PSTR("<closure>"), pfun);
  else if (listp(form)) plist(form, pfun);
  else if (integerp(form)) pint(form->integer, pfun);
  else if (symbolp(form)) { if (form->name != NOTHING) pstring(symbolname(form->name), pfun); }
  else if (characterp(form)) pcharacter(form->chars, pfun);
  else if (stringp(form)) printstring(form, pfun);
  else if (streamp(form)) pstream(form, pfun);
  else error2(0, PSTR("Error in print"));
}

void prin1object (object *form, pfun_t pfun) {
  char temp = Flags;
  clrflag(PRINTREADABLY);
  printobject(form, pfun);
  Flags = temp;
}

// For Lisp Badge
volatile int WritePtr = 0, ReadPtr = 0;
const int KybdBufSize = 333; // 42*8 - 3
char KybdBuf[KybdBufSize];
volatile uint8_t KybdAvailable = 0;

// Read functions

int glibrary () {
  if (LastChar) { 
    char temp = LastChar;
    LastChar = 0;
    return temp;
  }
  char c = pgm_read_byte(&LispLibrary[GlobalStringIndex++]);
  return (c != 0) ? c : -1; // -1?
}

void loadfromlibrary (object *env) {   
  GlobalStringIndex = 0;
  object *line = read(glibrary);
  while (line != NULL) {
    eval(line, env);
    line = read(glibrary);
  }
}

int gserial () {
  if (LastChar) { 
    char temp = LastChar;
    LastChar = 0;
    return temp;
  }
  #if defined (serialmonitor)
  while (!Serial.available() && !KybdAvailable);
  if (Serial.available()) {
    char temp = Serial.read();
    if (temp != '\n') Serial.print(temp); // pserial(temp);
    return temp;
  } else {
    if (ReadPtr != WritePtr) {
      char temp = KybdBuf[ReadPtr++];
      Serial.write(temp);
      return temp;
    }
    KybdAvailable = 0;
    WritePtr = 0;
    return '\n';
  }
  #else
  while (!KybdAvailable);
  if (ReadPtr != WritePtr) return KybdBuf[ReadPtr++];
  KybdAvailable = 0;
  WritePtr = 0;
  return '\n';
  #endif
}

#define issp(x) (x == ' ' || x == '\n' || x == '\r' || x == '\t')

object *nextitem (gfun_t gfun) {
  int ch = gfun();
  while(issp(ch)) ch = gfun();

  if (ch == ';') {
    while(ch != '(') ch = gfun();
    ch = '(';
  }
  if (ch == '\n') ch = gfun();
  if (ch == -1) return nil;
  if (ch == ')') return (object *)KET;
  if (ch == '(') return (object *)BRA;
  if (ch == '\'') return (object *)QUO;
  if (ch == '.') return (object *)DOT;

  // Parse string
  if (ch == '"') return readstring('"', gfun);

  // Parse symbol, character, or number
  int index = 0, base = 10, sign = 1;
  char *buffer = SymbolTop;
  int bufmax = maxbuffer(buffer); // Max index
  unsigned int result = 0;
  if (ch == '+' || ch == '-') {
    buffer[index++] = ch;
    if (ch == '-') sign = -1;
    ch = gfun();
  }

  // Parse reader macros
  else if (ch == '#') {
    ch = gfun();
    char ch2 = ch & ~0x20; // force to upper case
    if (ch == '\\') { // Character
      base = 0; ch = gfun();
      if (issp(ch) || ch == ')' || ch == '(') return character(ch);
      else LastChar = ch;
    } else if (ch == '|') {
      do { while (gfun() != '|'); }
      while (gfun() != '#');
      return nextitem(gfun);
    } else if (ch2 == 'B') base = 2;
    else if (ch2 == 'O') base = 8;
    else if (ch2 == 'X') base = 16;
    else if (ch == '\'') return nextitem(gfun);
    else if (ch == '.') {
      setflag(NOESC);
      object *result = eval(read(gfun), NULL);
      clrflag(NOESC);
      return result;
    } else error2(0, PSTR("illegal character after #"));
    ch = gfun();
  }

  int isnumber = (digitvalue(ch)<base);
  buffer[2] = '\0'; // In case symbol is one letter

  while(!issp(ch) && ch != ')' && ch != '(' && index < bufmax) {
    buffer[index++] = ch;
    int temp = digitvalue(ch);
    result = result * base + temp;
    isnumber = isnumber && (digitvalue(ch)<base);
    ch = gfun();
  }

  buffer[index] = '\0';
  if (ch == ')' || ch == '(') LastChar = ch;

  if (isnumber) {
    if (base == 10 && result > ((unsigned int)INT_MAX+(1-sign)/2)) 
      error2(0, PSTR("Number out of range"));
    return number(result*sign);
  } else if (base == 0) {
    if (index == 1) return character(buffer[0]);
    PGM_P p = ControlCodes; char c = 0;
    while (c < 33) {
      if (strcasecmp_P(buffer, p) == 0) return character(c);
      p = p + strlen_P(p) + 1; c++;
    }
    error2(0, PSTR("unknown character"));
  }
  
  int x = builtin(buffer);
  if (x == NIL) return nil;
  if (x < ENDFUNCTIONS) return newsymbol(x);
  else if (index < 4 && valid40(buffer)) return newsymbol(pack40(buffer));
  else return newsymbol(longsymbol(buffer));
}

object *readrest (gfun_t gfun) {
  object *item = nextitem(gfun);
  object *head = NULL;
  object *tail = NULL;

  while (item != (object *)KET) {
    if (item == (object *)BRA) {
      item = readrest(gfun);
    } else if (item == (object *)QUO) {
      item = cons(symbol(QUOTE), cons(read(gfun), NULL));
    } else if (item == (object *)DOT) {
      tail->cdr = read(gfun);
      if (readrest(gfun) != NULL) error2(0, PSTR("malformed list"));
      return head;
    } else {
      object *cell = cons(item, NULL);
      if (head == NULL) head = cell;
      else tail->cdr = cell;
      tail = cell;
      item = nextitem(gfun);
    }
  }
  return head;
}

object *read (gfun_t gfun) {
  object *item = nextitem(gfun);
  if (item == (object *)KET) error2(0, PSTR("incomplete list"));
  if (item == (object *)BRA) return readrest(gfun);
  if (item == (object *)DOT) return read(gfun);
  if (item == (object *)QUO) return cons(symbol(QUOTE), cons(read(gfun), NULL));
  return item;
}

// Lisp Badge terminal and keyboard support

// These are the bit positions in PORTA
int const clk = 7;   // PA7
int const data = 6;  // PA6
int const dc = 5;    // PA5
int const cs = 4;    // PA4

// Terminal **********************************************************************************

// Character set - stored in program memory
const uint8_t CharMap[96][6] PROGMEM = {
{ 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },
{ 0x00, 0x00, 0x5F, 0x00, 0x00, 0x00 },
{ 0x00, 0x07, 0x00, 0x07, 0x00, 0x00 },
{ 0x14, 0x7F, 0x14, 0x7F, 0x14, 0x00 },
{ 0x24, 0x2A, 0x7F, 0x2A, 0x12, 0x00 },
{ 0x23, 0x13, 0x08, 0x64, 0x62, 0x00 },
{ 0x36, 0x49, 0x56, 0x20, 0x50, 0x00 },
{ 0x00, 0x08, 0x07, 0x03, 0x00, 0x00 },
{ 0x00, 0x1C, 0x22, 0x41, 0x00, 0x00 },
{ 0x00, 0x41, 0x22, 0x1C, 0x00, 0x00 },
{ 0x2A, 0x1C, 0x7F, 0x1C, 0x2A, 0x00 },
{ 0x08, 0x08, 0x3E, 0x08, 0x08, 0x00 },
{ 0x00, 0x80, 0x70, 0x30, 0x00, 0x00 },
{ 0x08, 0x08, 0x08, 0x08, 0x08, 0x00 },
{ 0x00, 0x00, 0x60, 0x60, 0x00, 0x00 },
{ 0x20, 0x10, 0x08, 0x04, 0x02, 0x00 },
{ 0x3E, 0x51, 0x49, 0x45, 0x3E, 0x00 },
{ 0x00, 0x42, 0x7F, 0x40, 0x00, 0x00 },
{ 0x72, 0x49, 0x49, 0x49, 0x46, 0x00 },
{ 0x21, 0x41, 0x49, 0x4D, 0x33, 0x00 },
{ 0x18, 0x14, 0x12, 0x7F, 0x10, 0x00 },
{ 0x27, 0x45, 0x45, 0x45, 0x39, 0x00 },
{ 0x3C, 0x4A, 0x49, 0x49, 0x31, 0x00 },
{ 0x41, 0x21, 0x11, 0x09, 0x07, 0x00 },
{ 0x36, 0x49, 0x49, 0x49, 0x36, 0x00 },
{ 0x46, 0x49, 0x49, 0x29, 0x1E, 0x00 },
{ 0x00, 0x36, 0x36, 0x00, 0x00, 0x00 },
{ 0x00, 0x56, 0x36, 0x00, 0x00, 0x00 },
{ 0x00, 0x08, 0x14, 0x22, 0x41, 0x00 },
{ 0x14, 0x14, 0x14, 0x14, 0x14, 0x00 },
{ 0x00, 0x41, 0x22, 0x14, 0x08, 0x00 },
{ 0x02, 0x01, 0x59, 0x09, 0x06, 0x00 },
{ 0x3E, 0x41, 0x5D, 0x59, 0x4E, 0x00 },
{ 0x7C, 0x12, 0x11, 0x12, 0x7C, 0x00 },
{ 0x7F, 0x49, 0x49, 0x49, 0x36, 0x00 },
{ 0x3E, 0x41, 0x41, 0x41, 0x22, 0x00 },
{ 0x7F, 0x41, 0x41, 0x41, 0x3E, 0x00 },
{ 0x7F, 0x49, 0x49, 0x49, 0x41, 0x00 },
{ 0x7F, 0x09, 0x09, 0x09, 0x01, 0x00 },
{ 0x3E, 0x41, 0x41, 0x51, 0x73, 0x00 },
{ 0x7F, 0x08, 0x08, 0x08, 0x7F, 0x00 },
{ 0x00, 0x41, 0x7F, 0x41, 0x00, 0x00 },
{ 0x20, 0x40, 0x41, 0x3F, 0x01, 0x00 },
{ 0x7F, 0x08, 0x14, 0x22, 0x41, 0x00 },
{ 0x7F, 0x40, 0x40, 0x40, 0x40, 0x00 },
{ 0x7F, 0x02, 0x1C, 0x02, 0x7F, 0x00 },
{ 0x7F, 0x04, 0x08, 0x10, 0x7F, 0x00 },
{ 0x3E, 0x41, 0x41, 0x41, 0x3E, 0x00 },
{ 0x7F, 0x09, 0x09, 0x09, 0x06, 0x00 },
{ 0x3E, 0x41, 0x51, 0x21, 0x5E, 0x00 },
{ 0x7F, 0x09, 0x19, 0x29, 0x46, 0x00 },
{ 0x26, 0x49, 0x49, 0x49, 0x32, 0x00 },
{ 0x03, 0x01, 0x7F, 0x01, 0x03, 0x00 },
{ 0x3F, 0x40, 0x40, 0x40, 0x3F, 0x00 },
{ 0x1F, 0x20, 0x40, 0x20, 0x1F, 0x00 },
{ 0x3F, 0x40, 0x38, 0x40, 0x3F, 0x00 },
{ 0x63, 0x14, 0x08, 0x14, 0x63, 0x00 },
{ 0x03, 0x04, 0x78, 0x04, 0x03, 0x00 },
{ 0x61, 0x59, 0x49, 0x4D, 0x43, 0x00 },
{ 0x00, 0x7F, 0x41, 0x41, 0x41, 0x00 },
{ 0x02, 0x04, 0x08, 0x10, 0x20, 0x00 },
{ 0x00, 0x41, 0x41, 0x41, 0x7F, 0x00 },
{ 0x04, 0x02, 0x01, 0x02, 0x04, 0x00 },
{ 0x40, 0x40, 0x40, 0x40, 0x40, 0x00 },
{ 0x00, 0x03, 0x07, 0x08, 0x00, 0x00 },
{ 0x20, 0x54, 0x54, 0x78, 0x40, 0x00 },
{ 0x7F, 0x28, 0x44, 0x44, 0x38, 0x00 },
{ 0x38, 0x44, 0x44, 0x44, 0x28, 0x00 },
{ 0x38, 0x44, 0x44, 0x28, 0x7F, 0x00 },
{ 0x38, 0x54, 0x54, 0x54, 0x18, 0x00 },
{ 0x00, 0x08, 0x7E, 0x09, 0x02, 0x00 },
{ 0x18, 0xA4, 0xA4, 0x9C, 0x78, 0x00 },
{ 0x7F, 0x08, 0x04, 0x04, 0x78, 0x00 },
{ 0x00, 0x44, 0x7D, 0x40, 0x00, 0x00 },
{ 0x20, 0x40, 0x40, 0x3D, 0x00, 0x00 },
{ 0x7F, 0x10, 0x28, 0x44, 0x00, 0x00 },
{ 0x00, 0x41, 0x7F, 0x40, 0x00, 0x00 },
{ 0x7C, 0x04, 0x78, 0x04, 0x78, 0x00 },
{ 0x7C, 0x08, 0x04, 0x04, 0x78, 0x00 },
{ 0x38, 0x44, 0x44, 0x44, 0x38, 0x00 },
{ 0xFC, 0x18, 0x24, 0x24, 0x18, 0x00 },
{ 0x18, 0x24, 0x24, 0x18, 0xFC, 0x00 },
{ 0x7C, 0x08, 0x04, 0x04, 0x08, 0x00 },
{ 0x48, 0x54, 0x54, 0x54, 0x24, 0x00 },
{ 0x04, 0x04, 0x3F, 0x44, 0x24, 0x00 },
{ 0x3C, 0x40, 0x40, 0x20, 0x7C, 0x00 },
{ 0x1C, 0x20, 0x40, 0x20, 0x1C, 0x00 },
{ 0x3C, 0x40, 0x30, 0x40, 0x3C, 0x00 },
{ 0x44, 0x28, 0x10, 0x28, 0x44, 0x00 },
{ 0x4C, 0x90, 0x90, 0x90, 0x7C, 0x00 },
{ 0x44, 0x64, 0x54, 0x4C, 0x44, 0x00 },
{ 0x00, 0x08, 0x36, 0x41, 0x00, 0x00 },
{ 0x00, 0x00, 0x77, 0x00, 0x00, 0x00 },
{ 0x00, 0x41, 0x36, 0x08, 0x00, 0x00 },
{ 0x02, 0x01, 0x02, 0x04, 0x02, 0x00 },
{ 0xC0, 0xC0, 0xC0, 0xC0, 0xC0, 0xC0 }
};

void Send (uint8_t d) {  
  for (uint8_t bit = 0x80; bit; bit >>= 1) {
    PINA = 1<<clk;                        // clk low
    if (d & bit) PORTA = PORTA | (1<<data); else PORTA = PORTA & ~(1<<data);
    PINA = 1<<clk;                        // clk high
  }
}

void InitDisplay () {
  // Define pins
  DDRA = DDRA | 1<<clk | 1<<dc | 1<<cs | 1<<data;   // All outputs
  PORTA = PORTA | 1<<clk | 1<<dc | 1<<cs;           // All high
  //
  ClearDisplay(0);
  PINA = 1<<dc | 1<<cs;                   // dc and cs low
  Send(0xD3);Send(0x00);                  // Clear scroll
  Send(0x81);Send(0xC0);                  // Increase contrast
  Send(0xAF);                             // Display on
  PINA = 1<<dc | 1<<cs;                   // dc and cs low
}

// Character terminal **********************************************

uint8_t Grey = 0xF;                       // Grey level; 0 to 15

// Optimised for fast scrolling
void ClearLine (uint8_t line, uint8_t grey) {
  PINA = 1<<dc | 1<<cs;                   // dc and cs low
  Send(0x00);                             // Column start low
  Send(0x10);                             // Column start high
  Send(0xB0); Send(line<<3);              // Row start
  PINA = 1<<dc;                           // dc high
  for (int i=0; i<128*8; i++) Send(grey);
  PINA = 1<<cs;                           // cs high
}

void ClearDisplay (uint8_t grey) {
  for (uint8_t p=0; p < 8; p++) ClearLine(p, grey);
  PINA = 1<<dc | 1<<cs;                   // dc and cs low
  Send(0x40);
  PINA = 1<<dc | 1<<cs;                   // dc and cs low
}

// Clears the top line, then scrolls the display up by one line
void ScrollDisplay (uint8_t *scroll) {
  ClearLine(*scroll, 0);
  *scroll = (*scroll + 1) & 0x07;
  PINA = 1<<dc | 1<<cs;                   // dc and cs low
  Send(0x40 + (*scroll<<3));
  PINA = 1<<dc | 1<<cs;                   // dc and cs low
}

void PlotChar (uint8_t ch, uint8_t line, uint8_t column) {
  uint8_t row = line<<3; uint8_t col = column*3;
  uint8_t off = (ch & 0x80) ? 0x7 : 0;    // Parenthesis highlight
  ch = (ch & 0x7f) - 32;
  PINA = 1<<cs;                           // cs low
  for (uint8_t r = 0 ; r<8; r++) {
    PINA = 1<<dc;                         // dc low
    Send(0x00 + (col & 0x0F));            // Column start low
    Send(0x10 + (col >> 4));              // Column start high
    Send(0xB0); Send((row+r) & 0x3F);     // Row start
    PINA = 1<<dc;                         // dc high
    for (uint8_t c = 0 ; c < 3; c++) {
      const uint8_t *adds = &CharMap[ch][c*2];
      uint8_t hi = pgm_read_byte(adds);
      uint8_t lo = pgm_read_byte(adds + 1);
      uint8_t mask = 1<<r;
      hi = hi & mask ? Grey<<4 : off<<4;
      lo = lo & mask ? Grey : off;
      Send(hi | lo);
    }
  }
  PINA = 1<<cs;                           // cs high
}

void PlotByte (int x, int y, int grey) {
  PINA = 1<<cs | 1<<dc;                   // cs and dc low
  Send(0x00 + (x & 0x0F));                // Column start low
  Send(0x10 + (x >> 4));                  // Column start high
  Send(0xB0); Send(63-y);                 // Row start
  PINA = 1<<dc;                           // dc high
  Send(grey);
  PINA = 1<<cs;                           // cs high
}

const int LastColumn = 41;

// Prints a character to display, with cursor, handling control characters
void Display (char c) {
  static uint8_t Line = 0, Column = 0, Scroll = 0;
  // These characters don't affect the cursor
  if (c == 8) {                    // Backspace
    if (Column == 0) {
      Line--; Column = LastColumn;
    } else Column--;
    return;
  }
  if (c == 9) {                    // Cursor forward
    if (Column == LastColumn) {
      Line++; Column = 0;
    } else Column++;
    return;
  }
  if ((c >= 17) && (c <= 20)) {    // Parentheses
    if (c == 17) PlotChar('(', Line+Scroll, Column);
    else if (c == 18) PlotChar('(' | 0x80, Line+Scroll, Column);
    else if (c == 19) PlotChar(')', Line+Scroll, Column);
    else PlotChar(')' | 0x80, Line+Scroll, Column);
    return;
  }
  // Hide cursor
  PlotChar(' ', Line+Scroll, Column);
  if (c == 0x7F) {                 // DEL
    if (Column == 0) {
      Line--; Column = LastColumn;
    } else Column--;
  } else if ((c & 0x7f) >= 32) {   // Normal character
    PlotChar(c, Line+Scroll, Column++);
    if (Column > LastColumn) {
      Column = 0;
      if (Line == 7) ScrollDisplay(&Scroll); else Line++;
    }
  // Control characters
  } else if (c == 12) {            // Clear display
    ClearDisplay(0); Line = 0; Column = 0;
  } else if (c == '\n') {          // Newline
    Column = 0;
    if (Line == 7) ScrollDisplay(&Scroll); else Line++;
  } else if (c == 7) tone(4, 440, 125); // Beep
  // Show cursor
  PlotChar(0x7F, Line+Scroll, Column);
}

// Keyboard **********************************************************************************

const int ColumnsC = 0b01111100;            // Columns 0 to 4 in port C
const int ColumnsD = 0b11111100;            // Columns 5 to 11 in port D
const int RowBits  = 0b00001111;            // Rows 0 to 4 in port B

// Character set - stored in program memory
const char Keymap[] PROGMEM = 
// Without shift
"1234567890\b" "qwertyuiop\n" "asdfghjkl \e" " zxcvbnm()."
// With shift
"\'\"#=-+/*\\;\b" "QWERTYUIOP\n" "ASDFGHJKL ~" " ZXCVBNM<>,";

// Parenthesis highlighting
void Highlight (int p, uint8_t invert) {
  if (p) {
    for (int n=0; n < p; n++) Display(8);
    Display(17 + invert);
    for (int n=1; n < p; n++) Display(9);
    Display(19 + invert);
    Display(9);
  }
}

ISR(TIMER1_OVF_vect, ISR_NOBLOCK) {
  static uint8_t column = 0, nokey = 0;
  uint8_t rows, shift, row;
  // Check rows and shift key
  shift = (PINC & 1<<PINC7) ? 0 : 1;
  rows = PINB & RowBits;
  if (rows == RowBits) { if (nokey < 11) nokey++; }
  else if (nokey < 11) nokey = 0;
  else {
    nokey = 0; row = 0;
    while ((rows & (1<<row)) != 0) row++;
    char c = pgm_read_byte(&Keymap[(3-row)*11 + column + 44*shift]);
    ProcessKey(c);
  }
  // Take last column high and next column low
  if (column < 5) PORTC = PORTC | 1<<(6-column); else PORTD = PORTD | 1<<(12-column);
  column = (column + 1) % 11;   // 0 to 10
  if (column < 5) PORTC = PORTC & ~(1<<(6-column)); else PORTD = PORTD & ~(1<<(12-column));
}
  
void ProcessKey (char c) {
  static int parenthesis = 0;
  if (c == 27) { setflag(ESCAPE); return; }    // Escape key
  // Undo previous parenthesis highlight
  Highlight(parenthesis, 0);
  parenthesis = 0;
  // Edit buffer
  if (c == '\n') {
    pserial('\n');
    KybdAvailable = 1;
    ReadPtr = 0;
    return;
  }
  if (c == 8) {     // Backspace key
    if (WritePtr > 0) {
      WritePtr--;
      Display(0x7F);
      if (WritePtr) c = KybdBuf[WritePtr-1];
    }
  } else if (WritePtr < KybdBufSize) {
    KybdBuf[WritePtr++] = c;
    Display(c);
  }
  // Do new parenthesis highlight
  if (c == ')') {
    int search = WritePtr-1, level = 0;
    while (search >= 0 && parenthesis == 0) {
      c = KybdBuf[search--];
      if (c == ')') level++;
      if (c == '(') {
        level--;
        if (level == 0) parenthesis = WritePtr-search-1;
      }
    }
    Highlight(parenthesis, 1);
  }
  return;
}

void InitKybd () {
  // Make rows input pullups
  PORTB = PORTB | RowBits;
  // Make shift key input pullup
  PORTC = PORTC | 1<<PINC7;
  // Make columns outputs
  DDRC = DDRC | ColumnsC;         // Columns 0 to 4
  DDRD = DDRD | ColumnsD;         // Columns 5 to 11
  // Take columns high
  PORTC = PORTC | ColumnsC;       // Columns 0 to 4
  PORTD = PORTD | ColumnsD;       // Columns 5 to 11
  // Start timer for interrupt
  TCCR1A = 1<<WGM10;              // Fast PWM mode, 8-bit
  TCCR1B = 1<<WGM12 | 3<<CS10;    // Divide clock by 64
  TIMSK1 = 1<<TOIE1;              // Overflow interrupt
}

// Setup

void initenv () {
  GlobalEnv = NULL;
  tee = symbol(TEE);
}

void setup () {
  InitDisplay();
  InitKybd();
  #if defined (serialmonitor)
  pinMode(8, INPUT_PULLUP); // RX0
  Serial.begin(9600);
  int start = millis();
  while (millis() - start < 5000) { if (Serial) break; }
  #endif
  initworkspace();
  initenv();
  initsleep();
  pfstring(PSTR("uLisp 3.3 "), pserial); pln(pserial);
}

// Read/Evaluate/Print loop

void repl (object *env) {
  for (;;) {
    randomSeed(micros());
    gc(NULL, env);
    #if defined (printfreespace)
    pint(Freespace, pserial);
    #endif
    if (BreakLevel) {
      pfstring(PSTR(" : "), pserial);
      pint(BreakLevel, pserial);
    }
    pfstring(PSTR("> "), pserial);
    object *line = read(gserial);
    if (BreakLevel && line == nil) { pln(pserial); return; }
    if (line == (object *)KET) error2(0, PSTR("unmatched right bracket"));
    push(line, GCStack);
    pfl(pserial);
    line = eval(line, env);
    pfl(pserial);
    printobject(line, pserial);
    pop(GCStack);
    pfl(pserial);
    pln(pserial);
  }
}

void loop () {
  if (!setjmp(exception)) {
    #if defined(resetautorun)
    volatile int autorun = 12; // Fudge to keep code size the same
    #else
    volatile int autorun = 13;
    #endif
    if (autorun == 12) autorunimage();
  }
  // Come here after error
  delay(100); while (Serial.available()) Serial.read();
  clrflag(NOESC); BreakLevel = 0; nonote(4);
  for (int i=0; i<TRACEMAX; i++) TraceDepth[i] = 0;
  #if defined(sdcardsupport)
  SDpfile.close(); SDgfile.close();
  #endif
  #if defined(lisplibrary)
  if (!tstflag(LIBRARYLOADED)) { setflag(LIBRARYLOADED); loadfromlibrary(NULL); }
  #endif
  repl(NULL);
}
