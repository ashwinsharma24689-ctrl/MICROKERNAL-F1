#include <stdint.h>
#include <stm32f1xx.h>
#include <stdio.h>

#define USART1_CLK_EN 	(1U<<14)
#define AFIO_CLK_EN 		(1U<<0)
#define GPIOA_EN      	(1U<<2)
#define USART1_REMAP 		(1U<<2)

#define PERIPHERAL_CLK 	72000000
#define BAUD_RATE      	115200

#define TXEIE           (1U<<7)
#define RXNEIE          (1U<<5)
#define TE              (1U<<3)
#define RE              (1U<<2)
#define UE              (1U<<13)
#define RX_BUF_SIZE      64
#define TX_BUF_SIZE      64

static volatile char tx_buf [TX_BUF_SIZE];
static volatile uint8_t tx_head, tx_tail;
static volatile uint8_t tx_busy;   // is a transmission currently in progress?
static volatile char rx_buf[RX_BUF_SIZE];
static volatile uint8_t rx_head, rx_tail;

static uint16_t compute_baudrate_calc(uint32_t PeripheralClk, uint32_t Baud_Rate);

	
void USART1_Init(void){

		RCC->APB2ENR |= USART1_CLK_EN;
		RCC->APB2ENR |= GPIOA_EN;
		RCC->APB2ENR |= AFIO_CLK_EN;
	
		AFIO->MAPR &= ~(USART1_REMAP);
	
		USART1->BRR = compute_baudrate_calc(PERIPHERAL_CLK,BAUD_RATE);
		USART1->CR1 |= UE;   
		USART1->CR1 |= (RE | TE);
		USART1->CR1 |= RXNEIE;
		NVIC_SetPriority(USART1_IRQn, 5);   
		NVIC_EnableIRQ(USART1_IRQn);        
	
	  //Tx
		GPIOA->CRH &= ~(0xFU<<4); 
		GPIOA->CRH |= (2U<<6);
		GPIOA->CRH |=(3U<<4);	//or  GPIOA->CRH |= (0xB<<4);    // MODE=11, CNF=10 in one shot //
		//Rx
		GPIOA->CRH   &= ~(0xFU<<8);
		GPIOA->CRH   |= (2U<<10);
		GPIOA->CRH   |= (0U<<8);
		GPIOA->ODR 	 |= (1U<<10);   

	
}

void USART1_IRQHandler(void){
		
			if (USART1->SR & (1U<<5))   
    {
        uint8_t received_byte = USART1->DR;  
				rx_buf[rx_head] = received_byte;
				rx_head = (rx_head + 1) % RX_BUF_SIZE;
    }	
			else if ((tx_head != tx_tail) & (USART1->SR & (1U<<7))& tx_busy)	
		{
				USART1->DR = tx_buf[tx_tail];
				tx_tail = (tx_tail + 1) % TX_BUF_SIZE;
			if (tx_head == tx_tail) 
		{
				USART1->CR1 &= ~(TXEIE);
				tx_busy = 0;
		}
		
	}
}

int usart_read_byte(void) {
    
			if (rx_head == rx_tail) return -1;  // buffer empty, nothing to read
				char c = rx_buf[rx_tail];
				rx_tail = (rx_tail + 1) % RX_BUF_SIZE;
				return c;
}

void usart_send_string(const char *str) {
	
			if(!((TX_BUF_SIZE - tx_head)<= *str))
		{
			while (*str) 
		{
        tx_buf[tx_head] = *str;
        tx_head = (tx_head + 1) % TX_BUF_SIZE;
        str++;
    }

			if (!tx_busy) {
				
				tx_busy = 1;
        USART1->DR = tx_buf[tx_tail];
        tx_tail = (tx_tail + 1) % TX_BUF_SIZE;
        USART1->CR1 |= TXEIE;
    }
}
}
static uint16_t compute_baudrate_calc(uint32_t PeripheralClk, uint32_t Baud_Rate){
							
							return((PeripheralClk+(Baud_Rate/2))/Baud_Rate);
}