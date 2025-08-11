
#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/i2c.h"
#include "pico/cyw43_arch.h"
#include "hardware/uart.h"
#include "time.h"
#include <stddef.h>
#include "hardware/gpio.h"
#include <stdlib.h>
#include <string>
#include <cstring>
#include <stdint.h>


bool dbug = false;
bool printraw = true; //if true, print raw nmea lines, if false, print parsed data



// I2C defines
// This example will use I2C0 on GPIO8 (SDA) and GPIO9 (SCL) running at 400KHz.
// Pins can be changed, see the GPIO function select table in the datasheet for information on GPIO assignments
#define I2C_PORT i2c0
#define I2C_SDA 8
#define I2C_SCL 9

bool led_state = false; // Track the state of the LED
void blink_on(){
    // Turn on Pico W LED
    cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, 1);
    led_state = true;
}
void blink_off(){
    // Turn off Pico W LED
    cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, 0);
    led_state = false;
}
void blink_toggle(){
    // Toggle Pico W LED
    if (led_state) {
        blink_off();
    } else {
        blink_on();

    }
}


//set UART1 for reading from gps module UART0 is default for the pico to 
//use for other communications
#define UART_ID uart1
#define BAUD_RATE 9600 //default baud rate for most neo6 gps modules
#define TX 4 //for pico use pin 6,7(which are translated to gpio pins 4,5)
#define RX 5 //these are uart1 enabled pins pins

void initilize(){//call first in main
    
    // Initialise the stdio library, which is used for printf and uart_puts
    stdio_init_all();
    
    // Initialise the UART for GPS communication
    uart_init(UART_ID, BAUD_RATE);
    gpio_set_function(TX, GPIO_FUNC_UART);
    gpio_set_function(RX, GPIO_FUNC_UART);

    // Initialise the Wi-Fi chip
    if (cyw43_arch_init()) {
        printf("Wi-Fi init failed\n");
        while (1) {}
    }

        // I2C Initialisation. Using it at 400Khz.
    i2c_init(I2C_PORT, 400*1000);
    
    gpio_set_function(I2C_SDA, GPIO_FUNC_I2C);
    gpio_set_function(I2C_SCL, GPIO_FUNC_I2C);
    gpio_pull_up(I2C_SDA);
    gpio_pull_up(I2C_SCL);
    // For more examples of I2C use see https://github.com/raspberrypi/pico-examples/tree/master/i2c


}
// Use some the various UART functions to send out data
// In a default system, printf will also output via the default UART
// For more examples of UART use see https://github.com/raspberrypi/pico-examples/tree/master/uart

const uint8_t max_NMEA_length = 87; // Maximum length of NMEA sentence

struct NMEA_sentence {
    char characters[max_NMEA_length]; // Array to hold the NMEA sentence
    uint8_t length; // Length of the NMEA sentence
};

typedef char NMEA_talk_id[2];
typedef char NEMA_sentence_formatter[3];
typedef char NMEA_MAN_CODE[3];

enum class NMEA_adr_field_type:uint8_t {
    invalid     = 0, // Invalid field
    approved    = 1, // Approved field
    query       = 2, // Query field
    proprietary = 3, // Proprietary field
};

struct NMEA_adr_field_approved {
    NMEA_talk_id talker; // Talker ID
    NEMA_sentence_formatter formatter; // Sentence formatter
};

struct NMEA_adr_field_query {
    NMEA_talk_id listener; 
    NMEA_talk_id talker;
};

struct NMEA_adr_field_proprietary {
    NMEA_MAN_CODE manufacturer; // Manufacturer code
    // man def char not include
};

struct NMEA_adr_field {
    NMEA_adr_field_type type; // Type of the address field
    union {
        NMEA_adr_field_approved approved; // Approved field
        NMEA_adr_field_query query; // Query field
        NMEA_adr_field_proprietary proprietary; // Proprietary field
    };
};

enum class NMEA_sentence_type : uint8_t {
    invalid         = 0, // Invalid sentence
    parameteric     = 1, // Parameteric sentence
    encapsulation   = 2, // Encapsulation sentence
    query           = 3, // Query sentence
    proprietary     = 4, // Proprietary sentence
};

struct NMEA_data_content {
    NMEA_sentence_type type; // Type of the NMEA sentence
    NMEA_adr_field address; // Address field
};

class NMEA_listener{
    public:
        NMEA_listener(): sentence{ "", 0} {};

        bool sentence_available() {return false;};
        NMEA_sentence pull() {return sentence;};
    protected:
        NMEA_sentence sentence;
};

class NMEA_listener_uart: public NMEA_listener {
    public:
        NMEA_sentence sentence; // Add this field to match constructor initialization
        NMEA_listener_uart() = delete; // No default constructor
        NMEA_listener_uart(uart_inst_t *uart):
            uart(uart), 
            sentence_status(none), 
            sentence{"", 0} 
        {};

        bool sentence_available() {
            char character;

            while ((uart_is_readable(uart)) && (sentence_status != completed)) {
                character = uart_getc(uart); // Read a character from UART

                switch(sentence_status) {
                    case none:
                        if (character == '$') {
                            sentence.characters[0] = character; // Start of NMEA sentence
                            sentence.length = 1; // iterate length
                            sentence_status = started; // Change status to started
                        }
                        break;
                    case started:
                        sentence.characters[sentence.length] = character; // Add character to sentence
                        sentence.length++; // Increment length
                        if (character == '\r') {
                            sentence_status = terminated; // Change status to terminated
                        } else {
                            if (sentence.length >= max_NMEA_length) {
                                sentence.length = 0; // Reset length if it exceeds max
                                sentence_status = none; // Reset status if length exceeds max
                            }
                        }
                        break;
                    case terminated:
                        sentence.characters[sentence.length] = character; // Add character to sentence
                        sentence.length++; // Increment length
                        if (character == '\n') {
                            sentence_status = completed; // Change status to completed
                        } else {
                            sentence.length = 0; // Reset length if it exceeds max
                            sentence_status = none; // Reset status if length exceeds max
                        }
                        break;

                }
        }
        return (sentence_status == completed);
    };

    NMEA_sentence pull() {
        NMEA_sentence sentence_r = {"", 0};
        if (sentence_status == completed) {
            sentence_r = sentence; // Copy the completed sentence
            sentence_status = none; // Reset status for next sentence
            sentence.length = 0; // Reset length for next sentence
        }
        return sentence_r;
    };  

    private:

        uart_inst_t *uart; // Pointer to the UART instance
        enum: uint8_t {
            none        = 0, //waiting for line start [$]
            started     = 1, //line started and receiving characters [$]
            terminated  = 2, //return  (13, '\r')
            completed   = 3  //newline (10, '\n')
        } sentence_status;
};

NMEA_listener_uart gps_listen(UART_ID);

void nmea_print_adr_field(const NMEA_adr_field& address) {
    char* type;
    switch (address.type) {
        case NMEA_adr_field_type::approved:
            printf("[%.*s][%.*s]",2, address.approved.talker,3, address.approved.formatter);
            break;
        case NMEA_adr_field_type::query:
            printf("[%.*s][%.*s]",2, address.query.listener, 2,address.query.talker);
            break;
        case NMEA_adr_field_type::proprietary:
            printf("[%.*s]", 3,address.proprietary.manufacturer);
            break;
        case NMEA_adr_field_type::invalid:
            printf("[???]");
            break;
    }
}

void nmea_print_data_content(const NMEA_data_content& content) {
    char* type;
    switch (content.type) {
        case NMEA_sentence_type::parameteric:
            printf("PARAM");
            break;
        case NMEA_sentence_type::query:
            printf("QUERY");
            break;
        case NMEA_sentence_type::encapsulation:
            printf("ENCAP");
            break;
        case NMEA_sentence_type::proprietary:
            printf("PROPR");
            break;
        case NMEA_sentence_type::invalid:
            printf("INVAL");
            break;
    }
    if (content.type != NMEA_sentence_type::invalid) {
        nmea_print_adr_field(content.address);
    }
}

NMEA_adr_field nmea_dec_adr_approved(const NMEA_sentence& sentence) {
    NMEA_adr_field address;
    strncpy(address.approved.talker, sentence.characters + 1, 2); // Copy talker ID
    strncpy(address.approved.formatter, sentence.characters + 3, 3); // Copy formatter
    address.type = NMEA_adr_field_type::approved; // Set type to approved
    return address;
};

NMEA_adr_field nmea_dec_adr_query(const NMEA_sentence& sentence) {
    NMEA_adr_field address;
    strncpy(address.query.talker, sentence.characters + 1, 2); // Copy talker ID
    strncpy(address.query.listener, sentence.characters + 3, 2); // Copy listener ID
    address.type = NMEA_adr_field_type::query; // Set type to query
    return address;
};

NMEA_adr_field nmea_dec_adr_proprietary(const NMEA_sentence& sentence) {
    NMEA_adr_field address;
    strncpy(address.proprietary.manufacturer, sentence.characters + 2, 3); // Copy manufacturer code
    address.type = NMEA_adr_field_type::proprietary; // Set type to proprietary
    return address;
};

NMEA_data_content nmea_decode(const NMEA_sentence& sentence){
    NMEA_data_content content;
    switch (sentence.characters[0]) {
        case '!':
            content.type = NMEA_sentence_type::encapsulation;
            content.address = nmea_dec_adr_approved(sentence);
            break;
        case '$':
            switch (sentence.characters[1]) {
                case 'P':
                    content.type = NMEA_sentence_type::proprietary;
                    content.address = nmea_dec_adr_proprietary(sentence);
                    break;
                default:
                switch (sentence.characters[5]) {
                    case 'Q':
                        content.type = NMEA_sentence_type::query;
                        content.address = nmea_dec_adr_query(sentence);
                    default:
                        content.type = NMEA_sentence_type::parameteric;
                        content.address = nmea_dec_adr_approved(sentence);
                }
            }
            break;
        default:
            content.type = NMEA_sentence_type::invalid; // Invalid sentence type
            break;
    }
    return content;
};



int main() {
    initilize();


    // LED on
    blink_on();
    sleep_ms(2000);
    blink_off();
    sleep_ms(500);
    while (true) {
        blink_toggle();
        NMEA_sentence sentence;
        if (gps_listen.sentence_available()) {
            sentence = gps_listen.pull();
        }
        printf("NMEA: %.*s\n", sentence.length, sentence.characters);

    }
    return 0;
}