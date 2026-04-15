#include "rp2040_hal.h"
#include <stdint.h>

// Trigger: RP2040 GPIO 4 -> CW TIO4 (pin 16)
#define TRIGGER_PIN     4

// UART0: GPIO 0 (TX) / GPIO 1 (RX)
//   GPIO 0 = UART0_TX (func 2) -> CW TIO2 (pin 12, CW reads)
//   GPIO 1 = UART0_RX (func 2) <- CW TIO1 (pin 10, CW writes)
#define CW_UART_BASE    0x40034000u     // UART0
#define CW_UART_TX_PIN  0               // GPIO 0 = UART0_TX
#define CW_UART_RX_PIN  1               // GPIO 1 = UART0_RX

// Clock: crystal removed, CW HS2 drives XIN directly.
// CW typically provides 7.37 MHz; scope.clock.clkgen_freq sets the frequency.
// Baud rates at 7.37 MHz: 38400 -> IBRD=12 FBRD=0 / 230400 -> IBRD=2 FBRD=0

// ============================================================
// Register definitions (bare metal, no SDK required)
// ============================================================

// RESETS
#define RESETS_BASE             0x4000c000u
#define RESETS_RESET            (*(volatile uint32_t *)(RESETS_BASE + 0x00))
#define RESETS_RESET_DONE       (*(volatile uint32_t *)(RESETS_BASE + 0x08))
#define RESET_IO_BANK0          (1u << 5)
#define RESET_PADS_BANK0        (1u << 8)
#define RESET_UART0             (1u << 22)

// IO_BANK0 — GPIO function select
#define IO_BANK0_BASE           0x40014000u
#define GPIO_CTRL(n)            (*(volatile uint32_t *)(IO_BANK0_BASE + (n)*8 + 4))
#define GPIO_FUNC_SIO           5u
#define GPIO_FUNC_UART          2u

// SIO — software-controlled GPIO
#define SIO_BASE                0xd0000000u
#define SIO_GPIO_OUT_SET        (*(volatile uint32_t *)(SIO_BASE + 0x014))
#define SIO_GPIO_OUT_CLR        (*(volatile uint32_t *)(SIO_BASE + 0x018))
#define SIO_GPIO_OE_SET         (*(volatile uint32_t *)(SIO_BASE + 0x024))

// UART0 (PL011)
#define UART_DR                 (*(volatile uint32_t *)(CW_UART_BASE + 0x000))
#define UART_FR                 (*(volatile uint32_t *)(CW_UART_BASE + 0x018))
#define UART_IBRD               (*(volatile uint32_t *)(CW_UART_BASE + 0x024))
#define UART_FBRD               (*(volatile uint32_t *)(CW_UART_BASE + 0x028))
#define UART_LCR_H              (*(volatile uint32_t *)(CW_UART_BASE + 0x02c))
#define UART_CR                 (*(volatile uint32_t *)(CW_UART_BASE + 0x030))
#define UART_FR_TXFF            (1u << 5)   // TX FIFO full
#define UART_FR_RXFE            (1u << 4)   // RX FIFO empty
#define UART_LCR_H_WLEN_8       (3u << 5)   // 8-bit word length
#define UART_LCR_H_FEN          (1u << 4)   // enable FIFOs
#define UART_CR_UARTEN          (1u << 0)   // UART enable
#define UART_CR_TXE             (1u << 8)   // TX enable
#define UART_CR_RXE             (1u << 9)   // RX enable

// XOSC
#define XOSC_BASE               0x40024000u
#define XOSC_CTRL               (*(volatile uint32_t *)(XOSC_BASE + 0x000))
#define XOSC_STATUS             (*(volatile uint32_t *)(XOSC_BASE + 0x004))
#define XOSC_CTRL_FREQ_RANGE    0x000AA0u   // 1-15 MHz range
#define XOSC_CTRL_ENABLE        0xFAB000u   // enable
#define XOSC_STATUS_STABLE      (1u << 31)

// CLOCKS
#define CLOCKS_BASE             0x40008000u
#define CLK_REF_CTRL            (*(volatile uint32_t *)(CLOCKS_BASE + 0x30))
#define CLK_REF_SELECTED        (*(volatile uint32_t *)(CLOCKS_BASE + 0x38))
#define CLK_SYS_CTRL            (*(volatile uint32_t *)(CLOCKS_BASE + 0x3c))
#define CLK_SYS_SELECTED        (*(volatile uint32_t *)(CLOCKS_BASE + 0x44))
#define CLK_PERI_CTRL           (*(volatile uint32_t *)(CLOCKS_BASE + 0x48))
#define CLK_PERI_CTRL_ENABLE    (1u << 11)

// PLL_SYS
#define PLL_SYS_BASE            0x40028000u
#define PLL_SYS_PWR             (*(volatile uint32_t *)(PLL_SYS_BASE + 0x04))
#define PLL_PWR_PD              (1u << 0)
#define PLL_PWR_DSMPD           (1u << 2)
#define PLL_PWR_POSTDIVPD       (1u << 3)
#define PLL_PWR_VCOPD           (1u << 5)

// ============================================================

static void unreset_wait(uint32_t mask)
{
    RESETS_RESET &= ~mask;
    while ((RESETS_RESET_DONE & mask) != mask);
}

void platform_init(void)
{
    // Unreset IO_BANK0 and PADS_BANK0 for GPIO function select
    unreset_wait(RESET_IO_BANK0 | RESET_PADS_BANK0);

    // Enable XOSC — CW HS2 drives XIN directly, XOUT floating
    XOSC_CTRL = XOSC_CTRL_FREQ_RANGE | XOSC_CTRL_ENABLE;
    while (!(XOSC_STATUS & XOSC_STATUS_STABLE));

    // Switch clk_ref to XOSC (src = 2), wait for glitchless mux
    CLK_REF_CTRL = 2u;
    while (!(CLK_REF_SELECTED & (1u << 2)));

    // Switch clk_sys to clk_ref (src = 0), wait for glitchless mux
    CLK_SYS_CTRL = 0u;
    while (!(CLK_SYS_SELECTED & (1u << 0)));

    // Power down PLL_SYS — not needed, reduces noise on power traces
    PLL_SYS_PWR = PLL_PWR_PD | PLL_PWR_DSMPD | PLL_PWR_POSTDIVPD | PLL_PWR_VCOPD;

    // Enable clk_peri from clk_sys (auxsrc = 0 = clk_sys = XOSC)
    CLK_PERI_CTRL = CLK_PERI_CTRL_ENABLE;
}

void init_uart(void)
{
    unreset_wait(RESET_UART0);

    // Disable UART before configuring
    UART_CR = 0;

    // Baud divisor = UARTCLK / (16 * baud). At 7.37 MHz:
    //   38400  baud -> IBRD=12, FBRD=0
    //   230400 baud -> IBRD=2,  FBRD=0
#if SS_VER == SS_VER_2_0
    UART_IBRD = 2;
#else
    UART_IBRD = 12;
#endif
    UART_FBRD = 0;

    // 8N1, FIFOs enabled
    UART_LCR_H = UART_LCR_H_WLEN_8 | UART_LCR_H_FEN;

    // Enable UART, TX and RX
    UART_CR = UART_CR_UARTEN | UART_CR_TXE | UART_CR_RXE;

    // Route GPIO pins to UART0 function
    GPIO_CTRL(CW_UART_TX_PIN) = GPIO_FUNC_UART;
    GPIO_CTRL(CW_UART_RX_PIN) = GPIO_FUNC_UART;
}

void trigger_setup(void)
{
    GPIO_CTRL(TRIGGER_PIN) = GPIO_FUNC_SIO;
    SIO_GPIO_OUT_CLR = (1u << TRIGGER_PIN);
    SIO_GPIO_OE_SET  = (1u << TRIGGER_PIN);
}

void trigger_high(void)
{
    SIO_GPIO_OUT_SET = (1u << TRIGGER_PIN);
}

void trigger_low(void)
{
    SIO_GPIO_OUT_CLR = (1u << TRIGGER_PIN);
}

char getch(void)
{
    while (UART_FR & UART_FR_RXFE);
    return (char)(UART_DR & 0xFF);
}

void putch(char c)
{
    while (UART_FR & UART_FR_TXFF);
    UART_DR = (uint32_t)(uint8_t)c;
}
