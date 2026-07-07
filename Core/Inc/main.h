/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.h
  * @brief          : Header for main.c file.
  *                   This file contains the common defines of the application.
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

/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef __MAIN_H
#define __MAIN_H

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include "stm32h7xx_hal.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */

/* USER CODE END Includes */

/* Exported types ------------------------------------------------------------*/
/* USER CODE BEGIN ET */

/* USER CODE END ET */

/* Exported constants --------------------------------------------------------*/
/* USER CODE BEGIN EC */

/* USER CODE END EC */

/* Exported macro ------------------------------------------------------------*/
/* USER CODE BEGIN EM */

/* USER CODE END EM */

/* Exported functions prototypes ---------------------------------------------*/
void Error_Handler(void);

/* USER CODE BEGIN EFP */

/* USER CODE END EFP */

/* Private defines -----------------------------------------------------------*/
#define DAC_XSMT_Pin GPIO_PIN_3
#define DAC_XSMT_GPIO_Port GPIOE
#define PWR5V_EN_Pin GPIO_PIN_13
#define PWR5V_EN_GPIO_Port GPIOC
#define AMP_EN_Pin GPIO_PIN_2
#define AMP_EN_GPIO_Port GPIOA
#define CHG_STAT_Pin GPIO_PIN_4
#define CHG_STAT_GPIO_Port GPIOA
#define BTN_PLAY_Pin GPIO_PIN_7
#define BTN_PLAY_GPIO_Port GPIOA
#define BTN_PLAY_EXTI_IRQn EXTI9_5_IRQn
#define PG_STAT_Pin GPIO_PIN_4
#define PG_STAT_GPIO_Port GPIOC
#define JACK_DET_Pin GPIO_PIN_5
#define JACK_DET_GPIO_Port GPIOC
#define JACK_DET_EXTI_IRQn EXTI9_5_IRQn
#define PWR_SW_SENSE_Pin GPIO_PIN_7
#define PWR_SW_SENSE_GPIO_Port GPIOE
#define BTN_NEXT_Pin GPIO_PIN_8
#define BTN_NEXT_GPIO_Port GPIOE
#define BTN_NEXT_EXTI_IRQn EXTI9_5_IRQn
#define BTN_PREV_Pin GPIO_PIN_9
#define BTN_PREV_GPIO_Port GPIOE
#define BTN_PREV_EXTI_IRQn EXTI9_5_IRQn
#define BTN_MENU_Pin GPIO_PIN_10
#define BTN_MENU_GPIO_Port GPIOE
#define BTN_MENU_EXTI_IRQn EXTI15_10_IRQn
#define VOL_UP_Pin GPIO_PIN_11
#define VOL_UP_GPIO_Port GPIOE
#define VOL_UP_EXTI_IRQn EXTI15_10_IRQn
#define VOL_DOWN_Pin GPIO_PIN_12
#define VOL_DOWN_GPIO_Port GPIOE
#define VOL_DOWN_EXTI_IRQn EXTI15_10_IRQn
#define BTN_FN_Pin GPIO_PIN_13
#define BTN_FN_GPIO_Port GPIOE
#define BTN_FN_EXTI_IRQn EXTI15_10_IRQn
#define DISP_TE_Pin GPIO_PIN_14
#define DISP_TE_GPIO_Port GPIOE
#define DISP_TE_EXTI_IRQn EXTI15_10_IRQn
#define PWR_HOLD_Pin GPIO_PIN_15
#define PWR_HOLD_GPIO_Port GPIOE
#define EMMC_RST_Pin GPIO_PIN_8
#define EMMC_RST_GPIO_Port GPIOD
#define PMIC_EN_Pin GPIO_PIN_9
#define PMIC_EN_GPIO_Port GPIOD
#define PMIC_CTRL_Pin GPIO_PIN_10
#define PMIC_CTRL_GPIO_Port GPIOD
#define DISP_RST_Pin GPIO_PIN_13
#define DISP_RST_GPIO_Port GPIOD
#define N_BTN_Pin GPIO_PIN_15
#define N_BTN_GPIO_Port GPIOD
#define N_BTN_EXTI_IRQn EXTI15_10_IRQn
#define SD_CD_Pin GPIO_PIN_6
#define SD_CD_GPIO_Port GPIOC
#define SD_CD_EXTI_IRQn EXTI9_5_IRQn
#define USB_PHY_RST_Pin GPIO_PIN_11
#define USB_PHY_RST_GPIO_Port GPIOA

/* USER CODE BEGIN Private defines */

/* USER CODE END Private defines */

#ifdef __cplusplus
}
#endif

#endif /* __MAIN_H */
