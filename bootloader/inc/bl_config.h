#ifndef BL_CONFIG_H
#define BL_CONFIG_H
#define BL_VERSION_MAJOR   1u
#define BL_VERSION_MINOR   0u
#define BL_VERSION_PATCH   0u
#define BL_BOOT_WINDOW_MS  400u      /* after a good image is found, wait this long for a HELLO from the host */
#define BL_BAUD            115200u
#define BL_IDLE_RESET_US   50000u    /* inter-byte timeout: reset the frame parser if a frame stalls */
#endif
