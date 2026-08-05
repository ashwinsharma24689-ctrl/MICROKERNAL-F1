#ifndef USART1_H
#define USART1_H

#include <stdint.h>
#include <stm32f1xx.h>
#include <stdio.h>
#include <string.h>

void USART1_Init(void);
int usart_read_byte(void);
void usart_send_string(const char *str);

#endif