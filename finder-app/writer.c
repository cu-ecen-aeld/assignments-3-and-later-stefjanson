#include <stdio.h>
#include <stdlib.h>
#include <syslog.h>

int main(int argc, char *argv[]) {
    // Open syslog with a specific identifier and options
    openlog("writer_program", LOG_PID, LOG_USER);

    if (argc != 3) {
        // Log an error message to syslog
        syslog(LOG_ERR, "Usage: %s writefile writestr", argv[0]);
        // Close syslog before exiting
        closelog();
        return 1;
    }

    const char *writefile = argv[1];
    const char *writestr = argv[2];

    FILE *file = fopen(writefile, "w");
    if (file == NULL) {
        // Log an error message to syslog
        syslog(LOG_ERR, "File could not be created");
        // Close syslog before exiting
        closelog();
        return 1;
    }

    fprintf(file, "%s", writestr);
    fclose(file);

    // Log a success message to syslog
    syslog(LOG_INFO, "Write to file successful");

    // Close syslog before exiting
    closelog();

    return 0;
}
