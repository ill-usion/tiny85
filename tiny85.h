#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>


// 32 Registers
#define REGISTERS_START 0x0000
#define REGISTERS_END   0x001F
// 64 I/O Registers
#define IO_START        0x0020
#define IO_END          0x005F
// Internal SRAM
#define SRAM_START      0x0060
#define SRAM_END        0x025F

#define MEMORY_SIZE     607
#define FLASH_SIZE      0x0FFF
#define EEPROM_SIZE     512

#define BITN(x, n) ((x >> n) & 1) 
#define print_sreg(m) printf("C: %u, Z: %u, N: %u, V: %u, S: %u, H: %u, T: %u, I: %u\n", \
        m->sreg.flags.C, m->sreg.flags.Z, m->sreg.flags.N, m->sreg.flags.V, m->sreg.flags.S, m->sreg.flags.H, \
        m->sreg.flags.T, m->sreg.flags.I)

typedef union {
    struct {
        uint8_t C : 1;
        uint8_t Z : 1;
        uint8_t N : 1;
        uint8_t V : 1;
        uint8_t S : 1;
        uint8_t H : 1;
        uint8_t T : 1;
        uint8_t I : 1;
    } flags;
    uint8_t value;
} SREG;

typedef struct {
    // Registers + Memory-mapped I/O + SRAM
    uint8_t memory[MEMORY_SIZE];
    // Non-volatile memory (EEPROM)
    uint8_t eeprom[EEPROM_SIZE];
    // Flash memory
    uint16_t flash[FLASH_SIZE];
    // Program counter
    uint16_t pc;
    // Stack pointer
    uint16_t sp;
    // Registers r0-r31
    uint8_t* reg;
    // IO registers
    uint8_t* io_reg;
    // Status register
    SREG sreg;
} Attiny85;

typedef struct {
    // Fixed bits denoted with 1, variables with 0
    uint16_t fixedbits;
    uint16_t opcode;
    uint8_t cycles;
    void (*funcptr)(Attiny85* mcu, uint16_t op);
} Instruction; 

void tiny85_init(Attiny85* mcu);
bool tiny85_read_hex(Attiny85* mcu, const char* filename);
void tiny85_cycle(Attiny85* mcu);
inline void tiny85_stack_push_u8(Attiny85* mcu, uint8_t c);
inline void tiny85_stack_push_u16(Attiny85* mcu, uint16_t c);
inline uint8_t tiny85_stack_pop_u8(Attiny85* mcu);
inline uint16_t tiny85_stack_pop_u16(Attiny85* mcu);

static void RJMP(Attiny85* mcu, uint16_t op);
static void LDI(Attiny85* mcu, uint16_t op);
static void MOV(Attiny85* mcu, uint16_t op);
static void MOVW(Attiny85* mcu, uint16_t op);
static void OUT(Attiny85* mcu, uint16_t op);
static void SBI(Attiny85* mcu, uint16_t op);
static void CBI(Attiny85* mcu, uint16_t op);
static void RCALL(Attiny85* mcu, uint16_t op);
static void DEC(Attiny85* mcu, uint16_t op);
static void BRNE(Attiny85* mcu, uint16_t op);
static void RET(Attiny85* mcu, uint16_t op);
static void ADD(Attiny85* mcu, uint16_t op);
static void ADC(Attiny85* mcu, uint16_t op);
static void ADIW(Attiny85* mcu, uint16_t op);
static void ANDI(Attiny85* mcu, uint16_t op);
static void AND(Attiny85* mcu, uint16_t op);
static void SBIW(Attiny85* mcu, uint16_t op);
static void ORI(Attiny85* mcu, uint16_t op);
static void COM(Attiny85* mcu, uint16_t op);
static void NEG(Attiny85* mcu, uint16_t op);
static void NOP(Attiny85* mcu, uint16_t op);

static Instruction opcodes[] = {
    {0b1111000000000000, 0b1100000000000000, 2, RJMP},
    {0b1111000000000000, 0b1110000000000000, 1, LDI},
    {0b1111110000000000, 0b0010110000000000, 1, MOV},
    {0b1111111100000000, 0b0000000100000000, 1, MOVW},
    {0b1111100000000000, 0b1011100000000000, 1, OUT},
    {0b1111111100000000, 0b1001101000000000, 2, SBI},
    {0b1111111100000000, 0b1001100000000000, 2, CBI},
    {0b1111000000000000, 0b1101000000000000, 2, RCALL},
    {0b1111111000001111, 0b1001010000001010, 1, DEC},
    {0b1111110000000111, 0b1111010000000001, 2, BRNE}, // NOTE: could be 1 clock cycles 
    {0b1111111111111111, 0b1001010100001000, 1, RET},
    {0b1111110000000000, 0b0000110000000000, 1, ADD}, 
    {0b1111110000000000, 0b0001110000000000, 1, ADC},
    {0b1111111100000000, 0b1001011000000000, 2, ADIW},
    {0b1111000000000000, 0b0111000000000000, 1, ANDI},
    {0b1111110000000000, 0b0010000000000000, 1, AND},
    {0b1111111100000000, 0b1001011100000000, 2, SBIW},
    {0b1111000000000000, 0b0110000000000000, 1, ORI},
    {0b1111111000001111, 0b1001010000000000, 1, COM},
    {0b1111111000001111, 0b1001010000000001, 1, NEG},
    {0b1111111111111111, 0b0000000000000000, 1, NOP}
};

#ifdef TINY85_IMPLEMENTATION

void tiny85_init(Attiny85* mcu)
{
    mcu->pc = 0;
    mcu->sp = SRAM_END;
    memset(mcu->memory, 0, MEMORY_SIZE * sizeof(uint8_t));
    memset(mcu->eeprom, 0, EEPROM_SIZE * sizeof(uint8_t));
    memset(mcu->flash, 0, FLASH_SIZE * sizeof(uint16_t));

    mcu->sreg.value = 0;
    mcu->reg = mcu->memory + REGISTERS_START;
    mcu->io_reg = mcu->memory + IO_START;
}

bool tiny85_read_hex(Attiny85* mcu, const char* filename)
{
    FILE* file = fopen(filename, "r");
    if (file == NULL)
        return false;

    const size_t BUF_SIZE = 1024;
    char* linebuf = (char*)malloc(BUF_SIZE * sizeof(char));
    uint16_t flash_idx = 0; 
    while(fgets(linebuf, BUF_SIZE, file))
    {
        // +1 skip colon
        char* databuf = linebuf + 1;
        
        char size_chars[3];
        sscanf(databuf, "%2s", size_chars);
        int linesize = strtol(size_chars, 0, 16);
        
        // Skip length + offset
        databuf += 2 + 4;
        char record_type[3];
        sscanf(databuf, "%2s", record_type);
        // End of file
        if (strncmp(record_type, "01", 2) == 0)
            break;
        // Ignore line if it's other than data
        else if (strncmp(record_type, "00", 2) != 0)
            continue;

        // Skip record type
        databuf += 2;

        for (int i = 0; i < linesize * 2; i += 4)
        {
            char wordl[3], wordh[5];
            sscanf(databuf + i, "%2s%2s", wordl, wordh);
            strcat(wordh, wordl);

            uint16_t word = strtol(wordh, 0, 16);
            mcu->flash[flash_idx++] = word;
            printf("%hu) %b\n", flash_idx, word);
        }
    }
    
    fclose(file);
    free(linebuf);
    
    return true;
}

void tiny85_cycle(Attiny85* mcu)
{
    if (mcu->pc + 1 >= FLASH_SIZE) {
        printf("Reached end of flash.\n");
        return;
    }

    uint16_t word = mcu->flash[mcu->pc];
    for (int i = 0; i < sizeof(opcodes) / sizeof(opcodes[0]); i++)
    {
        Instruction* inst = &opcodes[i];
        if ((inst->fixedbits & word) == inst->opcode)
        {
            inst->funcptr(mcu, word);
            break;
        }
    }
}

inline void tiny85_stack_push_u8(Attiny85* mcu, uint8_t c)
{
    mcu->memory[--mcu->sp] = c;
}

inline void tiny85_stack_push_u16(Attiny85* mcu, uint16_t c)
{
    tiny85_stack_push_u8(mcu, (c >> 8) & 0xFF);
    tiny85_stack_push_u8(mcu, (c & 0xFF));
}

inline uint8_t tiny85_stack_pop_u8(Attiny85* mcu)
{
    return mcu->memory[mcu->sp++];
}

inline uint16_t tiny85_stack_pop_u16(Attiny85* mcu)
{
    uint8_t l = tiny85_stack_pop_u8(mcu);
    uint8_t h = tiny85_stack_pop_u8(mcu);

    return (h << 8) | l;
}

// == Instruction implementations ==

static void RJMP(Attiny85* mcu, uint16_t op)
{
    printf("[INST] %s\n", __func__);
    
    uint16_t offset = (op & 0xFFF);
    mcu->pc += offset + 1;
}

static void LDI(Attiny85* mcu, uint16_t op)
{
    printf("[INST] %s\n", __func__);

    uint8_t nibh = ((op & 0x0F00) >> 4) & 0xF0;
    uint8_t nibl = (op & 0x000F);
    uint8_t c = nibh | nibl;
    uint8_t reg = (op & 0x00F0) >> 4;
    reg += 0x10; // To immediate-compatible register

    mcu->reg[reg] = c;
    mcu->pc += 1;

    printf("Load immediate 0x%02X to reg 0x%02X\n", c, reg);
}

static void MOV(Attiny85* mcu, uint16_t op)
{
    printf("[INST] %s\n", __func__);
    
    uint8_t rr = ((op >> 5) & 0x10) | (op & 0xF);
    uint8_t rd = (op >> 4) & 0x1F;

    mcu->reg[rd] = mcu->reg[rr];
    printf("Rd: 0x%02X, Rr: 0x%02X\n", rd, rr);
    mcu->pc++;
}

static void MOVW(Attiny85* mcu, uint16_t op)
{
    printf("[INST] %s\n", __func__);
    uint8_t rr = op & 0xF;
    uint8_t rd = op & 0xF0;
    
    mcu->reg[rd] = mcu->reg[rr];
    mcu->reg[rd + 1] = mcu->reg[rr + 1];

    printf("Rd: 0x%02X:0x%02X, Rr: 0x%02X:0x%02X\n", rd+ 1, rd, rr + 1, rr);
    mcu->pc++;
}

static void OUT(Attiny85* mcu, uint16_t op)
{
    printf("[INST] %s\n", __func__);

    uint8_t rd = ((op & 0x600) >> 5) | (op & 0xF);  
    uint8_t rr = ((op & 0x1F0) >> 4);
    printf("Source reg: 0x%2X. Destination reg: 0x%2X\n", rr, rd);
    
    mcu->io_reg[rd] = mcu->reg[rr];
    mcu->pc += 1;
}

static void SBI(Attiny85* mcu, uint16_t op)
{
    printf("[INST] %s\n", __func__);

    uint8_t bit = (op & 0x7);
    uint8_t reg = ((op & 0xF8) >> 3);
    
    printf("IO Reg: 0x%2X. Bit: %hhu\n", reg, bit);
    mcu->io_reg[reg] |= (1 << bit);
    mcu->pc += 1;
}

static void CBI(Attiny85* mcu, uint16_t op)
{
    printf("[INST] %s\n", __func__);

    uint8_t bit = (op & 0x7);
    uint8_t reg = ((op & 0xF8) >> 3);
    
    printf("IO Reg: 0x%2X. Bit: %hhu\n", reg, bit);
    mcu->io_reg[reg] &= ~(1 << bit);
    mcu->pc += 1;
}

static void RCALL(Attiny85* mcu, uint16_t op)
{
    printf("[INST] %s\n", __func__);
    
    tiny85_stack_push_u16(mcu, mcu->pc + 1);
    printf("Pushed pc to stack: 0x%04X\n", mcu->pc);
    uint16_t offset = (op & 0xFFF);
    
    mcu->pc += offset + 1;

    printf("PC + offset + 1: 0x%04X\n", mcu->pc);
}

static void DEC(Attiny85* mcu, uint16_t op)
{
    printf("[INST] %s\n", __func__);
    
    uint8_t reg = ((op & 0x01F0) >> 4) & 0xFF; 
    uint8_t v = --mcu->reg[reg];
    mcu->sreg.flags.V = !BITN(v, 7) && BITN(v, 6) && BITN(v, 5) && BITN(v, 4)
                            && BITN(v, 3) && BITN(v, 2) && BITN(v, 1) && BITN(v, 0);
    mcu->sreg.flags.N = !!BITN(v, 7);
    mcu->sreg.flags.Z = v == 0;
    mcu->sreg.flags.S = mcu->sreg.flags.N ^ mcu->sreg.flags.V;
    printf("Decrement reg 0x%02X: 0x%02X\n", reg, mcu->reg[reg]);
    print_sreg(mcu);
    mcu->pc += 1;
}

static void BRNE(Attiny85* mcu, uint16_t op)
{
    printf("[INST] %s\n", __func__);
    
    if (!mcu->sreg.flags.Z) {
        int8_t offset = (op >> 3) & 0x7F;
        if (offset & 0x40)
            offset |= 0x80;
        
        mcu->pc += offset + 1;

        printf("Branching with offset of %i\n", offset);
        return;
    }

    mcu->pc += 1;
    printf("Not branching\n");
}

static void RET(Attiny85* mcu, uint16_t op)
{
    printf("[INST] %s\n", __func__);
    uint16_t pc = tiny85_stack_pop_u16(mcu);
    mcu->pc = pc;

    printf("Popped pc from stack: 0x%04X\n", mcu->pc);
}

static void ADD(Attiny85* mcu, uint16_t op)
{
    printf("[INST] %s\n", __func__);
    
    uint8_t rr = ((op >> 5) & 0x10) | (op & 0xF);
    uint8_t rd = ((op >> 4) & 0x1F);

    uint8_t result = mcu->reg[rd] + mcu->reg[rr];

    mcu->sreg.flags.H = (BITN(mcu->reg[rd], 3) && BITN(mcu->reg[rr], 3))
        || (BITN(mcu->reg[rr], 3) && !BITN(result, 3))
        || (!BITN(result, 3) && BITN(mcu->reg[rd], 3));
    mcu->sreg.flags.V = (BITN(mcu->reg[rd], 7) && BITN(mcu->reg[rr], 7) && !BITN(result, 7))
        || (!BITN(mcu->reg[rd], 7) && !BITN(mcu->reg[rr], 7) && BITN(result, 7)); 
    mcu->sreg.flags.Z = result == 0;
    mcu->sreg.flags.S = mcu->sreg.flags.N ^ mcu->sreg.flags.V;
    mcu->sreg.flags.N = !!BITN(result, 7);
    mcu->sreg.flags.C = (BITN(mcu->reg[rd], 7) && BITN(mcu->reg[rr], 7))
        || (BITN(mcu->reg[rr], 7) && !BITN(result, 7))
        || (!BITN(result, 7) && BITN(mcu->reg[rd], 7));
    mcu->reg[rd] = result;
    
    printf("Rd: 0x%02X, Rr: 0x%02X\n", rd, rr);
    print_sreg(mcu);

    mcu->pc++;
}

static void ADC(Attiny85* mcu, uint16_t op)
{

    printf("[INST] %s\n", __func__);
    
    uint8_t rr = ((op >> 5) & 0x10) | (op & 0xF);
    uint8_t rd = ((op >> 4) & 0x1F);

    uint8_t result = mcu->reg[rd] + mcu->reg[rr] + mcu->sreg.flags.C;

    mcu->sreg.flags.H = (BITN(mcu->reg[rd], 3) && BITN(mcu->reg[rr], 3))
        || (BITN(mcu->reg[rr], 3) && !BITN(result, 3))
        || (!BITN(result, 3) && BITN(mcu->reg[rd], 3));
    mcu->sreg.flags.V = (BITN(mcu->reg[rd], 7) && BITN(mcu->reg[rr], 7) && !BITN(result, 7))
        || (!BITN(mcu->reg[rd], 7) && !BITN(mcu->reg[rr], 7) && BITN(result, 7)); 
    mcu->sreg.flags.Z = result == 0;
    mcu->sreg.flags.S = mcu->sreg.flags.N ^ mcu->sreg.flags.V;
    mcu->sreg.flags.N = !!BITN(result, 7);
    mcu->sreg.flags.C = (BITN(mcu->reg[rd], 7) && BITN(mcu->reg[rr], 7))
        || (BITN(mcu->reg[rr], 7) && !BITN(result, 7))
        || (!BITN(result, 7) && BITN(mcu->reg[rd], 7));
    mcu->reg[rd] = result;
    
    printf("Rd: 0x%02X, Rr: 0x%02X\n", rd, rr);
    print_sreg(mcu);

    mcu->pc++;
}

static void ADIW(Attiny85* mcu, uint16_t op)
{
    printf("[INST] %s\n", __func__);
    
    uint8_t regs[] = {24, 26, 28, 30};
    uint8_t d = (op >> 4) & 0x03;
    uint8_t k = ((op >> 2) & 0x30) | (op & 0x0F);
    uint8_t rd = regs[d];
    uint16_t result = ((mcu->reg[rd + 1] << 8) | mcu->reg[rd]) + k;

    mcu->sreg.flags.V = !BITN(mcu->reg[rd + 1], 7) && BITN(result, 15);
    mcu->sreg.flags.N = !!BITN(result, 15);
    mcu->sreg.flags.Z = result == 0;
    mcu->sreg.flags.C = !BITN(result, 15) && BITN(mcu->reg[rd + 1], 7);

    mcu->reg[rd] = result & 0xFF;
    mcu->reg[rd + 1] = (result >> 8) & 0xFF;
    
    printf("Add immediate word result = 0x%04X\n", result);
    print_sreg(mcu);
    mcu->pc++;
}

static void ANDI(Attiny85* mcu, uint16_t op)
{
    printf("[INST] %s\n", __func__);

    uint8_t k = ((op >> 4) & 0xF0) | (op & 0x0F);
    uint8_t rd = (op >> 4) & 0x0F;
    uint8_t result = mcu->reg[rd] & k;
    mcu->sreg.flags.V = 0;
    mcu->sreg.flags.N = !!BITN(result, 7);
    mcu->sreg.flags.Z = result == 0;
    mcu->sreg.flags.S = mcu->sreg.flags.N ^ mcu->sreg.flags.V;
    mcu->reg[rd] = result;

    printf("Reg 0x%02X and immediate 0x%02X. Result 0x%02X\n", rd, k, result);
    print_sreg(mcu);

    mcu->pc++;
}

static void AND(Attiny85* mcu, uint16_t op)
{
    printf("[INST] %s\n", __func__);
    
    uint8_t rd = (op >> 4) & 0x1F;
    uint8_t rr = ((op >> 5) & 0x10) | (op & 0x0F);
    uint8_t result = mcu->reg[rd] & mcu->reg[rr];

    mcu->sreg.flags.V = 0;
    mcu->sreg.flags.N = !!BITN(result, 7);
    mcu->sreg.flags.Z = result == 0;
    mcu->sreg.flags.S = mcu->sreg.flags.N ^ mcu->sreg.flags.V;
    mcu->reg[rd] = result;

    printf("Reg 0x%02X and 0x%02X. Result 0x%02X\n", rd, rr, result);
    print_sreg(mcu);

    mcu->pc++;
}

static void SBIW(Attiny85* mcu, uint16_t op)
{
    printf("[INST] %s\n", __func__);
    
    uint8_t regs[] = {24, 26, 28, 30};
    uint8_t d = (op >> 4) & 0x03;
    uint8_t k = ((op >> 2) & 0x30) | (op & 0x0F);
    uint8_t rd = regs[d];
    uint16_t result = ((mcu->reg[rd + 1] << 8) | mcu->reg[rd]) - k;

    mcu->sreg.flags.V = !BITN(mcu->reg[rd + 1], 7) && BITN(result, 15);
    mcu->sreg.flags.N = !!BITN(result, 15);
    mcu->sreg.flags.Z = result == 0;
    mcu->sreg.flags.C = !BITN(result, 15) && BITN(mcu->reg[rd + 1], 7);

    mcu->reg[rd] = result & 0xFF;
    mcu->reg[rd + 1] = (result >> 8) & 0xFF;
    
    printf("Subtract immediate word result = 0x%04X\n", result);
    print_sreg(mcu);
    
    mcu->pc++;
}

static void ORI(Attiny85* mcu, uint16_t op)
{
    printf("[INST] %s\n", __func__);

    uint8_t k = ((op >> 4) & 0xF0) | (op & 0x0F);
    uint8_t rd = (op >> 4) & 0x0F;
    uint8_t result = mcu->reg[rd] | k;
    mcu->sreg.flags.V = 0;
    mcu->sreg.flags.N = !!BITN(result, 7);
    mcu->sreg.flags.Z = result == 0;
    mcu->sreg.flags.S = mcu->sreg.flags.N ^ mcu->sreg.flags.V;
    mcu->reg[rd] = result;

    printf("Reg 0x%02X or immediate 0x%02X. Result 0x%02X\n", rd, k, result);
    print_sreg(mcu);
    mcu->pc++;
}

static void COM(Attiny85* mcu, uint16_t op)
{
    printf("[INST] %s\n", __func__);
    uint8_t rd = (op >> 4) | 0x1F;
    uint8_t result = 0xFF - mcu->reg[rd];

    mcu->sreg.flags.V = 0;
    mcu->sreg.flags.N = !!BITN(result, 7);
    mcu->sreg.flags.Z = result == 0;
    mcu->sreg.flags.C = 1;
    mcu->sreg.flags.S = mcu->sreg.flags.N ^ mcu->sreg.flags.V;
    mcu->reg[rd] = result;

    print_sreg(mcu);
    mcu->pc++;
}

static void NEG(Attiny85* mcu, uint16_t op)
{
    printf("[INST] %s\n", __func__);
    uint8_t rd = (op >> 4) | 0x1F;
    uint8_t result = 0x00 - mcu->reg[rd];
    
    mcu->sreg.flags.H = BITN(result, 3) && BITN(mcu->reg[rd], 3);
    mcu->sreg.flags.V = BITN(result, 7) && !BITN(result, 6) && BITN(result, 5) && BITN(result, 4) && BITN(result, 3) && BITN(result, 2) && BITN(result, 1) && BITN(result, 0); 
    mcu->sreg.flags.N = !!BITN(result, 7);
    mcu->sreg.flags.Z = result == 0;
    mcu->sreg.flags.C = result != 0;
    mcu->sreg.flags.S = mcu->sreg.flags.N ^ mcu->sreg.flags.V;
    mcu->reg[rd] = result;

    print_sreg(mcu);
    mcu->pc++;
}

static void NOP(Attiny85* mcu, uint16_t op)
{
    printf("[INST] %s\n", __func__);
    mcu->pc++;
}
#endif
