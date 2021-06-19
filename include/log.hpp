#include <syslog.h>

#define DEBUG(fmt, ...)  syslog(LOG_DEBUG, "[DEBUG] " fmt, ##__VA_ARGS__)
#define INFO(fmt, ...)   syslog(LOG_INFO, "[INFO] " fmt, ##__VA_ARGS__)
#define NOTICE(fmt, ...) syslog(LOG_NOTICE, "[NOTICE] " fmt, ##__VA_ARGS__)
#define WARN(fmt, ...)   syslog(LOG_WARNING, "[WARNING] " fmt, ##__VA_ARGS__)
#define ERROR(fmt, ...)  syslog(LOG_ERR, "[ERROR] " fmt, ##__VA_ARGS__)

/*
int main() {
  // setlogmask (LOG_UPTO (LOG_NOTICE));
  openlog("chandola", LOG_CONS | LOG_PID | LOG_PERROR, LOG_USER);
  DEBUG("Debug %d", 1);
  INFO("Info");
  NOTICE("Notice");
  WARN("Warning");
  ERROR("Error");
  closelog();
}
*/
