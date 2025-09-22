#include "ldConfig.h"
#include "arm_2d_types.h"
#include "ldMem.h"

#if 0
/**
 * @brief   获取触摸坐标
 * 
 * @param   x 返回的x坐标
 * @param   y 返回的y坐标
 * @return  true 有效触摸
 * @return  false 无效触摸
 */
bool ldCfgTouchGetPoint(int16_t *x,int16_t *y)
{
    bool touchState=false;
    arm_2d_location_t loc;
    
    //添加触摸函数
    touchState=VT_mouse_get_location(&loc);

    if((touchState!=0)&&(((loc.iX!=-1)&&(loc.iY!=-1))||((loc.iX!=0)&&(loc.iY!=0))))
    {
        if(loc.iX<0)
        {
            loc.iX=0;
        }
        if(loc.iY<0)
        {
            loc.iY=0;
        }
        if(loc.iX>LD_CFG_SCEEN_WIDTH)
        {
            loc.iX=LD_CFG_SCEEN_WIDTH;
        }
        if(loc.iY>LD_CFG_SCEEN_HEIGHT)
        {
            loc.iY=LD_CFG_SCEEN_HEIGHT;
        }
        *x=loc.iX;
        *y=loc.iY;
        touchState=true;
    }
    else
    {
        touchState=false;
        *x=-1;
        *y=-1;
    }
    return touchState;
}
#endif