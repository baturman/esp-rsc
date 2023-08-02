#ifndef STASSID
#define STASSID "EXODUS"
#define STAPSK "n952004$"
#endif

#define HOSTNAME "esxi01-rc"
#define TIMEZONE "EET-2EEST,M3.5.0/3,M10.5.0/4" // https://gist.github.com/alwynallan/24d96091655391107939
#define PORT 80


#define THERMISTORNOMINAL 10000                 // resistance at 25 degrees C
#define TEMPERATURENOMINAL 25                   // temp. for nominal resistance (almost always 25 C)
#define NUMSAMPLES 10                           // Number of samples
#define BCOEFFICIENT 3950                       // The beta coefficient of the thermistor (usually 3000-4000)
#define SERIESRESISTOR 10000                    // the value of the 'other' resistor

const char *ssid = STASSID;
const char *password = STAPSK;

const char* ntpServer1 = "pool.ntp.org";
const char* ntpServer2 = "time.nist.gov";
const long  gmtOffset_sec = 3600;
const int   daylightOffset_sec = 3600;