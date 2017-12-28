import processing.serial.*;
import java.io.FileWriter;
Serial mySerial;
PrintWriter output;
void setup() {
   mySerial = new Serial( this, Serial.list()[2], 9600 ); //index of list needs to change to match serial port
   output = createWriter("P:/ece_scratch/ECE414MamHyde/data.csv"); //creates csv in directory
}



void draw() {
         String value = mySerial.readString(); //new data stored in value
         if ( value != null  && second() > 0) { //writes after 0 seconds in minute
              output.println( value ); //writes new data to file
         }else if(second() == 0) //flushes at 0 seconds
         {
           output.flush(); //flushes remaining data
         }
}