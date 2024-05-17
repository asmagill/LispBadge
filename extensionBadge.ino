/*
  Badge Extensions
*/

#if defined(LISP_BADGE)

// Lisp Badge terminal and keyboard support

// These are the bit positions in PORTA
uint8_t const clk = 7;   // PA7
uint8_t const data = 6;  // PA6
uint8_t const dc = 5;    // PA5
uint8_t const cs = 4;    // PA4

const uint16_t xsize = 128;
const uint16_t ysize = 64;

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

uint8_t const Grey = 0xF;                       // Grey level; 0 to 15

int16_t Xpos, Ypos ;

// Optimised for fast scrolling
void ClearLine (uint8_t line, uint8_t grey) {
  PINA = 1<<dc | 1<<cs;                   // dc and cs low
  Send(0x00);                             // Column start low
  Send(0x10);                             // Column start high
  Send(0xB0); Send(line<<3);              // Row start
  PINA = 1<<dc;                           // dc high
  for (uint16_t i=0; i<xsize*8; i++) Send(grey);
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

void PlotByte (int16_t x, int16_t y, int16_t grey) {
  if ((x < 0) || (x >= xsize) || (y < 0) || (y >= ysize)) return ;
  PINA = 1<<cs | 1<<dc;                   // cs and dc low
  Send(0x00 + (x & 0x0F));                // Column start low
  Send(0x10 + (x >> 4));                  // Column start high
  Send(0xB0); Send((ysize - 1)-y);        // Row start
  PINA = 1<<dc;                           // dc high
  Send(grey);
  PINA = 1<<cs;                           // cs high
}

// Move current plot position to x,y
void MoveTo (int16_t x, int16_t y) {
  Xpos = x; Ypos = y;
}

// Draw a line from Xpos,Ypos to x,y
void DrawTo (int16_t x, int16_t y, uint8_t grey) {
  int16_t sx, sy, e2, err;
  int16_t dx = abs(x - Xpos);
  int16_t dy = abs(y - Ypos);
  if (Xpos < x) sx = 1; else sx = -1;
  if (Ypos < y) sy = 1; else sy = -1;
  err = dx - dy;
  for (;;) {
    PlotByte(Xpos, Ypos, grey);
    if (Xpos==x && Ypos==y) break;
    e2 = err<<1;
    if (e2 > -dy) { err = err - dy; Xpos = Xpos + sx; }
    if (e2 < dx) { err = err + dx; Ypos = Ypos + sy; }
  }
//   PlotByte(255, 0, grey); // Flush
}

void DrawRect (int16_t w, int16_t h, uint8_t colour) {
  if (w < 0) w = 0 ;
  if (h < 0) h = 0 ;
  int16_t x = Xpos, y = Ypos;
  MoveTo(x, y); DrawTo(x+w-1, y, colour);
  DrawTo(x+w-1, y+h-1, colour); DrawTo(x, y+h-1, colour);
  DrawTo(x, y, colour);
}

void FillRect (int16_t w, int16_t h, uint8_t colour) {
  if (w < 0) w = 0 ;
  if (h < 0) h = 0 ;
  int16_t x = Xpos, y = Ypos;
  for (int16_t i=x; i<x+w; i++) {
    MoveTo(i, y); DrawTo(i, y-h+1, colour);
  }
  Xpos = x; Ypos = y;
}

void DrawCircle (int16_t radius, uint8_t colour) {
  if (radius < 0) radius = 0 ;
  int16_t x1 = Xpos, y1 = Ypos; int16_t dx = 1, dy = 1;
  int16_t x = radius - 1, y = 0;
  int16_t err = dx - (radius<<1);
  while (x >= y) {
    PlotByte(x1+x, y1-y, colour); PlotByte(x1+x, y1+y, colour); //4
    PlotByte(x1+y, y1-x, colour); PlotByte(x1+y, y1+x, colour); //3
    PlotByte(x1-y, y1-x, colour); PlotByte(x1-y, y1+x, colour); //2
    PlotByte(x1-x, y1-y, colour); PlotByte(x1-x, y1+y, colour); //1
    if (err > 0) {
      x = x - 1; dx = dx + 2;
      err = err - (radius<<1) + dx;
    } else {
      y = y + 1; err = err + dy;
      dy = dy + 2;
    }
  }
  Xpos = x1; Ypos = y1;
}

void FillCircle (uint8_t radius, uint8_t colour) {
  if (radius < 0) radius = 0 ;
  int16_t x1 = Xpos, y1 = Ypos; int16_t dx = 1, dy = 1;
  int16_t x = radius - 1, y = 0;
  int16_t err = dx - (radius<<1);
  while (x >= y) {
    MoveTo(x1+x, y1-y); DrawTo(x1+x, y1+y, colour); //4
    MoveTo(x1+y, y1-x); DrawTo(x1+y, y1+x, colour); //3
    MoveTo(x1-y, y1-x); DrawTo(x1-y, y1+x, colour); //2
    MoveTo(x1-x, y1-y); DrawTo(x1-x, y1+y, colour); //1
    if (err > 0) {
      x = x - 1; dx = dx + 2;
      err = err - (radius<<1) + dx;
    } else {
      y = y + 1; err = err + dy;
      dy = dy + 2;
    }
  }
  Xpos = x1; Ypos = y1;
}

#define swap(a, b) { a = a ^ b; b = b ^ a; a = a ^ b; }

void DrawTriangle(int16_t x0, int16_t y0, int16_t x1,
                  int16_t y1, int16_t x2, int16_t y2, uint8_t colour) {
  MoveTo(x0, y0); DrawTo(x1, y1, colour); DrawTo(x2, y2, colour); DrawTo(x0, y0, colour);
}

void FillTriangle(int16_t x0, int16_t y0, int16_t x1,
                  int16_t y1, int16_t x2, int16_t y2, uint8_t colour) {
  // Sort coordinates by y order (y2 >= y1 >= y0)
  if (y0 > y1) { swap(y0, y1); swap(x0, x1); }
  if (y1 > y2) { swap(y1, y2); swap(x1, x2); }
  if (y0 > y1) { swap(y0, y1); swap(x0, x1); }
  TriangleQuad(x0, y0, x1, y1, x2, y2, x2, y2, colour);
}

void TriangleQuad(int16_t x0, int16_t y0, int16_t x1, int16_t y1,
                  int16_t x2, int16_t y2, int16_t x3, int16_t y3, uint8_t colour) {
  // Coordinates already in y order (y3 >= y2 >= y1 >= y0)
  int16_t a, b, y;

  // Special case?
  int16_t x4 = x0 + (x3 - x0) * (y1 - y0) / (y3 - y0);
  int16_t x5 = x0 + (x3 - x0) * (y2 - y0) / (y3 - y0);

  if ((x5 > x2) == (x4 > x1)) {
    swap(x2, x5);
  } else { // Kite
    x4 = x0 + (x2 - x0) * (y1 - y0) / (y2 - y0);
    x5 = x1 + (x3 - x1) * (y2 - y1) / (y3 - y1);
  }

  // Fill bottom section
  for (y = y0; y <= y1; y++) {
    a = x0 + (x4 - x0) * (y - y0) / (y1 - y0);
    b = x0 + (x1 - x0) * (y - y0) / (y1 - y0);
    if (a > b) swap(a, b);
    MoveTo(a, y); FillRect(b - a + 1, 1, colour);
  }

  // Fill middle section
  for (; y <= y2; y++) {
    a = x4 + (x2 - x4) * (y - y1) / (y2 - y1);
    b = x1 + (x5 - x1) * (y - y1) / (y2 - y1);
    if (a > b) swap(a, b);
    MoveTo(a, y); FillRect(b - a + 1, 1, colour);
  }

  // Fill top section
  for (; y <= y3; y++) {
    a = x2 + (x3 - x2) * (y - y2) / (y3 - y2);
    b = x5 + (x3 - x5) * (y - y2) / (y3 - y2);
    if (a > b) swap(a, b);
    MoveTo(a, y); FillRect(b - a + 1, 1, colour);
  }
}

const uint8_t LastColumn = 41;

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

const uint8_t ColumnsC = 0b01111100;            // Columns 0 to 4 in port C
const uint8_t ColumnsD = 0b11111100;            // Columns 5 to 11 in port D
const uint8_t RowBits  = 0b00001111;            // Rows 0 to 4 in port B

// Character set - stored in program memory
const char Keymap[] PROGMEM =
// Without shift
"1234567890\b" "qwertyuiop\n" "asdfghjkl?\e" " zxcvbnm()."
// With shift
"\'\"#=-+/*\\;%" "QWERTYUIOP:" "ASDFGHJKL?~" "?ZXCVBNM<>,";

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

// Support Functions and Variables

#define SHIFT_KEY 23

extern const uint8_t CharMap[96][6] PROGMEM;

void plotsub (uint8_t x, uint8_t y, uint8_t n, int16_t ys[5]) {
  if (y<ysize) {
    uint8_t grey = 0x0F-n*3;
    uint8_t blob = grey;
    if ((x&1) == 0) { blob = grey<<4; ys[n] = y; }
    else {
      for (uint8_t i=0; i<5; i++) {
        if (y == ys[i]) blob = (0x0F-i*3)<<4 | grey;
      }
    }
    PlotByte(x>>1, y, blob);
  }
}

bool checkkey (char key) {
  for (uint8_t k=0; k<44; k++) {
    if (pgm_read_byte(&Keymap[k]) == key) {
      uint8_t column = k % 11;
      if (column < 5) PORTC = PORTC & ~(1<<(6-column)); else PORTD = PORTD & ~(1<<(12-column));
      uint8_t row = 3 - k/11; // Gives port time to settle
      uint8_t input = PINB;
      if (column < 5) PORTC = PORTC | 1<<(6-column); else PORTD = PORTD | 1<<(12-column);
      return ((input & 1<<row) == 0);
    }
  }
  return false;
}

void keyboard (bool enable) {
  if (enable) TIMSK1 = 1<<TOIE1;
  else {
    TIMSK1 = 0;
    // Take columns high
    PORTC = PORTC | ColumnsC;       // Columns 0 to 4
    PORTD = PORTD | ColumnsD;       // Columns 5 to 11
  }
}

void display_sleep(bool state) {
  PINA = 1<<dc | 1<<cs;                   // dc and cs low
  Send(state ? 0xAE : 0xAF);              // Display off : on
  PINA = 1<<dc | 1<<cs;                   // dc and cs low
}
// Extension Definitions

object *fn_plot (object *args, object *env) {
  int16_t ys[5] = {-1, -1, -1, -1, -1};
  int16_t xaxis = -1, yaxis = -1;
  delay(20);
  ClearDisplay(0); // Clear display
  if (args != NULL && integerp(first(args))) { xaxis = checkinteger(first(args)); args = cdr(args); }
  if (args != NULL && integerp(first(args))) { yaxis = checkinteger(first(args)); args = cdr(args); }
  uint8_t nargs = min(listlength(args),4);
  for (uint16_t x=0; x<(xsize * 2); x++) {
    object *rest = args;
    for (uint8_t n=0; n<nargs; n++) {
      object *function = first(rest);
      int16_t y = checkinteger(apply(function, cons(number(x), NULL), env));
      plotsub(x, y, n+1, ys);
      rest = cdr(rest);
    }
    plotsub(x, yaxis, 0, ys);
    if (x == xaxis) for (int16_t y=0; y<ysize; y++) plotsub(x, y, 0, ys);
    if ((x&1) != 0) for (uint8_t i=0; i<5; i++) ys[i] = -1;
  }
  while (!tstflag(ESCAPE)); clrflag(ESCAPE);
  return symbol(NOTHING);
}

object *fn_plot3d (object *args, object *env) {
  int16_t xaxis = -1, yaxis = -1;
  uint8_t blob;
  delay(20);
  ClearDisplay(0); // Clear display
  if (args != NULL && integerp(first(args))) { xaxis = checkinteger(first(args)); args = cdr(args); }
  if (args != NULL && integerp(first(args))) { yaxis = checkinteger(first(args)); args = cdr(args); }
  if (args != NULL) {
    object *function = first(args);
    for (uint16_t y=0; y<ysize; y++) {
      for (uint16_t x=0; x<(xsize * 2); x++) {
        int16_t z = checkinteger(apply(function, cons(number(x), cons(number(y), NULL)), env));
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
  int16_t x = checkinteger(first(args));
  int16_t y = checkinteger(second(args));
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

object *fn_drawline (object *args, object *env) {
  (void) env;
  uint8_t grey = 0xff;
  int16_t params[4];
  for (uint8_t i=0; i<4; i++) { params[i] = checkinteger(car(args)); args = cdr(args); }
  if (args != NULL) grey = checkinteger(car(args));
  MoveTo(params[0], (ysize - 1)-params[1]);
  DrawTo(params[2], (ysize - 1)-params[3], grey);
  return nil;
}

object *fn_drawrect (object *args, object *env) {
  (void) env;
  uint8_t grey = 0xff;
  int16_t params[4];
  for (uint8_t i=0; i<4; i++) { params[i] = checkinteger(car(args)); args = cdr(args); }
  if (args != NULL) grey = checkinteger(car(args));
  MoveTo(params[0], (ysize - 1)-params[1]);
  DrawRect(params[2]-params[0], params[3]-params[1], grey);
  return nil;
}

object *fn_fillrect (object *args, object *env) {
  (void) env;
  uint8_t grey = 0xff;
  int16_t params[4];
  for (uint8_t i=0; i<4; i++) { params[i] = checkinteger(car(args)); args = cdr(args); }
  if (args != NULL) grey = checkinteger(car(args));
  MoveTo(params[0], (ysize - 1)-params[1]);
  FillRect(params[2]-params[0], params[3]-params[1], grey);
  return nil;
}

object *fn_drawcircle (object *args, object *env) {
  (void) env;
  uint8_t grey = 0xff;
  int16_t params[3];
  for (uint8_t i=0; i<3; i++) { params[i] = checkinteger(car(args)); args = cdr(args); }
  if (args != NULL) grey = checkinteger(car(args));
  MoveTo(params[0], (ysize - 1)-params[1]);
  DrawCircle(params[2], grey);
  return nil;
}

object *fn_fillcircle (object *args, object *env) {
  (void) env;
  uint8_t grey = 0xff;
  int16_t params[3];
  for (uint8_t i=0; i<3; i++) { params[i] = checkinteger(car(args)); args = cdr(args); }
  if (args != NULL) grey = checkinteger(car(args));
  MoveTo(params[0], (ysize - 1)-params[1]);
  FillCircle(params[2], grey);
  return nil;
}

object *fn_drawtriangle (object *args, object *env) {
  (void) env;
  uint8_t grey = 0xff;
  int16_t params[6];
  for (uint8_t i=0; i<6; i++) { params[i] = checkinteger(car(args)); args = cdr(args); }
  if (args != NULL) grey = checkinteger(car(args));
  DrawTriangle(params[0], params[1], params[2], params[3], params[4], params[5], grey);
  return nil;
}

object *fn_filltriangle (object *args, object *env) {
  (void) env;
  uint8_t grey = 0xff;
  int16_t params[6];
  for (uint8_t i=0; i<6; i++) { params[i] = checkinteger(car(args)); args = cdr(args); }
  if (args != NULL) grey = checkinteger(car(args));
  FillTriangle(params[0], params[1], params[2], params[3], params[4], params[5], grey);
  return nil;
}

object *fn_checkkey (object *args, object *env) {
  (void) env;
  return checkkey(checkchar(first(args))) ? tee : nil;
}

object *fn_keyboard (object *args, object *env) {
  (void) env;
  if (args != NULL) {
    object *enable = first(args);
    keyboard(enable != NULL);
    return enable;
  } else {
    return (TIMSK1 & (1<<TOIE1)) ? tee : nil ;
  }
}

// Symbol Names

const char stringPLT[] PROGMEM = "plot";
const char stringP3D[] PROGMEM = "plot3d";
const char stringGPX[] PROGMEM = "glyph-pixel";
const char stringPPX[] PROGMEM = "plot-pixel";
const char stringFSN[] PROGMEM = "fill-screen";

const char stringDRP[] PROGMEM = "draw-pixel";
const char stringDRL[] PROGMEM = "draw-line";
const char stringDRR[] PROGMEM = "draw-rect";
const char stringFIR[] PROGMEM = "fill-rect";
const char stringDRC[] PROGMEM = "draw-circle";
const char stringFIC[] PROGMEM = "fill-circle";
const char stringDRT[] PROGMEM = "draw-triangle";
const char stringFIT[] PROGMEM = "fill-triangle";
// const char stringG13[] PROGMEM = "draw-char";

const char stringCHK[] PROGMEM = "check-key";
const char stringKBD[] PROGMEM = "keyboard";
const char stringSFT[] PROGMEM = ":shift-key";

// Documentation strings

const char docPLT[] PROGMEM = "(plot [x-intercept y-intercept] [function]...)\n"
"Plots up to four functions on the same graph, optionally with axes.\n"
"Each function should be a function of one parameter, the x coordinate, and it will be called with\n"
"each value of x from 0 to 255. The function should return the y value, from 0 to 63.";
const char docP3D[] PROGMEM = "(plot3d [x-intercept y-intercept] [function])\n"
"The function should be a function of two parameters, the x and y coordinates.\n"
"It will be called with each value of x from 0 to 255 and y from 0 to 63\n"
"The function should return the greyscale value to be plotted, from 0 to 15.";
const char docGPX[] PROGMEM = "(glyph-pixel char x y)\n"
"Returns the pixel value from the 6x8 bitmap for character char, where x can be from 0 to 5\n"
"and y can be from 0 to 7.";
const char docPPX[] PROGMEM = "(plot-pixel|draw-pixel x y [pixel])\n"
"Plots a pixel to the specified x,y coordinates, where x should be 0 to 127 and y should\n"
"be 0 to 63.\n"
"Because of the way that the display memory is mapped to the display, plot-pixel plots\n"
"two adjacent pixels with an x resolution of 128. The third parameter pixel specified the\n"
"greys of the two pixels; if omitted it defaults to #xFF which gives two adjacent white pixels.\n";
const char docFSN[] PROGMEM = "(fill-screen [grey])\n"
"Clears the screen to black, or fills it with a specified pixel value.";

// const char docG1[] PROGMEM = "(draw-pixel x y [grey])\n"
// "Draws a pixel at coordinates (x,y) in grey, or white if omitted.";
const char docDRL[] PROGMEM = "(draw-line x0 y0 x1 y1 [grey])\n"
"Draws a line from (x0,y0) to (x1,y1) in grey, or white if omitted.";
const char docDRR[] PROGMEM = "(draw-rect x y w h [grey])\n"
"Draws an outline rectangle with its top left corner at (x,y), with width w,\n"
"and with height h. The outline is drawn in grey, or white if omitted.";
const char docFIR[] PROGMEM = "(fill-rect x y w h [grey])\n"
"Draws a filled rectangle with its top left corner at (x,y), with width w,\n"
"and with height h. The outline is drawn in grey, or white if omitted.";
const char docDRC[] PROGMEM = "(draw-circle x y r [grey])\n"
"Draws an outline circle with its centre at (x, y) and with radius r.\n"
"The circle is drawn in grey, or white if omitted.";
const char docFIC[] PROGMEM = "(fill-circle x y r [grey])\n"
"Draws a filled circle with its centre at (x, y) and with radius r.\n"
"The circle is drawn in grey, or white if omitted.";
const char docDRT[] PROGMEM = "(draw-triangle x0 y0 x1 y1 x2 y2 [grey])\n"
"Draws an outline triangle between (x1,y1), (x2,y2), and (x3,y3).\n"
"The outline is drawn in grey, or white if omitted.";
const char docFIT[] PROGMEM = "(fill-triangle x0 y0 x1 y1 x2 y2 [grey])\n"
"Draws a filled triangle between (x1,y1), (x2,y2), and (x3,y3).\n"
"The outline is drawn in grey, or white if omitted.";
// const char docG13[] PROGMEM = "(draw-char x y char [grey background size])\n"
// "Draws the character char with its top left corner at (x,y).\n"
// "The character is drawn in a 5 x 7 pixel font in grey against background,\n"
// "which default to white and black respectively.\n"
// "The character can optionally be scaled by size.";

const char docCHK[] PROGMEM = "(check-key char)\n"
"Returns t if the key char is pressed, or nil if not.";
const char docKBD[] PROGMEM = "(keyboard [enable])\n"
"Enables or disables the keyboard if enable is given. Returns the current state.";

// Symbol lookup table
const tbl_entry_t lookup_table2[] PROGMEM = {
  { stringPLT, fn_plot,               0206, docPLT },
  { stringP3D, fn_plot3d,             0203, docP3D },
  { stringGPX, fn_glyphpixel,         0233, docGPX },
  { stringPPX, fn_plotpixel,          0223, docPPX },
  { stringFSN, fn_fillscreen,         0201, docFSN },

  { stringDRP, fn_plotpixel,          0223, docPPX },
  { stringDRL, fn_drawline,           0245, docDRL },
  { stringDRR, fn_drawrect,           0245, docDRR },
  { stringFIR, fn_fillrect,           0245, docFIR },
  { stringDRC, fn_drawcircle,         0234, docDRC },
  { stringFIC, fn_fillcircle,         0234, docFIC },
  { stringDRT, fn_drawtriangle,       0267, docDRT },
  { stringFIT, fn_filltriangle,       0267, docFIT },
//   { stringG13, fn_drawchar, 0236, docG13 },

  { stringCHK, fn_checkkey,           0211, docCHK },
  { stringKBD, fn_keyboard,           0201, docKBD },
  { stringSFT, (fn_ptr_type)SHIFT_KEY,   0, NULL },
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
