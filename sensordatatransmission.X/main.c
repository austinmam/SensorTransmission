/*
 * Author:      Austin Mam & Matthew Hyde
 * Target PIC:  PIC32MX250F128B
 */

// graphics libraries
#include "config.h"
#include "tft_master.h"
#include "tft_gfx.h"
#include <stdlib.h>
#include <plib.h>
#include <xc.h>

#define	SYS_FREQ 40000000
#define use_uart_serial 1
#include "pt_cornell_1_2.h"

//pragma config in config.h changed to on, usually it was OFF

// === thread structures ============================================
// thread control structs
static struct pt pt_input, pt_radio, pt_logger, pt_rx;
int count = 0;
//interrrupt from real time clock
void __ISR(_RTCC_VECTOR, ipl2soft) RTCCInt(void){
    __XC_UART = 1;
    printf("data1,data2,%d,_", count); //underscore at end is used as delimiter
    count++;
    __XC_UART = 2;
    mRTCCClearIntFlag();
}

int send_to_radio;
char data_str[100]; //str to hold logger data
int star_received, data_received;

// === Radio Thread =================================================
// This thread is responsible for sending data from the PIC to the radio
static PT_THREAD (protothread_radio(struct pt *pt))
{
    PT_BEGIN(pt);
      while(1) {
          //scans input from UART and changes certain value based on input
          printf("Sending data ");
          if(send_to_radio){
              __XC_UART = 1; //change UART channel to 1
              printf(data_str); //send data to radio
              //Reset booleans
              send_to_radio = 0;
              star_received = 0;
              data_received = 0;   
              __XC_UART = 2;
          }         
          PT_YIELD_TIME_msec(50); 
      } // END WHILE(1)
  PT_END(pt);
} // UART thread

char star_str[3]; //str to hold UART input
// === RX Thread =================================================
// This thread is responsible for receiving incoming UART data from the logger
static PT_THREAD (protothread_datarx(struct pt *pt))
{
    PT_BEGIN(pt);
    star_received = 0;
    data_received = 0;
      while(1) {
         //scans input from UART and changes certain value based on input  
        if(star_received == 0){  
            PT_SPAWN(pt, &pt_input, PT_GetSerialBuffer(&pt_input));
            sscanf(PT_term_buffer, "%s", &star_str);
            if(star_str[2] == '*'){
                star_received = 1;
                tft_setCursor(0, 50);
                tft_setTextColor(ILI9341_WHITE);  tft_setTextSize(3);
                tft_writeString("* received!\n"); 
            }
        }
        //Once data logger has entered telecommunications mode:
        if(star_received == 1){
            //store data 
            PT_SPAWN(pt, &pt_input, PT_GetSerialBuffer(&pt_input));
            sscanf(PT_term_buffer, "%s", &data_str); //wait for ascii dump
            data_received = 1;
        }
   
        PT_YIELD_TIME_msec(30);
      } // END WHILE(1)
  PT_END(pt);
} // UART thread


char portbuffer[60];
uint32_t portval;
// === Logger Thread =================================================
// This thread is responsible for Logger communication.
static PT_THREAD (protothread_logger(struct pt *pt))
{
    PT_BEGIN(pt);
    send_to_radio = 0;
      while(1) {
          //PT_YIELD_TIME_msec(5000); //wait 5 seconds to ring
          LATA = 0x01; // set ring high
          portval = (PORTA & 0x02);
          //while(portval == 0){portval = (PORTA & 0x02);} 
          //wait for Modem Enable to go high
          tft_setCursor(0, 0);
          tft_setTextColor(ILI9341_WHITE);  tft_setTextSize(3);
          tft_writeString("Modem Enable!\n");   
          //Begin sending carriage returns and wait for response          
          
          while(star_received == 0){ //'*' not received from logger
              PT_YIELD_TIME_msec(27);
              //debugging message one LCD
              tft_setCursor(50,100 );
              tft_setTextColor(ILI9341_WHITE);  tft_setTextSize(2);
              tft_writeString("matching baud rate\n");
              //printf("A"); //carriage return send
              PT_YIELD_TIME_msec(27);
              tft_setCursor(50,100 );
              tft_setTextColor(ILI9341_BLACK);  tft_setTextSize(2);
              tft_writeString("matching baud rate\n");
          }
          //Once star_received is asserted, 
          //logger has entered telecommunications mode
          //We can now send commands to the logger to receive data
          PT_YIELD_TIME_msec(20);
          printf('D'); //Send D command to logger which is 'ASCII DUMP'
          //data_rx pt at this time will be ready to receive data from logger
          
          while(data_received == 0){} //wait for data to be received
          printf('E'); //send command E to exit telecommunications mode
          
          //Send to Arduino
          send_to_radio = 1;
          //DONE
          
          tft_setCursor(50, 50);
          tft_setTextColor(ILI9341_WHITE);  tft_setTextSize(3);
          tft_writeString("star received?"); 
          PT_YIELD_TIME_msec(30);
      } // END WHILE(1)
  PT_END(pt);
} // logger thread


// === Main  ======================================================
void main(void) {
  SYSTEMConfigPerformance(PBCLK); 
  TRISA = 0x02;
  mPORTASetPinsDigitalIn(BIT_1);
  LATA = 0x00;
  
  //Real Time Clock Setup
  RtccInit(); //init real time clock
  RtccSetTimeDate(0x01301500, 0x17111605); //set date of real time clock
  RtccChimeEnable(); //enable alarms indefinitely
  RtccAlarmEnable(); // enable alarms
  RtccSetAlarmTime(0x01301500); //enable alarm time
  RtccSetAlarmRpt(RTCC_RPT_TEN_SEC); //repeat alarms every ten seconds
  
  //setup interrupts
  INTEnable(INT_RTCC, INT_ENABLED);
  INTSetVectorPriority(INT_RTCC_VECTOR, INT_PRIORITY_LEVEL_2);
  INTSetVectorSubPriority(INT_RTCC_VECTOR, INT_SUB_PRIORITY_LEVEL_1);
  
  RtccEnable();
  
  // === setup threads ==========
  PT_setup();

  INTEnableSystemMultiVectoredInt();

  //Setup for UART1 for feather
  PPSInput (3, U1RX, RPB13); //Assign U1RX to pin RPB13
  PPSOutput(1, RPB7, U1TX); //Assign U1TX to pin RPB17
  UARTConfigure(UART1, UART_ENABLE_PINS_TX_RX_ONLY);
  UARTSetLineControl(UART1, UART_DATA_SIZE_8_BITS | 
          UART_PARITY_NONE | UART_STOP_BITS_1);
  UARTSetDataRate(UART1, pb_clock, BAUDRATE);
  UARTEnable(UART1, UART_ENABLE_FLAGS(UART_PERIPHERAL | UART_RX | UART_TX));
  
  // init the threads
  PT_INIT(&pt_radio);
  PT_INIT(&pt_logger);
  PT_INIT(&pt_rx);

  // init the display
  tft_init_hw();
  tft_begin();
  tft_fillScreen(ILI9341_BLACK);
  //240x320 vertical display
  tft_setRotation(3);
  
  // round-robin scheduler for threads
  while (1){
      PowerSaveSleep(); //set PIC in sleep mode
      PT_SCHEDULE(protothread_radio(&pt_radio));
      PT_SCHEDULE(protothread_logger(&pt_logger));
      PT_SCHEDULE(protothread_datarx(&pt_rx));
      }
  } // main
// === end  ======================================================