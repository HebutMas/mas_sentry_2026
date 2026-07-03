#ifndef _MODULE_RGB_H_
#define _MODULE_RGB_H_

#include <stdint.h>

#define LED_Black  0XFF000000
#define LED_White  0XFFFFFFFF
#define LED_Red    0XFFFF0000
#define LED_Green  0XFF00FF00
#define LED_Blue   0XFF0000FF
#define LED_Yellow 0XFFFFFF00

/**
 * @description: rgb初始化
 * @return {*}
 */
void Module_Rgb_init(void);
/**
 * @brief      显示RGB
 * @param      aRGB:0xaaRRGGBB,'aa' 是透明度,'RR'是红色,'GG'是绿色,'BB'是蓝色
 * @return     none
 */
void Module_Rgb_show(uint32_t aRGB);


#endif // _MODULE_RGB_H_