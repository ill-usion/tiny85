#define TINY85_IMPLEMENTATION
#include "tiny85.h"

int main(void)
{
    Attiny85 mcu;
    tiny85_init(&mcu);
    bool success = tiny85_read_hex(&mcu, "./asm/main.hex");
    if (!success)
    {
        fprintf(stderr, "Failed to open and read hex file.\n");
        return 1;
    }

    printf("First instruction: %b\n", mcu.flash[0]);

    char i = 0;
    while (true) {
        scanf("%c", &i);
        if (i == 'q') break;
        tiny85_cycle(&mcu);   
    }

    // printf("const uint16_t bytecode[%d] = {", FLASH_SIZE);
    // for (int i = 0; i < FLASH_SIZE; i++)
    //     printf("0x%04X, ", mcu.flash[i]);

    return 0;
}

