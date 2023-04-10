#include "esphome.h"

#define STATUS_START 0x01
#define STATUS_LENGTH 0x02
#define STATUS_CMD 0x03
#define STATUS_DATA 0x04
#define STATUS_CHECKSUM 0x05

#define HEADER_REQUEST 0x11
#define HEADER_RESPONSE 0x16

class ZPHS01 : public Component, public UARTDevice {
 public:
    ZPHS01(UARTComponent *parent) : UARTDevice(parent) {}
    
    void setup() override {}
    
    u_char read_status = STATUS_START;
    u_char buffer[20];
    u_char read_len = 0;
    u_char read_cmd = 0;
    u_char read_pos = 0;

    Sensor *sensor_co2 = new Sensor();
    Sensor *sensor_voc = new Sensor();
    Sensor *sensor_humidity = new Sensor();
    Sensor *sensor_temperature = new Sensor();
    Sensor *sensor_pm25 = new Sensor();

    bool readline(u_char readch) {
        switch (read_status) { 
            case STATUS_START:
                if(readch == HEADER_RESPONSE){
                    read_status = STATUS_LENGTH;
                    ESP_LOGD("custom", "Status: %i", read_status);
                }
                break;
            case STATUS_LENGTH:
                read_status = STATUS_CMD;
                ESP_LOGD("custom", "Status: %i", read_status);
                read_len = readch;
                break;
            case STATUS_CMD: // pre end
                read_status = STATUS_DATA;
                ESP_LOGD("custom", "Status: %i", read_status);
                read_cmd = readch;
                read_pos = 0;
                break;
            case STATUS_DATA: // end
                if(read_len > read_pos+1){
                    buffer[read_pos++] = readch;
                }
                if(read_len==read_pos+1){
                    read_status = STATUS_CHECKSUM;
                    ESP_LOGD("custom", "Status: %i\n", read_status);
                }
                break;
            case STATUS_CHECKSUM:
                read_status = STATUS_START;
                ESP_LOGD("custom", "Status: %i, %X-%X", read_status, checksumm(), readch);
                flush();
                return checksumm() == readch;
        }
        // No end of line has been found, so return -1.
        return false;
    }

    u_char checksumm(){
        u_char tempq = 0;

        tempq += HEADER_RESPONSE;
        tempq += read_len;
        tempq += read_cmd;

        for(int i=0; i<read_len-1; i++){
            tempq += buffer[i];
        }

        tempq = (~tempq) + 1;
        return tempq;
    }

    bool started = false;


    void loop() override {
        if(!started){
            write_array({0x11,0x03,0x0C,0x02,0x1E,0xC0});
            write_array({0x11,0x02,0x01,0x00,0xEC});
            started = true;
        }
        
        while (available()) {
            if(readline(read())){
                if(read_cmd==0x01){
                    sensor_co2->publish_state(buffer[0]*256+buffer[1]);
                    sensor_voc->publish_state(buffer[2]*256+buffer[3]);
                    sensor_humidity->publish_state((buffer[4]*256+buffer[5])/10);
                    sensor_temperature->publish_state((buffer[6]*256+buffer[7]-500)/10);
                    sensor_pm25->publish_state(buffer[8]*256+buffer[9]);
                }
                if(read_cmd==0x0C){
                    if(buffer[0]==0x01){
                        id(dust_measurement_switch).publish_state(false);
                    } else if(buffer[0]==0x02){
                        id(dust_measurement_switch).publish_state(true);
                    }
                }
            }
        }
    }
};