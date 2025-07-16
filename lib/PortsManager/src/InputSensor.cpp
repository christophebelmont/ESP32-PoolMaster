#include "Arduino.h"
#include "InputSensor.h"


void InputSensor::begin() {
    localMutex = portMUX_INITIALIZER_UNLOCKED;

    localTimerHandle = xTimerCreate("ReadTimer", pdMS_TO_TICKS(readIntervalMs), pdTRUE, this, timerCallbackStatic);
    if (localTimerHandle != nullptr) {
        xTimerStart(localTimerHandle, 0);
    }

    xTaskCreatePinnedToCore(taskDetectStatic, "DetectionTask", 2048, this, 1, &localTaskHandle, 1);
}

bool InputSensor::getState() {
    return currentState;
}

//Function overiden for derived class hierarchie but they do nothing
void InputSensor::SetOperationMode(bool) {}
bool InputSensor::GetOperationMode(void) {return false;}
void InputSensor::SetTankLevelPIN(uint8_t) {}
void InputSensor::SetTankFill(double) {}
void InputSensor::SetTankVolume(double) {}
void InputSensor::SetFlowRate(double) {}
void InputSensor::SetMaxUpTime(unsigned long ) {}
double InputSensor::GetTankFill() {return 0.0;}
void InputSensor::ResetUpTime() {}
void InputSensor::SetInterlock(PIN*) {}
void InputSensor::SetInterlock(uint8_t) {}
uint8_t InputSensor::GetInterlockId(void) {return 0;}
bool InputSensor::IsRelay(void) {return false;}

// --- Advanced Detection ---

void InputSensor::timerCallbackStatic(TimerHandle_t xTimer) {
    InputSensor* self = static_cast<InputSensor*>(pvTimerGetTimerID(xTimer));
    self->timerCallback();
}

void InputSensor::timerCallback() {
    if (digitalRead(GetPinNumber()) == GetActiveLevel()) {
        unsigned long maintenant = millis();
        portENTER_CRITICAL_ISR(&localMutex);
        tableDetections[tableHead] = maintenant;
        tableHead = (tableHead + 1) % HISTO_SIZE;
        if (tableTotal < HISTO_SIZE) tableTotal++;
        portEXIT_CRITICAL_ISR(&localMutex);
    }
}

void InputSensor::taskDetectStatic(void* param) {
    InputSensor* self = static_cast<InputSensor*>(param);
    self->taskDetection();
}


void InputSensor::taskDetection() {
    while (true) {
        int count = 0;
        unsigned long maintenant = millis();

        portENTER_CRITICAL(&localMutex);
        int headLocal = tableHead;
        int totalLocal = tableTotal;
        for (int i = 0; i < totalLocal; i++) {
            int idx = (headLocal + HISTO_SIZE - i - 1) % HISTO_SIZE;
            if (maintenant - tableDetections[idx] <= timeWindow) {
                count++;
                if (count >= highThreshold) break;
            } else {
                break;
            }
        }
        portEXIT_CRITICAL(&localMutex);

        // Check if the state has changed
        // and call the appropriate handler
        bool newState = (count >= highThreshold);
        if (newState != currentState) {
            currentState = newState;
            if (currentState && onStartHandler) onStartHandler();
            else if (!currentState && onStopHandler) onStopHandler();
        }

        vTaskDelay(pdMS_TO_TICKS(analysisFrequency));
    }
}
