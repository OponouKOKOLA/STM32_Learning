/* USER CODE BEGIN Header */
/**
  * @file           : main.c
  * @brief          : Mesure de fréquence par Timer Input Capture avec transmission UART
  * @project        : Timer_Input_Capture
  * @author         : Oponou KOKOLA
  *
  * @details        :
  *   1. TIM2 en Input Capture (Canal 1, PA0) : mesure la période d'un signal externe.
  *   2. TIM6 génère un signal de test sur PB0 (bascule toutes les 500 µs -> 1 kHz).
  *   3. La fréquence est calculée et envoyée sur USART2 (115200 bauds).
  *   4. Architecture non bloquante : interruptions + boucle principale.
  *
  * Câblage de test :  PB0 à PA0.
  *
  * Calculs :
  *   TIM2 : 84 MHz / (83 + 1) = 1 MHz -> 1 tick = 1 µs
  *   TIM6 : PSC = 83, ARR = 499 -> interruption toutes les 500 µs
  *          -> PB0 change d'état toutes les 500 µs -> période 1 ms -> 1 kHz
  *   Fréquence mesurée = 1 000 000 / diff (diff = nombre de ticks entre 2 fronts montants)

  ******************************************************************************
  */
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "main.h"

/* USER CODE BEGIN Includes */
#include <stdio.h>
#include <string.h>
#include <stdint.h>
/* USER CODE END Includes */

/* Private variables ---------------------------------------------------------*/
TIM_HandleTypeDef htim2;
TIM_HandleTypeDef htim6;
UART_HandleTypeDef huart2;

/* USER CODE BEGIN PV */

volatile uint32_t prev = 0;          // Compteur lors de la capture précédente
volatile uint32_t diff = 0;          // Ticks écoulés entre deux captures
volatile uint8_t  premier = 1;       // 1 = on attend encore la toute première capture
volatile uint8_t  mesure_prete = 0;  // 1 = une nouvelle mesure est disponible

const uint32_t freq_inter = 1000000U; // Fréquence du compteur TIM2 : 84 MHz / (83+1)
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_TIM2_Init(void);
static void MX_TIM6_Init(void);
static void MX_USART2_UART_Init(void);

int main(void)
{
  HAL_Init();
  SystemClock_Config();

  MX_GPIO_Init();
  MX_TIM2_Init();
  MX_TIM6_Init();
  MX_USART2_UART_Init();

  /* USER CODE BEGIN 2 */
  BSP_LED_Init(LED2);

  /* Démarrage des deux timers avec interruption */
  HAL_TIM_Base_Start_IT(&htim6);               // Génère le signal de test sur PB0
  HAL_TIM_IC_Start_IT(&htim2, TIM_CHANNEL_1);  // Capture les fronts montants sur PA0

  char     msg[64];       // Buffer du message à envoyer
  uint32_t freq;          // Fréquence calculée (Hz)
  uint32_t dernier = 0;   // Date (ms) du dernier affichage
  /* USER CODE END 2 */

  while (1)
  {
    /* USER CODE BEGIN 3 */
    /* Une mesure est dispo et au moins 500 ms depuis le dernier affichage.
     * Sans ce délai, on enverrait 1000 messages par seconde : trop pour
     * l'UART à 115200 bauds. */
    if (mesure_prete && (HAL_GetTick() - dernier >= 500))
    {
      mesure_prete = 0;
      dernier = HAL_GetTick();

      uint32_t d = diff;               // Copie locale (diff peut changer en interruption)
      if (d > 0)
      {
        freq = freq_inter / d;         // Fréquence = 1 MHz / nombre de ticks

        int len = snprintf(msg, sizeof(msg), "Periode = %lu us\r\n", (unsigned long)d);
        HAL_UART_Transmit(&huart2, (uint8_t*)msg, len, 100);

        HAL_Delay(10);

        len = snprintf(msg, sizeof(msg), "Frequence = %lu Hz\r\n", (unsigned long)freq);
        HAL_UART_Transmit(&huart2, (uint8_t*)msg, len, 100);

      }
    }
    /* USER CODE END 3 */
  }
}

void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

  __HAL_RCC_PWR_CLK_ENABLE();
  __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE3);

  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSI;
  RCC_OscInitStruct.PLL.PLLM = 16;
  RCC_OscInitStruct.PLL.PLLN = 336;
  RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV4;
  RCC_OscInitStruct.PLL.PLLQ = 2;
  RCC_OscInitStruct.PLL.PLLR = 2;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV2;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_2) != HAL_OK)
  {
    Error_Handler();
  }
}

/**
  * @brief TIM2 : Input Capture, compteur 32 bits à 1 MHz
  */
static void MX_TIM2_Init(void)
{
  TIM_ClockConfigTypeDef sClockSourceConfig = {0};
  TIM_MasterConfigTypeDef sMasterConfig = {0};
  TIM_IC_InitTypeDef sConfigIC = {0};

  htim2.Instance = TIM2;
  htim2.Init.Prescaler = 83;
  htim2.Init.CounterMode = TIM_COUNTERMODE_UP;
  htim2.Init.Period = 4294967295;
  htim2.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
  htim2.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
  if (HAL_TIM_Base_Init(&htim2) != HAL_OK)
  {
    Error_Handler();
  }
  sClockSourceConfig.ClockSource = TIM_CLOCKSOURCE_INTERNAL;
  if (HAL_TIM_ConfigClockSource(&htim2, &sClockSourceConfig) != HAL_OK)
  {
    Error_Handler();
  }
  if (HAL_TIM_IC_Init(&htim2) != HAL_OK)
  {
    Error_Handler();
  }
  sMasterConfig.MasterOutputTrigger = TIM_TRGO_RESET;
  sMasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
  if (HAL_TIMEx_MasterConfigSynchronization(&htim2, &sMasterConfig) != HAL_OK)
  {
    Error_Handler();
  }

  sConfigIC.ICPolarity = TIM_INPUTCHANNELPOLARITY_RISING;
  sConfigIC.ICSelection = TIM_ICSELECTION_DIRECTTI;
  sConfigIC.ICPrescaler = TIM_ICPSC_DIV1;
  sConfigIC.ICFilter = 0;
  if (HAL_TIM_IC_ConfigChannel(&htim2, &sConfigIC, TIM_CHANNEL_1) != HAL_OK)
  {
    Error_Handler();
  }
}

/**
  * @brief TIM6 : interruption toutes les 500 µs (signal de test)
  */
static void MX_TIM6_Init(void)
{
  TIM_MasterConfigTypeDef sMasterConfig = {0};

  htim6.Instance = TIM6;
  htim6.Init.Prescaler = 83;                 // 1 MHz -> 1 tick = 1 µs
  htim6.Init.CounterMode = TIM_COUNTERMODE_UP;
  htim6.Init.Period = 499;                   // 500 ticks = 500 µs
  htim6.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_ENABLE;
  if (HAL_TIM_Base_Init(&htim6) != HAL_OK)
  {
    Error_Handler();
  }
  sMasterConfig.MasterOutputTrigger = TIM_TRGO_RESET;
  sMasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
  if (HAL_TIMEx_MasterConfigSynchronization(&htim6, &sMasterConfig) != HAL_OK)
  {
    Error_Handler();
  }
}

static void MX_USART2_UART_Init(void)
{
  huart2.Instance = USART2;
  huart2.Init.BaudRate = 115200;
  huart2.Init.WordLength = UART_WORDLENGTH_8B;
  huart2.Init.StopBits = UART_STOPBITS_1;
  huart2.Init.Parity = UART_PARITY_NONE;
  huart2.Init.Mode = UART_MODE_TX_RX;
  huart2.Init.HwFlowCtl = UART_HWCONTROL_NONE;
  huart2.Init.OverSampling = UART_OVERSAMPLING_16;
  if (HAL_UART_Init(&huart2) != HAL_OK)
  {
    Error_Handler();
  }
}

/**
  * @brief GPIO : PB0 en sortie (signal de test généré par TIM6)
  */
static void MX_GPIO_Init(void)
{
  GPIO_InitTypeDef GPIO_InitStruct = {0};

  __HAL_RCC_GPIOC_CLK_ENABLE();
  __HAL_RCC_GPIOH_CLK_ENABLE();
  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();


  HAL_GPIO_WritePin(GPIOB, GPIO_PIN_0, GPIO_PIN_RESET);

  GPIO_InitStruct.Pin = GPIO_PIN_0;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);
}

/* USER CODE BEGIN 4 */

/**
  * @brief  Interruption TIM6 (toutes les 500 µs) : génère le signal de test sur PB0
  */
void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{
  if (htim->Instance == TIM6)
  {
	  HAL_GPIO_TogglePin(GPIOB, GPIO_PIN_0); // Signal carré de 1kHz (vérifié à l'analyseur logique)
  }
}

/**
  * @brief  Interruption d'Input Capture TIM2 (à chaque front montant sur PA0)
  */
void HAL_TIM_IC_CaptureCallback(TIM_HandleTypeDef *htim)
{
  if (htim->Instance == TIM2 && htim->Channel == HAL_TIM_ACTIVE_CHANNEL_1)
  {
	  // Valeur du compteur mémorisée dans CCR1 par le matériel au moment du front montant
	  uint32_t now = HAL_TIM_ReadCapturedValue(htim, TIM_CHANNEL_1);

    if (premier)
    {
      // Première capture
      prev = now;
      premier = 0;
    }
    else
    {
      // Période = différence entre deux captures
      diff = now - prev;
      prev = now;
      mesure_prete = 1;   // Prévient la boucle principale
    }
  }
}

/* USER CODE END 4 */

void Error_Handler(void)
{
  __disable_irq();
  while (1)
  {
  }
}

#ifdef USE_FULL_ASSERT
void assert_failed(uint8_t *file, uint32_t line)
{
}
#endif /* USE_FULL_ASSERT */
