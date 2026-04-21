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

/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/

/* USER CODE BEGIN PV */

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
uint16_t ADCRead( void );

/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

/* USER CODE END 0 */

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{
  HAL_Init();
  SystemClock_Config();

  // Setting PE8 (LD8-BLUE) as digital output
  RCC->AHBENR |= RCC_AHBENR_GPIODEN;
  GPIOD->MODER |= GPIO_MODER_MODER8_0;

  // [18]:	IOPBEN:			I/O port B clock enable					=	1		->	I/O port B clock enabled
  RCC->AHBENR	|=	RCC_AHBENR_GPIOBEN;
  // [31:30]:	MODER15[1:0]:	Pin B.15 mode						=	11		->	Analog function mode
  GPIOB->MODER	|=	GPIO_MODER_MODER15;

  // [13:9]:	ADC34PRES[4:0]:	ADC34 prescaler						=	10000	->	PLL clock divided by 1
  RCC->CFGR2		|=	RCC_CFGR2_ADCPRE34_DIV1;

  // [29]:	ADC34EN:		ADC3 and ADC4 enable					=	1		->	ADC3 and ADC4 clock enabled
  RCC->AHBENR		|=	RCC_AHBENR_ADC34EN;

  // [29:28]:	ADVREGEN[1:0]:	ADC voltage regulator enable		=	00		->	Intermediate state
  // [29:28]:	ADVREGEN[1:0]:	ADC voltage regulator enable		=	01		->	Enabled state
  // [31]:		ADCAL:			ADC calibration						=	1		->	Start ADC calibration
  ADC4->CR		&=	~ADC_CR_ADVREGEN;	// In order to enable the regulator, first it must be switched
  ADC4->CR		|=	ADC_CR_ADVREGEN_0;	// into the intermediate state, then into the enabled one and
  HAL_Delay(1);							// finally must wait ~1ms in order to stabilize
  ADC4->CR		|=	ADC_CR_ADCAL;
  while( ADC4->CR & ADC_CR_ADCAL );		// Waiting for calibration to finish

  // [4:3]:	RES[1:0]:		Data resolution						=	00		->	12-bit
  // [5]:	ALIGN:			Data alignment						=	0		->	Right alignment
  // [13]:	MODER9[1:0]:	Single/continuous conversion mode 	=	0		->	Single conversion mode
  //							for regular conversions
  ADC4->CFGR		&=	~ADC_CFGR_RES;
  ADC4->CFGR		&=	~ADC_CFGR_ALIGN;
  ADC4->CFGR		&=	~ADC_CFGR_CONT;

  // [3:0]:		L[3:0]:			 Regular channel sequence length	=	0000	->	1 conversion
  // [10:6]:	SQ1[4:0]:		 1st conversion in regular sequence	=	00101	->	Channel 5 is mapped into 1st
  //																				conversion placeholder
  ADC4->SQR1		&=	~ADC_SQR1_L;
  ADC4->SQR1		|=	5<<6;

  // [19:18]:	SMP5[2:0]:		Channel 5 sampling time selection	=	000		->	1.5 ADC clock cycles
  ADC4->SMPR1		&=	~ADC_SMPR1_SMP5;

  // [0]:		ADEN:			ADC enable control					=	0		->	ADC Enabled
  ADC4->CR		|=	ADC_CR_ADEN;
  while( !( ADC4->ISR & ADC_ISR_ADRD ) ); // Waiting for ADC to stabilize

  // Setting PA4 as DAC
  RCC->AHBENR |= RCC_AHBENR_GPIOAEN;
  GPIOA->MODER |= GPIO_MODER_MODER4;
  RCC->APB1ENR |= RCC_APB1ENR_DAC1EN;
  DAC->CR |= DAC_CR_EN1;


  /* Configuring TIM2 as Base Timer with T = 0.5s with interruption support */
  RCC->APB1ENR |= RCC_APB1ENR_TIM2EN;
  TIM2->PSC = 1 - 1;
  TIM2->ARR = 72000 - 1;
  TIM2->CNT = 0;
  TIM2->DIER |= TIM_DIER_UIE;
  NVIC_EnableIRQ(TIM2_IRQn);
  __enable_irq();
  TIM2->CR1 |= TIM_CR1_CEN;


  while (1)
  {

  }
}

void TIM2_IRQHandler(void)
{
  // Number of elements in input vector and filter
  int32_t     K = 3;

  // Inputs vector
  static float X[3] = {};

  // Filter coefficients
  static float B[3] = {1.0, 0.0, 0.0};

  // Output per sample
  float y = 0.0;

  // Index of new input sample
  static int32_t xZero = 0;

  // Counter for the number of elements in the inputs vector
  int32_t     k = 0;

  TIM2->SR &= ~TIM_SR_UIF;
  GPIOD->ODR |= (1<<8);

  X[xZero] = (float)ADCRead();   // Save the new sample into the zero of the inputs vector
  y = 0.0;
  for( k=0; k<K; k++ )    // Make convolution between the filter vector and inputs vector
  {
    y += X[xZero]*B[k];
    if( --xZero < 0 )     // Traverse circularly the inputs vector.
                              // If going before the index zero, set index
                              // to the greatest one inside the vector
    {
      xZero = K - 1;
    }
  }
  if( ++xZero > K - 1 )   // Set a new xZero value in order to save next sample
                              // If going after the greatest index in the array, set
                              // index to zero
  {
    xZero = 0;
  }

  if(y > 4095.0)
      y = 4095.0;
  else if(y < 0.0)
      y = 0.0;

  DAC->DHR12R1 = (uint16_t)y;
  GPIOD->ODR &= ~(1<<8);
}

uint16_t ADCRead( void )
{
	uint16_t adcInput;

	// [2]:		ADSTART:		ADC start of regular conversion		=	1		->	Regular Conversion Started
	ADC4->CR |= ADC_CR_ADSTART;
	while( !( ADC4->ISR & ADC_ISR_EOC ) );	// Waiting for end of conversion
	adcInput = ADC4->DR & 0xFFF;			// Reading and adjusting ADC 12-bit value

	return adcInput;						// Returning ADC data
}


/**
  * @brief System Clock Configuration
  * @retval None
  */
void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};
  RCC_PeriphCLKInitTypeDef PeriphClkInit = {0};

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI|RCC_OSCILLATORTYPE_HSE;
  RCC_OscInitStruct.HSEState = RCC_HSE_BYPASS;
  RCC_OscInitStruct.HSEPredivValue = RCC_HSE_PREDIV_DIV1;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
  RCC_OscInitStruct.PLL.PLLMUL = RCC_PLL_MUL6;
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

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_1) != HAL_OK)
  {
    Error_Handler();
  }
  PeriphClkInit.PeriphClockSelection = RCC_PERIPHCLK_USB|RCC_PERIPHCLK_I2C1;
  PeriphClkInit.I2c1ClockSelection = RCC_I2C1CLKSOURCE_HSI;
  PeriphClkInit.USBClockSelection = RCC_USBCLKSOURCE_PLL;
  if (HAL_RCCEx_PeriphCLKConfig(&PeriphClkInit) != HAL_OK)
  {
    Error_Handler();
  }
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
