/*
 * CSE 2206: Lab-02
 * Task-02: Delay Generation
 *
 * Author: Md. Samiul Islam Siam (Roll: 02)
 *         Partho Kumar Modnal (Roll: 07)
 */

#ifndef HELPER_H_
#define HELPER_H_

void SystemClock_Config(void);

void USART2_Init(void);
void USART2_SendString(const char *);

void LED_Init(void);
void LED_On(void);
void LED_Off(void);
void LED_Toggle(void);

#endif /* HELPER_H_ */
