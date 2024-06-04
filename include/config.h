#define HOSTNAME "esxi01-rc"
#define PORT 80

#define THERMISTORNOMINAL 10000                 // resistance at 25 degrees C
#define TEMPERATURENOMINAL 25                   // temp. for nominal resistance (almost always 25 C)
#define NUMSAMPLES 10                           // Number of samples
#define BCOEFFICIENT 3950                       // The beta coefficient of the thermistor (usually 3000-4000)
#define SERIESRESISTOR 10000                    // the value of the 'other' resistor

const char* ntpServer1 = "pool.ntp.org";
const char* ntpServer2 = "time.nist.gov";
const long  gmtOffset_sec = 3600 * 3;           // UTC+3
const int   daylightOffset_sec = 0;             // No daylight savings.