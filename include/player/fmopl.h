#ifndef SCHISM_PLAYER_FMOPL_H_
#define SCHISM_PLAYER_FMOPL_H_

#include <stdint.h>

#define logerror(...) /**/

typedef int16_t OPLSAMPLE;

typedef void (*OPL_TIMERHANDLER)(void *param,int timer,double period);
typedef void (*OPL_IRQHANDLER)(void *param,int irq);
typedef void (*OPL_UPDATEHANDLER)(void *param,int min_interval_us);
typedef void (*OPL_PORTHANDLER_W)(void *param,unsigned char data);
typedef unsigned char (*OPL_PORTHANDLER_R)(void *param);

/* OPL2 */
void *ym3812_init(uint32_t clock, uint32_t rate);
void ym3812_shutdown(void *chip);
void ym3812_reset_chip(void *chip);
int  ym3812_write(void *chip, int a, int v);
unsigned char ym3812_read(void *chip, int a);
int  ym3812_timer_over(void *chip, int c);
void ym3812_update_one(void *chip, OPLSAMPLE *buffer, int length);

void ym3812_set_timer_handler(void *chip, OPL_TIMERHANDLER TimerHandler, void *param);
void ym3812_set_irq_handler(void *chip, OPL_IRQHANDLER IRQHandler, void *param);
void ym3812_set_update_handler(void *chip, OPL_UPDATEHANDLER UpdateHandler, void *param);

/* OPL3 */
void *ymf262_init(uint32_t clock, uint32_t rate);
void ymf262_shutdown(void *chip);
void ymf262_reset_chip(void *chip);
int  ymf262_write(void *chip, int a, int v);
unsigned char ymf262_read(void *chip, int a);
int  ymf262_timer_over(void *chip, int c);
void ymf262_update_one(void *chip, OPLSAMPLE **buffers, int length);

void ymf262_set_timer_handler(void *chip, OPL_TIMERHANDLER TimerHandler, void *param);
void ymf262_set_irq_handler(void *chip, OPL_IRQHANDLER IRQHandler, void *param);
void ymf262_set_update_handler(void *chip, OPL_UPDATEHANDLER UpdateHandler, void *param);

#endif /* SCHISM_PLAYER_FMOPL_H_ */
