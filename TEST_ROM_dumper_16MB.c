#include <stdio.h>
#include <stdint.h>
#include <assert.h>

#define AD_BUS 2 
#define ALE_L 18 
#define ALE_H 19 
#define READ 20 
#define WRITE 21 
#define RESET 22

#define PI_INPUT 0
#define PI_OUTPUT 1

#define LOW 0
#define HIGH 1

#define ACTIVE(signal) (((signal) == HIGH) ? HIGH : LOW)
#define INACTIVE(signal) (((signal) == LOW) ? LOW : HIGH)

typedef unsigned int uint;
typedef uint16_t uint16;
typedef uint32_t uint32;
typedef uint64_t uint64;

enum addressBoundary 
{
  LowerAddress = 0, 
  UpperAddress = 1
};

// Mock GPIO variables for assertions
uint gpio_set_mode[24] = {0};
uint gpio_write[32] = {0};

// GPIO Mock functions
void mock_gpioSetMode(uint gpio, uint mode)
{
    gpio_set_mode[gpio] = mode;
    printf("gpioSetMode: gpio=%d, mode=%d\n", gpio, mode);
}
void mock_gpioWrite(uint gpio, uint level)
{
    gpio_write[gpio] = level;
    printf("gpioWrite: gpio=%d, level=%d\n", gpio, level);
}
int mock_gpioRead(uint gpio)
{
    printf("gpioRead: gpio=%d\n", gpio);
    return (gpio % 2); // Simulate alternating 0/1 values
}
void mock_gpioDelay(uint32 micros)
{
    printf("gpioDelay: %d µs\n", micros);
}

// Functions to test
void SetADBusPinsMode(uint mode) 
{
  for(uint pin = AD_BUS;
        pin < AD_BUS + 16;
        pin++)
  {
    mock_gpioSetMode(pin, mode);
  }
}

void SetAddress(uint32 address, uint addressBoundary)
{
  uint bitCount = (addressBoundary == UpperAddress) ? 8 : 16;
  uint bitShift = (addressBoundary == UpperAddress) ? 16 : 0;

  for(uint bitOffset = 0;
      bitOffset < bitCount;
      bitOffset++)
  {
    mock_gpioWrite(AD_BUS + bitOffset, (address >> (bitOffset + bitShift)) & 0x1);
  }
}

void LatchAddress(uint ControlSignal)
{
    mock_gpioWrite(ControlSignal, ACTIVE(HIGH)); // Activate latch
    mock_gpioDelay(1); // Allow latch signal to stabilize
    mock_gpioWrite(ControlSignal, INACTIVE(LOW)); // Deactivate latch
}

// Unit tests
void test_SetADBusPinsMode(void)
{
    printf("Testing SetADBusPinsMode...\n");

    SetADBusPinsMode(PI_INPUT);
    for(uint pin = AD_BUS;
        pin < AD_BUS + 16;
        pin++)
    {
        assert(gpio_set_mode[pin] == PI_INPUT);
    }

    SetADBusPinsMode(PI_OUTPUT);
    for(uint pin = AD_BUS;
        pin < AD_BUS + 16;
        pin++)
    {
        assert(gpio_set_mode[pin] == PI_OUTPUT);
    }
    printf("SetADBusPinsMode passed.\n\n");
}

void test_SetAddress(void)
{
    printf("Testing SetAddress...\n");

    uint32 address = 0x123456;

    SetAddress(address, LowerAddress);
    for(uint bitOffset = 0; 
        bitOffset < 16; 
        bitOffset++)
    {
        uint expectedBit = (address >> bitOffset) & 0x1;
        assert(gpio_write[AD_BUS + bitOffset] == expectedBit);
    }

    SetAddress(address, UpperAddress);
    for(uint bitOffset = 0;
        bitOffset < 8;
        bitOffset++)
    {
        uint expectedBit = ((address >> (16 + bitOffset)) & 0x1);
        assert(gpio_write[AD_BUS + bitOffset] == (expectedBit));
    }

    printf("SetAddress passed.\n\n");
}

void test_LatchAddress(void)
{
    printf("Testing LatchAddress...\n");

    LatchAddress(ALE_L);
    assert(gpio_write[ALE_L] == INACTIVE(LOW));

    LatchAddress(ALE_H);
    assert(gpio_write[ALE_H] == INACTIVE(LOW));

    printf("LatchAddress passed.\n\n");
}

void test_MainLoop(void)
{
    printf("Testing main ROM dumping loop...\n");

    uint32 address = 0;
    while(address < 0x10) // Testing first few addresses only
    {
        SetAddress(address, LowerAddress);
        LatchAddress(ALE_L);
        SetAddress(address, UpperAddress);
        LatchAddress(ALE_H);

        SetADBusPinsMode(PI_INPUT);

        mock_gpioWrite(READ, ACTIVE(LOW)); 
        mock_gpioDelay(1); 

        uint16 data = 0;
        for(uint bitOffset = 0;
            bitOffset < 16;
            bitOffset++)
        {
            data |= (mock_gpioRead(AD_BUS + bitOffset) << bitOffset);
        }

        for(uint bitOffset = 0; 
            bitOffset < 16; 
            bitOffset++)
        {
            assert(((data >> bitOffset) & 0x1) == (AD_BUS + bitOffset) % 2);
        }

        assert(data <= 0xFFFF); // Assert data is 16-bit value

        mock_gpioWrite(READ, INACTIVE(HIGH));

        SetADBusPinsMode(PI_OUTPUT);

        printf("0x%06X: 0x%04X\n", address, data);

        address += 2;
    }

    printf("Main loop test passed.\n");
}

int main(void)
{
    freopen("OUTPUT_ROM_dumper_16MB.txt", "w", stdout);

    test_SetADBusPinsMode();
    test_SetAddress();
    test_LatchAddress();
    test_MainLoop();

    printf("All tests passed.\n");

    fclose(stdout);

    return 0;
}