/*
  Badge Extensions
*/

#if defined(LISP_BADGE)

// Support Functions and Variables

extern const uint8_t CharMap[96][6] PROGMEM;

void plotsub (uint8_t x, uint8_t y, uint8_t n, int ys[5]) {
  if (y<64) {
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

// Extension Definitions

object *fn_plot (object *args, object *env) {
  int ys[5] = {-1, -1, -1, -1, -1};
  int xaxis = -1, yaxis = -1;
  delay(20);
  ClearDisplay(0); // Clear display
  if (args != NULL && integerp(first(args))) { xaxis = checkinteger(first(args)); args = cdr(args); }
  if (args != NULL && integerp(first(args))) { yaxis = checkinteger(first(args)); args = cdr(args); }
  int nargs = min(listlength(args),4);
  for (int x=0; x<256; x++) {
    object *rest = args;
    for (int n=0; n<nargs; n++) {
      object *function = first(rest);
      int y = checkinteger(apply(function, cons(number(x), NULL), env));
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
  if (args != NULL && integerp(first(args))) { xaxis = checkinteger(first(args)); args = cdr(args); }
  if (args != NULL && integerp(first(args))) { yaxis = checkinteger(first(args)); args = cdr(args); }
  if (args != NULL) {
    object *function = first(args);
    for (int y=0; y<64; y++) {
      for (int x=0; x<256; x++) {
        int z = checkinteger(apply(function, cons(number(x), cons(number(y), NULL)), env));
        if (x == xaxis || y == yaxis) z = 0xF;
        if ((x&1) == 0) blob = z<<4; else blob = blob | (z&0xF);
        PlotByte(x>>1, y, blob);
      }
    }
  }
  while (!tstflag(ESCAPE)); clrflag(ESCAPE);
  return symbol(NOTHING);
}

object *fn_glyphpixel (object *args, object *env) {
  (void) env;
  uint8_t c = 0, x = 6, y = 8;
  c = checkchar(first(args));
  x = checkinteger(second(args));
  y = checkinteger(third(args));
  if (x > 5 || y > 7) return number(0);
  return pgm_read_byte(&CharMap[(c & 0x7f) - 32][x]) & 1 << (7 - y) ? number(15) : number(0);
}

object *fn_plotpixel (object *args, object *env) {
  (void) env;
  int x = checkinteger(first(args));
  int y = checkinteger(second(args));
  args = cddr(args);
  uint8_t grey = 0xff;
  if (args != NULL) grey = checkinteger(first(args));
  PlotByte(x, y, grey);
  return nil;
}

object *fn_fillscreen (object *args, object *env) {
  (void) env;
  uint8_t grey = 0;
  if (args != NULL) grey = checkinteger(first(args));
  ClearDisplay(grey);
  return nil;
}

// Symbol Names

const char stringPLT[] PROGMEM = "plot";
const char stringP3D[] PROGMEM = "plot3d";
const char stringGPX[] PROGMEM = "glyph-pixel";
const char stringPPX[] PROGMEM = "plot-pixel";
const char stringFSN[] PROGMEM = "fill-screen";

// Documentation strings

const char doc_plot[] PROGMEM = "(plot [x-intercept y-intercept] [function]...)\n"
"Plots up to four functions on the same graph, optionally with axes.\n"
"Each function should be a function of one parameter, the x coordinate, and it will be called with\n"
"each value of x from 0 to 255. The function should return the y value, from 0 to 63.";
const char doc_plot3d[] PROGMEM = "(plot3d [x-intercept y-intercept] [function])\n"
"The function should be a function of two parameters, the x and y coordinates.\n"
"It will be called with each value of x from 0 to 255 and y from 0 to 63\n"
"The function should return the greyscale value to be plotted, from 0 to 15.";
const char doc_glyphpixel[] PROGMEM = "(glyph-pixel char x y)\n"
"Returns the pixel value from the 6x8 bitmap for character char, where x can be from 0 to 5\n"
"and y can be from 0 to 7.";
const char doc_plotpixel[] PROGMEM = "(plot-pixel x y [pixel])\n"
"Plots a pixel to the specified x,y coordinates, where x should be 0 to 127 and y should\n"
"be 0 to 63.\n"
"Because of the way that the display memory is mapped to the display, plot-pixel plots\n"
"two adjacent pixels with an x resolution of 128. The third parameter pixel specified the\n"
"colours of the two pixels; if omitted it defaults to #xFF which gives two adjacent white pixels.\n";
const char doc_fillscreen[] PROGMEM = "(fill-screen [colour])\n"
"Clears the screen to black, or fills it with a specified pixel value.";

// Symbol lookup table
const tbl_entry_t lookup_table2[] PROGMEM = {
  { stringPLT, fn_plot,       0206, doc_plot },
  { stringP3D, fn_plot3d,     0203, doc_plot3d },
  { stringGPX, fn_glyphpixel, 0233, doc_glyphpixel },
  { stringPPX, fn_plotpixel,  0223, doc_plotpixel },
  { stringFSN, fn_fillscreen, 0201, doc_fillscreen },
};

// Table cross-reference functions

tbl_entry_t *tables[] = {lookup_table, lookup_table2};
const unsigned int tablesizes[] = { arraysize(lookup_table), arraysize(lookup_table2) };

#else

tbl_entry_t *tables[] = {lookup_table};
const unsigned int tablesizes[] = { arraysize(lookup_table) };

#endif

const tbl_entry_t *table (int n) {
  return tables[n];
}

unsigned int tablesize (int n) {
  return tablesizes[n];
}
