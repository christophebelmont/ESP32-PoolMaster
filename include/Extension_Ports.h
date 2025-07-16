#ifndef _EXTENSIONS_H
#define _EXTENSIONS_H

#define _EXTENSIONS_

typedef void (*function) (void*);

struct ExtensionStruct {
    const char* name;
    uint        frequency;
    bool        detected;
    function    Task;
    function    LoadSettings;
    function    SaveSettings;
    function    LoadMeasures;
    function    SaveMeasures;
    function    HistoryStats;
    char*       MQTTTopicSettings;
    char*       MQTTTopicMeasures;
};

typedef ExtensionStruct (*initfunction) (const char*, int);

void  ExtensionsInit(void);
void  ExtensionsLoadSettings(void*);
//void  ExtensionsSaveSettings(void*);
//void  ExtensionsPublishSettings(void*);
//void  ExtensionsPublishMeasures(void*);
//void  ExtensionsHistoryStats(void*);
int   ExtensionsNb(void);
char* ExtensionsCreateMQTTTopic(const char*,const char*);
//extern uint8_t StatusLEDs;
#define SANITYDELAY delay(50);
 
#endif

