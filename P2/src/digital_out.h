#ifndef DIGITAL_OUT_H
#define DIGITAL_OUT_H

#include <stdint.h>

class Digital_out {
public:
    Digital_out(uint8_t pin);
    void init();
    void set_hi();
    void set_lo();
    void toggle();
private:
    uint8_t pinMask;
};

#endif