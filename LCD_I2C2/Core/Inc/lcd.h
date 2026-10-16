/*
 * lcd.h
 *
 *  Created on: 15 oct. 2026
 *      Author: kokolaoponouemmanuel
 */
#ifndef LCD_H
#define LCD_H

#include "stm32f4xx_hal.h"

HAL_StatusTypeDef LCD_SendNibble(uint8_t nibble, uint8_t rs);
HAL_StatusTypeDef LCD_SendCommand(uint8_t command);
HAL_StatusTypeDef LCD_SendChar(char character);
HAL_StatusTypeDef LCD_Init(void);
HAL_StatusTypeDef LCD_SetCursor(uint8_t row, uint8_t column);
HAL_StatusTypeDef LCD_SendString(char *texte);


#endif
