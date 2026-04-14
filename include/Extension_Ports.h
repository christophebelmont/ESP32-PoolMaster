#ifndef _EXTENSIONS_H
#define _EXTENSIONS_H

#define _EXTENSIONS_

void  ExtensionsInit(void);
int   ExtensionsNb(void);
#define SANITYDELAY delay(50);
#define I2C_MAXMESSAGE     64
extern float rfu_sensor_value;    // PSI sensor current value
#endif
