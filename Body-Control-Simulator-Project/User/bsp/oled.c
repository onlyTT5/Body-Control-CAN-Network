#include "oled.h"
#include "i2c.h"

#define OLED_ADDR (0x3C << 1)

void OLED_WriteCmd(uint8_t cmd)
{
    uint8_t data[2] = {0x00, cmd};   // 0x00 表示后面是命令
    HAL_I2C_Master_Transmit(&hi2c1, OLED_ADDR, data, 2, 100);
}

void OLED_Init(void)
{
    uint8_t init_cmds[] = {
        0x00,
        0xAE,       // 关闭显示
        0xD5, 0x80,
        0xA8, 0x3F, // 128x64
        0xD3, 0x00,
        0x40,
        0x8D, 0x14,
        0x20, 0x00,
        0xA1,
        0xC8,
        0xDA, 0x12,
        0x81, 0xCF,
        0xD9, 0xF1,
        0xDB, 0x40,
        0xA4,
        0xA6,
        0xAF        // 打开显示
    };

    HAL_I2C_Master_Transmit(
        &hi2c1,
        OLED_ADDR,
        init_cmds,
        sizeof(init_cmds),
        HAL_MAX_DELAY
    );
}

static void OLED_WriteData(const uint8_t *data, uint8_t len)
{
    uint8_t tx[17];
    uint8_t count;

    while (len > 0)
    {
        count = (len > 16) ? 16 : len;
        tx[0] = 0x40;   // 0x40 表示后面是显示数据

        for (uint8_t i = 0; i < count; i++)
        {
            tx[i + 1] = data[i];
        }

        HAL_I2C_Master_Transmit(
            &hi2c1, OLED_ADDR, tx, count + 1, HAL_MAX_DELAY
        );

        data += count;
        len -= count;
    }
}

static void OLED_SetCursor(uint8_t page, uint8_t col)
{
    OLED_WriteCmd(0xB0 | page);
    OLED_WriteCmd(0x00 | (col & 0x0F));
    OLED_WriteCmd(0x10 | (col >> 4));
}

void OLED_Clear(void)
{
    uint8_t empty[16] = {0};

    for (uint8_t page = 0; page < 8; page++)
    {
        OLED_SetCursor(page, 0);

        for (uint8_t i = 0; i < 8; i++)
        {
            OLED_WriteData(empty, 16);
        }
    }
}

static const uint8_t *OLED_GetGlyph(char ch)
{
    static const uint8_t sp[5] = {0, 0, 0, 0, 0};
    static const uint8_t dot[5] = {0, 0x60, 0x60, 0, 0};
    static const uint8_t colon[5] = {0, 0x36, 0x36, 0, 0};
    static const uint8_t n0[5] = {0x3E, 0x51, 0x49, 0x45, 0x3E};
    static const uint8_t n1[5] = {0, 0x42, 0x7F, 0x40, 0};
	static const uint8_t n2[5] = {0x42, 0x61, 0x51, 0x49, 0x46};
    static const uint8_t n3[5] = {0x21, 0x41, 0x45, 0x4B, 0x31};
    static const uint8_t n4[5] = {0x18, 0x14, 0x12, 0x7F, 0x10};
    static const uint8_t n5[5] = {0x27, 0x45, 0x45, 0x45, 0x39};
    static const uint8_t n6[5] = {0x3C, 0x4A, 0x49, 0x49, 0x30};
    static const uint8_t n7[5] = {0x01, 0x71, 0x09, 0x05, 0x03};
    static const uint8_t n8[5] = {0x36, 0x49, 0x49, 0x49, 0x36};
    static const uint8_t n9[5] = {0x06, 0x49, 0x49, 0x29, 0x1E};
    static const uint8_t A[5] = {0x7E, 0x11, 0x11, 0x11, 0x7E};
    static const uint8_t B[5] = {0x7F, 0x49, 0x49, 0x49, 0x36};
    static const uint8_t C[5] = {0x3E, 0x41, 0x41, 0x41, 0x22};
    static const uint8_t D[5] = {0x7F, 0x41, 0x41, 0x22, 0x1C};
    static const uint8_t E[5] = {0x7F, 0x49, 0x49, 0x49, 0x41};
    static const uint8_t F[5] = {0x7F, 0x09, 0x09, 0x09, 0x01};
    static const uint8_t I[5] = {0, 0x41, 0x7F, 0x41, 0};
    static const uint8_t L[5] = {0x7F, 0x40, 0x40, 0x40, 0x40};
    static const uint8_t K[5] = {0x7F, 0x08, 0x14, 0x22, 0x41};
    static const uint8_t M[5] = {0x7F, 0x02, 0x0C, 0x02, 0x7F};
    static const uint8_t N[5] = {0x7F, 0x02, 0x04, 0x08, 0x7F};
    static const uint8_t O[5] = {0x3E, 0x41, 0x41, 0x41, 0x3E};
    static const uint8_t P[5] = {0x7F, 0x09, 0x09, 0x09, 0x06};
    static const uint8_t R[5] = {0x7F, 0x09, 0x19, 0x29, 0x46};
    static const uint8_t S[5] = {0x46, 0x49, 0x49, 0x49, 0x31};
    static const uint8_t T[5] = {0x01, 0x01, 0x7F, 0x01, 0x01};
    static const uint8_t V[5] = {0x1F, 0x20, 0x40, 0x20, 0x1F};
	static const uint8_t W[5] = {0x7F, 0x20, 0x18, 0x20, 0x7F};
    static const uint8_t Y[5] = {0x07, 0x08, 0x70, 0x08, 0x07};
		static const uint8_t G[5] = {0x3E, 0x41, 0x51, 0x51, 0x32};
		static const uint8_t H[5] = {0x7F, 0x08, 0x08, 0x08, 0x7F};

    switch (ch)
    {
        case 'A': return A; case 'B': return B; case 'C': return C;
        case 'D': return D; case 'E': return E; case 'F': return F;
				case 'G': return G; case 'H': return H;
        case 'I': return I; case 'K': return K; case 'L': return L;
        case 'M': return M; case 'N': return N; case 'O': return O;
        case 'P': return P; case 'R': return R; case 'S': return S;
        case 'T': return T;
        case 'V': return V; case 'W': return W; case 'Y': return Y;
        case '0': return n0; case '1': return n1; case '2': return n2;
        case '3': return n3; case '4': return n4; case '5': return n5;
        case '6': return n6; case '7': return n7; case '8': return n8;
        case '9': return n9;
        case '.': return dot; case ':': return colon;
        default: return sp;
    }
}

void OLED_ShowString(uint8_t page, uint8_t col, const char *text)
{
    uint8_t data[6];

    OLED_SetCursor(page, col);

    while (*text)
    {
        const uint8_t *glyph = OLED_GetGlyph(*text++);

        for (uint8_t i = 0; i < 5; i++)
        {
            data[i] = glyph[i];
        }

        data[5] = 0x00;
        OLED_WriteData(data, 6);
    }
}
