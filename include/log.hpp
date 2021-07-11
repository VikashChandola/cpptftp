#include <syslog.h>
#include "project_config.hpp"

#define DEBUG(...)  syslog(LOG_DEBUG, ##__VA_ARGS__)
#define INFO(...)   syslog(LOG_INFO, ##__VA_ARGS__)
#define NOTICE(...) syslog(LOG_NOTICE, ##__VA_ARGS__)
#define WARN(...)   syslog(LOG_WARNING, ##__VA_ARGS__)
#define ERROR(...)  syslog(LOG_ERR, ##__VA_ARGS__)



#ifdef CONF_EXTENSIVE_DEBUG_LOG
#define XDEBUG(...) syslog(LOG_DEBUG, ##__VA_ARGS__)
#else
#define XDEBUG(...)
#endif

#define mark        DEBUG("M [%s][%d]", __func__, __LINE__)

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
