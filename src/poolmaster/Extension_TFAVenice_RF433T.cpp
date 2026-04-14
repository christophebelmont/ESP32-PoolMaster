
// Credits :
//      https://github.com/m5stack/M5Stack/blob/master/examples/Unit/RF433/RF433.ino
//      https://github.com/d10i/TFA433/blob/main/src/tfa433.cpp
//      https://github.com/zopieux/rtl_433_f007th/blob/master/src/main.cpp
//      https://github.com/merbanan/rtl_433/blob/master/src/devices/ambient_weather.c
//      https://github.com/AMcAnerney/Arduino-F007th-Sketches/blob/master/Final%20version%20(with%20CC3000%20wireless%20upload)
//      https://www.osengr.org/WxShield/Downloads/Weather-Sensor-RF-Protocols.pdf


// Usage in Indoor Pool, to get and display Water Temp on nice receiver.
// Air Temperature and Humidity of Indoor Pool are measured directly by the device
// Receiver :   TFA Dostmann Venise 30.3056.10 (without the floating sensor/transmiter)
//              https://www.amazon.fr/dp/B010NSG4V2
//              https://www.tfa-dostmann.de/en/product/wireless-pool-thermometer-venice-30-3056/
//             
// Transmiter : We simulate the Ambiant Weather F007TH
//              https://docs.m5stack.com/en/unit/rf433_t
//              5V - GPIO 
//https://www.osengr.org/WxShield/Downloads/Weather-Sensor-RF-Protocols.pdf

// TFA Venice is based on Ambient Weather F007TH

/* Ambient Weather F007TH :
Manchester coding is used at the physical layer by this sensor. Clock rate is
1024Hz within a very small window of variation. The preamble contains a total
of 13 bits; the first 11 are ones, followed by a 01 sequence. The next bit begins
the data frame. Each data frame here is six bytes, with the entire message including preamble
is sent three times with no delay between repetitions. All data is sent bigendian order, bits and bytes.
Because the preamble is 13 bits, each successive message repetition is shifted
one bit relative to byte or nibble boundaries. Extracting the repeated messages
therefore requires a one or two-bit shift for the 2nd and 3rd copies
respectively.
modulation  = OOK_PULSE_MANCHESTER_ZEROBIT
*/

// https://randomnerdtutorials.com/decode-and-send-433-mhz-rf-signals-with-arduino/
// https://github.com/denxhun/TFA433/blob/master/src/tfa433.cpp
// https://www.elecrow.com/wiki/315433mhz-rf-link-kit.html#resource
// https://electroniqueamateur.blogspot.com/2021/01/communication-rf-433-mhz-avec-radiohead.html

//- Transmitters:
///  - TX-C1 (433.92MHz)
///  - RFM85 from HopeRF http://www.hoperfusa.com/details.jsp?pid=127
///  - BS 433 Mhz   https://fr.aliexpress.com/item/1005009706095391.html
///                 https://www.amazon.fr/dp/B00VVDFY92
/// - Transceivers
///  - DR3100 (433.92MHz)
///

// Capture pulses in cu8 format with  ./rtl_433 -f 433.92M -S known -A -R 20
// analyse captured cu8 file with with https://triq.org/pdv3
// https://triq.org/rtl_433/PULSE_FORMATS.html
// https://github.com/oldrev/esp32c3-rmt-pwm-demo/blob/main/main/main.c
// https://github.com/oldrev/esp32c3-rmt-pwm-demo/blob/main/main/rmt_pwm_encoder.c


#include <Arduino.h>
#include "Config.h"
#include "PoolMaster.h"

#if defined(_EXTENSIONS_)
#include "Extension_TFAVenice_RF433T.h"
#include <driver/rmt.h>

ExtensionStruct myTFAVenice_RF433T = {0};
static int      tfaGPIO = 0; // disabled by default, suggest GPIO 14

extern void SuperVisor_Message(const char *, char*);
extern void PublishTopic(const char*, JsonDocument&);

void Init_RF433t()
{
    static bool driverinstalled = false;
    if (driverinstalled) {
        rmt_driver_uninstall(RMT_CHANNEL_0);
        driverinstalled = false;
    }
    // init the chip
    if (tfaGPIO<1) return;
    pinMode(tfaGPIO, OUTPUT);
    rmt_config_t config = RMT_DEFAULT_CONFIG_TX((gpio_num_t)tfaGPIO, RMT_CHANNEL_0);
    config.clk_div = 80; // input clock 80 MHz => output clk 1 MHz

    // config.tx_config.carrier_freq_hz = 1024;
    // config.tx_config.carrier_en = true;
    
    ESP_ERROR_CHECK(rmt_config(&config));
    ESP_ERROR_CHECK(rmt_driver_install(config.channel, 0, 0));
    driverinstalled = true;
}

void convertToRMT_ManchesterOOK(uint8_t* buff, int sizebits, rmt_item32_t *rmtitem, int clock)
{
    // using Manchester code
    // https://www.computer-dictionary-online.org/definitions-m/manchester-encoding
    for (int i=0; i<sizebits; i++) {
        uint8_t byte = buff[i/8];
        uint8_t bit =  (byte >> (7-(i%8))) & 1; // in BigEndian order!
        rmtitem[i].duration0 = clock/2; //0.45;
        rmtitem[i].duration1 = clock/2; //0.55;
        if (bit == 1) {
            rmtitem[i].level0 = 1;
            rmtitem[i].level1 = 0;
        }
        else {
            rmtitem[i].level0 = 0;
            rmtitem[i].level1 = 1;
        }
    }
}
/*
void convertToRMTManchesterOOKDifferential(uint8_t* buff, int sizebits, rmt_item32_t *rmtitem, int clock)
{
    // using Differential Manchester code
    // apparently TFAVenice does not use it
    // https://www.computer-dictionary-online.org/definitions-m/manchester-encoding
    static uint8_t lastbit = 1;
    rmtitem[0].level0 = 0;
    rmtitem[0].level1 = 1;
    for (int i=1; i<sizebits; i++) {
        uint8_t byte = buff[i/8];
        uint8_t bit =  (byte >> (7-(i%8))) & 1; // in BigEndian order!
        rmtitem[i].duration1 = clock/2;
        rmtitem[i].duration0 = clock/2;
        if (lastbit == bit)
             rmtitem[i].level0 = rmtitem[i-1].level1;
        else rmtitem[i].level0 = (rmtitem[i-1].level0==0) ? 1 : 0;
        lastbit = bit;
        rmtitem[i].level1 = (rmtitem[i].level0==0) ? 1 : 0;
    }
}*/

static void RF433Tsend(uint8_t* buff, int sizebits)
{
    rmt_item32_t *TFAVeniceData = (rmt_item32_t*)calloc(sizebits, sizeof(rmt_item32_t));
    convertToRMT_ManchesterOOK(buff, sizebits, TFAVeniceData, 976); //  1000ms/TFAClock=1024Hz => width=976 ms
    rmt_write_items(RMT_CHANNEL_0, TFAVeniceData, sizebits, true);
    free(TFAVeniceData);
}

static uint8_t lfsr_digest8(uint8_t const message[], unsigned bytes, uint8_t gen, uint8_t key)
{
    // From: https://github.com/merbanan/rtl_433/blob/master/src/util.c
    uint8_t sum = 0;
    for (unsigned k = 0; k < bytes; ++k) {
        uint8_t data = message[k];
        for (int i = 7; i >= 0; --i) {
            if ((data >> i) & 1)  sum ^= key;
            if (key & 1)          key = (key >> 1) ^ gen;
            else                  key = (key >> 1);
        }
    }
    return sum;
}

static void TFAVeniceFillData(uint8_t *data, uint8_t *input, int bitpos, int bitlenght)
{
    int startbyte = bitpos / 8;
    int startbit  = bitpos % 8;
    int nbbytes   = bitlenght / 8 + 1;

    for (int i = 0; i < nbbytes; i++, startbyte++) {
        data[startbyte] |= input[i] >> (startbit);
        if (startbit) data[startbyte+1] |= input[i] << (8-startbit);       
    }
}

void TFAVenice_RF433T_Task(void *pvParameters)
{
    if (tfaGPIO<1) return;

    #define TFA_CHANNEL 0x00         // TFA is using channel 1 of 8 (0-7)
    uint8_t data[30]  = { 0 };
    uint8_t seq[7]    = { 0 };
    uint8_t preamb[2];
    uint8_t preamb0[2];
   
    // with F07TH, tThe preamble contains a total of 13 bits; the first 11 are ones, followed by a 01 sequence
    // TFA Venice used a similar scheme, just 1st preamble is different 
    preamb0[0] = 0x00; // 00000000 with TFAVenice
    preamb0[1] = 0x10; // 0001____
    preamb[0] = 0x3F; // 00111111
    preamb[1] = 0xFA; // 1111101_

    /* the sequence, 6 bytes + 2 bits 00
    Byte 0   Byte 1   Byte 2   Byte 3   Byte 4   Byte 5
    xxxxMMMM IIIIIIII BCCCTTTT TTTTTTTT HHHHHHHH MMMMMMMM
    - x: Unknown 0x04 on F007TH/F012TH
    - M: Model Number?, 0x05 on F007TH/F012TH/SwitchDocLabs F016TH
    - I: ID byte (8 bits), volatie, changes at power up,
    - B: Battery Low
    - C: Channel (3 bits 1-8) - F007TH set by Dip switch, F012TH soft setting
    - T: Temperature 12 bits - Fahrenheit * 10 + 400
    - H: Humidity (8 bits)
    - M: Message integrity check LFSR Digest-8, gen 0x98, key 0x3e, init 0x64 */

    unsigned int temp_raw = int(PMData.WaterTemp * 90.0 / 5) + 720;
    seq[0] = 0x46;           // 0x46 or 0x45 for F007TH/F012TH/F016TH, mine is 0x46
    seq[1] = 0x48;           // random id
    seq[2] = 0x00;           // all 0 so Battery=0 (OK), 
    seq[2] |= TFA_CHANNEL << 4;
    seq[2] |= (temp_raw & 0xF00) >> 8;
    seq[3] = temp_raw & 0xFF;
    seq[4] = 50;             // % humidity, not used by TFA Venice
    seq[5] = lfsr_digest8(seq, 5, 0x98, 0x3e) ^ 0x64;   // CRC
    seq[6] = 0x00;
  
    int bitpos = 0;
    #define _lenpreamb0_ 12 // F007TH uses 3 times the same preamb, TFAVenice uses a different 1st preamble.
    #define _lenpreamb_  15 // 13 for F007TH
    #define _lenseq_     50 // 48 for F007TH
    
    TFAVeniceFillData(data, preamb0, bitpos, _lenpreamb0_);
    bitpos += _lenpreamb0_;
    TFAVeniceFillData(data, seq,    bitpos, _lenseq_);
    bitpos += _lenseq_;
    TFAVeniceFillData(data, preamb, bitpos, _lenpreamb_);
    bitpos += _lenpreamb_;
    TFAVeniceFillData(data, seq,    bitpos, _lenseq_);
    bitpos += _lenseq_;
    TFAVeniceFillData(data, preamb, bitpos, _lenpreamb_);
    bitpos += _lenpreamb_;
    TFAVeniceFillData(data, seq,    bitpos, _lenseq_);
    bitpos += _lenseq_;
    RF433Tsend(data, bitpos); // bitpos = 192
}

void TFAVenice_RF433T_Values(char* buffer)
{
    if (tfaGPIO > 0)
        //sprintf(buffer, "GPIO=%d", tfaGPIO);
        sprintf(buffer, "F=433Mhz, G=%d T=%d", tfaGPIO, int(PMData.WaterTemp*100));
    else sprintf(buffer, "none");
}

void TFAVenicePubMQTT()
{
    // Hey MQTT
    return;
    DynamicJsonDocument root(1024);
    root["GPIO"] = tfaGPIO;
    root["Send"] = int(PMData.WaterTemp*100);
    char topic[48];
    const char *roottopic = PMConfig.get<const char*>(MQTT_TOPIC);
    if (strcmp(roottopic, "") == 0) return;
    sprintf(topic, "%s/%s", roottopic, myTFAVenice_RF433T.name);
    PublishTopic(topic, root);   
}

void TFAVenice_RF433T_LoadSettings(void *pvParameters)
{
    int newgpio = 0;
    char buffer[I2C_MAXMESSAGE+5] = {0};
    SuperVisor_Message("GET_TFA_VENICE", buffer);
    if (strcmp(buffer, "none")==0) return;
    if (strcmp(buffer, "")==0) newgpio = 0;
    else sscanf(buffer, "%d", &newgpio);
    if (tfaGPIO != newgpio) {
        tfaGPIO = newgpio;
        Init_RF433t();
  //      TFAVenicePubMQTT();
    }
}

ExtensionStruct TFAVenice_RF433T_Init(char* name, int defaultIO)
{
    // Init structure
    myTFAVenice_RF433T.name         = name;
    myTFAVenice_RF433T.detected     = true;
    myTFAVenice_RF433T.Task         = TFAVenice_RF433T_Task;
    myTFAVenice_RF433T.frequency    = 53000;     //  check and broadcast temperature every 53 secs (per TFA)
    myTFAVenice_RF433T.LoadSettings = TFAVenice_RF433T_LoadSettings;
    myTFAVenice_RF433T.SaveSettings = 0;
    myTFAVenice_RF433T.LoadMeasures = 0;
    myTFAVenice_RF433T.SaveMeasures = 0;
    myTFAVenice_RF433T.Values       = TFAVenice_RF433T_Values;
    myTFAVenice_RF433T.HistoryStats = 0;

    return myTFAVenice_RF433T;
}

#endif
