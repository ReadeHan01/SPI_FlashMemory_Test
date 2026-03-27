/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2026 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "main.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */

/* Winbond W25Q SPI NOR flash driver: uses global hspi1 and GPIO CS (see MX_GPIO_Init). */
#include "w25qxx.h"

/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

/* Max time HAL_UART_Transmit waits for the shift register / line (ms); avoids infinite stall. */
#define UART_TX_TIMEOUT_MS    1000U

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
SPI_HandleTypeDef hspi1;

UART_HandleTypeDef huart2;

/* USER CODE BEGIN PV */

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_USART2_UART_Init(void);
static void MX_SPI1_Init(void);
/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

/* Return length of NUL-terminated string s (excluding '\0'); 0 if s is NULL (safe for HAL). */
static uint16_t Uart_StringLen(const char *s)
{
  uint16_t n = 0U;

  if (s == NULL)
  {
    return 0U;
  }

  /* Count bytes until terminator so we know how many to send. */
  while (s[n] != '\0')
  {
    n++;
  }
  return n;
}

/* Blocking TX of a C string on USART2 (what you see on serial terminal). */
static void Uart_Print(const char *s)
{
  if (s == NULL)
  {
    return;
  }

  (void)HAL_UART_Transmit(&huart2, (uint8_t *)s, Uart_StringLen(s), UART_TX_TIMEOUT_MS);
}

/* CRLF so typical terminals start the next line correctly (not only LF). */
static void Uart_PrintNewline(void)
{
  const char nl[] = "\r\n";

  (void)HAL_UART_Transmit(&huart2, (uint8_t *)nl, 2U, UART_TX_TIMEOUT_MS);
}

/* One byte as two hex ASCII chars plus a trailing space for readable dumps. */
static void Uart_PrintHexByte(uint8_t b)
{
  const char hex[] = "0123456789ABCDEF";
  char out[3];

  out[0] = hex[(b >> 4) & 0x0F]; /* Upper nibble -> one of 0..F */
  out[1] = hex[b & 0x0F];        /* Lower nibble */
  out[2] = ' ';
  (void)HAL_UART_Transmit(&huart2, (uint8_t *)out, 3U, UART_TX_TIMEOUT_MS);
}

/* USER CODE END 0 */

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{

  /* USER CODE BEGIN 1 */

  /* USER CODE END 1 */

  /* MCU Configuration--------------------------------------------------------*/

  /* Reset of all peripherals, Initializes the Flash interface and the Systick. */
  HAL_Init();

  /* USER CODE BEGIN Init */

  /* USER CODE END Init */

  /* Configure the system clock */
  SystemClock_Config();

  /* USER CODE BEGIN SysInit */

  /* USER CODE END SysInit */

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  MX_USART2_UART_Init();
  MX_SPI1_Init();
  /* USER CODE BEGIN 2 */
  /* One-time driver setup: chip detect, sizes, default CS/SPI usage. false = ID/read failed. */
  if (W25qxx_Init() == false)
  {
    Uart_Print("\r\nW25Q init FAIL\r\n");
  }
  else
  {
    Uart_Print("\r\nW25Q init OK\r\n");
  }

  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
    /* 24-bit JEDEC ID: [23:16] manufacturer (Winbond 0xEF), [15:8] memory type, [7:0] capacity ID. */
    uint32_t jedec = 0U;
    /* Run destructive erase/program/verify only once so flash does not churn every second. */
    static uint8_t didWriteTest = 0U;
    static const uint8_t pattern[16] = {
      0x00U, 0x11U, 0x22U, 0x33U, 0x44U, 0x55U, 0x66U, 0x77U,
      0x88U, 0x99U, 0xAAU, 0xBBU, 0xCCU, 0xDDU, 0xEEU, 0xFFU
    };
    uint8_t data[16] = {0};   /* Snapshot read from flash @ address 0 before optional write. */
    uint8_t after[16] = {0};  /* Read-back after program for verify. */

    Uart_Print("\r\nW25Q16 SPI test\r\n");

    jedec = W25qxx_ReadID();
    Uart_Print("JEDEC: ");
    /* Print MSB..LSB so it matches datasheet byte order: MFID, type, density. */
    Uart_PrintHexByte((uint8_t)((jedec >> 16) & 0xFFU));
    Uart_PrintHexByte((uint8_t)((jedec >> 8) & 0xFFU));
    Uart_PrintHexByte((uint8_t)(jedec & 0xFFU));
    Uart_PrintNewline();

    /* Fast read of first 16 bytes (non-destructive); often 0xFF if erased. */
    W25qxx_ReadBytes(data, 0U, (uint32_t)sizeof(data));
    Uart_Print("DATA[0..15]: ");
    for (uint16_t i = 0; i < (uint16_t)sizeof(data); i++)
    {
      Uart_PrintHexByte(data[i]);
    }
    Uart_PrintNewline();

    if (didWriteTest == 0U)
    {
      /* W25Q sector erase = 4 KiB; sector index 0 => starts at flash byte address 0. */
      Uart_Print("Erase 4KB @0x000000 ...\r\n");
      W25qxx_EraseSector(0U);
      Uart_Print("Erase OK\r\n");

      /* WriteSector: offset 0 within sector 0; length must fit page/sector rules in driver. */
      Uart_Print("Program 16 bytes @0x000000 ...\r\n");
      W25qxx_WriteSector((uint8_t *)pattern, 0U, 0U, (uint32_t)sizeof(pattern));
      Uart_Print("Program OK\r\n");

      W25qxx_ReadBytes(after, 0U, (uint32_t)sizeof(after));
      Uart_Print("AFTER[0..15]: ");
      for (uint16_t i = 0; i < (uint16_t)sizeof(after); i++)
      {
        Uart_PrintHexByte(after[i]);
      }
      Uart_PrintNewline();

      /* Byte-for-byte compare: any mismatch => VERIFY BAD. */
      uint8_t ok = 1U;
      for (uint16_t i = 0; i < (uint16_t)sizeof(after); i++)
      {
        if (after[i] != pattern[i])
        {
          ok = 0U;
        }
      }
      if (ok != 0U)
      {
        Uart_Print("VERIFY: OK\r\n");
      }
      else
      {
        Uart_Print("VERIFY: BAD\r\n");
      }

      didWriteTest = 1U;
    }

    /* Pace the loop so the serial log is readable and UART is not flooded. */
    HAL_Delay(1000U);
  }
  /* USER CODE END 3 */
}

/**
  * @brief System Clock Configuration
  * @retval None
  */
void SystemClock_Config(void)
{
  /* HSI + PLL: establishes SYSCLK and AHB/APB dividers; peripherals (SPI1, USART2) clock from these buses. */
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

  /** Configure the main internal regulator output voltage
  */
  __HAL_RCC_PWR_CLK_ENABLE();
  __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE3);

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
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

  /** Initializes the CPU, AHB and APB buses clocks
  */
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
  * @brief SPI1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_SPI1_Init(void)
{

  /* USER CODE BEGIN SPI1_Init 0 */

  /* USER CODE END SPI1_Init 0 */

  /* USER CODE BEGIN SPI1_Init 1 */

  /* USER CODE END SPI1_Init 1 */
  /* SPI master to W25Q: 8-bit frames, CPOL/CPHA 0, MSB first; NSS software — CS is a separate GPIO the driver toggles. */
  /* SPI1 parameter configuration*/
  hspi1.Instance = SPI1;
  hspi1.Init.Mode = SPI_MODE_MASTER;
  hspi1.Init.Direction = SPI_DIRECTION_2LINES;
  hspi1.Init.DataSize = SPI_DATASIZE_8BIT;
  hspi1.Init.CLKPolarity = SPI_POLARITY_LOW;
  hspi1.Init.CLKPhase = SPI_PHASE_1EDGE;
  hspi1.Init.NSS = SPI_NSS_SOFT;
  hspi1.Init.BaudRatePrescaler = SPI_BAUDRATEPRESCALER_16;
  hspi1.Init.FirstBit = SPI_FIRSTBIT_MSB;
  hspi1.Init.TIMode = SPI_TIMODE_DISABLE;
  hspi1.Init.CRCCalculation = SPI_CRCCALCULATION_DISABLE;
  hspi1.Init.CRCPolynomial = 10;
  if (HAL_SPI_Init(&hspi1) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN SPI1_Init 2 */

  /* USER CODE END SPI1_Init 2 */

}

/**
  * @brief USART2 Initialization Function
  * @param None
  * @retval None
  */
static void MX_USART2_UART_Init(void)
{

  /* USER CODE BEGIN USART2_Init 0 */

  /* USER CODE END USART2_Init 0 */

  /* USER CODE BEGIN USART2_Init 1 */

  /* USER CODE END USART2_Init 1 */
  /* USART2 @ 115200 8N1: primary debug/log link from the board to a PC terminal. */
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
  /* USER CODE BEGIN USART2_Init 2 */

  /* USER CODE END USART2_Init 2 */

}

/**
  * @brief GPIO Initialization Function
  * @param None
  * @retval None
  */
static void MX_GPIO_Init(void)
{
  /* Enables clocks and CS output for SPI flash (chip-select is active-low); B1 is user button with EXTI. */
  GPIO_InitTypeDef GPIO_InitStruct = {0};
  /* USER CODE BEGIN MX_GPIO_Init_1 */

  /* USER CODE END MX_GPIO_Init_1 */

  /* GPIO Ports Clock Enable */
  __HAL_RCC_GPIOC_CLK_ENABLE();
  __HAL_RCC_GPIOH_CLK_ENABLE();
  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(SPI1_CS_GPIO_Port, SPI1_CS_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pin : B1_Pin */
  GPIO_InitStruct.Pin = B1_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_IT_FALLING;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(B1_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pin : SPI1_CS_Pin */
  GPIO_InitStruct.Pin = SPI1_CS_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(SPI1_CS_GPIO_Port, &GPIO_InitStruct);

  /* USER CODE BEGIN MX_GPIO_Init_2 */

  /* USER CODE END MX_GPIO_Init_2 */
}

/* USER CODE BEGIN 4 */

/* USER CODE END 4 */

/**
  * @brief  This function is executed in case of error occurrence.
  * @retval None
  */
void Error_Handler(void)
{
  /* USER CODE BEGIN Error_Handler_Debug */
  /* HAL unrecoverable path: mask interrupts and park here (attach debugger / LED / reset policy as you like). */
  /* User can add his own implementation to report the HAL error return state */
  __disable_irq();
  while (1)
  {
  }
  /* USER CODE END Error_Handler_Debug */
}

#ifdef  USE_FULL_ASSERT
/**
  * @brief  Reports the name of the source file and the source line number
  *         where the assert_param error has occurred.
  * @param  file: pointer to the source file name
  * @param  line: assert_param error line source number
  * @retval None
  */
void assert_failed(uint8_t *file, uint32_t line)
{
  /* USER CODE BEGIN 6 */
  /* User can add his own implementation to report the file name and line number,
     ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */
