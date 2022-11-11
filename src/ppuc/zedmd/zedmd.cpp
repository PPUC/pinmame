#include <stdio.h>
#include <chrono>
#include <thread>

#include "zedmd.h"
#include "../serialib/serialib.h"

// Serial object
serialib device;

UINT16 deviceWidth = 0;
UINT16 deviceHeight = 0;

UINT8 deviceOutputBuffer[8245] = {};
char planeBytes[4] = {0};

int ZeDmdInit() {
    static int ret = 0;
    char device_name[22];

    // To shake hands, PPUC must send 7 bytes {0x5a, 0x65, 0x64, 0x72, 0x75, 0x6d, 12}.
    // The ESP32 will answer {0x5a, 0x65, 0x64, 0x72, width_low, width_high, height_low, height_high}.
    // Width (=width_low+width_high * 256) and height (=height_low+height_high * 256) are the
    // dimensions of the LED panel (128 * 32 or 256 * 64).
    UINT8 handshake[7] = {0x5a, 0x65, 0x64, 0x72, 0x75, 0x6d, 12};
    UINT8 acknowledge[8] = {0};

    for (int i = 1; i < 99; i++) {

#if defined (_WIN32) || defined( _WIN64)
        // Prepare the port name (Windows).
        sprintf(device_name, "\\\\.\\COM%d", i);
#elif defined (__linux__)
        // Prepare the port name (Linux).
        sprintf(device_name, "/dev/ttyUSB%d", i - 1);
#else
        // Prepare the port name (macOS).
        sprintf(device_name, "/dev/cu.usbserial-%04d", i);
#endif

        // Try to connect to the device.
        if (device.openDevice(device_name, 921600) == 1) {
            // printf("Device %s\n", device_name);

            // Reset the device.
            device.clearDTR();
            device.setRTS();
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            device.clearRTS();
            device.clearDTR();
            std::this_thread::sleep_for(std::chrono::milliseconds(100));

            // The ESP32 sends some information about itself first. That needs to be removed before handshake.
            while (device.available() > 0) {
                device.flushReceiver();
                // @todo avoid endless loop in case of a different device that sends permanently.
            }

            if (device.writeBytes(handshake, 7) == 1) {
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
                if (device.readBytes(acknowledge, 8, 1000)) {
                    printf("Ack hex %d\n", acknowledge[0]);
                    printf("Ack hex %d\n", acknowledge[1]);
                    printf("Ack hex %d\n", acknowledge[2]);
                    printf("Ack hex %d\n", acknowledge[3]);
                    printf("Ack hex %d\n", acknowledge[4]);
                    printf("Ack hex %d\n", acknowledge[5]);
                    printf("Ack hex %d\n", acknowledge[6]);
                    printf("Ack hex %d\n", acknowledge[7]);
                    if (
                            acknowledge[0] == 0x5a &&
                            acknowledge[1] == 0x65 &&
                            acknowledge[2] == 0x64 &&
                            acknowledge[3] == 0x72
                            ) {
                        deviceWidth = acknowledge[4] + acknowledge[5] * 256;
                        deviceHeight = acknowledge[6] + acknowledge[7] * 256;

                        if (deviceWidth > 0 && deviceHeight > 0) {
                            return 1;
                        }
                    }
                }
            }
            // Close the device before testing the next port.
            device.closeDevice();
        }
    }

    return 0;
}

void ZeDmdRender(UINT16 width, UINT16 height, UINT8* Buffer, int bitDepth, bool samSpa) {
    if (width <= deviceWidth && height <= deviceHeight) {
        // To send a 4-color frame, send {0x72, 0x64, 0x65, 0x5a, 8} followed by 3 * 4 bytes for the palette (R, G, B)
        // followed by 2 planes of width * height / 8 bytes for the frame. It is possible to send a colored palette or
        // standard colors (orange (255,127,0) gradient).
        // To send a 16-color frame, send {0x72, 0x64, 0x65, 0x5a, 9} followed by 3 * 16 bytes for the palette (R, G, B)
        // followed by 4 planes of width * height / 8 bytes for the frame. Once again, if you want to use the standard
        // colors, send (orange (255,127,0) gradient).

        int outputBufferIndex = 4;
        int frameSizeInByte = width * height / 8;
        int bitShift = 0;

        deviceOutputBuffer[0] = 0x72;
        deviceOutputBuffer[1] = 0x64;
        deviceOutputBuffer[2] = 0x65;
        deviceOutputBuffer[3] = 0x5a;
        if (bitDepth == 2) {
            deviceOutputBuffer[4] = 8;
            // Palette
            deviceOutputBuffer[5] = 255;
            deviceOutputBuffer[6] = 127;
            deviceOutputBuffer[7] = 0;
            deviceOutputBuffer[8] = 192;
            deviceOutputBuffer[9] = 76;
            deviceOutputBuffer[10] = 0;
            deviceOutputBuffer[11] = 144;
            deviceOutputBuffer[12] = 34;
            deviceOutputBuffer[13] = 0;
            deviceOutputBuffer[14] = 75;
            deviceOutputBuffer[15] = 0;
            deviceOutputBuffer[16] = 0;
            outputBufferIndex = 17;
        } else {
            deviceOutputBuffer[4] = 9;
            // Palette
            deviceOutputBuffer[5] = 255;
            deviceOutputBuffer[6] = 127;
            deviceOutputBuffer[7] = 0;
            outputBufferIndex = 53;
        }

        for (int y = 0; y < height; y++) {
            for (int x = 0; x < width; x++) {
                if (bitDepth == 2) {
                    switch (Buffer[y * width + x]) {
                        case 0x14: // 20%
                            //Activate if you want to have the entire Display to glow, a kind of background color.
                            //byte0 |= (1 << bitShift);
                            break;
                        case 0x21: // 33%
                            planeBytes[0] |= (1 << bitShift);
                            break;
                        case 0x43: // 67%
                            planeBytes[1] |= (1 << bitShift);
                            break;
                        case 0x64: // 100%
                            planeBytes[0] |= (1 << bitShift);
                            planeBytes[1] |= (1 << bitShift);
                            break;
                    }
                } else if (bitDepth == 4) {
                    if (samSpa) {
                        switch(Buffer[y * width + x]) {
                            case 0x00:
                                break;
                            case 0x14:
                                planeBytes[0] |= (1 << bitShift);
                                break;
                            case 0x19:
                                planeBytes[1] |= (1 << bitShift);
                                break;
                            case 0x1E:
                                planeBytes[0] |= (1 << bitShift);
                                planeBytes[1] |= (1 << bitShift);
                                break;
                            case 0x23:
                                planeBytes[2] |= (1 << bitShift);
                                break;
                            case 0x28:
                                planeBytes[0] |= (1 << bitShift);
                                planeBytes[2] |= (1 << bitShift);
                                break;
                            case 0x2D:
                                planeBytes[1] |= (1 << bitShift);
                                planeBytes[2] |= (1 << bitShift);
                                break;
                            case 0x32:
                                planeBytes[0] |= (1 << bitShift);
                                planeBytes[1] |= (1 << bitShift);
                                planeBytes[2] |= (1 << bitShift);
                                break;
                            case 0x37:
                                planeBytes[3] |= (1 << bitShift);
                                break;
                            case 0x3C:
                                planeBytes[0] |= (1 << bitShift);
                                planeBytes[3] |= (1 << bitShift);
                                break;
                            case 0x41:
                                planeBytes[1] |= (1 << bitShift);
                                planeBytes[3] |= (1 << bitShift);
                                break;
                            case 0x46:
                                planeBytes[0] |= (1 << bitShift);
                                planeBytes[1] |= (1 << bitShift);
                                planeBytes[3] |= (1 << bitShift);
                                break;
                            case 0x4B:
                                planeBytes[2] |= (1 << bitShift);
                                planeBytes[3] |= (1 << bitShift);
                                break;
                            case 0x50:
                                planeBytes[0] |= (1 << bitShift);
                                planeBytes[2] |= (1 << bitShift);
                                planeBytes[3] |= (1 << bitShift);
                                break;
                            case 0x5A:
                                planeBytes[1] |= (1 << bitShift);
                                planeBytes[2] |= (1 << bitShift);
                                planeBytes[3] |= (1 << bitShift);
                                break;
                            case 0x64:
                                planeBytes[0] |= (1 << bitShift);
                                planeBytes[1] |= (1 << bitShift);
                                planeBytes[2] |= (1 << bitShift);
                                planeBytes[3] |= (1 << bitShift);
                                break;
                        }
                    } else {
                        switch (Buffer[y * width + x]) {
                            case 0x00:
                                break;
                            case 0x1E:
                                planeBytes[0] |= (1 << bitShift);
                                break;
                            case 0x23:
                                planeBytes[1] |= (1 << bitShift);
                                break;
                            case 0x28:
                                planeBytes[0] |= (1 << bitShift);
                                planeBytes[1] |= (1 << bitShift);
                                break;
                            case 0x2D:
                                planeBytes[2] |= (1 << bitShift);
                                break;
                            case 0x32:
                                planeBytes[0] |= (1 << bitShift);
                                planeBytes[2] |= (1 << bitShift);
                                break;
                            case 0x37:
                                planeBytes[1] |= (1 << bitShift);
                                planeBytes[2] |= (1 << bitShift);
                                break;
                            case 0x3C:
                                planeBytes[0] |= (1 << bitShift);
                                planeBytes[1] |= (1 << bitShift);
                                planeBytes[2] |= (1 << bitShift);
                                break;
                            case 0x41:
                                planeBytes[3] |= (1 << bitShift);
                                break;
                            case 0x46:
                                planeBytes[0] |= (1 << bitShift);
                                planeBytes[3] |= (1 << bitShift);
                                break;
                            case 0x4B:
                                planeBytes[1] |= (1 << bitShift);
                                planeBytes[3] |= (1 << bitShift);
                                break;
                            case 0x50:
                                planeBytes[0] |= (1 << bitShift);
                                planeBytes[1] |= (1 << bitShift);
                                planeBytes[3] |= (1 << bitShift);
                                break;
                            case 0x55:
                                planeBytes[2] |= (1 << bitShift);
                                planeBytes[3] |= (1 << bitShift);
                                break;
                            case 0x5A:
                                planeBytes[0] |= (1 << bitShift);
                                planeBytes[2] |= (1 << bitShift);
                                planeBytes[3] |= (1 << bitShift);
                                break;
                            case 0x5F:
                                planeBytes[1] |= (1 << bitShift);
                                planeBytes[2] |= (1 << bitShift);
                                planeBytes[3] |= (1 << bitShift);
                                break;
                            case 0x64:
                                planeBytes[0] |= (1 << bitShift);
                                planeBytes[1] |= (1 << bitShift);
                                planeBytes[2] |= (1 << bitShift);
                                planeBytes[3] |= (1 << bitShift);
                                break;
                        }
                    }
                }

                bitShift++;
                if (bitShift > 7) {
                    bitShift = 0;
                    for (int i = 0; i < bitDepth; i++) {
                        deviceOutputBuffer[(frameSizeInByte * i) + outputBufferIndex] = planeBytes[i];
                        planeBytes[i] = 0;
                    }
                    outputBufferIndex++;
                }
            }
        }

        device.writeBytes(deviceOutputBuffer, outputBufferIndex);
    }
}
