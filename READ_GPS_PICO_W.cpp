/*
██████╗ ██╗██╗     ██╗  ██╗   ██╗   ██████╗  ██████╗ ████████╗
██╔══██╗██║██║     ██║  ╚██╗ ██╔╝   ██╔══██╗██╔═══██╗╚══██╔══╝
██████╔╝██║██║     ██║   ╚████╔╝    ██████╔╝██║   ██║   ██║   
██╔══██╗██║██║     ██║    ╚██╔╝     ██╔══██╗██║   ██║   ██║   
██████╔╝██║███████╗███████╗██║      ██████╔╝╚██████╔╝   ██║   
╚═════╝ ╚═╝╚══════╝╚══════╝╚═╝      ╚═════╝  ╚═════╝    ╚═╝    

        ___________
       |___________|
           |  |
         [ o  o ]
          \____/
           ||||
           ||||
           ||||
           ||||
         __||||__
        |_________|

IF you want to assist in development please see the repository at: https://github.com/jritchie325/PicoGPS-cpp
submit issues, check out the full development of the BillyBot project, and contribute on the GitHub page

Current micro-controller platform is: RaspberryPi PICO W
this code is adapted from the work of Robert's Scmorgasboard, including NMEA standards and code to utilize it.
this is the first in a series of videos used here: https://www.youtube.com/watch?v=aLeCaa7TUZA 


as of (8/12/2025) this script is a [WIP]work in progress, please be patient as there is still plenty to do
*/

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


// ---------- debugging and output settings ---------- //
bool dbug = false;
bool printraw = true; //if true, print raw nmea lines, if false, print parsed data
bool include_sentence_type = true; //if true, print the sentence type, if false, do not print the sentence type
bool include_address = true; //if true, print the address field, if false, do not print the address field
bool print_parametric_fields = true; //if true, print the parametric fields, if false, do not print the parametric fields



// ---------- I2C definition ---------- //
// This example will use I2C0 on GPIO8 (SDA) and GPIO9 (SCL) running at 400KHz.
// Pins can be changed, see the GPIO function select table in the datasheet for infomation on GPIO assignments
#define I2C_PORT i2c0
#define I2C_SDA 8
#define I2C_SCL 9


// ---------- Blink definition ---------- //
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


// ---------- UART definition ---------- //
//set UART1 for reading from gps module UART0 is default for the pico to 
//use for other communications
#define UART_ID uart1
#define BAUD_RATE 9600 //default baud rate for most neo6 gps modules
#define TX 4 //for pico use pin 6,7(which are translated to gpio pins 4,5)
#define RX 5 //these are uart1 enabled pins pins


// ---------- Initilization ---------- //
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





//
//----------------------------------------------------------------
//--   __ ___    __ _  __ ___    __ __   __ _____      __       --
//--   |*| *\\   |*||  |*| *\\  /* |*||  |*|****||    /**\\     --
//--   |*||\*\\  |*||  |*||\*\\/*//|*||  |*||___     /*//*\\    --
//--   |*|| \*\\ |*||  |*|| \*\*// |*||  |*|***||   /*//_\*\\   --
//--   |*||  \*\\|*||  |*||  \*//  |*||  |*||___   |********||  --
//--   |*||   \* |*||  |*||        |*||  |*|****|| |*||   |*||  --
//--                                                            --
//----------------------------------------------------------------
//


// ---------- Initial Declarations to use for NMEA sentences ---------- //
const uint8_t max_NMEA_length = 82; // Maximum length of NMEA sentence according to the NMEA 0183 standard

struct NMEA_sentence {//acts as a sentence buffer for NMEA sentence data from the GPS
    char characters[max_NMEA_length];   // Array to hold the NMEA sentence
    uint8_t length;                     // Length of the NMEA sentence
};


// ---------- NMEA Field Types ---------- //

typedef char NMEA_talk_id[2];               //identify the talker
typedef char NEMA_sentence_formatter[3];    //sentence type
typedef char NMEA_MAN_CODE[3];              // Manufacturer code

enum class NMEA_adr_field_type:uint8_t {//switches to determine the current NMEA field type 
    invalid     = 0, // Invalid field
    approved    = 1, // Approved field
    query       = 2, // Query field
    proprietary = 3, // Proprietary field
};

struct NMEA_adr_field_approved { //structure for the approved field
    NMEA_talk_id            talker;     // Talker ID
    NEMA_sentence_formatter formatter;  // Sentence formatter
};

struct NMEA_adr_field_query { //structure for the query field
    NMEA_talk_id listener; // Listener ID
    NMEA_talk_id talker;   // Talker ID
};

struct NMEA_adr_field_proprietary {//if you get this, basically ignore it
    NMEA_MAN_CODE manufacturer; // Manufacturer code
    // manufacturer defined characters not included
};

struct NMEA_adr_field {//with in a field structure declares the type, as well as pulls addressing for each type
    NMEA_adr_field_type type; // Type of the address field
    union {
        NMEA_adr_field_approved     approved; // Approved field
        NMEA_adr_field_query        query; // Query field
        NMEA_adr_field_proprietary  proprietary; // Proprietary field
    };
};

/*
sentence formater to number (u-int, 16 bit)
'A' 0b01000001 to 'z' 0b01011010, & 0b0001111, = 0b000ddddd
vvvvvvvv
"XYZ"   X: 0b0xxxxx0000000000   (<<10)
        Y: 0b000000yyyyy00000   (<<5)
        Z: 0b00000000000zzzzz   (<<0)
        |: 0b0xxxxxyyyyyzzzzz
*/

#define NMEA_sentence_formatter_number(formatter)\
    ((unsigned int)(formatter[0] & 0b00011111)  <<10 ) |\
    ((unsigned int)(formatter[1] & 0b00011111)  <<5  ) |\
    ((unsigned int)(formatter[2] & 0b00011111)       )


enum class NMEA_par_field_type : unsigned int {// to hold the sentence type identifier from the NMEA data

    txt = NMEA_sentence_formatter_number("TXT"),    // Text field
  //gga = NMEA_sentence_formatter_number("GGA"),    // Global Positioning System Fix Data
    gll = NMEA_sentence_formatter_number("GLL"),    // Geographic Position - Latitude/Longitude
  //gsa = NMEA_sentence_formatter_number("GSA"),    // GNSS DOP and Active Satellites
  //gsv = NMEA_sentence_formatter_number("GSV"),    // GNSS Satellites in View
  //rmc = NMEA_sentence_formatter_number("RMC"),    // Recommended Minimum Specific GNSS Data
  //vtg = NMEA_sentence_formatter_number("VTG"),    // Course Over Ground and Ground Speed

    unknown = 0xFFFF                                // Unknown parametric field type returns nothing
};

struct NMEA_par_field_position{ // Geographic Position - Latitude/Longitude
                                //ddmm.ssss, nulled
    bool    nulled;     // Indicates if the field is nulled
    uint8_t lat_deg;    // Latitude in ddmm.ssss --> dd (0 - 90)
    uint8_t lat_min;    // Latitude in ddmm.ssss --> mm (0' - 59')
    float   lat_sec;    // Latitude in ddmm.ssss --> mmmm (0" - 59.9...")
    char    lat_NS;     // Latitude North/South indicator ('N' or 'S')
    uint8_t lon_deg;    // Longitude in ddmm.ssss --> dd (0 - 180)
    uint8_t lon_min;    // Longitude in ddmm.ssss --> mm (0'- 59')
    float   lon_sec;    // Longitude in ddmm.ssss --> mmmm (0" - 59.9...")
    char    lon_EW;     // Longitude East/West indicator ('E' or 'W')
};

struct NMEA_par_field_time{
    //hhmmss.ss, nulled
    bool    nulled; // Indicates if the field is nulled
    uint8_t hour;   // Hour in hhmmss.ss --> hh (0 - 23)
    uint8_t minute; // Minute in hhmmss.ss --> mm (0 - 59)
    float   second; // Second in hhmmss.ss --> ss (0.00 - 59.99)
};

struct NMEA_par_field_txt {     //text transmission field
    
    uint8_t total_number;       //total sentences in the txt transmit
    uint8_t sentence_number;    //current sentence
    uint8_t text_id;            //identifier for the text message
    char    text_message[62];   //sets the string length for the txt msg data field

};

struct NMEA_par_field_gll {// Geographic Position - Latitude/Longitude
    NMEA_par_field_position position;   // Instantiates the position field
    NMEA_par_field_time     time;       // Instantiates the time field
    char                    status;     // Status ('A' for active, 'V' for void)
    char                    mode;       // Mode ('A' for autonomous, 'D' for differential, 'E' for estimated, 'N' for not valid)
};

struct NMEA_par_field {
    NMEA_par_field_type type;   // Type of the parametric field
    union {
        NMEA_par_field_txt txt; // Text field
      //NMEA_par_field_gga gga; // Global Positioning System Fix Data
        NMEA_par_field_gll gll; // Geographic Position - Latitude/Longitude
      //NMEA_par_field_gsa gsa; // GNSS DOP and Active Satellites
      //NMEA_par_field_gsv gsv; // GNSS Satellites in View
      //NMEA_par_field_rmc rmc; // Recommended Minimum Specific GNSS Data
      //NMEA_par_field_vtg vtg; // Course Over Ground and Ground Speed
    };
};

// ---------- NMEA sentences ---------- //
enum class NMEA_sentence_type : uint8_t {

    invalid         = 0, // Invalid sentence
    parameteric     = 1, // Parameteric sentence
    encapsulation   = 2, // Encapsulation sentence
    query           = 3, // Query sentence
    proprietary     = 4, // Proprietary sentence

};

struct NMEA_data_content { //based on the field type and address for that field can write the NMEA sentence type being recieved
    NMEA_sentence_type  type;       // Type of the NMEA sentence
    NMEA_adr_field      address;    // Address field

    union {
        NMEA_par_field parametric;              // Parametric field
      //NMEA_encapsulation_field encapsulation; // Encapsulation field
      //NMEA_query_field query;                 // Query field
      //NMEA_proprietary_field proprietary;     // Proprietary field
    };
};


// ---------- Getting Values from the GPS module ---------- //

class NMEA_listener{//default listener class
    public:

        NMEA_listener(): sentence{ "", 0} {};       // Creates an empty NMEA_sentence
        bool sentence_available() {return false;};  // Will be overridden in derived classes
        NMEA_sentence pull() {return sentence;};    // Placeholder for the pull function
    
    protected:
    
        NMEA_sentence sentence;                     // declares buffer to hold the current sentence

};

class NMEA_listener_uart: public NMEA_listener {    //derived from the above class work specifically with UART

    public:

        NMEA_sentence sentence;                     // Add this field to match constructor initialization
        NMEA_listener_uart() = delete;              // No default constructor
        NMEA_listener_uart(uart_inst_t *uart):      // Constructor that initializes the UART instance
            uart(uart), 
            sentence_status(none), 
            sentence{"", 0} 
        {};

        bool sentence_available() {// Listens if NMEA data is being sent from the GPS
            char character;

            while ((uart_is_readable(uart))             // UART data is available to read
                && (sentence_status != completed)) {    // sentence status is not in completed state

                character = uart_getc(uart);            // Read a character from UART

                switch(sentence_status) {               // Next based on the curent sentence status

                    case none:
                        if (character == '$') {                 // looks for the start of a NMEA sentence
                            sentence.characters[0] = character; // Adds the starting char to buffer
                            sentence.length = 1;                // moves to the next character in the NMEA sentence
                            sentence_status = started;          // Change sentence status to started
                        }
                        break;

                    case started:
                        sentence.characters[sentence.length] = character;   // Adds the next the character to sentence untill it sees an end of line
                        sentence.length++;                                  // Moves to the next character in the NMEA sentence
                        if (character == '\r') {
                            sentence_status = terminated;                   // Change status to terminated
                        } else {
                            if (sentence.length >= max_NMEA_length) {
                                sentence.length = 0;                        // Reset length if it exceeds max
                                sentence_status = none;                     // Reset status if length exceeds max
                            }
                        }
                        break;

                    case terminated:
                        sentence.characters[sentence.length] = character;   // After getting terminator adds final character to sentence
                        sentence.length++;                                  // Increment length
                        if (character == '\n') {
                            sentence_status = completed;                    // Change status to completed
                        } else {
                            sentence.length = 0;                            // Reset length if it exceeds max
                            sentence_status = none;                         // Reset status if length exceeds max
                        }
                        break;


                }
        }

        return (sentence_status == completed);
        
    };

    NMEA_sentence pull() {
        NMEA_sentence sentence_r = {"", 0}; //instantiates and empty return sentence
        if (sentence_status == completed) {
            sentence_r = sentence;          // Copy the completed sentence to return variable
            sentence_status = none;         // Reset status for next sentence
            sentence.length = 0;            // Reset length for next sentence
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


// ---------- Print Function ---------- //
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

    printf("\n");

}

void nmea_print_parametric_fields_txt(const NMEA_par_field_txt& txt) {
    printf("[%d of %d] [ID: %d] [Message: %s]", txt.sentence_number, txt.total_number, txt.text_id, txt.text_message);
    printf("\n");
}

void nmea_print_parametric_field_position(const NMEA_par_field_position& position) {
    char buffer[strlen("[89°59'59.99999\"N, 179°59'59.99999\"E]_")];
    char lat_sec[strlen("59.99999_")];
    char lon_sec[strlen("59.99999_")];

    if (!position.nulled){
        snprintf(lat_sec, sizeof(lat_sec), "%08.5f", position.lat_sec); // Format float as string
        if (lat_sec[0] == ' ') lat_sec[0] = '0';
        snprintf(lon_sec, sizeof(lon_sec), "%08.5f", position.lon_sec); // Format float as string
        if (lon_sec[0] == ' ') lon_sec[0] = '0';

        sprintf(
            buffer, "[%02u°%02u'%s\"%c,%03u°%02u'%s\"%c]",
            position.lat_deg,position.lat_min,lat_sec,position.lat_NS,
            position.lon_deg,position.lon_min,lon_sec,position.lon_EW
        );
        printf("%s", buffer);
    } else {
        printf("[NULL POS]");
    }
}

void nmea_print_parametric_field_time(const NMEA_par_field_time& time){
    char buffer[strlen("[hh:mm:ss.ss]_")];
    char seconds[strlen("ss.ss_")];

    if (!time.nulled){
        sprintf(seconds, "%05.2f", time.second);
        if (seconds[0] == ' ') seconds[0] = '0';

        sprintf(buffer, "[%02u:%02u:%s]_", time.hour, time.minute, seconds);
        printf("%s", buffer);
    } else {
        printf("[NULL TIME]");
    }
}

void nmea_print_parametric_field_gll(const NMEA_par_field_gll& gll) {
    
    nmea_print_parametric_field_position(gll.position);
    nmea_print_parametric_field_time(gll.time);
    
    printf(gll.status == 'A' ? "[VALID]" : "[INVALID]");
    
    if(gll.mode != '\0'){
        printf("[%c]", gll.mode);
    }

    printf("\n");
    sleep_ms(10);

}

void nmea_print_parametric_field(const NMEA_par_field& fields) {

    switch (fields.type) {

        case NMEA_par_field_type::txt:
            nmea_print_parametric_fields_txt(fields.txt);
            break;

        case NMEA_par_field_type::gll:
            nmea_print_parametric_field_gll(fields.gll);
            break;

        default:
            //printf("[UNKNOWN]");
            break;
    }
}

void nmea_print_data_content(const NMEA_data_content& content) {
    char* type;
    if (include_sentence_type) {
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
                printf("PROPR\n");
                break;

            case NMEA_sentence_type::invalid:
                printf("INVAL\n");
                break;

        }
    }

    if ((content.type != NMEA_sentence_type::invalid) && include_address) {
        nmea_print_adr_field(content.address);
    }

    switch (content.type) {
        case NMEA_sentence_type::parameteric:
            if (print_parametric_fields) {   
                nmea_print_parametric_field(content.parametric);
            }
            break;
        
        case NMEA_sentence_type::query:         // Handle query content if needed
            break;
        
        case NMEA_sentence_type::encapsulation: // Handle encapsulation content if needed
            break;
        
        case NMEA_sentence_type::proprietary:   // Handle proprietary content if needed
            break;
    }
}


// ---------- Decoding From Raw GPS Data ---------- //
NMEA_adr_field nmea_dec_adr_approved(const NMEA_sentence& sentence) {
    NMEA_adr_field address;
    strncpy(address.approved.talker,    sentence.characters + 1, 2);    // Copy talker ID
    strncpy(address.approved.formatter, sentence.characters + 3, 3);    // Copy formatter
    address.type = NMEA_adr_field_type::approved;                       // Set type to approved
    return address;
};

NMEA_adr_field nmea_dec_adr_query(const NMEA_sentence& sentence) {
    NMEA_adr_field address;
    strncpy(address.query.talker,   sentence.characters + 1, 2);    // Copy talker ID
    strncpy(address.query.listener, sentence.characters + 3, 2);    // Copy listener ID
    address.type = NMEA_adr_field_type::query;                      // Set type to query
    return address;
};

NMEA_adr_field nmea_dec_adr_proprietary(const NMEA_sentence& sentence) {
    NMEA_adr_field address;
    strncpy(address.proprietary.manufacturer, sentence.characters + 2, 3); // Copy manufacturer code
    address.type = NMEA_adr_field_type::proprietary; // Set type to proprietary
    return address;
};

unsigned int nmea_sent_form_num(const char formatter[3]){
    return NMEA_sentence_formatter_number(formatter);
}

uint8_t nmea_chop_par_field(const char characters[], const uint8_t length, char string[]){
    uint8_t i;
    uint8_t field_count;

    field_count = 1;

    for (i = 0; i<length; i++){
        if (characters[i] == ','){      // If a comma is found, it indicates the end of a field
            field_count ++;
            string[i] = '\0';           // Null-terminate the string
        } else {
            string[i] = characters[i];  // Copy character to string
        }
    }

    string[length] = '\0';

    return field_count;

}

NMEA_par_field_txt nmea_dec_par_field_txt(const char string[], const uint8_t fields){
    
    NMEA_par_field_txt txt;

    txt.total_number = atoi(string);    // Convert the first part of the string to total number
    string += strlen(string) + 1;       // Move to the next part of the string
    txt.sentence_number = atoi(string); // Convert the next part of the string to sentence number
    string += strlen(string) + 1;       // Move to the next part of the string
    txt.text_id = atoi(string);         // Convert the next part of the string to text ID
    string += strlen(string) + 1;       // Move to the next part of the string
    strncpy(txt.text_message, string, sizeof(txt.text_message) - 1); // Copy the remaining part of the string to text message
    return txt;
}   

NMEA_par_field_position nmea_dec_par_field_position(char*& string, uint8_t& fields) {
    NMEA_par_field_position position;
    char* field;
    char* decimals;
    position.nulled = false; // assume no null fields in position
    
    
    // Lat digrees, minutes, dmm[.ssss]
    field = string;                 //save pointer to latidude field
    string += strlen(string) + 1;   // Move to the next part of the string

    if (strlen(field) >= 4){
        decimals = strchr(field, '.');  // Find the decimal point in the latitude field
        if (decimals != NULL) {
            position.lat_sec = atof(decimals)*6;    // Convert the decimal part to float
            decimals[0] = '\0';                     // Null-terminate the string at the decimal point
        }

        position.lat_min = atoi(field + 2); // Convert the minutes part of the latitude field to integer
        field[2] = '\0';                    // Null-terminate the string after the degrees part
        position.lat_deg = atoi(field);     // Convert the degrees part of the latitude field to integer
    }else {
        position.nulled = true; // Mark as nulled if the field is too short
    }

    // Latitude North/South indicator
    position.lat_NS = string[0];    // Copy the North/South indicator
    if (position.lat_NS == '\0') {
    position.nulled = true;         // Mark as nulled if the indicator is null
    }
    string += strlen(string) + 1;   // Move to the next part of the string

    // Longitude degrees, minutes, dmm[.ssss]
    field = string;                 // Save pointer to longitude field
    string += strlen(string) + 1;   // Move to the next part of the string
    if (strlen(field) >= 5) {
        decimals = strchr(field, '.'); // Find the decimal point in the longitude field
        if (decimals != NULL) {
            position.lon_sec = atof(decimals)*6;    // Convert the decimal part to float
            decimals[0] = '\0';                     // Null-terminate the string at the decimal point
        }

        position.lon_min = atoi(field + 2); // Convert the minutes part of the longitude field to integer
        field[2] = '\0';                    // Null-terminate the string after the degrees part
        position.lon_deg = atoi(field);     // Convert the degrees part of the longitude field to integer
    } else {
        position.nulled = true;             // Mark as nulled if the field is too short
    }
    // Longitude East/West indicator
    position.lon_EW = string[0];    // Copy the East/West indicator
    if (position.lon_EW == '\0') {
        position.nulled = true;     // Mark as nulled if the indicator is null
    }
    string += strlen(string) + 1;   // Move to the next part of the string

    return position; // Return the decoded position
    
}

NMEA_par_field_time nmea_dec_par_field_time(char*& string, uint8_t& fields) {
    NMEA_par_field_time time;
    char* field;
    char* decimals;

    time.nulled = false; // assume no null fields in time

    // Time in hhmmss.ss format
    field = string; // Save pointer to time field
    string += strlen(string) + 1; // Move to the next part of the string

    if (strlen(field) > 6) {        
        time.second = atof(field+4); // Convert the decimal part to float
        field[4] = '\0'; // Null-terminate the string at the decimal point
        time.minute = atoi(field + 2); // Convert the minutes part of the time field to integer
        field[2] = '\0'; // Null-terminate the string after the hours part
        time.hour = atoi(field); // Convert the hours part of the time field to integer
    } else {
        time.nulled = true; // Mark as nulled if the field is too short
    }

    return time; // Return the decoded time
}

NMEA_par_field_gll nmea_dec_par_field_gll(char string[], uint8_t fields) {
    NMEA_par_field_gll gll;

    // Decode position
    gll.position = nmea_dec_par_field_position(string, fields);

    // Decode time
    gll.time = nmea_dec_par_field_time(string, fields);

    // Decode status
    gll.status = string[0]; // Copy the status character
    if (fields > 6) {
        string += strlen(string) + 1; // Move to the next part of the string
        gll.mode = string[0]; // Copy the mode character
    }else {
        gll.mode = '\0'; // Default mode if not provided
    }

    return gll; // Return the decoded GLL field
    
} 


NMEA_par_field NMEA_dec_par_field(const char formatter[3], const char characters[], const uint8_t length){// Decode the parametric field based on the formatter and characters
    NMEA_par_field fields;// Declare a NMEA_par_field to hold the decoded fields
    char string[max_NMEA_length -7 -5 +1]; // Create a string to hold the chopped parametric field
    uint8_t field_count;// Number of fields in the parametric field

    field_count = nmea_chop_par_field(characters, length, string); // Chop the parametric field into parts

    switch (nmea_sent_form_num(formatter)){// Check the formatter to determine the type of parametric field
        case (unsigned int)NMEA_par_field_type::txt:// Text field
            fields.type = NMEA_par_field_type::txt;
            fields.txt = nmea_dec_par_field_txt(string, field_count);
            break;
        case (unsigned int)NMEA_par_field_type::gll:// Geographic Position - Latitude/Longitude
            // Check if the field count is valid for GLL
            fields.type = NMEA_par_field_type::gll;// Set the type to GLL
            fields.gll = nmea_dec_par_field_gll(string, field_count); // Decode GLL field
            break;
        default:
            fields.type = NMEA_par_field_type::unknown; // Unknown parametric field type
            break;
    }

    return fields;

}

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
                        content.parametric = NMEA_dec_par_field(content.address.approved.formatter,
                                                                sentence.characters + 7,
                                                                sentence.length -7-5); // Decode parametric field
                }
            }
            break;
        default:
            content.type = NMEA_sentence_type::invalid; // Invalid sentence type
            break;
    }
    return content;
};


// ---------- Main Program ---------- //
//plan to put all above into header/library file to make everything more user friendly

int main() {
    initilize();

    bool blink_on_boot = false; // Flag to control LED blinking on boot
    if (blink_on_boot) {
        // LED on
        blink_on();
        sleep_ms(5000);
        blink_off();
        sleep_ms(2000);
    }

    if (dbug){
    
    }

    while (true) {
        blink_toggle();
        NMEA_sentence sentence;
        if (gps_listen.sentence_available()) {
            sentence = gps_listen.pull();
        }

        nmea_print_data_content(nmea_decode(sentence)); // Decode and print the NMEA sentence
        if (printraw) {// If raw printing is enabled, print the raw NMEA sentence
        printf("NMEA: %.*s", sentence.length, sentence.characters);
        }
    }
    
    return 0;
}