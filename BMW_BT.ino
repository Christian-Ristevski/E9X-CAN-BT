#include <SPI.h>

#include <mcp2515.h>

const bool DEBUG = true;

const int SPI_CS_PIN = 999;         // Define chip-select pin for MCP (CAN) SPI interface
const int INT_PIN = 999;

const uint32_t FILTER_MASK = 11111111111;   // Use filter to isolate singular specified ID
const uint32_t FILTER_ID = 0x1D6;           // Steering wheel buttons CAN-ID

const int musicControlParameters[3] = {0x07, 0x09, 0x0A};   // BM83 AVRCP music_control command parameters (PP, Next, Prev)

struct can_frame canMsg;
volatile bool interrupt = false;
MCP2515 mcp2515(SPI_CS_PIN);


// MCP interrupt service routine
void IRAM_ATTR mcpIRS() {
    interrupt = true;
}


// Send UART message to BM83
void canForward(uint8_t commandParameter) {
    uint8_t packet[6];    // Fixed packet length, only intended to send "Music_Control"
    packet[0] = 0xAA;     // Start word
    packet[1] = 0x00;     // MSB of packet length
    packet[2] = 0x02;     // LSB of packet length
    packet[3] = 0x04;     // Music Control OP-code
    packet[4] = commandParameter;
    
    // Calcualte checksum
    uint8_t CHKSUM = 1 + ~(packet[1] + packet[2] + packet[3] + packet[4]);    // Checksum is 2's complement of byte 1-4
    packet[5] = CHKSUM;

    Serial.write(packet, 6);
}


void setup() {
  Serial.begin(115200);
  while(DEBUG && !Serial);    // Awaits connection to computer with DEBUG set to true

  // Configure MCP2515 object
  mcp2515.reset();
  mcp2515.setBitrate(CAN_100KBPS, MCP_8MHZ);
  mcp2515.setListenOnlyMode();
  Serial.println("---- CAN Controller Initialized Successfully ----");

  // Enable MCP2515 filters
  mcp2515.setConfigMode();
  mcp2515.setFilterMask(MCP2515::MASK0, false, FILTER_MASK);
  mcp2515.setFilter(MCP2515::RXF0, false, FILTER_ID);
  mcp2515.setListenOnlyMode();
  Serial.println("---- CAN Filter Initialized Successfully ----");

  // Configure external interrupt pin
  pinMode(INT_PIN, INPUT);
  attachInterrupt(INT_PIN, mcpIRS, FALLING);
  Serial.println("---- CAN Interrupt attached Successfully ----");
}


void loop() {
    if (interrupt) {
        interrupt = false;

        uint8_t irq = mcp2515.getInterrupts();

        if (irq & MCP2515::CANINTF_RX0IF) {
            if (mcp2515.readMessage(MCP2515::RXB0, &canMsg) == MCP2515::ERROR_OK) {    // Reads RXB0 buffer, if OK, stores in "canMsg"
                
                for (int i = 0; i<canMsg.can_dlc; i++)  {  // print the data
                    Serial.print(canMsg.data[i],HEX);
                    Serial.print(" ");
                }

                uint16_t canWord = (canMsg.data[0] << 8) + canMsg.data[1];   // Adding together CAN data's MSB and LSB through 1 Byte bitshift

                switch (canWord) {
                    case 0xC80C:
                    case 0xC40C:
                    case 0xE00C:
                        canForward(musicControlParameters[1]);    // Next
                    case 0xD00C:
                        canForward(musicControlParameters[2]);    // Prev
                    case 0xC10C:
                        canForward(musicControlParameters[0]);    // Play/Pause toggle
                    case 0xC00D:
                    case 0xC01C:
                    case 0xC04C:
                    default:
                        break;
                }
            }
        }
    }
}
