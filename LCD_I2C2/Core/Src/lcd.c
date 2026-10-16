/*
 * lcd.c
 *
 *  Created on: 15 oct. 2026
 *      Author: kokolaoponouemmanuel
 */
#include "lcd.h"
#include "main.h"

#define LCD_ADDRESS (0x27<<1) // Adresse I2C du module LCD

extern I2C_HandleTypeDef hi2c2;



//Envoie 4 bits au LCD et génère l'impulsion E
HAL_StatusTypeDef LCD_SendNibble(uint8_t nibble, uint8_t rs)
{
    HAL_StatusTypeDef status;
    uint8_t output = 0;

    // 1. Placement des 4 bits de données sur P4-P7 et activation du rétroéclairage (0x08)
    output = (nibble << 4) | 0x08;

    // 2. Gestion de RS (0 pour commande, 1 pour donnée)
    if(rs == 1)
    {
        output |= 0x01; // RS = 1
    }
    else
    {
        output &= ~0x01; // RS = 0
    }

    // --- IMPULSION SUR LA BROCHE ENABLE (E = bit 2, soit 0x04) ---

    //S'assurer que E est à 0 au départ
    output &= ~0x04;
    status = HAL_I2C_Master_Transmit(&hi2c2, LCD_ADDRESS, &output, 1, HAL_MAX_DELAY);
    if(status != HAL_OK) return status;

    //Mettre E à 1
    output |= 0x04;
    status = HAL_I2C_Master_Transmit(&hi2c2, LCD_ADDRESS, &output, 1, HAL_MAX_DELAY);
    if(status != HAL_OK) return status;

    // Remettre E à 0 pour valider l'écriture
    output &= ~0x04;
    status = HAL_I2C_Master_Transmit(&hi2c2, LCD_ADDRESS, &output, 1, HAL_MAX_DELAY);
    if(status != HAL_OK) return status;

    return HAL_OK;
}

//Envoie une commande au LCD, en deux nibbles
// instruction au contrôleur LCD
//reçoit une commande de 8 bits
//et transmet ses deux nibbles a la fois en une fois

HAL_StatusTypeDef LCD_SendCommand(uint8_t command)
{
  HAL_StatusTypeDef status;

  status = LCD_SendNibble((command >> 4) & 0x0F, 0); // Nibble haut
  if(status != HAL_OK)
    {
        return status;
    }

    status = LCD_SendNibble(command & 0x0F, 0); // Nibble faible
    if(status != HAL_OK)
      {
          return status;
      }

    return HAL_OK;
}

//Envoie un caractère à afficher, en deux nibbles

HAL_StatusTypeDef LCD_SendChar(char character)
{
    HAL_StatusTypeDef status;

    uint8_t high_nibble = (character >> 4) & 0x0F;//extrate le nibble haut du caractère
    uint8_t low_nibble  = character & 0x0F;

    status = LCD_SendNibble(high_nibble, 1); // Envoie le nibble haut avec RS = 1 (mode donnée)
     if(status != HAL_OK)
    {
        return status;
    }


    status = LCD_SendNibble(low_nibble, 1); // Envoie le nibble faible avec RS = 1 (mode donnée)
 if(status != HAL_OK)
    {
        return status;
    }

    return HAL_OK;
  }

//Initialise le LCD en mode 4 bits
HAL_StatusTypeDef LCD_Init(void)
{
    HAL_StatusTypeDef status;

    HAL_Delay(50);    // attente après alimentation

    if ((status = LCD_SendNibble(0x03, 0)) != HAL_OK) return status;
    HAL_Delay(5);
    if ((status = LCD_SendNibble(0x03, 0)) != HAL_OK) return status;
    HAL_Delay(1);
    if ((status = LCD_SendNibble(0x03, 0)) != HAL_OK) return status;
    HAL_Delay(1);
    if ((status = LCD_SendNibble(0x02, 0)) != HAL_OK) return status;  // passage en 4 bits
    HAL_Delay(1);

    if ((status = LCD_SendCommand(0x28)) != HAL_OK) return status;    // 4 bits, 2 lignes, 5x8
    if ((status = LCD_SendCommand(0x0C)) != HAL_OK) return status;    // affichage ON, curseur OFF
    if ((status = LCD_SendCommand(0x01)) != HAL_OK) return status;    // clear
    HAL_Delay(2);
    if ((status = LCD_SendCommand(0x06)) != HAL_OK) return status;    // auto-incrément

    return HAL_OK;
}

HAL_StatusTypeDef LCD_SetCursor(uint8_t row, uint8_t column)
{
    uint8_t address_DDRAM;
    uint8_t commande;

    // 1. Vérification des limites
    if (row > 1 || column > 15)
    {
        return HAL_ERROR;
    }

    // 2. Calcule de l'adresse DDRAM
    //(0x00-> pour la 1er ligne et 0x40-> pour la 2e)
    if (row == 0)
    {
        address_DDRAM = 0x00 + column;
    }
    else
    {
        address_DDRAM = 0x40 + column;
    }

    // 3. envoye de la commande

    // Le bit 7 vaut 1 (0x80) pour sélectionner l'instruction Set DDRAM Address.
    commande = 0x80 | address_DDRAM; // Bits 6..0 : adresse DDRAM

    return LCD_SendCommand(commande);
}

HAL_StatusTypeDef LCD_SendString(char *texte)
{
	HAL_StatusTypeDef  status;
    for (int i = 0; texte[i] != '\0'; i++)
    {
    	status = LCD_SendChar(texte[i]);
    	if (status != HAL_OK)
    	    {
    	    	return HAL_ERROR;
    	    }
    }


    return HAL_OK;
}
