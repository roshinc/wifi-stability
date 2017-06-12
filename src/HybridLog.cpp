#include "SdFat.h"
#include "HybridLog.h"
#include "externs.h"
#include "constants.h"
int lastSend = 0;

HybridLogHandler::HybridLogHandler(String system, LogLevel level,
    const LogCategoryFilters &filters) : LogHandler(level, filters), m_system(system){
    LogManager::instance()->addHandler(this);
    if(!initilized)
    {
      // Initialize SdFat or print a detailed error message and halt
      // Use half speed like the native library.
      // Change to SPI_FULL_SPEED for more performance(was SPI_HALF_SPEED).
      if (!sd.begin(chipSelect, SPI_HALF_SPEED))
      {
        sd.initErrorHalt();
      }
    }
    initilized = true;
}

  void HybridLogHandler::loop()
  {
    if(!WiFi.ready())
    {
      return;
    }
    if(!Particle.connected)
    {
      return;
    }
    Particle.process();
    if(Time.minute() != lastSend)
    {
      sendLine();
      lastSend = Time.minute();
    }
    return;
  }

  void HybridLogHandler::sendLine()
  {
    File myFile;
    int bytesRead = 0;
    int cPos;
    int data;
    char buffer[350];
    // Read a value (2 bytes in this case) from EEPROM addres
    int addr = 10;
    int toSeek;
    EEPROM.get(addr, toSeek);
    if(toSeek == 0xFFFF) {
      // EEPROM was empty -> initialize value
      toSeek = 0;
    }
    // open the file for reading, if it can't
    // be opened someone else is using it,
    wd.checkin();
    Particle.process();
    if (!myFile.open("Hlog.txt", O_READ)) {
      return;
    }
    if(toSeek >= myFile.size())
    {
      //Serial.println("end of file");
      //close the file as soon as possible cause someone else might want it:
      myFile.close();
      return;
    }

    // Seek to where we left off
    myFile.seekSet(toSeek);

    // read from the file until there's nothing else in it or a newline:
    while( ((data = myFile.read()) >= 0) && (myFile.peek() != '\n') && (bytesRead < 350) )
    {
      buffer[bytesRead] = data;
      bytesRead++;
      wd.checkin();
      Particle.process();
    }

    cPos = myFile.curPosition();

    // close the file as soon as possible cause someone else might want it:
    myFile.close();

    wd.checkin();
    Particle.process();
    // add a terminating null at the end of our string.
    buffer[bytesRead] = '\0';

    // if the string has actuall content send it.
    if(bytesRead > 10){
      //if the sending fails we don't change the toSeek value.
      if (Particle.publish("HLog", buffer, 21600))
      {
        toSeek = cPos+2;
        // Write a value (2 bytes) to the EEPROM address
        EEPROM.put(addr, toSeek);
      }
    }
    else
    {
      //if its not a vaild string, just change the seek value.
      toSeek = cPos+2;
      // Write a value (2 bytes in this case) to the EEPROM address
      EEPROM.put(addr, toSeek);
    }
  }

  /// Send the log message to Papertrail.
  void HybridLogHandler::log(String message) {
      File myFile1;
      String time = Time.format(Time.now(), TIME_FORMAT_ISO8601_FULL);
      String logLine = String::format("%s %s %s - - - %s\n", time.c_str(), m_system.c_str(), message.c_str());
      wd.checkin();
      wd.checkin();
      // open the file for write at end like the "Native SD library"
      if (!myFile1.open("Hlog.txt", O_RDWR | O_CREAT | O_AT_END))
      {
         sd.errorHalt("opening test.txt for write failed");
      }
      wd.checkin();
        // if the file opened okay, write to it:
       // Serial.print("Writing to test.txt...");
        myFile1.println(logLine);
        // close the file:
        myFile1.close();
      wd.checkin();
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
