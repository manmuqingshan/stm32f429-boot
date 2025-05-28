#ifndef _USART_H
#define _USART_H

void usart_setup(void *base, uint32_t clk_freq, uint32_t baud);
void usart_putch(void *base, char ch);

#endif /* _USART_H */
