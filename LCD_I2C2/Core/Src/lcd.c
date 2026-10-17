/**
  ******************************************************************************
  * @file           : lcd.c
  * @brief          : Pilote logiciel pour afficheur LCD I2C (16x2) via PCF8574
  * @author         : kokolaoponouemmanuel

  *
  * @details        :
  *   Ce fichier implémente les fonctions de communication I2C et de contrôle
  *   pour un écran LCD caractères géré par le contrôleur HD44780 en mode 4 bits,
  *   interfacé via un expandeur I2C PCF8574.
  *   - Gestion des nibbles (poids forts / poids faibles)
  *   - Gestion de l'impulsion Enable (E) et du registre de sélection (RS)
  *   - Commandes d'initialisation, de positionnement et d'affichage de texte
  *   - Bus Utilisé I2C2 Cablage : PC12(STM32F446RETx) -->SDA (LCD)
  *   							   PB10(STM32F446RETx) -->SCL (LCD)
  ******************************************************************************
  */

#include "lcd.h"
#include "main.h"

#define LCD_ADDRESS (0x27 << 1) // Adresse I2C du module LCD (décalée de 1 pour HAL)

extern I2C_HandleTypeDef hi2c2;

/**
  * @brief  Envoie un quartet (4 bits) au LCD et génère l'impulsion sur la broche Enable (E)
  * @param  nibble: Les 4 bits de données ou de commande à transmettre
  * @param  rs:     Sélection du registre (0 = Commande, 1 = Donnée)
  * @retval HAL_StatusTypeDef : Statut de la transmission I2C (HAL_OK ou HAL_ERROR)
  */
HAL_StatusTypeDef LCD_SendNibble(uint8_t nibble, uint8_t rs)
{
    HAL_StatusTypeDef status;
    uint8_t output = 0;

    // 1. Placement des 4 bits de données sur P4-P7 et activation du rétroéclairage (0x08)
    output = (nibble << 4) | 0x08;

    // 2. Gestion de RS (0 pour commande, 1 pour donnée)
    if (rs == 1)
    {
        output |= 0x01; // RS = 1 (Donnée)
    }
    else
    {
        output &= ~0x01; // RS = 0 (Commande)
    }

    // --- IMPULSION SUR LA BROCHE ENABLE (E = bit 2, soit 0x04) ---

    // E est à 0 au départ

    output &= ~0x04;
    status = HAL_I2C_Master_Transmit(&hi2c2, LCD_ADDRESS, &output, 1, HAL_MAX_DELAY);
    if (status != HAL_OK) return status;

    // E est à 1

    output |= 0x04;
    status = HAL_I2C_Master_Transmit(&hi2c2, LCD_ADDRESS, &output, 1, HAL_MAX_DELAY);
    if (status != HAL_OK) return status;

    // Remise de E à 0

    output &= ~0x04;
    status = HAL_I2C_Master_Transmit(&hi2c2, LCD_ADDRESS, &output, 1, HAL_MAX_DELAY);
    if (status != HAL_OK) return status;

    return HAL_OK;
}

/**
  * @brief  Envoie un octet de commande au LCD en le scindant en deux nibbles (haut puis bas)
  * @param  command: L'octet de commande à transmettre au contrôleur LCD
  */

HAL_StatusTypeDef LCD_SendCommand(uint8_t command)
{
    HAL_StatusTypeDef status;

    // Envoi du nibble haut (bits 4 à 7)

    status = LCD_SendNibble((command >> 4) & 0x0F, 0);
    if (status != HAL_OK)
    {
        return status;
    }

    // Envoi du nibble faible (bits 0 à 3)
    status = LCD_SendNibble(command & 0x0F, 0);
    if (status != HAL_OK)
    {
        return status;
    }

    return HAL_OK;
}

/**
  * @brief  Envoie un caractère ASCII à afficher sur le LCD en deux nibbles
  * @param  character: Le caractère à afficher
  * @retval HAL_StatusTypeDef : Statut de la transmission
  */
HAL_StatusTypeDef LCD_SendChar(char character)
{
    HAL_StatusTypeDef status;

    uint8_t high_nibble = (character >> 4) & 0x0F;
    uint8_t low_nibble  = character & 0x0F;

    // Envoi du nibble haut avec RS = 1 (mode donnée)
    status = LCD_SendNibble(high_nibble, 1);
    if (status != HAL_OK)
    {
        return status;
    }

    // Envoi du nibble faible avec RS = 1 (mode donnée)
    status = LCD_SendNibble(low_nibble, 1);
    if (status != HAL_OK)
    {
        return status;
    }

    return HAL_OK;
}

/**
  * @brief  Initialisation de  l'afficheur LCD en mode 4 bits (car de mon LCD_16X2)
  * @param  None
  * @retval HAL_StatusTypeDef : Statut de l'initialisation
  */
HAL_StatusTypeDef LCD_Init(void)
{
    HAL_StatusTypeDef status;

    HAL_Delay(50); // Attente de stabilisation après mise sous tension

    //forcée en mode 4 bits
    if ((status = LCD_SendNibble(0x03, 0)) != HAL_OK) return status;
    HAL_Delay(5);
    if ((status = LCD_SendNibble(0x03, 0)) != HAL_OK) return status;
    HAL_Delay(1);
    if ((status = LCD_SendNibble(0x03, 0)) != HAL_OK) return status;
    HAL_Delay(1);
    if ((status = LCD_SendNibble(0x02, 0)) != HAL_OK) return status; // Passage explicite en mode 4 bits
    HAL_Delay(1);

    // Configuration générale du LCD
    if ((status = LCD_SendCommand(0x28)) != HAL_OK) return status; // Interface 4 bits, 2 lignes, 16 colones
    if ((status = LCD_SendCommand(0x0C)) != HAL_OK) return status; // Affichage ON, curseur OFF, clignotement OFF
    if ((status = LCD_SendCommand(0x01)) != HAL_OK) return status; // Nettoyage de l écran
    HAL_Delay(2);
    if ((status = LCD_SendCommand(0x06)) != HAL_OK) return status; // Auto-incrémentation du curseur

    return HAL_OK;
}

/**
  * @brief  Positionne le curseur à une ligne et une colonne spécifiques sur l'écran
  * @param  row:    Numéro de la ligne (0 pour la première, 1 pour la seconde)
  * @param  column: Numéro de la colonne (0 à 15)
  * @retval HAL_StatusTypeDef : Statut de l'opération (HAL_OK ou HAL_ERROR si hors limites)
  */
HAL_StatusTypeDef LCD_SetCursor(uint8_t row, uint8_t column)
{
    uint8_t address_DDRAM;
    uint8_t commande;

    //  Vérification des limites de l'afficheur (16x2)
    if (row > 1 || column > 15)
    {
        return HAL_ERROR;
    }

    //  Calcul de l'adresse DDRAM en fonction de la ligne
    // (0x00 pour la 1ère ligne, 0x40 pour la 2ème ligne)
    if (row == 0)
    {
        address_DDRAM = 0x00 + column;
    }
    else
    {
        address_DDRAM = 0x40 + column;
    }

    //  Construction de la commande (Bit 7 à 1 pour Set DDRAM Address)
    commande = 0x80 | address_DDRAM;

    return LCD_SendCommand(commande);
}

/**
  * @brief  Affiche une chaîne de caractères complète sur le LCD
  * @param  texte: Pointeur vers la chaîne de caractères (terminée par '\0')
  * @retval HAL_StatusTypeDef : Statut de l'envoi
  */
HAL_StatusTypeDef LCD_SendString(char *texte)
{
    HAL_StatusTypeDef status;

    // Parcours de la chaîne jusqu'au caractère nul de fin
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
