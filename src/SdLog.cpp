#include "SdFat.h"
#include "SdLog.h"
#include "externs.h"
#include "constants.h"

//Constructor
SDcardLogHandler::SDcardLogHandler(String system, LogLevel level,
  const LogCategoryFilters &filters) : LogHandler(level, filters), m_system(system){
    LogManager::instance()->addHandler(this);
  }

  /// Write the log message to SD Card.
  void SDcardLogHandler::log(String message) {
    File myFileSDLog; //Hold the log file.
    String time = Time.format(Time.now(), TIME_FORMAT_ISO8601_FULL); // Create a time stamp
    String logLine = String::format("%s %s %s - - - %s\n", time.c_str(), m_system.c_str(), message.c_str()); //Create a log line

    //Reset watchdog timer.
    wd.checkin();

    //Make sure sd card is initilized, else return
    if(!LazyInitSDL())
    {
      return;
    }
    sdlIsWriting = true;
    // open the file for write at end like the "Native SD library"
    if (!myFileSDLog.open("log.txt", O_RDWR | O_CREAT | O_AT_END))
    {
      sd.errorPrint("opening test.txt for write failed");
    }

    // if the file opened okay, write to it:
    // Serial.print("Writing to test.txt...");
    myFileSDLog.println(logLine);
    myFileSDLog.flush();
    // close the file:
    myFileSDLog.close();
    sdlIsWriting = false;
    //Reset watchdog timer.
    wd.checkin();
  }

  /// initilize sd if not already done.
  bool SDcardLogHandler::LazyInitSDL()
  {
    //if it was not initilized.
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
        Serial.println("Sd card could not be initilized by SDLog");
        return false;
      }
      initilized = true;
    }
    return true;
  }


  SDcardLogHandler::~SDcardLogHandler() {
    LogManager::instance()->removeHandler(this);
  }

  // The following methods are taken from Particle FW, specifically spark::StreamLogHandler.
  // See https://github.com/spark/firmware/blob/develop/wiring/src/spark_wiring_logging.cpp
  const char* SDcardLogHandler::SDcardLogHandler::extractFileName(const char *s) {
    const char *s1 = strrchr(s, '/');
    if (s1) {
      return s1 + 1;
    }
    return s;
  }

  const char* SDcardLogHandler::extractFuncName(const char *s, size_t *size) {
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

    void SDcardLogHandler::logMessage(const char *msg, LogLevel level, const char *category, const LogAttributes &attr) {

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
