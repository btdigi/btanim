#include <EEPROM.h>

unsigned char i;
unsigned char j; 

int pin_din = 2;
int pin_cs = 3;
int pin_clk = 4;
int pin_jgnd = A0;
int pin_jvcc = A1;
int pin_jvrx = A2;
int pin_jvry = A3;
int pin_jsw = A4;

// the frames should fit in the 1024 bytes eeprom of the atmega328p, with an extra frame reserved
#define FRAMES 63
unsigned char frames[FRAMES+1][8];
unsigned char framebuffer[8];
#define IOBUFLEN (FRAMES*16+8)

unsigned long lasthoriz = 0;
unsigned long lastvert = 0;
unsigned long lastbutton = 0;
unsigned long lastadv = 0;
int nframes;
int curframe = 0;
int editmode = 0;
int editx, edity;
int again = 0;
#define TIMERS 12
int timerlist[TIMERS] = {5, 10, 15, 30, 60, 120, 250, 500, 1000, 2000, 5000, 0};  // zero means paused
int curtimer = 5;
int paused = 0;

#define BLINK_PERIOD 750
#define BLINK_SPLIT 500
#define FAST_BLINK_PERIOD 150
#define FAST_BLINK_SPLIT 100
#define KEY_REPEAT 500

void spi_send(unsigned char addr, unsigned char dat) {
  unsigned char i;
  digitalWrite(pin_cs, LOW);
  for(i=0;i<8;++i) {
    digitalWrite(pin_clk, LOW);
    digitalWrite(pin_din, addr & (0x80 >> i));
    digitalWrite(pin_clk, HIGH);
  }
  for(i=0;i<8;++i) {
    digitalWrite(pin_clk, LOW);
    digitalWrite(pin_din, dat & (0x80 >> i));
    digitalWrite(pin_clk, HIGH);
  }
  digitalWrite(pin_cs, HIGH);
}
 
void display_init() {
  // from max7219 datasheet
  spi_send(0x09, 0x00);  // decode mode (individual leds, not numbers for 7-segment display)
  spi_send(0x0a, 0x03);  // intensity (7/32 duty cycle, ~20% brightness)
  spi_send(0x0b, 0x07);  // scan limit (use all lines)
  spi_send(0x0c, 0x01);  // shutdown register (normal operation)
  spi_send(0x0f, 0x00);  // display test mode (off)
}

void display_show(unsigned char buf[8]) {
  for(int i=0; i<8; ++i) spi_send(i+1, buf[i]);  
}

#define DEMO_FRAMES 14
const unsigned char demo_frames[DEMO_FRAMES][8] = {
  0xff, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x3f, 0x01, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x0f, 0x01, 0x01, 0x01, 0x01, 0x00, 0x00, 0x00,
  0x03, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x00,
  0x00, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x03,
  0x00, 0x00, 0x00, 0x01, 0x01, 0x01, 0x01, 0x0f,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x01, 0x3f,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xff,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x80, 0x80, 0xfc,
  0x00, 0x00, 0x00, 0x80, 0x80, 0x80, 0x80, 0xf0,
  0x00, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0xc0,
  0xc0, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x00,
  0xf0, 0x80, 0x80, 0x80, 0x80, 0x00, 0x00, 0x00,
  0xfc, 0x80, 0x80, 0x00, 0x00, 0x00, 0x00, 0x00,
};

const unsigned char magic_frame[8] = "btanim01";

boolean is_initialized() {
  return !memcmp(frames[FRAMES], magic_frame, 8); 
}

void load_demo_content() {
  memset(frames, 0, FRAMES*8);
  memcpy(frames, demo_frames, DEMO_FRAMES*8);
  memcpy(frames[FRAMES], magic_frame, 8);
  count_frames();
}

void persist_save() {
  EEPROM.put(0, frames);
}

void persist_load() {
  EEPROM.get(0, frames);
  count_frames();
}

void count_frames() {
  nframes = 1;
  for(int i=FRAMES-1; i>=0; --i) {
    for(int j=0; j<8; ++j) {
      if(frames[i][j]) {
        nframes = i+1;
        return;
      }
    }
  }  
}

void setup() {
  Serial.begin(9600);
  
  pinMode(pin_clk, OUTPUT);
  pinMode(pin_cs, OUTPUT);
  pinMode(pin_din, OUTPUT);

  pinMode(pin_jgnd, OUTPUT);
  digitalWrite(pin_jgnd, LOW);
  pinMode(pin_jvcc, OUTPUT);
  digitalWrite(pin_jvcc, HIGH);
  pinMode(pin_jvrx, INPUT);
  pinMode(pin_jvry, INPUT);
  pinMode(pin_jsw, INPUT_PULLUP);
  
  delay(50);
  display_init();

  persist_load();
  if(!is_initialized()) {
    load_demo_content();
  }
}

void show_blink(unsigned char buf[8]) {
  int bp = again ? FAST_BLINK_PERIOD : BLINK_PERIOD;
  int bs = again ? FAST_BLINK_SPLIT  : BLINK_SPLIT;
  if(millis() % bp < bs) {
    display_show(buf);
  } else {
    memcpy(framebuffer, buf, 8);
    framebuffer[edity] ^= 0x80 >> editx;
    display_show(framebuffer);    
  }
}

void update_display() {
  if(editmode) {
    show_blink(frames[curframe]);
  } else {
    display_show(frames[curframe]);
  }
}

void enter_editmode() {
  editx = edity = 0;
  editmode = 1;
  again = 0;
}

void exit_editmode() {
  persist_save();
  count_frames();
  if(curframe >= nframes) curframe = 0;
  editmode = 0;
  lastadv = millis();
}

void process_key(int key) {
  if(editmode) {
    switch(key) {
      case 0: // left
        if(editx == 0) {
          if(again) {
            if(curframe > 0) --curframe;
          } else {
            again = 1;
          }
        } else {
          --editx;
          again = 0;
        }
        break;
      case 1: // right
        if(editx == 7) {
          if(again) {
            if((curframe < nframes) && (curframe < FRAMES-1)) ++curframe;
          } else {
            again = 1;
          }
        } else {
          ++editx;
          again = 0;
        }
        break;
      case 2: // up
        if(edity == 0) {
          if(again) {
            exit_editmode();
          } else {
            again = 1;
          }
        } else {
          --edity;
          again = 0;
        }
        break;
      case 3: // down
        if(edity == 7) {
          if(again) {
            exit_editmode();
          } else {
            again = 1;
          }
        } else {
          ++edity;
          again = 0;
        }
        break;
      case 4: // button
        frames[curframe][edity] ^= 0x80 >> editx;
        if(curframe >= nframes-1) count_frames();
        again = 0;
        break;
    }
  } else {
    // outside edit mode
    switch(key) {
      case 0: // left
        paused = 1;
        if(curframe > 0) {
          --curframe;
        } else {
          curframe = nframes-1;
        }
        break;
      case 1: // right
        paused = 1;
        if(curframe < nframes-1) {
          ++curframe;
        } else {
          curframe = 0;
        }
        break;
      case 2: // up
        if(!paused && curtimer > 0) --curtimer;
        paused = 0;
        break;
      case 3: // down
        if(!paused && curtimer < TIMERS-1) ++curtimer;
        paused = 0;
        break;
      case 4: // button
        enter_editmode();
        break;        
    }
  }
}

void process_timer() {
  if(editmode) return;
  if(paused) return;
  int t = timerlist[curtimer];
  if(t == 0) return;
  if(millis() > lastadv + t) {
    lastadv = millis();
    if(curframe < nframes-1) {
      ++ curframe;
    } else {
      curframe = 0;
    }
  }  
}

void process_input() {
  int xpos = analogRead(pin_jvrx);
  int left = xpos < 256;
  int right = xpos >= 768;
  if(left || right) {
    if((lasthoriz == 0) || (millis() > lasthoriz + KEY_REPEAT)) {
      lasthoriz = millis();
      if(left) process_key(0);
      if(right) process_key(1);
    }    
  } else {
    lasthoriz = 0;
  }

  int ypos = analogRead(pin_jvry);
  int up = ypos < 256;
  int down = ypos >= 768;
  if(up || down) {
    if((lastvert == 0) || (millis() > lastvert + KEY_REPEAT)) {
      lastvert = millis();
      if(up) process_key(2);
      if(down) process_key(3);
    }
  } else {
    lastvert = 0;
  }
  
  int button = !digitalRead(pin_jsw);
  if(button) {
    if((lastbutton == 0) || (millis() > lastbutton + KEY_REPEAT)) {
      lastbutton = millis();
      process_key(4);      
    }
  } else {
    lastbutton = 0;
  }
}

unsigned char from_hex(unsigned char i) {
  if(48 <= i && i < 58) return i-48;
  if(65 <= i && i < 71) return i-55;
  if(97 <= i && i < 103) return i-87;
  return '!';
}

unsigned char to_hex(unsigned char i) {
  if(i < 10) return i+48;
  if(i < 16) return i+87;
  return '!';  
}

int save_to_buffer(unsigned char *iobuffer) {
  strncpy(iobuffer, "load:", 5);
  int ptr = 5;
  for(int i=0; i<nframes; ++i) {
    for(int j=0; j<8; ++j) {
      unsigned char c = frames[i][j];
      iobuffer[ptr++] = to_hex(c/16);
      iobuffer[ptr++] = to_hex(c%16);     
    }
  }
  iobuffer[ptr++] = '\n';
  return ptr;
}

boolean load_from_buffer(unsigned char *iobuffer, int ilen) {
  int ptr = 5;
  int i = 0;
  while(ptr+16 <= ilen) {
    for(int j=0; j<8; ++j) {
      unsigned char c1 = from_hex(iobuffer[ptr++]);
      unsigned char c2 = from_hex(iobuffer[ptr++]);
      if(c1 == '!' || c2 == '!') return false;
      frames[i][j] = c1*16 + c2;
    }
    ++i;    
  }
  if(ptr == ilen) {
    memset(frames[i], 0, (FRAMES-i)*8);
    return true;
  } else {
    return false;
  }
}

void process_serial() {
  unsigned char iobuffer[IOBUFLEN];
  if(Serial.available()) {
    int ilen = Serial.readBytesUntil('\n', iobuffer, IOBUFLEN);
    if(iobuffer[ilen-1] == '\r') --ilen;   // support dos-style line endings
    if(ilen == 0) return;
    if((ilen == 4) && !strncmp(iobuffer, "save", 4)) {
      if(int olen = save_to_buffer(iobuffer)) {
        Serial.write(iobuffer, olen);
      } else {
        Serial.write("error:save\n");
      }
    }
    else if((ilen >= 5) && !strncmp(iobuffer, "load:", 5)) {
      if(load_from_buffer(iobuffer, ilen)) {
        exit_editmode();
        Serial.write("ok\n");
      } else {
        Serial.write("error:load\n");
      }
    }
    else {
      Serial.write("error:parse\n");      
    }    
  }  
}
 
void loop() {
  process_timer();
  process_input();
  process_serial();
  update_display();
  delay(5);
}
