#include <stdint.h>
#include <string.h>
#include <stm32f1xx.h>
#include "USART1.h"

// ---- RCC clock enable bits ---- 
#define USART1_CLK_EN   (1U << 14)  // APB2ENR: USART1 clock enable 
#define GPIOA_CLK_EN    (1U << 2)   // APB2ENR: GPIOA clock enable 
#define AFIO_CLK_EN     (1U << 0)   // APB2ENR: AFIO clock enable 

// ---- AFIO remap ---- 
#define USART1_REMAP    (1U << 2)  // AFIO_MAPR: 0 = default pins (PA9/PA10) 

// ---- Clock / baud configuration ---- 
#define PERIPHERAL_CLK  72000000U  // PCLK2 - USART1 sits on APB2 
#define BAUD_RATE       115200U

// ---- USART1->CR1 bit definitions ---- 
#define UE     (1U << 13)  // USART enable 
#define TE     (1U << 3)   // Transmitter enable 
#define RE     (1U << 2)   // Receiver enable 
#define TXEIE  (1U << 7)   // TX register empty interrupt enable 
#define RXNEIE (1U << 5)   // RX register not empty interrupt enable 

// ---- USART1->SR bit definitions ---- 
#define SR_RXNE (1U << 5)  // Read data register not empty 
#define SR_TXE  (1U << 7)  // Transmit data register empty 

#define RX_BUF_SIZE 64  // must be a power of 2 - see index wraparound below 
#define TX_BUF_SIZE 64  // must be a power of 2 - see index wraparound below 

/**
 * TX ring buffer and state.
 * - Producer: usart_send_string() (application context)
 * - Consumer: USART1_IRQHandler() (interrupt context)
 */
static volatile char    tx_buf[TX_BUF_SIZE];
static volatile uint8_t tx_head, tx_tail;
static volatile uint8_t tx_busy;  // 1 while a transmission is actively in progress 

/**
 * RX ring buffer and state.
 * - Producer: USART1_IRQHandler() (interrupt context)
 * - Consumer: usart_read_byte() (application context)
 */
static volatile char     rx_buf[RX_BUF_SIZE];
static volatile uint8_t  rx_head, rx_tail;
static volatile uint32_t rx_overflow_count = 0;

static uint16_t compute_baudrate_calc(uint32_t peripheral_clk, uint32_t baud_rate);

/**
 * @brief  Configures USART1 for interrupt-driven, full-duplex operation.
 * @retval None
 */
void USART1_Init(void)
{
    // Enable peripheral clocks: USART1, GPIOA (for PA9/PA10), AFIO (for MAPR) 
    RCC->APB2ENR |= USART1_CLK_EN;
    RCC->APB2ENR |= GPIOA_CLK_EN;
    RCC->APB2ENR |= AFIO_CLK_EN;

    // Ensure USART1 uses its default pin mapping (PA9 = TX, PA10 = RX) 
    AFIO->MAPR &= ~USART1_REMAP;

    // Baud rate, calculated from PCLK2 (72MHz, default APB2 prescaler) 
    USART1->BRR = compute_baudrate_calc(PERIPHERAL_CLK, BAUD_RATE);

    /* Enable USART, transmitter, receiver, and RX interrupt.
       TXEIE is intentionally left off here - it's only enabled per
       transmission, inside usart_send_string(), to avoid an immediate
       interrupt storm on an idle (already-empty) TX register. */
    USART1->CR1 |= UE;
    USART1->CR1 |= (RE | TE);
    USART1->CR1 |= RXNEIE;

    NVIC_SetPriority(USART1_IRQn, 5);
    NVIC_EnableIRQ(USART1_IRQn);

    // PA9 (TX): alternate function push-pull, 10MHz 
    GPIOA->CRH &= ~(0xFU << 4);
    GPIOA->CRH |= (0xBU << 4);  // MODE9 = 11 (output, 50MHz... see note below), CNF9 = 10 (AF push-pull) 

    // PA10 (RX): input, floating (or pull-up, set via ODR below) 
    GPIOA->CRH &= ~(0xFU << 8);
    GPIOA->CRH |= (2U << 10);   // CNF10 = 10 (input with pull-up/pull-down) 
    GPIOA->CRH |= (0U << 8);    // MODE10 = 00 (input) 
    GPIOA->ODR |= (1U << 10);   // select pull-up (vs pull-down) for PA10 
}

/**
 * @brief  USART1 interrupt handler - services both RX and TX events.
 * @note   Kept minimal by design: RX buffers the byte and returns; TX
 *         feeds one byte from the ring buffer and returns. No parsing
 *         or blocking work happens here.
 * @retval None
 */
void USART1_IRQHandler(void)
{
    if (USART1->SR & SR_RXNE)
    {
        /* Free space in the RX ring buffer (one slot deliberately kept
           empty so head == tail unambiguously means "buffer empty") */
        uint8_t free_space = (uint8_t)((rx_tail - rx_head - 1) & (RX_BUF_SIZE - 1));

        /* Reading DR clears RXNE - always do this, even on overflow,
           or the interrupt would immediately re-fire forever. */
        uint8_t received_byte = USART1->DR;

        if (free_space > 0)
        {
            rx_buf[rx_head] = received_byte;
            rx_head = (rx_head + 1) & (RX_BUF_SIZE - 1);
        }
        else
        {
            rx_overflow_count++;  // byte dropped - consumer isn't keeping up 
        }
    }
    else if ((tx_head != tx_tail) && (USART1->SR & SR_TXE) && tx_busy)
    {
        USART1->DR = tx_buf[tx_tail];
        tx_tail = (tx_tail + 1) & (TX_BUF_SIZE - 1);

        if (tx_head == tx_tail)
        {
            /* Buffer drained - stop the interrupt, or it would keep
               firing forever demanding data that no longer exists. */
            USART1->CR1 &= ~TXEIE;
            tx_busy = 0;
        }
    }
}

/**
 * @brief  Reads one byte from the RX ring buffer, if available.
 * @retval Next received byte, or -1 if the buffer is empty.
 */
int usart_read_byte(void)
{
    if (rx_head == rx_tail)
    {
        return -1;  // buffer empty, nothing to read 
    }

    char c = rx_buf[rx_tail];
    rx_tail = (rx_tail + 1) & (RX_BUF_SIZE - 1);
    return c;
}

/**
 * @brief  Queues a null-terminated string for transmission.
 * @param  str Null-terminated string to send.
 * @retval None
 */
void usart_send_string(const char *str)
{
    uint16_t len = strlen(str);

    /* Free space in the TX ring buffer. Reading tx_tail here (outside
       any critical section) is safe: the ISR only ever advances
       tx_tail, which can only increase free space, never decrease it -
       so a stale read gives at worst a harmless, conservative
       underestimate. */
    uint8_t free_space = (uint8_t)((tx_tail - tx_head - 1) & (TX_BUF_SIZE - 1));

    if (len > free_space)
    {
        return;  // reject entirely - no partial writes 
    }

    while (*str)
    {
        tx_buf[tx_head] = *str;
        tx_head = (tx_head + 1) & (TX_BUF_SIZE - 1);
        str++;
    }

    /* Critical section: only the tx_busy check-and-set and the first
       primed byte need protecting from the ISR, so TXEIE (not the
       whole NVIC line) is disabled just for this narrow window - RX
       interrupts remain live throughout. */
    USART1->CR1 &= ~TXEIE;
    if (!tx_busy)
    {
        /* Nothing in flight - prime the first byte manually, since the
           ISR only fires *after* a byte has already been sent. */
        tx_busy = 1;
        USART1->DR = tx_buf[tx_tail];
        tx_tail = (tx_tail + 1) & (TX_BUF_SIZE - 1);
        USART1->CR1 |= TXEIE;
    }
    else
    {
        /* A transmission is already running - just restore TXEIE.
           The running ISR will drain this new data on its own. */
        USART1->CR1 |= TXEIE;
    }
}

/**
 * @brief  Returns the number of RX bytes dropped due to buffer overflow.
 * @retval Total dropped-byte count since startup.
 */
uint32_t usart_get_rx_overflow_count(void)
{
    return rx_overflow_count;
}

/**
 * @brief  Computes the USART1->BRR value for a given clock and baud rate.
 * @note   Rounds to the nearest integer divisor (rather than truncating)
 *         for the most accurate achievable baud rate.
 * @param  peripheral_clk Peripheral clock feeding USART1 (PCLK2), in Hz.
 * @param  baud_rate      Desired baud rate, in bits/sec.
 * @retval Value to write directly to USART1->BRR.
 */
static uint16_t compute_baudrate_calc(uint32_t peripheral_clk, uint32_t baud_rate)
{
    return (uint16_t)((peripheral_clk + (baud_rate / 2)) / baud_rate);
}