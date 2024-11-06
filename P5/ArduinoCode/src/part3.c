#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <termios.h>
#include <errno.h>
#include <syslog.h>
#include <time.h>

#define MODBUS_ADDRESS 0x02
#define REGISTER_COUNT 10
#define CONTROL_REGISTER 0x0000
#define STATE_OPERATIONAL 0x01
#define STATE_STOP 0x02

uint16_t compute_crc(uint8_t *buf, uint8_t len) {
    uint16_t crc = 0xFFFF;
    
    for (uint8_t pos = 0; pos < len; pos++) {
        crc ^= (uint16_t)buf[pos];

        for (uint8_t i = 8; i != 0; i--) {
            if ((crc & 0x0001) != 0) {

                crc >>= 1;
                crc ^= 0xA001;

            } else {

                crc >>= 1;
            }
        }
    }

    return crc;

}

int open_serial(const char *device) {

    int fd = open(device, O_RDWR | O_NOCTTY | O_NDELAY);

    if (fd == -1) {

        syslog(LOG_ERR, "Failed to open serial port %s: %s", device, strerror(errno));
        return -1;

    }

    struct termios options;

    tcgetattr(fd, &options);
    cfsetispeed(&options, B115200);
    cfsetospeed(&options, B115200);

    options.c_cflag |= (CLOCAL | CREAD);
    options.c_cflag &= ~PARENB;
    options.c_cflag &= ~CSTOPB;
    options.c_cflag &= ~CSIZE;
    options.c_cflag |= CS8;

    tcsetattr(fd, TCSANOW, &options);
    return fd;
}

int read_register(int fd, uint8_t address, uint16_t reg) {
    uint8_t request[8];
    uint8_t response[7];

    request[0] = address;
    request[1] = 0x03;  // Function code for reading a register
    request[2] = (reg >> 8) & 0xFF;
    request[3] = reg & 0xFF;
    request[4] = 0x00;
    request[5] = 0x01;  // Reading one register

    uint16_t crc = compute_crc(request, 6);
    request[6] = crc & 0xFF;
    request[7] = (crc >> 8);  

    if (write(fd, request, 8) != 8) {

        syslog(LOG_ERR, "Failed to send read request: %s", strerror(errno));
        return -1;

    }

    if (read(fd, response, 7) != 7) {

        syslog(LOG_ERR, "Failed to read response: %s", strerror(errno));
        return -1;

    }

    return (response[3] << 8) | response[4];
}

int write_register(int fd, uint8_t address, uint16_t reg, uint16_t value) {
    uint8_t request[8];

    request[0] = address;
    request[1] = 0x06;  // Function code for writing a single register
    request[2] = (reg >> 8) & 0xFF;
    request[3] = reg & 0xFF;
    request[4] = (value >> 8) & 0xFF;
    request[5] = value & 0xFF;

    uint16_t crc = compute_crc(request, 6);
    request[6] = crc & 0xFF;
    request[7] = (crc >> 8) & 0xFF;

    if (write(fd, request, 8) != 8) {
        syslog(LOG_ERR, "Failed to send write request: %s", strerror(errno));
        return -1;
    }

    return 0;
}

void control_loop(int fd1, int fd2) {
    while (1) {
        int state = read_register(fd1, MODBUS_ADDRESS, CONTROL_REGISTER);
        if (state == -1) {
            syslog(LOG_ERR, "Failed to read state from Arduino 1");
            continue;
        }

        uint16_t command = (state == STATE_OPERATIONAL) ? STATE_OPERATIONAL : STATE_STOP;
        if (write_register(fd2, MODBUS_ADDRESS, CONTROL_REGISTER, command) == -1) {
            syslog(LOG_ERR, "Failed to write command to Arduino 2");
        } else {
            syslog(LOG_INFO, "Controlled state based on read state: %d", state);
        }

        usleep(500000);
    }
}

int main() {

    openlog("ArduinoControl", LOG_PID | LOG_CONS, LOG_USER);
    syslog(LOG_INFO, "Starting Arduino Control Program");

    int fd1 = open_serial("/dev/ttyUSB0");
    int fd2 = open_serial("/dev/ttyUSB1");

    if (fd1 == -1 || fd2 == -1) {
        syslog(LOG_ERR, "Failed to open serial ports");
        return 1;
    }

    control_loop(fd1, fd2);

    close(fd1);
    close(fd2);
    closelog();
    return 0;
}
