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
#include <string.h>
#include <stdint.h>

/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
#define PIN_FLASH_ADDRESS 0x08060000U
#define PIN_MAGIC         0x12345678U
#define DEFAULT_PIN       1234U
/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
UART_HandleTypeDef huart2;

/* USER CODE BEGIN PV */

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_USART2_UART_Init(void);
/* USER CODE BEGIN PFP */
void Update_PIN(void);
/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */
HAL_StatusTypeDef Flash_Write_PIN(uint32_t pin)
{
    FLASH_EraseInitTypeDef erase;
    uint32_t error;
    HAL_FLASH_Unlock();
    // Erase Sector 7
    erase.TypeErase = FLASH_TYPEERASE_SECTORS;
    erase.Sector = FLASH_SECTOR_7;
    erase.NbSectors = 1;
    erase.VoltageRange = FLASH_VOLTAGE_RANGE_3;
    if (HAL_FLASHEx_Erase(&erase, &error) != HAL_OK)
    {
        HAL_FLASH_Lock();
        return HAL_ERROR;
    }
    // First store the PIN
    if (HAL_FLASH_Program(FLASH_TYPEPROGRAM_WORD, PIN_FLASH_ADDRESS + 4U, pin) != HAL_OK)
    {
        HAL_FLASH_Lock();
        return HAL_ERROR;
    }
    // Store validity marker last
    if (HAL_FLASH_Program(FLASH_TYPEPROGRAM_WORD, PIN_FLASH_ADDRESS, PIN_MAGIC) != HAL_OK)
    {
        HAL_FLASH_Lock();
        return HAL_ERROR;
    }
    HAL_FLASH_Lock();
    // Verify saved data
    if (*(volatile uint32_t *)(PIN_FLASH_ADDRESS) != PIN_MAGIC ||
        *(volatile uint32_t *)(PIN_FLASH_ADDRESS + 4U) != pin)
    {
        return HAL_ERROR;
    }

    return HAL_OK;
}
uint32_t Flash_Read_PIN(void)
{
    uint32_t pin;
    pin = *(volatile uint32_t *)(PIN_FLASH_ADDRESS + 4U);
    return pin;
}
HAL_StatusTypeDef PIN_Flash_Init(void)
{
    uint32_t magic;
    uint32_t pin;
    magic = *(volatile uint32_t *)PIN_FLASH_ADDRESS;
    pin = Flash_Read_PIN();

    // Check whether valid PIN data exists
    if (magic != PIN_MAGIC || pin > 9999U)
    {
        // Save default PIN
        return Flash_Write_PIN(DEFAULT_PIN);
    }

    return HAL_OK;
}
uint32_t UART_Read_PIN(void)
{
    uint8_t ch;
    uint32_t pin = 0;
    uint8_t count = 0;
    char message[] = "\r\nEnter 4-digit PIN: ";
    HAL_UART_Transmit(&huart2,(uint8_t *)message,strlen(message),HAL_MAX_DELAY);

    while (count < 4)
    {
        HAL_UART_Receive(&huart2,&ch, 1, HAL_MAX_DELAY);

        if (ch >= '0' && ch <= '9')
        {
            pin = (pin * 10U) + (ch - '0');
            HAL_UART_Transmit(&huart2,(uint8_t *)"*", 1, HAL_MAX_DELAY);
            count++;
        }
    }

    return pin;
}
void Verify_PIN(void)
{
    uint32_t stored_pin;
    uint32_t entered_pin;
    stored_pin = Flash_Read_PIN();
    entered_pin = UART_Read_PIN();
    if (entered_pin == stored_pin)
    {
        char success[] = "\r\nPIN Matched\r\n";
        HAL_UART_Transmit(&huart2, (uint8_t *)success, strlen(success), HAL_MAX_DELAY);
        Update_PIN();
    }
    else
    {
        char failure[] = "\r\nInvalid PIN\r\n";
        HAL_UART_Transmit(&huart2, (uint8_t *)failure, strlen(failure), HAL_MAX_DELAY);
    }
}

void Update_PIN(void)
{
    char command[10];
    uint8_t ch;
    uint8_t i = 0;
    uint32_t new_pin;
    char msg[] = "\r\nEnter command (CHANGE/EXIT): ";
    HAL_UART_Transmit(&huart2, (uint8_t *)msg,strlen(msg), HAL_MAX_DELAY);
    // Read command until Enter
    while (1)
    {
        HAL_UART_Receive(&huart2, &ch, 1, HAL_MAX_DELAY);
        if (ch == '\r' || ch == '\n')
            break;

        if (i < 9)
            command[i++] = ch;
    }
    command[i] = '\0';
    if (strcmp(command, "CHANGE") == 0)
    {
        char msg2[] = "\r\nEnter new PIN: ";
        HAL_UART_Transmit(&huart2, (uint8_t *)msg2,strlen(msg2), HAL_MAX_DELAY);
        new_pin = UART_Read_PIN();

        if (new_pin >= 1000 && new_pin <= 9999)
        {
            if (Flash_Write_PIN(new_pin) == HAL_OK)
            {
                char success[] = "\r\nPIN Updated Successfully!\r\n";

                HAL_UART_Transmit(&huart2, (uint8_t *)success,strlen(success), HAL_MAX_DELAY);
            }
            else
            {
                char error[] = "\r\nPIN Update Failed!\r\n";
                HAL_UART_Transmit(&huart2, (uint8_t *)error, strlen(error), HAL_MAX_DELAY);
            }
        }
        else
        {
            char error[] = "\r\nInvalid PIN!\r\n";
            HAL_UART_Transmit(&huart2, (uint8_t *)error, strlen(error), HAL_MAX_DELAY);
        }
    }
    else if (strcmp(command, "EXIT") == 0)
    {
        char msg3[] = "\r\nPIN not changed.\r\n";
        HAL_UART_Transmit(&huart2, (uint8_t *)msg3, strlen(msg3), HAL_MAX_DELAY);
    }
    else
    {
        char error[] = "\r\nInvalid command!\r\n";
        HAL_UART_Transmit(&huart2, (uint8_t *)error, strlen(error), HAL_MAX_DELAY);
    }
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
  /* USER CODE BEGIN 2 */
  char welcome[] = "\r\n--- SECURE ACCESS SYSTEM ---\r\n";

  HAL_UART_Transmit(&huart2,
                    (uint8_t *)welcome,
                    strlen(welcome),
                    HAL_MAX_DELAY);

  // Initialize Flash
  if (PIN_Flash_Init() != HAL_OK)
  {
      char error[] = "\r\nFlash Error!\r\n";

      HAL_UART_Transmit(&huart2,
                        (uint8_t *)error,
                        strlen(error),
                        HAL_MAX_DELAY);

      Error_Handler();
  }

  // Verify PIN
  Verify_PIN();
  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
  }
  /* USER CODE END 3 */
}

/**
  * @brief System Clock Configuration
  * @retval None
  */
void SystemClock_Config(void)
{
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
  GPIO_InitTypeDef GPIO_InitStruct = {0};
  /* USER CODE BEGIN MX_GPIO_Init_1 */

  /* USER CODE END MX_GPIO_Init_1 */

  /* GPIO Ports Clock Enable */
  __HAL_RCC_GPIOC_CLK_ENABLE();
  __HAL_RCC_GPIOH_CLK_ENABLE();
  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(LD2_GPIO_Port, LD2_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pin : B1_Pin */
  GPIO_InitStruct.Pin = B1_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_IT_FALLING;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(B1_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pin : LD2_Pin */
  GPIO_InitStruct.Pin = LD2_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(LD2_GPIO_Port, &GPIO_InitStruct);

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
  /* User can add his own implementation to report the HAL error return state */
  __disable_irq();
  while (1)
  {
  }
  /* USER CODE END Error_Handler_Debug */
}
#ifdef USE_FULL_ASSERT
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
