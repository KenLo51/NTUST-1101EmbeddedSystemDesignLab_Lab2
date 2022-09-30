/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
  ******************************************************************************
  * @attention
  *
  * <h2><center>&copy; Copyright (c) 2021 STMicroelectronics.
  * All rights reserved.</center></h2>
  *
  * This software component is licensed by ST under BSD 3-Clause license,
  * the "License"; You may not use this file except in compliance with the
  * License. You may obtain a copy of the License at:
  *                        opensource.org/licenses/BSD-3-Clause
  *
  ******************************************************************************
  */
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "main.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "stm32746g_discovery_lcd.h"
#include "stm32746g_discovery_ts.h"
#include "stm32746g_discovery_sdram.h"
#include "stm32f7xx_hal.h"

#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <math.h>
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */
typedef struct{
	float x, y;
}vec2;

typedef struct{//postition在中心
	vec2 prePos;
	vec2 pos;
	uint16_t width, height;
	uint8_t holded;
}paddle_typedef;
typedef struct{//postition在中心
	vec2 prePos;
	vec2 pos;//postition
	vec2 vel;//vel
	float radius;
}ball_typedef;
typedef struct{//postition在左上
	vec2 pos;
	uint16_t width, height;
	uint8_t destroyed, preState;
}block_typedef;
typedef struct{
	uint16_t width, height;
	uint8_t row_size, col_size;
	block_typedef *blocks;
	uint16_t blocks_num;
}board_typedef;
typedef struct{
	board_typedef board;
	ball_typedef ball;
	paddle_typedef paddle;
	uint16_t time_sec, score;
	uint8_t updateTime;
}Breakout_typedef;

typedef struct{
	uint16_t width, height;
	uint8_t row_size, col_size;
	uint8_t *blocks;
}Level_typedef;
const uint8_t lv1_blocks[] = {1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1};
const Level_typedef level1 = {.row_size=8, .col_size=2, .blocks=(uint8_t*)lv1_blocks};
/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
#define PI 3.1415926535897f

#define BLOCK_WIDTH 56//default 56 (pixel)
#define BLOCK_HEIGHT 10//default 10 (pixel)
#define BLOCK_MARGIN 4//default 4 (pixel)
#define BLOCK_STATE_ALIVE 0
#define BLOCK_STATE_DESTORYED 1
#define BLOCK_STATE_DISAPPEARING 2


#define PADDLE_WIDTH 60//default 60 (pixel)
#define PADDLE_HEIGHT 5//default 10 (pixel)

#define BALL_SPEED 1.0f//default 1 (pixel/10ms)
#define BALL_RADIOUS 5.0f//default 5

#define RESULT_GAMEOVER 1
#define RESULT_WIN 2
#define RESULT_CONTINUE 0

#define numInRange(v,min,max) ( ((v)>=(min)) && ((v)<=(max)) )
/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/

DMA2D_HandleTypeDef hdma2d;

LTDC_HandleTypeDef hltdc;

TIM_HandleTypeDef htim6;

UART_HandleTypeDef huart1;

SDRAM_HandleTypeDef hsdram1;

/* USER CODE BEGIN PV */
uint8_t flag_1ms;
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_DMA2D_Init(void);
static void MX_FMC_Init(void);
static void MX_LTDC_Init(void);
static void MX_TIM6_Init(void);
static void MX_USART1_UART_Init(void);
/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */
char text[80];
void vec2Rotation(vec2* tar, float deg){//unuse
	vec2 out;
	out.x = tar->x*cos(deg) - tar->y*sin(deg);
	out.y = tar->x*cos(deg) - tar->y*sin(deg);
	*tar = out;
}
void vec2Reflection(vec2* tar, float deg){//unuse
	vec2 out;
	out.x = tar->x*(cos(deg)*cos(deg)-1) + tar->y*(cos(deg)*sin(deg));
	out.y = tar->x*(cos(deg)*sin(deg))   + tar->y*(-cos(deg)*cos(deg) + sin(deg)*sin(deg)) ;
	*tar = out;
}
void vec2ReflectionSlope(vec2* tar, vec2 slop){
	vec2 out;
	//if(slop.x == 0.0f) return;
	//if(slop.y == 0.0f) return;
	float r = sqrt((slop.x*slop.x)+(slop.y*slop.y));
	out.x = tar->x*(slop.x*slop.x-slop.y*slop.y) + tar->y*(2*slop.x*slop.y);
	out.x = out.x / (r*r);
	out.y = tar->x*(2*slop.x*slop.y) + tar->y*(slop.y*slop.y-slop.x*slop.x);
	out.y = out.y / (r*r);
	*tar = out;
}
void LCDreprintLevel(Breakout_typedef* level){
	BSP_LCD_Clear(LCD_COLOR_BLACK);
	//draw time
	char time_str[7];
	sprintf(time_str, "%3d:%-3d", (level->time_sec)/60, (level->time_sec)%60);
	BSP_LCD_DisplayStringAt(0, 18, (uint8_t*)time_str, CENTER_MODE);
	
	//draw blocks
	uint16_t i;
	for(i=0; i<(level->board.blocks_num);i++){
		BSP_LCD_FillRect((uint16_t)level->board.blocks[i].pos.x, (uint16_t)level->board.blocks[i].pos.y, level->board.blocks[i].width, level->board.blocks[i].height);
	}
	
	//draw ball
	BSP_LCD_FillCircle((uint16_t)level->ball.pos.x, (uint16_t)level->ball.pos.y, (uint16_t)level->ball.radius);

	//draw paddle
	BSP_LCD_FillRect((uint16_t)(level->paddle.pos.x - level->paddle.width/2), (uint16_t)(level->paddle.pos.y - level->paddle.height/2), level->paddle.width, level->paddle.height);

}

Breakout_typedef* newGames(const Level_typedef* level){
	//copy level
	Breakout_typedef* newLevel = (Breakout_typedef*)malloc(sizeof(Breakout_typedef));
	
	newLevel->board.blocks_num = 0;
	newLevel->board.blocks = (block_typedef*)malloc(sizeof(block_typedef) * (level->col_size)*(level->row_size));
	for(uint16_t i=0; i<(level->col_size)*(level->row_size); i++){
		uint16_t x, y;
		if(level->blocks[i]){
			x = i%level->row_size;
			y = i/level->row_size;
			newLevel->board.blocks[i].destroyed = 0;
			newLevel->board.blocks[i].preState = 0;
			newLevel->board.blocks[i].width = BLOCK_WIDTH;
			newLevel->board.blocks[i].height = BLOCK_HEIGHT;
			newLevel->board.blocks[i].pos.x = BLOCK_MARGIN/2.0 + x*(BLOCK_WIDTH+BLOCK_MARGIN);
			newLevel->board.blocks[i].pos.y = 40+BLOCK_MARGIN/2.0 + y*(BLOCK_HEIGHT+BLOCK_MARGIN);
			newLevel->board.blocks_num ++;
		}
	}
	newLevel->board.height = 272;
	newLevel->board.width = 480;

	newLevel->ball.pos.x = 480.0f/2.0f;
	newLevel->ball.pos.y = 272.0f/2;
	newLevel->ball.prePos = newLevel->ball.pos;
	newLevel->ball.radius = BALL_RADIOUS;
	newLevel->ball.vel.x = 0.0f;
	newLevel->ball.vel.y = 0.0f;
	
	newLevel->paddle.pos.x = 480/2;
	newLevel->paddle.pos.y = 272-35;
	newLevel->paddle.prePos = newLevel->paddle.pos;
	newLevel->paddle.width = PADDLE_WIDTH;
	newLevel->paddle.height = PADDLE_HEIGHT;
	newLevel->paddle.holded = 0;
	
	newLevel->score = 0;
	newLevel->time_sec = 0;
	newLevel->updateTime = 0;
	
	return newLevel;
}
void BreakoutInit(Breakout_typedef* level){
	float direction;
	while(1){
		direction = (float)rand()/RAND_MAX*2*PI;
		if(direction<0.1*PI)continue;
		if(direction>1.9*PI)continue;
		if(numInRange(direction, 0.9*PI, 1.1*PI))continue;
		break;
	}
	level->ball.vel.x = cos(direction)*BALL_SPEED;
	level->ball.vel.y = sin(direction)*BALL_SPEED;
}
uint8_t boardCollision(ball_typedef* ball, board_typedef* board){//邊界碰撞
	if(ball->pos.x < 0.0f+(ball->radius)){//超過左邊界
		ball->pos.x = (0.0f+(ball->radius)) + ( (0.0f+(ball->radius)) - ball->pos.x );
		ball->vel.x = -ball->vel.x;
	}
	if(ball->pos.x > (board->width)-(ball->radius)){//超過右邊界
		ball->pos.x = ((board->width)-(ball->radius)) - (ball->pos.x - ((board->width)-(ball->radius)));
		ball->vel.x = -ball->vel.x;
	}
	if(ball->pos.y < 0.0f+(ball->radius)){//超過上邊界
		ball->pos.y = (0.0f+(ball->radius)) + ( (0.0f+(ball->radius)) - ball->pos.y );
		ball->vel.y = -ball->vel.y;
	}
	if(ball->pos.y > (board->height)-(ball->radius)){//超過下邊界
		ball->pos.y = ((board->height)-(ball->radius)) - ( ball->pos.y - ((board->height)-(ball->radius)) );
		ball->vel.y = -ball->vel.y;
	}
	return 0;
}
uint8_t paddleCollision(ball_typedef* ball, paddle_typedef* paddle){//反彈板子碰撞
	if(!numInRange(ball->pos.y, paddle->pos.y-paddle->height/2.0-ball->radius, paddle->pos.y+paddle->height/2.0+ball->radius)) return 0;
	if(!numInRange(ball->pos.x, paddle->pos.x-paddle->width/2.0 - ball->radius, paddle->pos.x+paddle->width/2.0 + ball->radius))return 0;
	
	//在反彈板子內
	float bound_y = paddle->pos.y - (ball->radius + PADDLE_HEIGHT/2.0);
	ball->pos.y = bound_y - (ball->pos.y - (bound_y));
	
	float direction = -( (paddle->pos.x+paddle->width/2.0)-ball->pos.x)/paddle->width*PI ;
	if(direction>-0.1*PI) direction = -0.1*PI;
	if(direction<-0.9*PI) direction = -0.9*PI;
	ball->vel.x = cos(direction)*BALL_SPEED;
	ball->vel.y = sin(direction)*BALL_SPEED; 
	//ball->vel.y = -ball->vel.y;
	return 0;
}
uint8_t blockCollision(ball_typedef* ball, block_typedef* block){
	uint8_t inBlockXRange = numInRange(ball->pos.x, block->pos.x-ball->radius, block->pos.x+block->width+ball->radius);
	uint8_t inBlockYRange = numInRange(ball->pos.y, block->pos.y-ball->radius, block->pos.y+block->height+ball->radius);
	if(!inBlockXRange) return 0;
	if(!inBlockYRange) return 0;
	
	if(numInRange(ball->pos.x, block->pos.x, block->pos.x+block->width)){
		if(numInRange(ball->pos.y, block->pos.y+block->height, block->pos.y+block->height+ball->radius)){//向下彈
			ball->vel.y = -ball->vel.y;
			//block->destroyed = BLOCK_STATE_DESTORYED;
			return 1;
		}
		if(numInRange(ball->pos.y, block->pos.y-ball->radius, block->pos.y)){//向上彈
			ball->vel.y = -ball->vel.y;
			//block->destroyed = BLOCK_STATE_DESTORYED;
			return 1;
		}
	}
	if(numInRange(ball->pos.y, block->pos.y, block->pos.y+block->height)){
		if(numInRange(ball->pos.x, block->pos.x-ball->radius, block->pos.x)){//向左彈
			ball->vel.x = -ball->vel.x;
			//block->destroyed = BLOCK_STATE_DESTORYED;
			return 1;
		}
		if(numInRange(ball->pos.x, block->pos.x+block->width, block->pos.x+block->width+ball->radius)){//向右彈
			ball->vel.x = -ball->vel.x;
			//block->destroyed = BLOCK_STATE_DESTORYED;
			return 1;
		}
	}
	
	vec2 ballPos = ball->pos;
	ballPos.x = ballPos.x - block->pos.x;
	ballPos.y = ballPos.y - block->pos.y;
	if(ballPos.x >= block->width) ballPos.x-=block->width;
	if(ballPos.y >= block->height) ballPos.y-=block->height;

	if(fabs(ballPos.x + ballPos.y) < ball->radius ){//四邊三角形範圍內
		vec2ReflectionSlope(&(ball->vel), ballPos);
		ball->vel.x = -ball->vel.x;
		ball->vel.y = -ball->vel.y;
		//block->destroyed = BLOCK_STATE_DESTORYED;
		return 1;
	}
	return 0;
}
#define COLLISION_LIMIT 3
uint8_t collisionProcess(Breakout_typedef *level){
	//level->ball.prePos = level->ball.pos;
	level->ball.pos.x += level->ball.vel.x;
	level->ball.pos.y += level->ball.vel.y;
	
	boardCollision(&(level->ball), &(level->board));
	
	paddleCollision(&(level->ball), &(level->paddle));
	
	uint8_t BlockCollisionDetected=0;
	for(uint16_t i=0; i<level->board.blocks_num; i++){
		if(level->board.blocks[i].destroyed)
			continue;
		if(blockCollision(&(level->ball), &(level->board.blocks[i]))){
			level->board.blocks[i].destroyed = BLOCK_STATE_DESTORYED;
			BlockCollisionDetected += 1;
		}
		if(BlockCollisionDetected >= COLLISION_LIMIT) break;
	}
	return BlockCollisionDetected;
	//sprintf(text, "%7.2f,%7.2f", level->ball.pos.x, level->ball.pos.y);
	//BSP_LCD_DisplayStringAt(0, 230, (uint8_t*)text, CENTER_MODE); 
}
uint8_t checkFinish(Breakout_typedef *level){
	if(level->ball.pos.y >272-35-5) return RESULT_GAMEOVER;
	uint8_t haveBlocks = 0;
	for(uint16_t i=0; i<level->board.blocks_num; i++){
		if(!level->board.blocks[i].destroyed){
			haveBlocks = 1;
			break;
		}
	}
	if(haveBlocks==0) return RESULT_WIN;
	return RESULT_CONTINUE;
}

#define PADDLE_CTRL_SIZE_X_EXTAND 0
#define PADDLE_CTRL_SIZE_Y_EXTAND 0
uint8_t paddleControl(paddle_typedef *paddle, float pos_min, float pos_max){
	TS_StateTypeDef ts;
	BSP_TS_GetState(&ts);
	
	if(!ts.touchDetected){//放開或未按下
		paddle->holded = 0;
		return 0;
	}
	
	if(paddle->holded){//被案住
		//paddle->prePos = paddle->pos;
		paddle->pos.x += (ts.touchX[0]-paddle->pos.x)*0.5f;
		if(paddle->pos.x > pos_max)paddle->pos.x = pos_max;
		if(paddle->pos.x < pos_min)paddle->pos.x = pos_min;
		return 0;
	}
	
	//剛被按下
	if(ts.touchY[0] < paddle->pos.y - paddle->height/2.0 - PADDLE_CTRL_SIZE_Y_EXTAND){//y軸不再範圍內
		paddle->holded = 0;
		return 0;
	}
	if(numInRange(ts.touchX[0], paddle->pos.x - paddle->width/2.0 - PADDLE_CTRL_SIZE_X_EXTAND, paddle->pos.x + paddle->width/2.0 + PADDLE_CTRL_SIZE_X_EXTAND)){//按下位置在範圍內
		paddle->holded = 1;
	}
	return 0;
}

uint8_t Breakout_updateLCD(Breakout_typedef *level){
	//update ball
	BSP_LCD_SetTextColor(LCD_COLOR_BLACK);
	BSP_LCD_FillCircle((uint16_t)level->ball.prePos.x, (uint16_t)level->ball.prePos.y, (uint16_t)level->ball.radius);
	BSP_LCD_SetTextColor(LCD_COLOR_WHITE);
	if((level->updateTime)||numInRange(level->ball.pos.x, 480/2-90/2, 480/2+90/2)||numInRange(level->ball.pos.y, 18/2-18/2, 18/2+18/2)){
		level->updateTime=0;
		char time_str[7];
		sprintf(time_str, "%3d:%-3d", (level->time_sec)/60, (level->time_sec)%60);
		BSP_LCD_DisplayStringAt(0, 18, (uint8_t*)time_str, CENTER_MODE);
	}
	BSP_LCD_FillCircle((uint16_t)level->ball.pos.x, (uint16_t)level->ball.pos.y, (uint16_t)level->ball.radius);
	
	//update paddle
	if(level->paddle.holded){
		BSP_LCD_SetTextColor(LCD_COLOR_BLACK);
		BSP_LCD_FillRect((uint16_t)(level->paddle.prePos.x - level->paddle.width/2), (uint16_t)(level->paddle.prePos.y - level->paddle.height/2), level->paddle.width, level->paddle.height);
		BSP_LCD_SetTextColor(LCD_COLOR_WHITE);
		BSP_LCD_FillRect((uint16_t)(level->paddle.pos.x - level->paddle.width/2), (uint16_t)(level->paddle.pos.y - level->paddle.height/2), level->paddle.width, level->paddle.height);
	}
	
	//update block
	BSP_LCD_SetTextColor(LCD_COLOR_BLACK);
	for(uint16_t i=0; i<level->board.blocks_num; i++){
		if(level->board.blocks[i].destroyed == BLOCK_STATE_DESTORYED){
			if(level->board.blocks[i].preState == BLOCK_STATE_DESTORYED) continue;
			if(level->board.blocks[i].preState == BLOCK_STATE_DISAPPEARING){
				BSP_LCD_FillRect((uint16_t)level->board.blocks[i].pos.x, (uint16_t)level->board.blocks[i].pos.y, level->board.blocks[i].width, level->board.blocks[i].height);
				level->board.blocks[i].preState = BLOCK_STATE_DESTORYED;
				continue;
			}
			BSP_LCD_FillRect((uint16_t)level->board.blocks[i].pos.x+1, (uint16_t)level->board.blocks[i].pos.y+1, level->board.blocks[i].width-2, level->board.blocks[i].height-2);
			level->board.blocks[i].preState = BLOCK_STATE_DISAPPEARING;
		}
	}
	/*
	BSP_LCD_SetTextColor(LCD_COLOR_BLACK);
	BSP_LCD_FillCircle((uint16_t)level->ball.prePos.x, (uint16_t)level->ball.prePos.y, (uint16_t)level->ball.radius);
	if(level->paddle.holded){
		BSP_LCD_FillRect((uint16_t)(level->paddle.prePos.x - PADDLE_WIDTH/2), (uint16_t)(level->paddle.prePos.y - PADDLE_HEIGHT/2), PADDLE_WIDTH, PADDLE_HEIGHT);
	}
	for(uint16_t i=0; i<level->board.blocks_num; i++){
		if(level->board.blocks[i].destroyed){
			if(level->board.blocks[i].preState) continue;
			BSP_LCD_FillRect((uint16_t)level->board.blocks[i].pos.x, (uint16_t)level->board.blocks[i].pos.y, level->board.blocks[i].width, level->board.blocks[i].height);
			level->board.blocks[i].preState = level->board.blocks[i].destroyed;
		}
	}
	//draw new
	BSP_LCD_SetTextColor(LCD_COLOR_WHITE);
	if((level->updateTime)||numInRange(level->ball.pos.x, 480/2-70/2, 480/2+70/2)||numInRange(level->ball.pos.y, 18/2-18/2, 18/2+18/2)){
		level->updateTime=0;
		char time_str[7];
		sprintf(time_str, "%3d:%-3d", (level->time_sec)/60, (level->time_sec)%60);
		BSP_LCD_DisplayStringAt(0, 18, (uint8_t*)time_str, CENTER_MODE);
	}
	BSP_LCD_FillCircle((uint16_t)level->ball.pos.x, (uint16_t)level->ball.pos.y, (uint16_t)level->ball.radius);
	if(level->paddle.holded){
		BSP_LCD_FillRect((uint16_t)(level->paddle.pos.x - PADDLE_WIDTH/2), (uint16_t)(level->paddle.pos.y - PADDLE_HEIGHT/2), PADDLE_WIDTH, PADDLE_HEIGHT);
	}*/
	level->paddle.prePos = level->paddle.pos;
	level->ball.prePos = level->ball.pos;
	BSP_LCD_SetTextColor(LCD_COLOR_WHITE);
	//sprintf(text, "(%7.2f,%7.2f)", level->ball.vel.x, level->ball.vel.y);
	//BSP_LCD_DisplayStringAt(0, 250, (uint8_t*)text, LEFT_MODE);
	return 0;
}

void delBreakout(Breakout_typedef *level){
	free(level->board.blocks);
	free(level);
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
  MX_DMA2D_Init();
  MX_FMC_Init();
  MX_LTDC_Init();
  MX_TIM6_Init();
  MX_USART1_UART_Init();
  /* USER CODE BEGIN 2 */
	HAL_TIM_Base_Start_IT(&htim6);
	//LCD initialization
	BSP_LCD_Init();
	BSP_LCD_LayerDefaultInit(1, LCD_FB_START_ADDRESS);
	BSP_LCD_SelectLayer(1);
	BSP_LCD_DisplayOn();
	BSP_LCD_Clear(LCD_COLOR_BLACK);
	
	//TouchScreen initialization
	BSP_TS_Init(BSP_LCD_GetXSize(), BSP_LCD_GetYSize());
	
	
	BSP_LCD_SetBackColor(LCD_COLOR_BLACK);
	BSP_LCD_SetTextColor(LCD_COLOR_WHITE);
	
	
	//BSP_LCD_FillRect(10, 10, 100, 100);
  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    /* USER CODE END WHILE */
		
    /* USER CODE BEGIN 3 */
		Breakout_typedef* game;
		game = newGames(&level1);
		LCDreprintLevel(game);
		
		while(HAL_GPIO_ReadPin(GPIOI, GPIO_PIN_11) == GPIO_PIN_RESET);
		srand(HAL_GetTick());
		BreakoutInit(game);
		uint16_t t_counter=0;
		uint8_t framet_counter=0;
		while(1){
			if(flag_1ms){
				flag_1ms = 0;
				t_counter++;
				framet_counter++;
				if(t_counter>=100){
					t_counter = 0;
					game->time_sec ++;
					game->updateTime = 1;
				}
				collisionProcess(game);
				paddleControl(&(game->paddle), game->paddle.width/2, game->board.width-game->paddle.width/2);
				if(framet_counter>=2){
					framet_counter=0;
					Breakout_updateLCD(game);
				}
				uint8_t result = checkFinish(game);
				if(result!=RESULT_CONTINUE){
					Breakout_updateLCD(game);
					BSP_LCD_SetTextColor(LCD_COLOR_WHITE);
					if(result==RESULT_WIN){
						BSP_LCD_DisplayStringAt(0,180,(uint8_t*)"WIN",CENTER_MODE);
					}
					if(result==RESULT_GAMEOVER){
						BSP_LCD_DisplayStringAt(0,180,(uint8_t*)"GAME OVER",CENTER_MODE);
					}
					free(game);
					while(HAL_GPIO_ReadPin(GPIOI, GPIO_PIN_11) == GPIO_PIN_RESET);//
					HAL_Delay(20);
					while(HAL_GPIO_ReadPin(GPIOI, GPIO_PIN_11) == GPIO_PIN_SET);
					break;
				}
				
			}
		}
		//TS_StateTypeDef  ts;
		//BSP_TS_GetState(&ts);
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
  RCC_PeriphCLKInitTypeDef PeriphClkInitStruct = {0};

  /** Configure the main internal regulator output voltage
  */
  __HAL_RCC_PWR_CLK_ENABLE();
  __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE1);
  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSI;
  RCC_OscInitStruct.PLL.PLLM = 8;
  RCC_OscInitStruct.PLL.PLLN = 216;
  RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV2;
  RCC_OscInitStruct.PLL.PLLQ = 2;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }
  /** Activate the Over-Drive mode
  */
  if (HAL_PWREx_EnableOverDrive() != HAL_OK)
  {
    Error_Handler();
  }
  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV4;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV2;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_7) != HAL_OK)
  {
    Error_Handler();
  }
  PeriphClkInitStruct.PeriphClockSelection = RCC_PERIPHCLK_LTDC|RCC_PERIPHCLK_USART1;
  PeriphClkInitStruct.PLLSAI.PLLSAIN = 50;
  PeriphClkInitStruct.PLLSAI.PLLSAIR = 2;
  PeriphClkInitStruct.PLLSAI.PLLSAIQ = 2;
  PeriphClkInitStruct.PLLSAI.PLLSAIP = RCC_PLLSAIP_DIV2;
  PeriphClkInitStruct.PLLSAIDivQ = 1;
  PeriphClkInitStruct.PLLSAIDivR = RCC_PLLSAIDIVR_2;
  PeriphClkInitStruct.Usart1ClockSelection = RCC_USART1CLKSOURCE_PCLK2;
  if (HAL_RCCEx_PeriphCLKConfig(&PeriphClkInitStruct) != HAL_OK)
  {
    Error_Handler();
  }
}

/**
  * @brief DMA2D Initialization Function
  * @param None
  * @retval None
  */
static void MX_DMA2D_Init(void)
{

  /* USER CODE BEGIN DMA2D_Init 0 */

  /* USER CODE END DMA2D_Init 0 */

  /* USER CODE BEGIN DMA2D_Init 1 */

  /* USER CODE END DMA2D_Init 1 */
  hdma2d.Instance = DMA2D;
  hdma2d.Init.Mode = DMA2D_M2M;
  hdma2d.Init.ColorMode = DMA2D_OUTPUT_ARGB8888;
  hdma2d.Init.OutputOffset = 0;
  hdma2d.LayerCfg[1].InputOffset = 0;
  hdma2d.LayerCfg[1].InputColorMode = DMA2D_INPUT_ARGB8888;
  hdma2d.LayerCfg[1].AlphaMode = DMA2D_NO_MODIF_ALPHA;
  hdma2d.LayerCfg[1].InputAlpha = 0;
  if (HAL_DMA2D_Init(&hdma2d) != HAL_OK)
  {
    Error_Handler();
  }
  if (HAL_DMA2D_ConfigLayer(&hdma2d, 1) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN DMA2D_Init 2 */

  /* USER CODE END DMA2D_Init 2 */

}

/**
  * @brief LTDC Initialization Function
  * @param None
  * @retval None
  */
static void MX_LTDC_Init(void)
{

  /* USER CODE BEGIN LTDC_Init 0 */

  /* USER CODE END LTDC_Init 0 */

  LTDC_LayerCfgTypeDef pLayerCfg = {0};
  LTDC_LayerCfgTypeDef pLayerCfg1 = {0};

  /* USER CODE BEGIN LTDC_Init 1 */

  /* USER CODE END LTDC_Init 1 */
  hltdc.Instance = LTDC;
  hltdc.Init.HSPolarity = LTDC_HSPOLARITY_AL;
  hltdc.Init.VSPolarity = LTDC_VSPOLARITY_AL;
  hltdc.Init.DEPolarity = LTDC_DEPOLARITY_AL;
  hltdc.Init.PCPolarity = LTDC_PCPOLARITY_IPC;
  hltdc.Init.HorizontalSync = 7;
  hltdc.Init.VerticalSync = 3;
  hltdc.Init.AccumulatedHBP = 14;
  hltdc.Init.AccumulatedVBP = 5;
  hltdc.Init.AccumulatedActiveW = 654;
  hltdc.Init.AccumulatedActiveH = 485;
  hltdc.Init.TotalWidth = 660;
  hltdc.Init.TotalHeigh = 487;
  hltdc.Init.Backcolor.Blue = 0;
  hltdc.Init.Backcolor.Green = 0;
  hltdc.Init.Backcolor.Red = 0;
  if (HAL_LTDC_Init(&hltdc) != HAL_OK)
  {
    Error_Handler();
  }
  pLayerCfg.WindowX0 = 0;
  pLayerCfg.WindowX1 = 0;
  pLayerCfg.WindowY0 = 0;
  pLayerCfg.WindowY1 = 0;
  pLayerCfg.PixelFormat = LTDC_PIXEL_FORMAT_ARGB8888;
  pLayerCfg.Alpha = 0;
  pLayerCfg.Alpha0 = 0;
  pLayerCfg.BlendingFactor1 = LTDC_BLENDING_FACTOR1_CA;
  pLayerCfg.BlendingFactor2 = LTDC_BLENDING_FACTOR2_CA;
  pLayerCfg.FBStartAdress = 0;
  pLayerCfg.ImageWidth = 0;
  pLayerCfg.ImageHeight = 0;
  pLayerCfg.Backcolor.Blue = 0;
  pLayerCfg.Backcolor.Green = 0;
  pLayerCfg.Backcolor.Red = 0;
  if (HAL_LTDC_ConfigLayer(&hltdc, &pLayerCfg, 0) != HAL_OK)
  {
    Error_Handler();
  }
  pLayerCfg1.WindowX0 = 0;
  pLayerCfg1.WindowX1 = 0;
  pLayerCfg1.WindowY0 = 0;
  pLayerCfg1.WindowY1 = 0;
  pLayerCfg1.PixelFormat = LTDC_PIXEL_FORMAT_ARGB8888;
  pLayerCfg1.Alpha = 0;
  pLayerCfg1.Alpha0 = 0;
  pLayerCfg1.BlendingFactor1 = LTDC_BLENDING_FACTOR1_CA;
  pLayerCfg1.BlendingFactor2 = LTDC_BLENDING_FACTOR2_CA;
  pLayerCfg1.FBStartAdress = 0;
  pLayerCfg1.ImageWidth = 0;
  pLayerCfg1.ImageHeight = 0;
  pLayerCfg1.Backcolor.Blue = 0;
  pLayerCfg1.Backcolor.Green = 0;
  pLayerCfg1.Backcolor.Red = 0;
  if (HAL_LTDC_ConfigLayer(&hltdc, &pLayerCfg1, 1) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN LTDC_Init 2 */

  /* USER CODE END LTDC_Init 2 */

}

/**
  * @brief TIM6 Initialization Function
  * @param None
  * @retval None
  */
static void MX_TIM6_Init(void)
{

  /* USER CODE BEGIN TIM6_Init 0 */

  /* USER CODE END TIM6_Init 0 */

  TIM_MasterConfigTypeDef sMasterConfig = {0};

  /* USER CODE BEGIN TIM6_Init 1 */

  /* USER CODE END TIM6_Init 1 */
  htim6.Instance = TIM6;
  htim6.Init.Prescaler = 10799;
  htim6.Init.CounterMode = TIM_COUNTERMODE_UP;
  htim6.Init.Period = 99;
  htim6.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
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
  /* USER CODE BEGIN TIM6_Init 2 */

  /* USER CODE END TIM6_Init 2 */

}

/**
  * @brief USART1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_USART1_UART_Init(void)
{

  /* USER CODE BEGIN USART1_Init 0 */

  /* USER CODE END USART1_Init 0 */

  /* USER CODE BEGIN USART1_Init 1 */

  /* USER CODE END USART1_Init 1 */
  huart1.Instance = USART1;
  huart1.Init.BaudRate = 115200;
  huart1.Init.WordLength = UART_WORDLENGTH_8B;
  huart1.Init.StopBits = UART_STOPBITS_1;
  huart1.Init.Parity = UART_PARITY_NONE;
  huart1.Init.Mode = UART_MODE_TX_RX;
  huart1.Init.HwFlowCtl = UART_HWCONTROL_NONE;
  huart1.Init.OverSampling = UART_OVERSAMPLING_16;
  huart1.Init.OneBitSampling = UART_ONE_BIT_SAMPLE_DISABLE;
  huart1.AdvancedInit.AdvFeatureInit = UART_ADVFEATURE_NO_INIT;
  if (HAL_UART_Init(&huart1) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN USART1_Init 2 */

  /* USER CODE END USART1_Init 2 */

}

/* FMC initialization function */
static void MX_FMC_Init(void)
{

  /* USER CODE BEGIN FMC_Init 0 */

  /* USER CODE END FMC_Init 0 */

  FMC_SDRAM_TimingTypeDef SdramTiming = {0};

  /* USER CODE BEGIN FMC_Init 1 */

  /* USER CODE END FMC_Init 1 */

  /** Perform the SDRAM1 memory initialization sequence
  */
  hsdram1.Instance = FMC_SDRAM_DEVICE;
  /* hsdram1.Init */
  hsdram1.Init.SDBank = FMC_SDRAM_BANK1;
  hsdram1.Init.ColumnBitsNumber = FMC_SDRAM_COLUMN_BITS_NUM_8;
  hsdram1.Init.RowBitsNumber = FMC_SDRAM_ROW_BITS_NUM_12;
  hsdram1.Init.MemoryDataWidth = FMC_SDRAM_MEM_BUS_WIDTH_16;
  hsdram1.Init.InternalBankNumber = FMC_SDRAM_INTERN_BANKS_NUM_4;
  hsdram1.Init.CASLatency = FMC_SDRAM_CAS_LATENCY_1;
  hsdram1.Init.WriteProtection = FMC_SDRAM_WRITE_PROTECTION_DISABLE;
  hsdram1.Init.SDClockPeriod = FMC_SDRAM_CLOCK_DISABLE;
  hsdram1.Init.ReadBurst = FMC_SDRAM_RBURST_DISABLE;
  hsdram1.Init.ReadPipeDelay = FMC_SDRAM_RPIPE_DELAY_0;
  /* SdramTiming */
  SdramTiming.LoadToActiveDelay = 16;
  SdramTiming.ExitSelfRefreshDelay = 16;
  SdramTiming.SelfRefreshTime = 16;
  SdramTiming.RowCycleDelay = 16;
  SdramTiming.WriteRecoveryTime = 16;
  SdramTiming.RPDelay = 16;
  SdramTiming.RCDDelay = 16;

  if (HAL_SDRAM_Init(&hsdram1, &SdramTiming) != HAL_OK)
  {
    Error_Handler( );
  }

  /* USER CODE BEGIN FMC_Init 2 */

  /* USER CODE END FMC_Init 2 */
}

/**
  * @brief GPIO Initialization Function
  * @param None
  * @retval None
  */
static void MX_GPIO_Init(void)
{
  GPIO_InitTypeDef GPIO_InitStruct = {0};

  /* GPIO Ports Clock Enable */
  __HAL_RCC_GPIOE_CLK_ENABLE();
  __HAL_RCC_GPIOG_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();
  __HAL_RCC_GPIOD_CLK_ENABLE();
  __HAL_RCC_GPIOC_CLK_ENABLE();
  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOJ_CLK_ENABLE();
  __HAL_RCC_GPIOI_CLK_ENABLE();
  __HAL_RCC_GPIOK_CLK_ENABLE();
  __HAL_RCC_GPIOF_CLK_ENABLE();
  __HAL_RCC_GPIOH_CLK_ENABLE();

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(OTG_FS_PowerSwitchOn_GPIO_Port, OTG_FS_PowerSwitchOn_Pin, GPIO_PIN_SET);

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOI, ARDUINO_D7_Pin|ARDUINO_D8_Pin|GPIO_PIN_1|LCD_DISP_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(LCD_BL_CTRL_GPIO_Port, LCD_BL_CTRL_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(DCMI_PWR_EN_GPIO_Port, DCMI_PWR_EN_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOG, ARDUINO_D4_Pin|ARDUINO_D2_Pin|EXT_RST_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pin : OTG_HS_OverCurrent_Pin */
  GPIO_InitStruct.Pin = OTG_HS_OverCurrent_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(OTG_HS_OverCurrent_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pin : QSPI_D2_Pin */
  GPIO_InitStruct.Pin = QSPI_D2_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
  GPIO_InitStruct.Alternate = GPIO_AF9_QUADSPI;
  HAL_GPIO_Init(QSPI_D2_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pins : RMII_TXD1_Pin RMII_TXD0_Pin RMII_TX_EN_Pin */
  GPIO_InitStruct.Pin = RMII_TXD1_Pin|RMII_TXD0_Pin|RMII_TX_EN_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
  GPIO_InitStruct.Alternate = GPIO_AF11_ETH;
  HAL_GPIO_Init(GPIOG, &GPIO_InitStruct);

  /*Configure GPIO pins : ARDUINO_SCL_D15_Pin ARDUINO_SDA_D14_Pin */
  GPIO_InitStruct.Pin = ARDUINO_SCL_D15_Pin|ARDUINO_SDA_D14_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_AF_OD;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  GPIO_InitStruct.Alternate = GPIO_AF4_I2C1;
  HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

  /*Configure GPIO pins : ULPI_D7_Pin ULPI_D6_Pin ULPI_D5_Pin ULPI_D3_Pin
                           ULPI_D2_Pin ULPI_D1_Pin ULPI_D4_Pin */
  GPIO_InitStruct.Pin = ULPI_D7_Pin|ULPI_D6_Pin|ULPI_D5_Pin|ULPI_D3_Pin
                          |ULPI_D2_Pin|ULPI_D1_Pin|ULPI_D4_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
  GPIO_InitStruct.Alternate = GPIO_AF10_OTG_HS;
  HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

  /*Configure GPIO pin : ARDUINO_PWM_D3_Pin */
  GPIO_InitStruct.Pin = ARDUINO_PWM_D3_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  GPIO_InitStruct.Alternate = GPIO_AF2_TIM3;
  HAL_GPIO_Init(ARDUINO_PWM_D3_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pin : SPDIF_RX0_Pin */
  GPIO_InitStruct.Pin = SPDIF_RX0_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  GPIO_InitStruct.Alternate = GPIO_AF8_SPDIFRX;
  HAL_GPIO_Init(SPDIF_RX0_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pins : SDMMC_CK_Pin SDMMC_D3_Pin SDMMC_D2_Pin PC9
                           PC8 */
  GPIO_InitStruct.Pin = SDMMC_CK_Pin|SDMMC_D3_Pin|SDMMC_D2_Pin|GPIO_PIN_9
                          |GPIO_PIN_8;
  GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
  GPIO_InitStruct.Alternate = GPIO_AF12_SDMMC1;
  HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);

  /*Configure GPIO pin : ARDUINO_PWM_D9_Pin */
  GPIO_InitStruct.Pin = ARDUINO_PWM_D9_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  GPIO_InitStruct.Alternate = GPIO_AF1_TIM2;
  HAL_GPIO_Init(ARDUINO_PWM_D9_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pins : DCMI_D6_Pin DCMI_D7_Pin */
  GPIO_InitStruct.Pin = DCMI_D6_Pin|DCMI_D7_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  GPIO_InitStruct.Alternate = GPIO_AF13_DCMI;
  HAL_GPIO_Init(GPIOE, &GPIO_InitStruct);

  /*Configure GPIO pin : QSPI_NCS_Pin */
  GPIO_InitStruct.Pin = QSPI_NCS_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
  GPIO_InitStruct.Alternate = GPIO_AF10_QUADSPI;
  HAL_GPIO_Init(QSPI_NCS_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pin : OTG_FS_VBUS_Pin */
  GPIO_InitStruct.Pin = OTG_FS_VBUS_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(OTG_FS_VBUS_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pin : Audio_INT_Pin */
  GPIO_InitStruct.Pin = Audio_INT_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_EVT_RISING;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(Audio_INT_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pins : OTG_FS_P_Pin OTG_FS_N_Pin OTG_FS_ID_Pin */
  GPIO_InitStruct.Pin = OTG_FS_P_Pin|OTG_FS_N_Pin|OTG_FS_ID_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
  GPIO_InitStruct.Alternate = GPIO_AF10_OTG_FS;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

  /*Configure GPIO pins : SAI2_MCLKA_Pin SAI2_SCKA_Pin SAI2_FSA_Pin SAI2_SDA_Pin */
  GPIO_InitStruct.Pin = SAI2_MCLKA_Pin|SAI2_SCKA_Pin|SAI2_FSA_Pin|SAI2_SDA_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  GPIO_InitStruct.Alternate = GPIO_AF10_SAI2;
  HAL_GPIO_Init(GPIOI, &GPIO_InitStruct);

  /*Configure GPIO pin : SAI2_SDB_Pin */
  GPIO_InitStruct.Pin = SAI2_SDB_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  GPIO_InitStruct.Alternate = GPIO_AF10_SAI2;
  HAL_GPIO_Init(SAI2_SDB_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pin : OTG_FS_PowerSwitchOn_Pin */
  GPIO_InitStruct.Pin = OTG_FS_PowerSwitchOn_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(OTG_FS_PowerSwitchOn_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pin : DCMI_D5_Pin */
  GPIO_InitStruct.Pin = DCMI_D5_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  GPIO_InitStruct.Alternate = GPIO_AF13_DCMI;
  HAL_GPIO_Init(DCMI_D5_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pins : ARDUINO_D7_Pin ARDUINO_D8_Pin PI1 LCD_DISP_Pin */
  GPIO_InitStruct.Pin = ARDUINO_D7_Pin|ARDUINO_D8_Pin|GPIO_PIN_1|LCD_DISP_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOI, &GPIO_InitStruct);

  /*Configure GPIO pin : uSD_Detect_Pin */
  GPIO_InitStruct.Pin = uSD_Detect_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(uSD_Detect_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pin : LCD_BL_CTRL_Pin */
  GPIO_InitStruct.Pin = LCD_BL_CTRL_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(LCD_BL_CTRL_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pin : DCMI_VSYNC_Pin */
  GPIO_InitStruct.Pin = DCMI_VSYNC_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  GPIO_InitStruct.Alternate = GPIO_AF13_DCMI;
  HAL_GPIO_Init(DCMI_VSYNC_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pin : OTG_FS_OverCurrent_Pin */
  GPIO_InitStruct.Pin = OTG_FS_OverCurrent_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(OTG_FS_OverCurrent_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pin : SDMMC_D0_Pin */
  GPIO_InitStruct.Pin = SDMMC_D0_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
  GPIO_InitStruct.Alternate = GPIO_AF12_SDMMC1;
  HAL_GPIO_Init(SDMMC_D0_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pins : TP3_Pin NC2_Pin */
  GPIO_InitStruct.Pin = TP3_Pin|NC2_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(GPIOH, &GPIO_InitStruct);

  /*Configure GPIO pin : DCMI_PWR_EN_Pin */
  GPIO_InitStruct.Pin = DCMI_PWR_EN_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(DCMI_PWR_EN_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pins : DCMI_D4_Pin DCMI_D3_Pin DCMI_D0_Pin DCMI_D2_Pin
                           DCMI_D1_Pin */
  GPIO_InitStruct.Pin = DCMI_D4_Pin|DCMI_D3_Pin|DCMI_D0_Pin|DCMI_D2_Pin
                          |DCMI_D1_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  GPIO_InitStruct.Alternate = GPIO_AF13_DCMI;
  HAL_GPIO_Init(GPIOH, &GPIO_InitStruct);

  /*Configure GPIO pin : ARDUINO_PWM_CS_D5_Pin */
  GPIO_InitStruct.Pin = ARDUINO_PWM_CS_D5_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  GPIO_InitStruct.Alternate = GPIO_AF2_TIM5;
  HAL_GPIO_Init(ARDUINO_PWM_CS_D5_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pin : PI11 */
  GPIO_InitStruct.Pin = GPIO_PIN_11;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(GPIOI, &GPIO_InitStruct);

  /*Configure GPIO pin : ARDUINO_PWM_D10_Pin */
  GPIO_InitStruct.Pin = ARDUINO_PWM_D10_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  GPIO_InitStruct.Alternate = GPIO_AF1_TIM1;
  HAL_GPIO_Init(ARDUINO_PWM_D10_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pin : LCD_INT_Pin */
  GPIO_InitStruct.Pin = LCD_INT_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_EVT_RISING;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(LCD_INT_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pins : ARDUINO_RX_D0_Pin ARDUINO_TX_D1_Pin */
  GPIO_InitStruct.Pin = ARDUINO_RX_D0_Pin|ARDUINO_TX_D1_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
  GPIO_InitStruct.Alternate = GPIO_AF8_USART6;
  HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);

  /*Configure GPIO pin : ULPI_NXT_Pin */
  GPIO_InitStruct.Pin = ULPI_NXT_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
  GPIO_InitStruct.Alternate = GPIO_AF10_OTG_HS;
  HAL_GPIO_Init(ULPI_NXT_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pins : ARDUINO_D4_Pin ARDUINO_D2_Pin EXT_RST_Pin */
  GPIO_InitStruct.Pin = ARDUINO_D4_Pin|ARDUINO_D2_Pin|EXT_RST_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOG, &GPIO_InitStruct);

  /*Configure GPIO pins : ARDUINO_A4_Pin ARDUINO_A5_Pin ARDUINO_A1_Pin ARDUINO_A2_Pin
                           ARDUINO_A3_Pin */
  GPIO_InitStruct.Pin = ARDUINO_A4_Pin|ARDUINO_A5_Pin|ARDUINO_A1_Pin|ARDUINO_A2_Pin
                          |ARDUINO_A3_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_ANALOG;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(GPIOF, &GPIO_InitStruct);

  /*Configure GPIO pins : ULPI_STP_Pin ULPI_DIR_Pin */
  GPIO_InitStruct.Pin = ULPI_STP_Pin|ULPI_DIR_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
  GPIO_InitStruct.Alternate = GPIO_AF10_OTG_HS;
  HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);

  /*Configure GPIO pins : RMII_MDC_Pin RMII_RXD0_Pin RMII_RXD1_Pin */
  GPIO_InitStruct.Pin = RMII_MDC_Pin|RMII_RXD0_Pin|RMII_RXD1_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
  GPIO_InitStruct.Alternate = GPIO_AF11_ETH;
  HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);

  /*Configure GPIO pin : PB2 */
  GPIO_InitStruct.Pin = GPIO_PIN_2;
  GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
  GPIO_InitStruct.Alternate = GPIO_AF9_QUADSPI;
  HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

  /*Configure GPIO pins : QSPI_D1_Pin QSPI_D3_Pin QSPI_D0_Pin */
  GPIO_InitStruct.Pin = QSPI_D1_Pin|QSPI_D3_Pin|QSPI_D0_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
  GPIO_InitStruct.Alternate = GPIO_AF9_QUADSPI;
  HAL_GPIO_Init(GPIOD, &GPIO_InitStruct);

  /*Configure GPIO pin : RMII_RXER_Pin */
  GPIO_InitStruct.Pin = RMII_RXER_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(RMII_RXER_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pins : RMII_REF_CLK_Pin RMII_MDIO_Pin RMII_CRS_DV_Pin */
  GPIO_InitStruct.Pin = RMII_REF_CLK_Pin|RMII_MDIO_Pin|RMII_CRS_DV_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
  GPIO_InitStruct.Alternate = GPIO_AF11_ETH;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

  /*Configure GPIO pin : ARDUINO_A0_Pin */
  GPIO_InitStruct.Pin = ARDUINO_A0_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_ANALOG;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(ARDUINO_A0_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pins : DCMI_HSYNC_Pin PA6 */
  GPIO_InitStruct.Pin = DCMI_HSYNC_Pin|GPIO_PIN_6;
  GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  GPIO_InitStruct.Alternate = GPIO_AF13_DCMI;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

  /*Configure GPIO pins : LCD_SCL_Pin LCD_SDA_Pin */
  GPIO_InitStruct.Pin = LCD_SCL_Pin|LCD_SDA_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_AF_OD;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
  GPIO_InitStruct.Alternate = GPIO_AF4_I2C3;
  HAL_GPIO_Init(GPIOH, &GPIO_InitStruct);

  /*Configure GPIO pins : ULPI_CLK_Pin ULPI_D0_Pin */
  GPIO_InitStruct.Pin = ULPI_CLK_Pin|ULPI_D0_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
  GPIO_InitStruct.Alternate = GPIO_AF10_OTG_HS;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

  /*Configure GPIO pin : ARDUINO_PWM_D6_Pin */
  GPIO_InitStruct.Pin = ARDUINO_PWM_D6_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  GPIO_InitStruct.Alternate = GPIO_AF9_TIM12;
  HAL_GPIO_Init(ARDUINO_PWM_D6_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pins : ARDUINO_MISO_D12_Pin ARDUINO_MOSI_PWM_D11_Pin */
  GPIO_InitStruct.Pin = ARDUINO_MISO_D12_Pin|ARDUINO_MOSI_PWM_D11_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  GPIO_InitStruct.Alternate = GPIO_AF5_SPI2;
  HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

}

/* USER CODE BEGIN 4 */
void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef* htm){
	if(htm->Instance == htim6.Instance){
		flag_1ms = 1;
	}
}
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

/************************ (C) COPYRIGHT STMicroelectronics *****END OF FILE****/
