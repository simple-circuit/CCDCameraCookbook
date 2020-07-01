/* CCD Camera Cookbook USB serial interface. The readout and timing are handled 
   by a serial command interpreter.

   This code is requires a new interface board. The preamp board and CCD do not need replacing. 
   However, the DS0026 drivers must be replaced with TC1426 drivers. A schematic is provided in
   the project folder.

2020 Simple-Circuit */

#include <TimerThree.h>

//ADC Constants
#define cs 10
#define clk 13
#define so 11
#define si 12

//TC245 Camera Constants
#define SRG1 0x00040000   //1<<18       //pin-14 = srg1 clock
#define SRG2 0x00080000  //1<<19       //pin-15 = srg2 clock
#define SRG3 0x00800000  //1<<23       //pin-16 2 = srg3 clock
#define TRG  0x00400000  //1<<22       //pin-17 = trg clock
#define IAG  0x00020000  //1<<17      //pin-18 = iag clock / LDC
#define SAG  0x00010000  //1<<16      //pin-19 = sag clock
#define LOWNOISE 0x04000000 //1<<26  //pin-20 = amp electro lum
#define CCDINIT  0      //normal static state of clock lines


// TC211 Camera Constants
#define TSRG SRG1 + TRG  //Pin-14 and Pin-17 are SRG normal
#define TIAG SRG2              //Pin-15 is IAG
#define SH   0                  //not used
#define CONV TRG             //Pin-17 is for SRG correlated double sample
 

#define TCCDINIT 0 //Starting condition for the CCD
#define TA0 0
#define TA1 0
#define TIAG1  TCCDINIT+TIAG      //Timing sequence for an image
#define TIAG2  TCCDINIT+TIAG+TSRG //area gate shift
#define TIAG3  TCCDINIT+TSRG
#define TIAG4  TCCDINIT

#define TSRG1  TCCDINIT+TSRG    //Timing sequence for a serial
#define TSRG2  TCCDINIT        //gate pixel shift

#define TCCD1  TCCDINIT+TSRG-TA1  //Timing sequence for a serial
#define TCCD2  TCCDINIT+TSRG-TA0 //gate pixel shift with an ADC
#define TCCD3  TCCDINIT         //conversion and read
#define TCCD4  TCCDINIT+SH
#define TCCD5  TCCDINIT

#define TCCD1L  TCCDINIT-TA1      //Low noise read out multiple
#define TCCD2L  TCCDINIT-TA0      //acquisition of data pixel

#define TCCD3D  TCCDINIT+TSRG     //Double sampling read out of
#define TCCD4D  TCCDINIT+TSRG+SH  //reset voltage
#define TCCD5D  TCCDINIT+TSRG

#define TCCD1C  TCCDINIT-TA1+TSRG    //Timing sequence for a serial
#define TCCD2C  TCCDINIT-TA0+TSRG
#define TCCD3C  TCCDINIT+TSRG-CONV   //gate pixel shift for a double
#define TCCD4C  TCCDINIT+SH+TSRG-CONV
#define TCCD5C  TCCDINIT+TSRG-CONV
#define TCCD6C  TCCDINIT-TA1
#define TCCD7C  TCCDINIT-TA0          //correlated sample
#define TCCD8C  TCCDINIT+TSRG-CONV    //gate pixel shift for a double
#define TCCD9C  TCCDINIT+SH+TSRG-CONV
#define TCCD10C TCCDINIT+TSRG

#define SHUTHI  (1<<18) + (1<<19) + (1<<22) //Extra control bit constants for -electroluminescence
#define SHUTLO  (1<<19) + (1<<22)

#define NODARK  IAG       //TCCDINIT + IAG low dark integrate mode

#define RST1 TSRG + TCCDINIT
#define RST2  TSRG + TCCDINIT + SH
#define RST3  (1<<16)
#define RST4  (1<<17)
#define RST5  TCCDINIT
#define Delay2 5    //op amp delay
 
volatile short img[213444];

volatile int  vl;     // line counter
volatile int vp;     // pixel counter
volatile int  vd;     // dummy pixel counter
volatile int  vph; 
volatile int  vlh; 
volatile int  vp2; 

volatile int imagex;
volatile unsigned long  resetavg, refavg;
volatile int  stopintegration; 
volatile unsigned int  itdelay, itdelay2, itd2; // use only in integration()

volatile int  xp, yp;
volatile int idsmode;
volatile int options;          //options such as low dark current 7 bit value
volatile int mode;             //image mode 7 bit value
volatile int xshift,yshift;    //center read image shift x and y 2 x (two 7 bit values)
volatile unsigned long tms;    //integration time in ms (three 7 bit values)
volatile unsigned long t;
volatile unsigned long te;
volatile int east,west,north,south;

void setup() {
  pinMode(14,OUTPUT);
  pinMode(15,OUTPUT);
  pinMode(16,OUTPUT);
  pinMode(17,OUTPUT);
  pinMode(18,OUTPUT);
  pinMode(19,OUTPUT);
  pinMode(20,OUTPUT);
  pinMode(21,OUTPUT);
  pinMode(22,OUTPUT);
  pinMode(23,OUTPUT);

  pinMode(clk,OUTPUT);   //sclk
  pinMode(si,INPUT);    //serin
  pinMode(so,OUTPUT);   //serout
  pinMode(cs,OUTPUT);   //cs  

  pinMode(1,OUTPUT);
  pinMode(2,OUTPUT);
  pinMode(3,OUTPUT);
  pinMode(4,OUTPUT);
  digitalWrite(1,0);
  digitalWrite(2,0);
  digitalWrite(3,0);
  digitalWrite(4,0);
  
  outlow(TCCDINIT);

  east =0;
  west= 0;
  north = 0;
  south = 0;
    
  Serial.begin(1843200);
  
  Timer3.initialize(10000);
  Timer3.attachInterrupt(guide);
}

void loop() {
  volatile char c;
  volatile int st;
  
  while (true){
   c=0;  
   while (c!=255){
    while (Serial.available() == 0);
    c = Serial.read();  
   }
   mode = 0; 
   while (Serial.available() == 0);
   c = Serial.read();   
   if (c==255) break;
   mode = c;
   while (Serial.available() == 0);
   c = Serial.read();
   options = c;
   if (c==255) break;
   
   xshift = 0;
   while (Serial.available() == 0);
   c = Serial.read();
   xshift = c * 128;
   if (c==255) break;
   while (Serial.available() == 0);
   c = Serial.read();
   xshift = xshift + c;
   if (c==255) break;
   
   yshift = 0;
   while (Serial.available() == 0);
   c = Serial.read();
   yshift = c * 128;
   if (c==255) break;
   while (Serial.available() == 0);
   c = Serial.read();
   yshift = yshift + c;
   if (c==255) break;
   
   tms = 0;
   while (Serial.available() == 0);
   c = Serial.read();
   tms = c*128*128;
   if (c==255) break;
   while (Serial.available() == 0);
   c = Serial.read();
   tms = tms + c*128;
   if (c==255) break;
   while (Serial.available() == 0);
   c = Serial.read();
   tms = tms + c;
   break;
  }

 if (c!=255){  
   if (mode==99) {
    Serial.write(211);
    Serial.write(211);  
    exit;   
   }

  if (mode==44) {
    if ((options & 8)==8) { east = xshift; west = 0;}
    if ((options & 4)==4) { west = xshift; east = 0;}
    if ((options & 2)==2) { north = yshift; south = 0;}
    if ((options & 1)==1) { south = yshift; north = 0;}
    if (xshift == 0) {east = 0; west = 0;}
    if (yshift == 0) {north = 0; south = 0;}
    exit;   
  }
  
   if ((mode > 18) && (mode < 30)){ 
    tclrimage(1000); 
    te = millis();
    t = millis() + tms;
    if (options==1) outlow(NODARK); //turn on low dark current mode
    if ((options & 2)==2) outlow(LOWNOISE);
    if (options == 3) outlow(NODARK + LOWNOISE);
    while (Serial.available() == 0){
      delay(1);
      if (millis()>=t) break;
    }
    outlow(TCCDINIT);
    if (options != 0) delay(2);
    tms = millis()-te;  
  
    if (mode==19) tgetimage();
    if (mode==20) tgetobject();
    if (mode==21) tgetcenter(xshift,yshift);
    if (mode==22) tgetdouble();
    if (mode==23) tgetquiet();
    if (mode==24) {
      tgetreset();
      exit;
    }
    if (mode==25) tgetdcs();
    if (mode==26){
     tclrimage(82); 
     tgetimage();   
    }
    Serial.write((tms>>14) & 127);
    Serial.write((tms>>7) & 127);
    Serial.write((tms) & 127);
    
    if (Serial.available()!= 0)Serial.read();
  }
 }

}
//---------------------------------------------------------------------------
void digitalWriteSlow(int pin,int level){
  digitalWrite(pin,level);
  delayNanoseconds(200);
}

unsigned int getADC(void){
 unsigned int d=0;
 digitalWrite(cs,0);
 digitalWriteSlow(clk,0);   //0
 digitalWriteSlow(clk,1);   //1
 digitalWriteSlow(clk,0);
 digitalWriteSlow(clk,1);   //2
 digitalWrite(so,0); 
 digitalWriteSlow(clk,0); 
 digitalWriteSlow(clk,1);   //3
 digitalWriteSlow(clk,0);
 digitalWriteSlow(clk,1);  //4
 digitalWriteSlow(clk,0);
 digitalWriteSlow(clk,1);  //5
 digitalWriteSlow(clk,0);
 digitalWriteSlow(clk,1);  //6
 digitalWriteSlow(clk,0);
 digitalWriteSlow(clk,1);  //7
 digitalWriteSlow(clk,0);
 digitalWriteSlow(clk,1);  //8
 d = (d<<1)+(digitalRead(si)&1);
 digitalWriteSlow(clk,0);
 digitalWriteSlow(clk,1);  //9
 d = (d<<1)+(digitalRead(si)&1);
 digitalWriteSlow(clk,0);
 digitalWriteSlow(clk,1);  //10
 d = (d<<1)+(digitalRead(si)&1);
 digitalWriteSlow(clk,0);
 digitalWriteSlow(clk,1);  //11
 d = (d<<1)+(digitalRead(si)&1);
 digitalWriteSlow(clk,0);
 digitalWriteSlow(clk,1);  //12
 d = (d<<1)+(digitalRead(si)&1);
 digitalWriteSlow(clk,0);
 digitalWriteSlow(clk,1);  //13
 d = (d<<1)+(digitalRead(si)&1);
 digitalWriteSlow(clk,0);
 digitalWriteSlow(clk,1);  //14
 d = (d<<1)+(digitalRead(si)&1);
 digitalWriteSlow(clk,0);
 digitalWriteSlow(clk,1);  //15
 d = (d<<1)+(digitalRead(si)&1);
 digitalWriteSlow(clk,0);
 digitalWriteSlow(clk,1);  //16
 d = (d<<1)+(digitalRead(si)&1);
 digitalWriteSlow(clk,0);
 digitalWriteSlow(clk,1);  //17
 d = (d<<1)+(digitalRead(si)&1);
 digitalWriteSlow(clk,0);
 digitalWriteSlow(clk,1);  //18
 d = (d<<1)+(digitalRead(si)&1);
 digitalWriteSlow(clk,0);
 digitalWriteSlow(clk,1);  //19
 d = (d<<1)+(digitalRead(si)&1);
 digitalWriteSlow(clk,0);
 digitalWriteSlow(clk,1);  //19a
 d = (d<<1)+(digitalRead(si)&1);
 digitalWriteSlow(clk,0);

 digitalWrite(cs,1);  //20
 digitalWrite(so,1);
 digitalWriteSlow(clk,1);
 return(d); 
}
//--------------------------------------------------------------------------- 
void outlow(uint32_t k){
 GPIO6_DR  = (GPIO6_DR & 0xF030FFFF) | k;
 delayNanoseconds(500);
}

void reducedark(int f){
  if (f==1) outlow(NODARK); else outlow(TCCDINIT);
}
//-------------------------------------------------------------------------

//Clear the 211 image area of charge
void tclrimage(int count){
int cx, im;
    outlow(CCDINIT);              //reset port state
    for (cx = 0; cx<count;cx++){     //set up to clear COUNT lines
     outlow(TIAG1);                 //storage area
     outlow(TIAG2);              //clock out image area charge
     outlow(TIAG3);
     outlow(TIAG4);
    } 
    outlow(TCCDINIT);              //reset port state
}
//---------------------------------------------------------------------------
void shift6(void){
       int i;
       for (i = 0;i<5;i++){ //Shift out 6 dummy pixels
        outlow(TSRG1);
        outlow(TSRG2);
       } 
}
//---------------------------------------------------------------------------
void tpresample(void){
      outlow(TCCD2);                 //Presample data
      outlow(TCCD3);                 //Set CCD for data out
      delayMicroseconds(Delay2);      //Wait for amp to settle
      img[imagex++]=getADC();
}
//--------------------------------------------------------------------------
//TC211 camera readout routines
//Read out the pixel data 165 lines x 192 rows  get_image()

void tgetimage(void){
int x, d;

 imagex = 0;

    outlow(TCCDINIT);
    for (vl=0;vl<165;vl++){
      outlow(TIAG1);  //Shift image one line
      outlow(TIAG2);
      outlow(TIAG3);
      outlow(TIAG4);
      shift6();
      for (vp=0;vp<192;vp++){ //Read out one line of data
        tpresample();
        Serial.write((img[imagex-1]&0xFFFF)>>8);
        Serial.write(img[imagex-1]&0xFF);        
      } // vp        Last pixel?
    } // vl                Last line?
 }

//----------------------------------------------------------------------------
//Read out the pixel data 165 lines x 192 rows  get_object()
//Routine bins adjacent lines and bins adjacent pixels for a 82 line by 96 row image

void  tgetobject(void){
  imagex = 0;
  outlow(TCCDINIT);
  outlow(TCCDINIT);
  for (vl=0; vl<82; vl++) {
                            //       ;Shift image two lines
    outlow(TIAG1);         //        ;with line binning
    outlow(TIAG4);
    outlow(TIAG1);
    outlow(TIAG2);
    outlow(TIAG3);
    outlow(TIAG4);

    shift6();
    
    for (vp = 0;vp < 96;vp++){      // ;Read out one line of data
      tpresample();                 //          ;combine adjacent pixels
      outlow(TCCD2);                 
      outlow(TCCD3);                 
      delayMicroseconds(Delay2); 
      imagex--;     
      img[imagex]=(getADC()+img[imagex])/2; 
      Serial.write((img[imagex]&0xFFFF)>>8);
      Serial.write(img[imagex]&0xFF); 
      imagex++;               
     }
    }

}
//---------------------------------------------------------------------------
//Read out the center data 82 lines x 91 rows  get_center(x,y)
//The data is from the bottom 1/4 frame image.

void tgetcenter(int dv, int lv){

 imagex = 0;
 
 dv = dv + 6;

    outlow(TCCDINIT);
    for (vl = 0;vl<lv;vl++){
      outlow(TIAG1);
      outlow(TIAG2);
      outlow(TIAG3);
      outlow(TIAG4);
    }
    outlow(TCCDINIT);

    for (vl = 0; vl<82;vl++) {
      outlow(TIAG1);  //          ;Shift image one line
      outlow(TIAG2);
      outlow(TIAG3);
      outlow(TIAG4);
      for (vd = 0; vd<dv;vd++) { //  ;Shift out 48+6 dummy pixels
        outlow(TSRG1);
        outlow(TSRG2);
      }
 
      for (vp = 0; vp<96; vp++) {    //  ;Read out partial line
        tpresample();
        Serial.write((img[imagex-1]&0xFFFF)>>8);
        Serial.write(img[imagex-1]&0xFF);     
      } // vp       
    } //vl
 
}
//---------------------------------------------------------------------------
//Read out the pixel data 165 lines x 192 rows  get_doubl()
//Use double sampling to reduce low frequency noise. Pixel data has ref. value subtacted

void tgetdouble(void){

    imagex = 0;

    outlow(TCCDINIT);
    for (vl = 0; vl< 165; vl++){
      outlow(TIAG1);  //Shift image one line
      outlow(TIAG2);
      outlow(TIAG3);
      outlow(TIAG4);
      shift6();
      for (vp = 0; vp< 192; vp++){   //Read out one line of data
        outlow(TCCD2);           // Select mid nibble      
        delayMicroseconds(Delay2);      
        img[imagex] = getADC(); //save ref
        outlow(TCCD3);
        delayMicroseconds(Delay2);      
        img[imagex] = getADC() - img[imagex];
        Serial.write((img[imagex]&0xFFFF)>>8);
        Serial.write(img[imagex]&0xFF); 
        imagex++;      
      } // vp        Last pixel?
    } // vl                Last line?
}
//---------------------------------------------------------------------------
//Read out the pixel data 165 lines x 192 rows  get_quiet()
//Use double sampling to reduce low frequency noise. The pixel is read twice and
//the average of the reset value read on each side of the sample is subtracted.


void tgetquiet(void){
    int r = 0;
    imagex = 0;

    outlow(TCCDINIT);
    for (vl = 0; vl< 165; vl++){
      outlow(TIAG1);  //Shift image one line
      outlow(TIAG2);
      outlow(TIAG3);
      outlow(TIAG4);
      shift6();
      for (vp = 0; vp< 192; vp++){   //Read out one line of data
        outlow(TCCD2);           // Select mid nibble      
        delayMicroseconds(Delay2);      
        r = getADC() + getADC(); 
        outlow(TCCD3);
        delayMicroseconds(Delay2);      
        img[imagex] = (getADC() + getADC() - r)/2;
        Serial.write((img[imagex]&0xFFFF)>>8);
        Serial.write(img[imagex]&0xFF); 
        imagex++;      
      } // vp        Last pixel?
    } // vl                Last line?
}
//---------------------------------------------------------------------------
//Get the reset and reference values for the TC211

void tgetreset() {
 resetavg = 0;
 refavg = 0;
 
 for (vph = 0; vph<256;vph++){ 
    outlow(TCCDINIT);        
                                     // outhigh(shutterbyte);
    for (vp=0;vp<35;vp++) shift6();
    outlow(TCCDINIT); 
    outlow(RST1);
    delayMicroseconds(Delay2);      //wait for op amp to settle
    refavg = refavg + getADC();
    outlow(RST3);                                       
    outlow(RST4);
    outlow(RST5);  
    outlow(TCCDINIT);
    
    outlow(TCCD4);
    outlow(TCCD5);  
    delayMicroseconds(Delay2);      //wait for op amp to settle
    resetavg = resetavg + getADC();
    outlow(RST3);  
    outlow(RST5);
    outlow(TCCDINIT);    
   }

  resetavg = resetavg / 256;
  refavg = refavg / 256;

  Serial.write((resetavg & 0x7F00)>>8);
  Serial.write(resetavg & 0xFF);   
  Serial.write((refavg & 0x7F00)>>8);
  Serial.write(refavg & 0xFF);  
}
//---------------------------------------------------------------------------
//Read out the pixel data 165 lines x 192 rows  get_tdcs()
//Use correlated double sampling to reduce low frequency noise. Pixel data has
//reset level subtacted. Requires circuit modification to work!

void tgetdcs(void){
 imagex = 0;
    outlow(TCCDINIT);
    for (vl = 0;vl<165;vl++){                             
     outlow(TIAG1);   //      ;Shift image one line
     outlow(TIAG2);
     outlow(TIAG3);
     outlow(TIAG4);
     shift6();
     for (vp = 0;vp<192;vp++){
           outlow(TCCD1C);
           outlow(TCCD3C);      //          ;Start convert of reset level
           delayMicroseconds(Delay2);      // ;Wait for amp to settle
           img[imagex] = getADC();          
           outlow(TCCD6C);             
           outlow(TCCD8C);                   //;Start convert of next pixel
           delayMicroseconds(Delay2);       //;Wait for amp to settle
           img[imagex] = getADC()-img[imagex];           
           outlow(TCCD10C);
           Serial.write((img[imagex]&0xFFFF)>>8);
           Serial.write(img[imagex]&0xFF); 
           imagex++;                           
     }
  }            
}

void guide(void)
{
  if (--east <= 0) {
    east = 0;
    digitalWriteFast(1,0);
  } else digitalWriteFast(1,1);
  if (--west <= 0) {
    west = 0;
    digitalWriteFast(2,0);
  } else digitalWriteFast(2,1);
  if (--north <= 0) {
    north = 0;
    digitalWriteFast(3,0);
  } else digitalWriteFast(3,1);
  if (--south <= 0) {
    south = 0;
    digitalWriteFast(4,0);
  } else digitalWriteFast(4,1);  
}
