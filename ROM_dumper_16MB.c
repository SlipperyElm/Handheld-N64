/*  
    Raspberry Pi GPIO to N64 Cartridge Pinout with Resistors
    ---------------------------------------------------------
    Pin Name   | N64 Cartridge Pin | Raspberry Pi Pin | Resistor Type
    ---------------------------------------------------------

                        *** Address / data bus ***
    AD0        | 28                | GPIO2            | 10kΩ Pull-Down
    AD1        | 29                | GPIO3            | 10kΩ Pull-Down
    AD2        | 30                | GPIO4            | 10kΩ Pull-Down
    AD3        | 32                | GPIO5            | 10kΩ Pull-Down
    AD4        | 36                | GPIO6            | 10kΩ Pull-Down
    AD5        | 37                | GPIO7            | 10kΩ Pull-Down
    AD6        | 40                | GPIO8            | 10kΩ Pull-Down
    AD7        | 41                | GPIO9            | 10kΩ Pull-Down
    AD8        | 16                | GPIO10           | 10kΩ Pull-Down
    AD9        | 15                | GPIO11           | 10kΩ Pull-Down
    AD10       | 12                | GPIO12           | 10kΩ Pull-Down
    AD11       | 11                | GPIO13           | 10kΩ Pull-Down
    AD12       | 7                 | GPIO14           | 10kΩ Pull-Down
    AD13       | 5                 | GPIO15           | 10kΩ Pull-Down
    AD14       | 4                 | GPIO16           | 10kΩ Pull-Down
    AD15       | 3                 | GPIO17           | 10kΩ Pull-Down

                        *** Control signals ***
    ALE_L      | 33                | GPIO18           | 10kΩ Pull-Up (Latch low address bits)
    ALE_H      | 35                | GPIO19           | 10kΩ Pull-Up (Latch high address bits)
    RD         | 10                | GPIO20           | 10kΩ Pull-Up (Active Low)
    WR         | 8                 | GPIO21           | 10kΩ Pull-Up (Active Low)
    RESET      | 20                | GPIO22           | 10kΩ Pull-Up (Active Low)

                        *** Power and ground ***
    VCC (3.3V) | 9, 17, 34, 42     | 3.3V Rail        | N/A
    GND        | 1, 2, 6, 22, etc. | GND Rail         | N/A

    ---------------------------------------------------------
*/

/* 
    *** TODO ***
    * Error handling
    * Map pin #defines to raspberry pi breadboard layout
    * Implement Bank-switching/Memory mapping to read > 16Mb
    * Dynamic cartridge addressing to dump valid memory only
*/

#include <stdio.h>
#include <pigpio.h>

// GPIO pins
#define AD_BUS 2 
#define ALE_L 18 
#define ALE_H 19 
#define READ 20 
#define WRITE 21 
#define RESET 22

#define LOW 0
#define HIGH 1

#define MAX_ROM_SIZE 0x4000000 // 64 Mb
#define ROM_BANK_SIZE 0x1000000 // 16 Mb

#define ACTIVE(signal) (((signal) == HIGH) ? HIGH : LOW)
#define INACTIVE(signal) (((signal) == LOW) ? LOW : HIGH)

enum addressBoundary 
{
  LowerAddress = 0, 
  UpperAddress = 1
};

int main(void)
{
    if(gpioInitialise() < 0)
    {
         fprintf(stderr, "Failed to initialize GPIO.")
         return 1;
    }

    // Pin setup
    // Set mode for addressing
    SetADBusPinsMode(PI_OUTPUT);
    gpioSetMode(ALE_L, PI_OUTPUT);
    gpioSetMode(ALE_H, PI_OUTPUT);
    gpioSetMode(READ, PI_OUTPUT);
    gpioSetMode(WRITE, PI_OUTPUT);
    gpioSetMode(RESET, PI_OUTPUT);

    // Setup writes for inactive control signals
    gpioWrite(ALE_L, INACTIVE(LOW));
    gpioWrite(ALE_H, INACTIVE(LOW));
    gpioWrite(READ, INACTIVE(HIGH));
    gpioWrite(WRITE, INACTIVE(HIGH));
    gpioWrite(RESET, INACTIVE(LOW));
    
    gpioDelay(100);

    // Loop through ROM addresses in 16-bit increments, up to the ROM bank size.
    uint32_t address = 0;
    while(address < ROM_BANK_SIZE)
    {
        // Set the lower 16 bits of the address on the multiplexed AD_BUS
        SetAddress(address, LowerAddress);
        LatchAddress(ALE_L);
        
        // Set the upper 8 bits of the address
        SetAddress(address, UpperAddress);
        LatchAddress(ALE_H);

        // Configure the AD_BUS for data reading 
        SetADBusPinsMode(PI_INPUT);

        // Activate read control signal
        gpioWrite(READ, ACTIVE(LOW)); 
        gpioDelay(1); 

        // Read data into AD Bus
        uint16_t data = 0;
        for(uint bitOffset = 0;
            bitOffset < 16;
            bitOffset++)
        {
            data |= (gpioRead(AD_BUS + bitOffset) << bitOffset);
        }

        gpioWrite(READ, INACTIVE(HIGH));

        SetADBusPinsMode(PI_OUTPUT);

        printf("0x%06X: 0x%04X\n", address, data);

        // Increment address on 16-bit boundaries
        address += 2;
    }

    gpioTerminate();
    return 0;
}

// Set mode to PI_OUTPUT for address set
// Set mode to PI_INPUT for data read
void SetADBusPinsMode(uint mode) 
{
  for(uint pin = AD_BUS;
        pin < AD_BUS + 16;
        pin++)
  {
    gpioSetMode(pin, mode);
  }
}

// Sets the multiplexed address bus (AD_BUS) for either lower or upper address bits.
// - address: The 24-bit address to send to the ROM.
// - addressBoundary: Specifies whether to set the lower (16 bits) or upper (8 bits) address.
//    * LowerAddress (0): Sets AD_BUS pins 0-15 to the lower 16 bits of the address.
//    * UpperAddress (1): Sets AD_BUS pins 0-7 to the upper 8 bits of the address.
void SetAddress(uint64_t address, uint addressBoundary)
{
  uint bitCount = (addressBoundary == UpperAddress) ? 8 : 16;
  uint bitShift = (addressBoundary == UpperAddress) ? 16 : 0;

  for(uint bitOffset = 0;
      bitOffset < bitCount;
      bitOffset++)
  {
    gpioWrite(AD_BUS + bitOffset, (address >> (bitOffset + bitShift)) & 0x1);
  }
}

// Pulses the specified latch control signal (ALE_L or ALE_H) to store address bits in the ROM.
// - controlSignal: The pin controlling the latch signal for either lower or upper address bits.
void LatchAddress(uint ControlSignal)
{
    gpioWrite(ControlSignal, ACTIVE(HIGH)); // Activate latch
    gpioDelay(1); // Allow latch signal to stabilize
    gpioWrite(ControlSignal, INACTIVE(LOW)); // Deactivate latch
}