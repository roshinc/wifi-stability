#include "SdFat.h"
#include "HybridLog.h"
#include "externs.h"
#include "constants.h"

//Used in loop to determine weather to send or not
int lastSend = 0;

//Constructor
HybridLogHandler::HybridLogHandler(String system, LogLevel level,
  const LogCategoryFilters &filters) : LogHandler(level, filters), m_system(system){
    LogManager::instance()->addHandler(this);
  }
  //Called in the loop, if the envirmoent is agreable, sends a line to the cloud.
  void HybridLogHandler::loop()
  {
    //If the wifi is not read, this was called too early
    if(!WiFi.ready())
    {
      //Serial.println("No WiFi");
      return;
    }
    //If the cloud is not connected, we have nothing to do.
    if(!Particle.connected)
    {
      //Serial.println("No Cloud");
      return;
    }

    Particle.process(); //Keep alive

    //If the files does not exist, we have nothing to do.
    if(!sd.exists("Hlog.txt"))
    {
      //If the files does not exist erase the seek value stored in EEPROM.
      EEPROM.put(10, 0);
      //Serial.println("No File");
      return;
    }
    //Serial.println("All Good");
    //Ensure there is at least a minutes diffrence in our sends
    //TODO: Make this better, using a timer fails.
    if(Time.minute() != lastSend)
    {
      //Make sure sd is initilized
      //if the sd card can't be initilized, don't do anything else.
      if(!LazyInitHL())
      {
        return;
      }

      //Send a single line of the log.
      sendLine();

      //Update the gobal(to this file) var that keeps track of when to send
      lastSend = Time.minute();
    }
  }
  //Send a single line of the log to the cloud
  void HybridLogHandler::sendLine()
  {
    File myFileSendLine; // Holds the log file
    int bytesRead = 0; //counter of how many bytes we have read
    int cPos;// hold current postion
    int data; // hold a byte we read
    char buffer[350]; // holds a line we read
    // Read the last read postion (2 bytes) from EEPROM addres
    int addr = 10; // location in EEPROM
    int toSeek; // hold the last read postion
    EEPROM.get(addr, toSeek); // retrive from EEPROM
    // Validate the value we got from EEPROM
    if(toSeek == 0xFFFF) {
      // EEPROM was empty -> initialize value
      toSeek = 0;
    }

    wd.checkin(); // reset watchdog timer
    Particle.process(); // keep alive

    // open the file for reading, if it can't
    // be opened someone else is using it,
    if (!myFileSendLine.open("Hlog.txt", O_READ)) {
      return;
    }
    //If the seek value is the same or bigger that the file value we have already finished the file.
    if(toSeek >= myFileSendLine.size())
    {
      //Serial.println("end of file");
      //close the file as soon as possible cause someone else might want it.
      myFileSendLine.close();
      return;
    }

    // Seek to where we left off
    myFileSendLine.seekSet(toSeek);

    // read from the file until there's nothing else in it or a newline or we exceed our buffer:
    while( ((data = myFileSendLine.read()) >= 0) && (myFileSendLine.peek() != '\n') && (bytesRead < 350) )
    {
      //add the byte we read to the buffer
      buffer[bytesRead] = data;
      //increment the buffer
      bytesRead++;
      //Reset the watchdog timer
      wd.checkin();
      //Keep alive
      Particle.process();
    }
    //Store the current postion of the file
    cPos = myFileSendLine.curPosition();

    // close the file as soon as possible cause someone else might want it:
    myFileSendLine.close();
    //Reset the watchdog timer
    wd.checkin();
    //Keep alive
    Particle.process();

    // add a terminating null at the end of our string.
    buffer[bytesRead] = '\0';

    // if the string has actuall content send it.
    if(bytesRead > 10){
      //if the sending fails we don't change the toSeek value.
      // if we do then change it.
      if (Particle.publish("HLog", buffer, 21600))
      {
        // The value to seek by to get the next line
        toSeek = cPos+2;
        // Write the seek value (2 bytes) to the EEPROM address
        EEPROM.put(addr, toSeek);
      }
    }
    else
    {
      //if its not a vaild string, just change the seek value.
      // The value to seek by to get the next line
      toSeek = cPos+2;
      // Write the seek value (2 bytes) to the EEPROM address
      EEPROM.put(addr, toSeek);
    }
  }

  /// Write the log message to sd card.
  void HybridLogHandler::log(String message) {
    File myFileHybrid; //Holds the file
    String time = Time.format(Time.now(), TIME_FORMAT_ISO8601_FULL); //Create a time stamp
    String logLine = String::format("%s %s %s - - - %s\n", time.c_str(), m_system.c_str(), message.c_str()); //Create the line to log
    wd.checkin(); // reset watchdog timer

    //Make sure sd is initilized
    //if the sd card can't be initilized, don't do anything else.
    if(!LazyInitHL())
    {
      return;
    }

    // open the file for write at end like the "Native SD library"
    if (!myFileHybrid.open("Hlog.txt", O_RDWR | O_CREAT | O_AT_END))
    {
      //sd.errorHalt("opening Hlog.txt for write failed");
      sd.errorPrint("opening Hlog.txt for write failed");
      return;
    }
    wd.checkin();// reset watchdog timer
    // if the file opened okay, write to it:
    // Serial.print("Writing to test.txt...");
    myFileHybrid.println(logLine);
    // close the file:
    myFileHybrid.close();
    wd.checkin();// reset watchdog timer
  }

  /// initilize sd if not already done.
  bool HybridLogHandler::LazyInitHL()
  {
    if(!initilized)
    {
      // Initialize SdFat or print a detailed error message ~~~and halt~~~
      // Use half speed like the native library.
      // Change to SPI_FULL_SPEED for more performance(was SPI_HALF_SPEED).
      if (!sd.begin(chipSelect, SPI_HALF_SPEED))
      {
        //sd.initErrorHalt();
        //Prints error to serial.
        sd.initErrorPrint();
        Serial.println("Sd card could not be initilized by HybridLog");
        return false;
      }
      initilized = true;
    }
    return true;
  }

  HybridLogHandler::~HybridLogHandler() {
    LogManager::instance()->removeHandler(this);
  }

  // The following methods are taken from Particle FW, specifically spark::StreamLogHandler.
  // See https://github.com/spark/firmware/blob/develop/wiring/src/spark_wiring_logging.cpp
  const char* HybridLogHandler::HybridLogHandler::extractFileName(const char *s) {
    const char *s1 = strrchr(s, '/');
    if (s1) {
      return s1 + 1;
    }
    return s;
  }

  const char* HybridLogHandler::extractFuncName(const char *s, size_t *size) {
    const char *s1 = s;
    for (; *s; ++s) {
      if (*s == ' ') {
        s1 = s + 1; // Skip return type
      } else if (*s == '(') {
        break; // Skip argument types
      }
    }
    *size = s - s1;
    return s1;
  }

  void HybridLogHandler::logMessage(const char *msg, LogLevel level, const char *category, const LogAttributes &attr) {

    String s;

    if (category) {
      s.concat("[");
      s.concat(category);
      s.concat("] ");
    }
    wd.checkin();
    // Source file
    if (attr.has_file) {
      s = extractFileName(attr.file); // Strip directory path
      s.concat(s); // File name
      if (attr.has_line) {
        s.concat(":");
        s.concat(String(attr.line)); // Line number
      }
      if (attr.has_function) {
        s.concat(", ");
      } else {
        s.concat(": ");
      }
    }
    wd.checkin();
    // Function name
    if (attr.has_function) {
      size_t n = 0;
      s = extractFuncName(attr.function, &n); // Strip argument and return types
      s.concat(s);
      s.concat("(): ");
    }

    // Level
    s.concat(levelName(level));
    s.concat(": ");

    // Message
    if (msg) {
      s.concat(msg);
    }

    // Additional attributes
    if (attr.has_code || attr.has_details) {
      s.concat(" [");
      // Code
      if (attr.has_code) {
        s.concat(String::format("code = %p" , (intptr_t)attr.code));
      }
      // Details
      if (attr.has_details) {
        if (attr.has_code) {
          s.concat(", ");
        }
        s.concat("details = ");
        s.concat(attr.details);
      }
      s.concat(']');
    }

    log(s);
  }
