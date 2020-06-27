/* CCD Camera Cookbook USB serial interface. The readout and timing are handled 
   by a serial command interpreter.

   This code is requires a new interface board. The preamp board and CCD do not need replacing. 
   However, the DS0026 drivers must be replaced with TC1426 drivers. A schematic is provided in
   the project folder.

2020 Simple-Circuit */

  
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
#define IAG  0x00020000  //1<<17      //pin-18 = iag clock
#define SAG  0x00010000  //1<<16      //pin-19 = sag clock
#define LOWNOISE 0x04000000 //1<<26  //pin-20 = low dark current
#define CCDINIT  LOWNOISE     //normal static state of clock lines
//Image area clear clock sequence

#define CL1 CCDINIT+TRG
#define CL2 CCDINIT+TRG+SAG
#define CL3 CCDINIT+SAG+IAG+SRG1+SRG2+SRG3
#define CL4 CCDINIT+IAG+SRG1+SRG2+SRG3

//Storage area clear clock sequence

#define CL1S CCDINIT+TRG
#define CL2S CCDINIT+TRG+SAG
#define CL3S CCDINIT+SAG+SRG1+SRG2+SRG3
#define CL4S CCDINIT+SRG1+SRG2+SRG3

//Transfer line from storage and bin three colums to one

#define LIN1   CCDINIT+TRG
#define LIN1A  CCDINIT
#define LIN1B  CCDINIT+TRG
#define LIN1C  CCDINIT
#define LIN1D  CCDINIT+TRG
#define LIN2   CCDINIT +SRG1+SRG2+SRG3
#define LIN3   CCDINIT+TRG
#define LIN4   CCDINIT+SRG1+SRG2+SRG3
#define LIN5   CCDINIT+TRG
#define LIN6   CCDINIT+SRG1+SRG2+SRG3
#define LIN7   CCDINIT+SAG

//Serial register flush clock sequence

#define SER1   CCDINIT+SRG1
#define SER2   CCDINIT+SRG2
#define SER3   CCDINIT+SRG3

//Serial shift for read out of charge from CCD

#define CCD1  CCDINIT+SRG1
#define CCD1A CCDINIT
#define CCD2  CCDINIT+SRG2
#define CCD2A CCDINIT
#define CCD3  CCDINIT+SRG3
#define CCD3A CCDINIT

#define CCD3R CCDINIT+SRG3+SRG1 //used for double sample only

#define Delay2 5    //op amp delay
 
volatile short img[213444];

volatile int  vl;     // line counter
volatile int  vp;     // pixel counter
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
  outlow(CCDINIT);
  
  Serial.begin(1843200);
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
  if (mode==1) clrimage();
  
  if (mode==2) movimage();
 
  if (mode==3) clrstore();
 
  if (mode==4) clrserial();

  if ((mode==5)|| (mode==6)) {
    getreset(mode);  
  }

  if (mode==99) {
    Serial.write(245);
    Serial.write(245);  
    exit;   
  }
   
  if ((mode > 6)&&(mode<19)){ 

    clrimage();
    clrimage();
    clrimage();
    clrimage();
    clrimage();
    clrimage();
    clrimage();
    clrimage();
    te = millis();
    t = millis() + tms;
    if (options==1) outlow(0); //turn on low dark current mode
    while (true){
      delay(1);
      if (millis()>=t) break;
      st = Serial.available();
      if (st != 0){
        c = Serial.read();
//        digitalWrite(clk,0);
//        delay(1000);
        break;
      }
    }
    outlow(CCDINIT);
    tms = millis()-te;
    clrstore();
    movimage();
    if (mode==7) getimage();
    if (mode==8) getimageL();
    if (mode==9) getobject();
    if (mode==10) getcenter(xshift,yshift);
    if (mode==11) getcenterfast(xshift,yshift);
    if (mode==12) getimx();
    if (mode==13) getobject();
    if (mode==14) getwide();
    if (mode==15) getwideL();
    if (mode==16) getwide_st();
    if (mode==17) getwide_stL();
    Serial.write((tms>>14) & 127);
    Serial.write((tms>>7) & 127);
    Serial.write((tms) & 127);    
    if (Serial.available()!= 0) Serial.read();
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
  if (f==1) outlow(LOWNOISE); else outlow(CCDINIT);
}
//-------------------------------------------------------------------------
//Clear the 245 storage area and the image area of charge
void clrimage(void){
 int cx;
    outlow(CCDINIT);
    for (cx = 0; cx<500; cx++){  //set up to clear 500 lines
     outlow(CL1);                 //storage area
     outlow(CL2);                  //clock out image area and
     outlow(CL3);                //storage area charge 
     outlow(CL4);
    }
    outlow(CCDINIT);              //reset port state
}
//-------------------------------------------------------------------------
//Move image area charge to the storage area

void movimage(void){
 int cx;
    outlow(CCDINIT);
    for (cx=0; cx<245;cx++){  //set count to shift image
                             //into storage area
     outlow(CL1);
     outlow(CL2);
     outlow(CL3);
     outlow(CL4);
    }           
    outlow(CCDINIT);              //reset port state
}

//---------------------------------------------------------------------------

//Clear the storage area of charge

void clrstore(void){
 int cx; 
    outlow(CCDINIT);
    for (cx = 0;cx<247;cx++){          //set to shift out 247 lines
      outlow(CL1S);                    //clock out storage area charge
      outlow(CL2S);
      outlow(CL3S);
      outlow(CL4S);
    }
    outlow(CCDINIT);              //reset port state
}
//--------------------------------------------------------------------------
//Clear the serial registers of charge

void clrserial(void){
 int cx;
    outlow(CCDINIT);
    for (cx = 0;cx<273;cx++){     //set count to clear serial
                                 //storage line
      outlow(SER1); //clock out line data serially                             
      outlow(SER2);
      outlow(SER3);
    }
    outlow(CCDINIT);              //reset port state
}

//--------------------------------------------------------------------------

//Read reset and reference values for TC245 chip

void getreset(int level){
int x, d, im; 

 resetavg = 0;
 refavg = 0;
 clrstore();
 clrstore();
 clrstore();
 clrstore();
 clrserial();
  for (x=0;x<256;x++){
    outlow(CCDINIT);     //reset port state
    outlow(CCD1);
    outlow(CCDINIT);
    outlow(CCD2);
    outlow(CCDINIT);
    outlow(CCD3);
    outlow(CCDINIT);
    outlow(CCD1);
    outlow(CCDINIT);
    outlow(CCD2);
    outlow(CCDINIT);
    outlow(CCD3);
    delayMicroseconds(Delay2);     //wait for op amp to settle
    im=getADC();
    resetavg = resetavg + im;

    outlow(CCDINIT);     //reset port state
    outlow(CCD1);     
    outlow(CCDINIT);       
    outlow(CCD2);      
    outlow(CCDINIT);        
    outlow(CCD3);    
    outlow(CCDINIT);      
    outlow(CCD1);       
    outlow(CCDINIT);    
    outlow(CCD2);     
    outlow(CCDINIT);        
    outlow(CCD3R);        
    delayMicroseconds(Delay2);     //wait for op amp to settle
    im=getADC();
    refavg = refavg + im;
    outlow(CCDINIT);     //reset port state
  }
      resetavg = resetavg / 256;
      refavg = refavg / 256; 

    Serial.write((resetavg & 0x7F00)>>8);
    Serial.write(resetavg & 0xFF);   
    Serial.write((refavg & 0x7F00)>>8);
    Serial.write(refavg & 0xFF);   
}
//--------------------------------------------------------------------------
void dumpline(void){
    outlow(CCDINIT);     //reset port state
    outlow(LIN1);        //dump one line
    outlow(LIN1A);
    outlow(LIN1B);
    outlow(LIN1C);
    outlow(LIN1D);
    outlow(LIN2);
    outlow(LIN7);
}

void shift11(void){
    for (vd = 0;vd<12;vd++){  //shift out 11 dummy pixels 
      outlow(CCDINIT);    
      outlow(SER1);
      outlow(CCDINIT);
      outlow(SER2);
      outlow(CCDINIT);
      outlow(SER3);   
    }  
}

void bin3(void){
    outlow(LIN1);               //bin three adjacent pixels
                                //to transfer line then shift
    outlow(LIN1A);              //line to serial area
    outlow(LIN1B);    
    outlow(LIN1C);
    outlow(LIN1D);
    outlow(LIN2);
    outlow(LIN7);            //shift one line from storage
                             //into transfer line for next
                            //line read out
}

void presample(void){
    outlow(CCD3);        //presample data
    outlow(CCDINIT);
    delayMicroseconds(Delay2);     //wait for op amp to settle
    img[imagex++]=getADC();
 
}
//--------------------------------------------------------------------------
//Read out the pixel data 242 lines x 252 rows
//One line from storage area is preshifted to clear charge

void getimage(void){
int  x, d;

 imagex = 0;
 dumpline();
     
  for (vl= 0;vl<242;vl++){        // read 242 image lines 
    bin3();
    shift11();
    presample();
    Serial.write((img[imagex-1]&0xFFFF)>>8);
    Serial.write(img[imagex-1]&0xFF);   
     
     for (vp = 1;vp<252;vp++){   //read out one line of data (get_imag3:)
      outlow(CCD1A);            //set MUX address and serial shift SRG1 clock
      outlow(CCD1);
      outlow(CCD2A);        //select new MUX address and serial shift SRG2 clock
      outlow(CCD2);
      outlow(CCD3A);        //select new MUX address and serial shift SRG3 clock
      presample();    
      Serial.write((img[imagex-1]&0xFFFF)>>8);
      Serial.write(img[imagex-1]&0xFF);   
     } //vp loop
    } //vl loop    
   outlow(CCDINIT);      //reset port state
}

//--------------------------------------------------------------------------

//Read out the pixel data 242 lines x 252 rows then line level bias subtract
//One line from storage area is preshifted to clear charge

void getimageL(void){
int  x, d;
imagex = 0;
dumpline();
    
  for (vl = 0; vl<242;vl++){   // read 242 image lines (get_imag1:)
    bin3();
    shift11();
    presample();
    
     for (vp = 1;vp<294;vp++){   //read out one line of data (get_imag3:)
      outlow(CCD1A);            //set MUX address and serial shift SRG1 clock
      outlow(CCD1);
      outlow(CCD2A);        //select new MUX address and serial shift SRG2 clock
      outlow(CCD2);
      outlow(CCD3A);        //select new MUX address and serial shift SRG3 clock
      presample();
     } //vp loop
     d = 0;
     for (x=1;x<33;x++){
      d = d + img[imagex-x];
     }
     d = d/32 -100;
     imagex = imagex-294;
     for (x=0;x<252;x++){
      img[imagex+x] = img[imagex+x]-d;
      Serial.write((img[imagex]&0xFFFF)>>8);
      Serial.write(img[imagex]&0xFF);  
     }
     imagex = imagex + 252;
    } //vl loop    
    outlow(CCDINIT);      //reset port state
            
}

//--------------------------------------------------------------------------

//Read out the pixel data 242 lines x 252 rows
//Routine bins adjacent lines and bins adjacent pixels for a 121 line by 126 row image

void getobject(void){
int x, d;


 imagex = 0;
 dumpline();

  for (vl = 0; vl<121; vl++){    // set count for 242/2 lines
    bin3();          //shift two lines from storage area                  
    outlow(LIN2);   
    outlow(LIN7);   //on top of each other for line binning

    shift11();
 //   presample();
    imagex--;
     for (vp = 0;vp<126;vp++){  //set count binning 256/2
      outlow(CCD1A);         //set MUX address and serial shift SRG1 clock
      outlow(CCD1);
      outlow(CCD2A);        //select new MUX address and serial shift SRG2 clock
      outlow(CCD2);
      outlow(CCD3A);        //select new MUX address and serial shift SRG3 clock
      outlow(CCD3);
      outlow(CCDINIT);
      delayMicroseconds(Delay2);    //wait for op amp to settle
      img[imagex]=getADC();
      outlow(CCD1A);            //set MUX address and serial shift SRG1 clock
      outlow(CCD1);
      outlow(CCD2A);        //select new MUX address and serial shift SRG2 clock
      outlow(CCD2);
      outlow(CCD3A);        //select new MUX address and serial shift SRG3 clock
      outlow(CCD3);
      outlow(CCDINIT);
      delayMicroseconds(Delay2);    //wait for op amp to settle
      img[imagex]= (img[imagex]+getADC())/2;
      Serial.write((img[imagex]&0xFFFF)>>8);
      Serial.write(img[imagex]&0xFF);    
      imagex++; 
      } //vp loop
    } //vl loop    
    outlow(CCDINIT);      //reset port state
}

//---------------------------------------------------------------------------
//Read out the center data 121 lines x 126 rows

void getcenter(int xfind245, int yfind245){
int  x, d, lv, dv;

 imagex = 0;
 lv = yfind245;
 dv = xfind245 + 11;            

    outlow(CCDINIT);
    for (vl=0;vl<lv;vl++){
      outlow(LIN1);      //shift lines to center 1/4 frame box
      outlow(LIN2);
      outlow(LIN3);
      outlow(LIN4);
      outlow(LIN5);
      outlow(LIN6);
      outlow(LIN7);
    }
    outlow(CCDINIT);
    for (vl=0;vl<121;vl++){ //set up to shift 121 lines
      outlow(CCDINIT);
      outlow(LIN1);        //shift out an image line
      outlow(LIN1A);
      outlow(LIN1B);    
      outlow(LIN1C);    
      outlow(LIN1D);    
      outlow(LIN2);   
      outlow(LIN7); 
      
      for (vd = 0;vd<dv;vd++){ //shift out offset + 11 dummy pixels 
        outlow(SER1);
        outlow(SER2);
        outlow(SER3);
      }
      presample();   
     Serial.write((img[imagex-1]&0xFFFF)>>8);
     Serial.write(img[imagex-1]&0xFF);   
       
    for (vp = 1;vp<126;vp++){ //read out only 1/2 of serial pixels
      outlow(CCD1A);            //set MUX address and serial shift SRG1 clock
      outlow(CCD1);
      outlow(CCD2A);        //select new MUX address and serial shift SRG2 clock
      outlow(CCD2);
      outlow(CCD3A);        //select new MUX address and serial shift SRG3 clock
      presample();
     Serial.write((img[imagex-1]&0xFFFF)>>8);
     Serial.write(img[imagex-1]&0xFF);      
     } //vp loop
    }//vl loop 
    outlow(CCDINIT);    
}
//---------------------------------------------------------------------------
//Read out the center data 96 x 82 

void getcenterfast(int xfind245, int yfind245){
int  x, d, lv, dv;

 imagex = 0;
 lv = yfind245;
 dv = xfind245 + 11;            

    outlow(CCDINIT);
    for (vl=0;vl<lv;vl++){
      outlow(LIN1);      //shift lines to center 1/4 frame box
      outlow(LIN2);
      outlow(LIN3);
      outlow(LIN4);
      outlow(LIN5);
      outlow(LIN6);
      outlow(LIN7);
    }
    outlow(CCDINIT);
    for (vl=0;vl<82;vl++){ //set up to shift 82 lines
      outlow(CCDINIT);
      outlow(LIN1);        //shift out an image line
      outlow(LIN1A);
      outlow(LIN1B);    
      outlow(LIN1C);    
      outlow(LIN1D);    
      outlow(LIN2);   
      outlow(LIN7); 
      
      for (vd = 0;vd<dv;vd++){ //shift out offset + 11 dummy pixels 
        outlow(SER1);
        outlow(SER2);
        outlow(SER3);
      }
      presample();   
     Serial.write((img[imagex-1]&0xFFFF)>>8);
     Serial.write(img[imagex-1]&0xFF);   
       
    for (vp = 1;vp<96;vp++){ //read out only part of serial pixels
      outlow(CCD1A);            //set MUX address and serial shift SRG1 clock
      outlow(CCD1);
      outlow(CCD2A);        //select new MUX address and serial shift SRG2 clock
      outlow(CCD2);
      outlow(CCD3A);        //select new MUX address and serial shift SRG3 clock
      presample();
     Serial.write((img[imagex-1]&0xFFFF)>>8);
     Serial.write(img[imagex-1]&0xFF);      
     } //vp loop
    }//vl loop 
    outlow(CCDINIT);    
}
//---------------------------------------------------------------------------
//Read out the pixel data 242 lines x 252 rows
//External binning is used to increase dynamic range; new span = 0 to 3071
//One line from storage area is preshifted to clear charge

void getimx(void){
int x, d;
 imagex = 0;

    outlow(CCDINIT);
    outlow(LIN7);       //shift line from storage area to transfer
     for (vl=0;vl<242;vl++){
      outlow(LIN1);          //shift left most pixel of 3-column
      outlow(LIN1A);        //group into serial line
      outlow(LIN2);
      shift11();
      presample();
      
      for (vp =0;vp<252;vp++){  //read out one line of data
        outlow(CCD1A);            //set MUX address and serial shift SRG1 clock
        outlow(CCD1);
        outlow(CCD2A);        //select new MUX address and serial shift SRG2 clock
        outlow(CCD2);
        outlow(CCD3A);        //select new MUX address and serial shift SRG3 clock
        presample();
      } //vp
      
      imagex = imagex - 252;         //move array point back to line start
      outlow(LIN3);         //shift center pixel of 3-column
                    //group to serial line
      outlow(LIN1A);  
      outlow(LIN4);
      shift11();
      outlow(CCD3);         //presample data
      delayMicroseconds(Delay2); 
      img[imagex++] += getADC();

       for (vp =0;vp<252;vp++){  //read out one line of data
        outlow(CCD1A);            //set MUX address and serial shift SRG1 clock
        outlow(CCD1);
        outlow(CCD2A);        //select new MUX address and serial shift SRG2 clock
        outlow(CCD2);
        outlow(CCD3A);        //select new MUX address and serial shift SRG3 clock
        outlow(CCD3);         //presample data
        delayMicroseconds(Delay2); 
        img[imagex++] += getADC();
      } //vp
      
       imagex = imagex - 252;         //restore line pointer in array

        outlow(LIN5);         //shift last pixel of 3-column
                 //group into serial line
        outlow(LIN1A);
        outlow(LIN6);
        outlow(LIN7);
        shift11();
        outlow(CCD3);         //presample data
        delayMicroseconds(Delay2); 
        img[imagex++] += getADC();
        
       for (vp =0;vp<252;vp++){  //read out one line of data
        outlow(CCD1A);            //set MUX address and serial shift SRG1 clock
        outlow(CCD1);
        outlow(CCD2A);        //select new MUX address and serial shift SRG2 clock
        outlow(CCD2);
        outlow(CCD3A);        //select new MUX address and serial shift SRG3 clock
        outlow(CCD3);         //presample data
        delayMicroseconds(Delay2); 
        img[imagex++] += getADC();
        Serial.write((img[imagex-1]&0xFFFF)>>8);
        Serial.write(img[imagex-1]&0xFF);  
      } //vp
    } //vl
    outlow(CCDINIT);
}
//---------------------------------------------------------------------------
//Read out the pixel data 242lines x 378 rows
//External binning is used to combine adjacent pixels by twos
//One line from storage area is preshifted to clear charge
void getwide(void){
int x,y,m,n;

 getwidest();
  m = 0;
  n = 0;
  for (y = 0; y< 242; y++){
   for (x = 0; x<378; x++){
         img[m] = (img[n]+img[n+1]) / 2;
         Serial.write((img[m]&0xFFFF)>>8);
         Serial.write(img[m]&0xFF); 
         delayMicroseconds(25);          
         m = m+1;
         n = n+2;
   }
 }
}
//---------------------------------------------------------------------------
//Read out the pixel data 242 lines x 378 rows with line bias subtract
//External binning is used to combine adjacent pixels by twos
//One line from storage area is preshifted to clear charge
void getwideL(void){
int x,y,m,n;

 getwideraw();
  m = 0;
  n = 0;
  for (y = 0; y< 242; y++){
   for (x = 0; x<378; x++){
         img[m] = (img[n]+img[n+1]) / 2;
         Serial.write((img[m]&0xFFFF)>>8);
         Serial.write(img[m]&0xFF); 
         delayMicroseconds(25);          
         m = m+1;
         n = n+2;
   }
 }
}

//---------------------------------------------------------------------------
//Read out the pixel data 242 lines x 756 rows
//External binning is used to combine adjacent pixels by twos
//One line from storage area is preshifted to clear charge
void getwide_st(void){
int x,y,m,n;

 getwidest();
  m = 0;
  n = 0;
  for (y = 0; y< 242; y++){
   for (x = 0; x<756; x++){
         Serial.write((img[m]&0xFFFF)>>8);
         Serial.write(img[m]&0xFF); 
         delayMicroseconds(25);  
         m++;        
   }
 }
}
//---------------------------------------------------------------------------
//Read out the pixel data 242 lines x 756 rows with line bias subtract
//External binning is used to combine adjacent pixels by twos
//One line from storage area is preshifted to clear charge
void getwide_stL(void){
int x,y,m,n;

 getwideraw();
  m = 0;
  n = 0;
  for (y = 0; y< 242; y++){
   for (x = 0; x<756; x++){
         Serial.write((img[m]&0xFFFF)>>8);
         Serial.write(img[m]&0xFF); 
         delayMicroseconds(25);  
         m++;        
   }
 }
}

//---------------------------------------------------------------------------
//Read out the pixel data 242 lines x 756 rows
//One line from storage area is preshifted to clear charge
void getwidest(void){
int x,d;
 imagex = 0;

    outlow(CCDINIT);
    outlow(LIN7);                 //shift line from storage area
                                 //to transfer line
   for (vl=0;vl<242;vl++){  //set line count for full frame
    outlow(LIN1);  //shift left most pixel of 3-column
                  //group into serial line
    outlow(LIN1A);
    outlow(LIN2);
    shift11();
    presample();
    imagex=imagex+2;
   for (vp =1 ; vp<252;vp++){  //read out one line of data
    outlow(CCD1A);
    outlow(CCD1);
    outlow(CCD2A);
    outlow(CCD2);
    outlow(CCD3A);
    outlow(CCD3);
    presample();
    imagex = imagex+2;
   }  //vp     //complete serial shift of line
    imagex = imagex - 755; //move array point back to line start + 1
    outlow(LIN3);         //shift center pixel of 3-column
               //group to serial line
    outlow(LIN1A);
    outlow(LIN4);
    shift11();
    presample();
    imagex=imagex+2;
   for (vp =1 ; vp<252;vp++){  //read out one line of data
    outlow(CCD1A);
    outlow(CCD1);
    outlow(CCD2A);
    outlow(CCD2);
    outlow(CCD3A);
    outlow(CCD3);
    presample();
    imagex = imagex+2;
   }  //vp     //complete serial shift of line
    imagex = imagex - 755;         //restore line pointer in array

    outlow(LIN5);         //shift last pixel of 3-column
               //group into serial line
    outlow(LIN1A);
    outlow(LIN6);
    outlow(LIN7);
    shift11();
    presample();
    imagex=imagex+2;
   for (vp =1 ; vp<252;vp++){  //read out one line of data
    outlow(CCD1A);
    outlow(CCD1);
    outlow(CCD2A);
    outlow(CCD2);
    outlow(CCD3A);
    outlow(CCD3);
    presample();
    imagex = imagex+2;
   }  //vp     //complete serial shift of line
   imagex = imagex-2;      //point to beginning of next line
   }  //vl          //loop until all lines are converted
    outlow(CCDINIT);
}

//---------------------------------------------------------------------------
//Read out the pixel data 242 lines x 756 rows with overscan average
//One line from storage area is preshifted to clear charge
void getwideraw(void){
int x,d,raw1,raw2,raw3,raw4;
 imagex = 0;



    outlow(CCDINIT);
    outlow(LIN7);                 //shift line from storage area
                                 //to transfer line
   for (vl=0;vl<242;vl++){  //set line count for full frame
    outlow(LIN1);  //shift left most pixel of 3-column
                  //group into serial line
    outlow(LIN1A);
    outlow(LIN2);
    shift11();
    presample();
    imagex=imagex+2;
   for (vp =1 ; vp<294;vp++){  //read out one line of data
    outlow(CCD1A);
    outlow(CCD1);
    outlow(CCD2A);
    outlow(CCD2);
    outlow(CCD3A);
    outlow(CCD3);
    presample();
    imagex = imagex+2;
   }  //vp     //complete serial shift of line
    imagex = imagex - 881; //move array point back to line start + 1
    outlow(LIN3);         //shift center pixel of 3-column
               //group to serial line
    outlow(LIN1A);
    outlow(LIN4);
    shift11();
    presample();
    imagex=imagex+2;
   for (vp =1 ; vp<294;vp++){  //read out one line of data
    outlow(CCD1A);
    outlow(CCD1);
    outlow(CCD2A);
    outlow(CCD2);
    outlow(CCD3A);
    outlow(CCD3);
    presample();
    imagex = imagex+2;
   }  //vp     //complete serial shift of line
    imagex = imagex - 881;         //restore line pointer in array

    outlow(LIN5);         //shift last pixel of 3-column
               //group into serial line
    outlow(LIN1A);
    outlow(LIN6);
    outlow(LIN7);
    shift11();
    presample();
    imagex=imagex+2;
   for (vp =1 ; vp<294;vp++){  //read out one line of data
    outlow(CCD1A);
    outlow(CCD1);
    outlow(CCD2A);
    outlow(CCD2);
    outlow(CCD3A);
    outlow(CCD3);
    presample();
    imagex = imagex+2;
   }  //vp     //complete serial shift of line
   imagex = imagex-2;      //point to beginning of next line
   }  //vl          //loop until all lines are converted
    outlow(CCDINIT);


//code to add overscan, move overscan to last three pixels and move up data
       raw1 = 0;
       for (raw2 = 0;raw2<242;raw2++){ 
        raw3 = 0;
        for (raw4 = 0;raw4<32;raw4++){
         raw3 = raw3 + img[raw1 + 786 + raw4 * 3];
        }
        img[raw1 + 753] = raw3 / 32;
        raw3 = 0;
        for (raw4 = 0;raw4<32;raw4++){
         raw3 = raw3 + img[raw1 + 787 + raw4 * 3];
        }
        img[raw1 + 754]= raw3 / 32;
        raw3 = 0;
        for (raw4 = 0;raw4<32;raw4++){
         raw3 = raw3 + img[raw1 + 788 + raw4 * 3];
        }
        img[raw1 + 755] = raw3 / 32;
        raw1 = raw1 + 882;
       }
//Restore image dimensions
       raw1 = 0;
       raw2 = 0;
       for (raw3 = 0;raw3<242;raw3++){
        for (raw4 = 0;raw4<756;raw4++){
         img[raw1+raw4] = img[raw2+raw4];
        }
        raw1 = raw1 + 756;
        raw2 = raw2 + 882;
       }
       
//code to remove line bias
       raw1 = 0;
       raw2 = 0;
       for (raw3 = 0;raw3<242;raw3++){
        raw1 = ((img[raw2+753] + img[raw2+754] + img[raw2+755]) / 3) - 100;
        if (idsmode == 0) raw1 = 0;
        for (raw4 = 0;raw4<753;raw4++){
         img[raw2+raw4] = img[raw2+raw4] - raw1;
        }
        img[raw2+753] = img[raw2+753] - raw1;
        img[raw2+754] = img[raw2+754] - raw1;
        img[raw2+755] = img[raw2+755] - raw1;
        raw2 = raw2 + 756;
       }
}

//---------------------------------------------------------------------------
