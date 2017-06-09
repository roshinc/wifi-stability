#pragma once

#include "application.h"

#if (SYSTEM_VERSION < SYSTEM_VERSION_v061)
#error This library requires FW version 0.6.1 and above.
#endif

/// LogHandler that send logs to Papertrail (https://papertrailapp.com/). Before using this class it's best to
/// familiarize yourself with Particle's loggin facility https://docs.particle.io/reference/firmware/photon/#logging.
/// You can use this as any other LogHandler - Initialize this class as a global, then call Log.info() and friends.
class SDcardLogHandler : public LogHandler {
    String m_system;

public:
    /// Initialize the log handler.
    /// \param system The name of the system in every log entry. Defaults to the deviceID.
    /// \para level Default log level.
    /// \param filters Category filters.
    ///
    /// Each log entry uses RFC 5424 with the following format:
    /// "%ISO8601_TIME% %system% %app% - - - [%category%] %log_level%: %text".
    explicit SDcardLogHandler(String system = System.deviceID(),LogLevel level = LOG_LEVEL_INFO, const LogCategoryFilters &filters = {});
    virtual ~SDcardLogHandler();

private:

    const char* extractFileName(const char *s);
    const char* extractFuncName(const char *s, size_t *size);
    void log(String message);

protected:
    virtual void logMessage(const char *msg, LogLevel level, const char *category, const LogAttributes &attr) override;
};
