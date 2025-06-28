
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
/* Ambient Weather F007TH
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

#include <Arduino.h>
#include "Config.h"
#include "PoolMaster.h"

#if defined(_EXTENSIONS_)
#include "Extension_TFAVenice_RF433T.h"
#include <driver/rmt.h>

ExtensionStruct myTFAVenice_RF433T = {0};

#define RMT_TX_CHANNEL  (RMT_CHANNEL_0)
//#define RMT_RX_CHANNEL  (RMT_CHANNEL_1)
//#define RTM_TX_GPIO_NUM (GPIO) 
//#define RTM_RX_GPIO_NUM (36)
#define RTM_BLOCK_NUM   (1)
#define RMT_CLK_DIV     (80) /*!< RMT counter clock divider */
#define RMT_1US_TICKS   (80000000 / RMT_CLK_DIV / 1000000)
#define RMT_1MS_TICKS   (RMT_1US_TICKS * 1000)

rmt_item32_t rmtbuff[2048];

#define RMT_CODE_H      {670, 1, 320, 0}
#define RMT_CODE_L      {348, 1, 642, 0}
#define RMT_START_CODE0 {4868, 1, 2469, 0}
#define RMT_START_CODE1 {1647, 1, 315, 0}

static void RF433Tsend(uint8_t* buff, size_t size)
{
 //   Debug.print(DBG_DEBUG, "[RF433Tsend] bits....");
 //   for (int i=0; i<size; i++)
 //        Debug.print(DBG_DEBUG, "[RF433Tsend]    bit[%d]=%x", i, buff[i]);
    rmtbuff[0] = (rmt_item32_t){RMT_START_CODE0};
    rmtbuff[1] = (rmt_item32_t){RMT_START_CODE1};
    for (int i = 0; i < size; i++) {
        uint8_t mark = 0x80;
        for (int n = 0; n < 8; n++) {
            rmtbuff[2 + i * 8 + n] = ((buff[i] & mark)) ? ((rmt_item32_t){RMT_CODE_H}) : ((rmt_item32_t){RMT_CODE_L});
            mark >>= 1;
        }
    }
    for (int i = 0; i < 8; i++) {
        ESP_ERROR_CHECK(rmt_write_items(RMT_TX_CHANNEL, rmtbuff, 42, false));
        ESP_ERROR_CHECK(rmt_wait_tx_done(RMT_TX_CHANNEL, portMAX_DELAY));
    }
}

static uint8_t lfsr_digest8(uint8_t const message[], unsigned bytes, uint8_t gen, uint8_t key)
{
    // From: https://github.com/merbanan/rtl_433/blob/master/src/util.c
    uint8_t sum = 0;
    for (unsigned k = 0; k < bytes; ++k) {
        uint8_t data = message[k];
        for (int i = 7; i >= 0; --i) {
            // fprintf(stderr, "key is %02x\n", key);
            // XOR key into sum if data bit is set
            if ((data >> i) & 1)
                sum ^= key;

            // roll the key right (actually the lsb is dropped here)
            // and apply the gen (needs to include the dropped lsb as msb)
            if (key & 1)
                key = (key >> 1) ^ gen;
            else
                key = (key >> 1);
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
            if (startbit)                 
                data[startbyte+1] |= input[i] << (8-startbit);       
    }
}

void TFAVenice_RF433T_Task(void *pvParameters)
{
    if (!myTFAVenice_RF433T.detected) return;

// debug
// #define TFA_CHANNEL 0x00         // TFA is using channel 1 of 8 (0-7)
#define TFA_CHANNEL 0x03
double watertemp = PMData.WaterTemp;
// debug
    
    uint8_t data[30]  = { 0 };// for (int i=0; i<30; i++) data[i] = 0x0;
    uint8_t seq[7]    = { 0 };//for (int i=0; i<6; i++) seq[i] = 0x0;
    uint8_t preamb2[2];
    uint8_t preamb1[2];
    // The preamble contains a total of 13 bits; the first 11 are ones, followed by a 01 sequence
    preamb1[0] = 0x00; // 00000000
    preamb1[1] = 0x10; // 00010000
    preamb2[0] = 0x3F; // 00111111
    preamb2[1] = 0xFA; // 11111010

    /* the sequence, 6 bytes
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

    unsigned int temp_raw = (((watertemp / 100.0 * 9 / 5) + 32) * 10 ) + 400;
    seq[0] = 0x46;             // 0x46 or 0x45 for F007TH/F012TH/F016TH, mine is 0x46
    seq[1] = 0x41;             // random id
    seq[2] = 0x00;             // all 0 so Battery=0 (OK), 
    seq[2] |= TFA_CHANNEL << 4;
    seq[2] |= (temp_raw & 0xF00) >> 8;
    seq[3] = temp_raw & 0xFF;
    seq[4] = 0x32;             // any, not used by TFA Venice
    seq[5] = lfsr_digest8(seq, 5, 0x98, 0x3e) ^ 0x64;
    seq[6] = 0x00;
  
    int bitpos = 0;
    #define _lenpreamb1_   12
    #define _lenpreamb2_   15
    #define _lenseq_       50
    TFAVeniceFillData(data, preamb1, bitpos, _lenpreamb1_);  bitpos += _lenpreamb1_;
    TFAVeniceFillData(data, seq,     bitpos, _lenseq_);      bitpos += _lenseq_;
    TFAVeniceFillData(data, preamb2, bitpos, _lenpreamb2_);  bitpos += _lenpreamb2_;
    TFAVeniceFillData(data, seq,     bitpos, _lenseq_);      bitpos += _lenseq_;
    TFAVeniceFillData(data, preamb2, bitpos, _lenpreamb2_);  bitpos += _lenpreamb2_;
    TFAVeniceFillData(data, seq,     bitpos, _lenseq_);
    
    RF433Tsend(data, 24);
}

ExtensionStruct TFAVenice_RF433T_Init(const char* name, int IO)
{
    // search and init the chip
    rmt_config_t txconfig;
    txconfig.rmt_mode                 = RMT_MODE_TX;
    txconfig.channel                  = RMT_TX_CHANNEL;
    txconfig.gpio_num                 = gpio_num_t(IO);
    txconfig.mem_block_num            = RTM_BLOCK_NUM;
    txconfig.tx_config.loop_en        = false;
    txconfig.tx_config.carrier_en     = false;
    txconfig.tx_config.idle_output_en = true;
    txconfig.tx_config.idle_level     = rmt_idle_level_t(0);
    txconfig.clk_div                  = RMT_CLK_DIV;
    if (rmt_config(&txconfig) == ESP_OK) myTFAVenice_RF433T.detected = true;
    if ((myTFAVenice_RF433T.detected) && (rmt_driver_install(txconfig.channel, 0, 0) != ESP_OK)) {
        myTFAVenice_RF433T.detected = false;
        Debug.print(DBG_INFO,"[TFAVenice_RF433TInit] no TFAVenice_RF433T detected");
    }

    // Init structure
    myTFAVenice_RF433T.name         = name;
    myTFAVenice_RF433T.Task         = TFAVenice_RF433T_Task;
    myTFAVenice_RF433T.frequency    = 1000;     //  check and broadcast temperature each minute   
    myTFAVenice_RF433T.LoadSettings = 0;
    myTFAVenice_RF433T.SaveSettings = 0;
    myTFAVenice_RF433T.LoadMeasures = 0;
    myTFAVenice_RF433T.SaveMeasures = 0;
    myTFAVenice_RF433T.HistoryStats = 0;

    return myTFAVenice_RF433T;
}

#endif

