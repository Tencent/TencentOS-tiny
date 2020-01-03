#include "tos_k.h"

#include "stm32l4xx_hal.h"
#include "stm32l4xx_hal_tim.h"
#include "stm32l4xx_hal_rtc.h"

#if TOS_CFG_TICKLESS_EN > 0u

/*
    systick本身也被实现为tickless的唤醒闹钟，实现原理是，当系统要进入tickless模式时，
    把systick的触发时间拉长
    比如说，下一个任务或者软件定时器是6000ms后执行，那么这6000ms内systick没有必要一直来，
    可以把systick的中断间隔设置为6000ms后再来（实际上systick内部的计时寄存器只有24位，systick
    最大的中断间隔其实很小，可以参考tickless_systick_wkup_alarm_max_delay实现），但即使是这样，
    功耗依然会比在进入idle时仅仅使得cpu进入sleep模式要低（因为systick步长拉长了，cpu可以在sleep
    模式中停留更久）

    实际上，tickless_systick_wkup_alarm_dismiss（tickless_wkup_alarm_dismiss）接口设计是有缺陷的，
    因为CPU从tickless中醒来，可能并不是因为闹钟时间到了，有可能是外部中断引起的CPU唤醒，因此一个较
    为完备的实现应该是，让tickless_wkup_alarm_dismiss接口返回实际睡眠的时间（可以通过闹钟的计时寄存
    器的值换算出来）。此缺陷后期会完善修复，现阶段对硬件尚不熟悉。

    注意，systick只能作为sleep模式下的唤醒闹钟。（参考tos_pm.h）

    目前这一块实现尚不完备，有很懂这方面的朋友欢迎赐教~
 */
static void tickless_systick_suspend(void)
{
    cpu_systick_suspend();
    cpu_systick_pending_reset();
}

static void tickless_systick_resume(void)
{
    cpu_systick_resume();
}

static void tickless_systick_wkup_alarm_expires_set(k_time_t millisecond)
{
    cpu_systick_expires_set(millisecond);
}

static int tickless_systick_wkup_alarm_setup(k_time_t millisecond)
{
    tickless_systick_suspend();
    tickless_systick_wkup_alarm_expires_set(millisecond);
    tickless_systick_resume();
    return 0;
}

static int tickless_systick_wkup_alarm_dismiss(void)
{
    // TODO:
    // if not wakeup by systick(that's say another interrupt), need to identify this and fix
    return 0;
}

static k_time_t tickless_systick_wkup_alarm_max_delay(void)
{
    return cpu_systick_max_delay_millisecond();
}

k_tickless_wkup_alarm_t tickless_wkup_alarm_systick = {
    .init       = K_NULL,
    .setup      = tickless_systick_wkup_alarm_setup,
    .dismiss    = tickless_systick_wkup_alarm_dismiss,
    .max_delay  = tickless_systick_wkup_alarm_max_delay,
};


/////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////
/*
   利用timer6实现的tickless唤醒闹钟，可以作为SLEEP模式下的唤醒源。（参考tos_pm.h）
   目前这一块实现尚不完备，有很懂这方面的朋友欢迎赐教~
 */
static TIM_HandleTypeDef tim6;

void HAL_TIM_Base_MspInit(TIM_HandleTypeDef *tim_handler)
{
    if (tim_handler->Instance == TIM6) {
        __HAL_RCC_TIM6_CLK_ENABLE();

        /* TIM6 interrupt Init */
        HAL_NVIC_SetPriority(TIM6_DAC_IRQn, 0, 0);
        HAL_NVIC_EnableIRQ(TIM6_DAC_IRQn);
    }
}

void HAL_TIM_Base_MspDeInit(TIM_HandleTypeDef *tim_handler)
{
    if (tim_handler->Instance == TIM6) {
        /* Peripheral clock disable */
        __HAL_RCC_TIM6_CLK_DISABLE();

        /* TIM6 interrupt Deinit */
        HAL_NVIC_DisableIRQ(TIM6_DAC_IRQn);
    }
}

static int tickless_tim6_wkup_alarm_init(void)
{
    tim6.Instance = TIM6;
    tim6.Init.Prescaler = 0;
    tim6.Init.CounterMode = TIM_COUNTERMODE_UP;
    tim6.Init.Period = 0;
    tim6.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
    tim6.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
    HAL_TIM_Base_Init(&tim6);
    return 0;
}

static int tickless_tim6_wkup_alarm_setup(k_time_t millisecond)
{
    tim6.Init.Prescaler = 8000 - 1;
    tim6.Init.Period = (millisecond * 10) - 1;

    HAL_TIM_Base_Stop(&tim6);
    __HAL_TIM_CLEAR_IT(&tim6, TIM_IT_UPDATE);

    HAL_TIM_Base_Init(&tim6);
    HAL_TIM_Base_Start_IT(&tim6);
    return 0;
}

static int tickless_tim6_wkup_alarm_dismiss(void)
{
    TOS_CPU_CPSR_ALLOC();

    TOS_CPU_INT_DISABLE();

    HAL_TIM_Base_Stop(&tim6);
    HAL_TIM_Base_Stop_IT(&tim6);

    TOS_CPU_INT_ENABLE();
    return 0;
}

static k_time_t tickless_tim6_wkup_alarm_max_delay(void)
{
    k_time_t millisecond;
    uint32_t max_period;

    max_period = ~((uint32_t)0u);
    millisecond = (max_period - 1) / 10;
    return millisecond;
}

void TIM6_DAC_IRQHandler(void)
{
    HAL_TIM_IRQHandler(&tim6);
}

k_tickless_wkup_alarm_t tickless_wkup_alarm_tim = {
    .init       = tickless_tim6_wkup_alarm_init,
    .setup      = tickless_tim6_wkup_alarm_setup,
    .dismiss    = tickless_tim6_wkup_alarm_dismiss,
    .max_delay  = tickless_tim6_wkup_alarm_max_delay,
};


/////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////
/*
   利用rtc wakeup中断实现的tickless唤醒闹钟，可以作为SLEEP、STOP模式下的唤醒源。（参考tos_pm.h）
   目前这一块实现尚不完备，有很懂这方面的朋友欢迎赐教~
 */
static RTC_HandleTypeDef rtc_handler;

static HAL_StatusTypeDef tickless_rtc_time_set(uint8_t hour, uint8_t minu, uint8_t sec, uint8_t format)
{
    RTC_TimeTypeDef rtc_time;

    rtc_time.Hours = hour;
    rtc_time.Minutes = minu;
    rtc_time.Seconds = sec;
    rtc_time.TimeFormat = format;
    rtc_time.DayLightSaving = RTC_DAYLIGHTSAVING_NONE;
    rtc_time.StoreOperation = RTC_STOREOPERATION_RESET;
    return HAL_RTC_SetTime(&rtc_handler, &rtc_time, RTC_FORMAT_BIN);
}

static HAL_StatusTypeDef tickless_rtc_date_set(uint8_t year, uint8_t month, uint8_t date, uint8_t week)
{
    RTC_DateTypeDef rtc_date;

    rtc_date.Date = date;
    rtc_date.Month = month;
    rtc_date.WeekDay = week;
    rtc_date.Year = year;
    return HAL_RTC_SetDate(&rtc_handler, &rtc_date, RTC_FORMAT_BIN);
}

static int tickless_rtc_wkup_alarm_init(void)
{
    rtc_handler.Instance = RTC;
    rtc_handler.Init.HourFormat = RTC_HOURFORMAT_24;
    rtc_handler.Init.AsynchPrediv = 0X7F;
    rtc_handler.Init.SynchPrediv = 0XFF;
    rtc_handler.Init.OutPut = RTC_OUTPUT_DISABLE;
    rtc_handler.Init.OutPutPolarity = RTC_OUTPUT_POLARITY_HIGH;
    rtc_handler.Init.OutPutType = RTC_OUTPUT_TYPE_OPENDRAIN;

    if (HAL_RTC_Init(&rtc_handler) != HAL_OK) {
        return -1;
    }

    if (HAL_RTCEx_BKUPRead(&rtc_handler, RTC_BKP_DR0) != 0X5050) {
        tickless_rtc_time_set(23, 59, 56, RTC_HOURFORMAT12_PM);
        tickless_rtc_date_set(15, 12, 27, 7);
        HAL_RTCEx_BKUPWrite(&rtc_handler, RTC_BKP_DR0,0X5050);
    }

    return 0;
}

static int tickless_rtc_wkupirq_wkup_alarm_setup(k_time_t millisecond)
{
    uint32_t wkup_clock = RTC_WAKEUPCLOCK_CK_SPRE_16BITS;
    if (millisecond < 1000) {
        millisecond = 1000;
    }
    uint32_t wkup_count = (millisecond / 1000) - 1;

    __HAL_RTC_WAKEUPTIMER_CLEAR_FLAG(&rtc_handler, RTC_FLAG_WUTF);

    HAL_RTCEx_SetWakeUpTimer_IT(&rtc_handler, wkup_count, wkup_clock);

    HAL_NVIC_SetPriority(RTC_WKUP_IRQn, 0x02, 0x02);
    HAL_NVIC_EnableIRQ(RTC_WKUP_IRQn);
    return 0;
}

static int tickless_rtc_wkupirq_wkup_alarm_dismiss(void)
{
#if defined(STM32F4) || defined(STM32L4)
    __HAL_PWR_CLEAR_FLAG(PWR_FLAG_WU);
#endif

    __HAL_RTC_WAKEUPTIMER_CLEAR_FLAG(&rtc_handler, RTC_FLAG_WUTF);

    if (HAL_RTCEx_DeactivateWakeUpTimer(&rtc_handler) != HAL_OK) {
        return -1;
    }

    HAL_NVIC_DisableIRQ(RTC_WKUP_IRQn);
    return 0;
}

static k_time_t tickless_rtc_wkupirq_wkup_alarm_max_delay(void)
{
    return 0xFFFF * K_TIME_MILLISEC_PER_SEC;
}

void HAL_RTC_MspInit(RTC_HandleTypeDef *rtc_handler)
{
    RCC_OscInitTypeDef rcc_osc;
    RCC_PeriphCLKInitTypeDef periph_clock;

    __HAL_RCC_PWR_CLK_ENABLE();
    HAL_PWR_EnableBkUpAccess();

    rcc_osc.OscillatorType = RCC_OSCILLATORTYPE_LSE;
    rcc_osc.PLL.PLLState = RCC_PLL_NONE;
    rcc_osc.LSEState = RCC_LSE_ON;
    HAL_RCC_OscConfig(&rcc_osc);

    periph_clock.PeriphClockSelection = RCC_PERIPHCLK_RTC;
    periph_clock.RTCClockSelection = RCC_RTCCLKSOURCE_LSE;
    HAL_RCCEx_PeriphCLKConfig(&periph_clock);

    __HAL_RCC_RTC_ENABLE();
}

void RTC_WKUP_IRQHandler(void)
{
    HAL_RTCEx_WakeUpTimerIRQHandler(&rtc_handler);
}

void HAL_RTCEx_WakeUpTimerEventCallback(RTC_HandleTypeDef *rtc_handler)
{
}

k_tickless_wkup_alarm_t tickless_wkup_alarm_rtc_wkupirq = {
    .init       = tickless_rtc_wkup_alarm_init,
    .setup      = tickless_rtc_wkupirq_wkup_alarm_setup,
    .dismiss    = tickless_rtc_wkupirq_wkup_alarm_dismiss,
    .max_delay  = tickless_rtc_wkupirq_wkup_alarm_max_delay,
};



/////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////
/*
   利用rtc alarm中断实现的tickless唤醒闹钟，可以作为SLEEP、STOP、STANDBY模式下的唤醒源。（参考tos_pm.h）
   目前这一块实现尚不完备，有很懂这方面的朋友欢迎赐教~
 */
static int tickless_rtc_alarmirq_wkup_alarm_setup(k_time_t millisecond)
{
    uint8_t hour, minute, second, subsecond, date;

    RTC_AlarmTypeDef rtc_alarm;
    RTC_TimeTypeDef rtc_time;
    RTC_DateTypeDef rtc_date;

    HAL_RTC_GetTime(&rtc_handler, &rtc_time, RTC_FORMAT_BIN);
    HAL_RTC_GetDate(&rtc_handler, &rtc_date, RTC_FORMAT_BIN);

    hour = rtc_time.Hours;
    minute = rtc_time.Minutes;
    second = rtc_time.Seconds;
#if 0
    date = rtc_date.Date;
#else
    date = rtc_date.WeekDay;
#endif

    printf("before >>>  %d  %d %d %d\n", date, hour, minute, second);

    /* I know it's ugly, I will find a elegant way. Welcome to tell me, 3ks~ */
    second += millisecond / K_TIME_MILLISEC_PER_SEC;
    if (second >= 60) {
        minute += 1;
        second -= 60;
    }
    if (minute >= 60) {
        hour += 1;
        minute -= 60;
    }
    if (hour >= 24) {
        date += 1;
        hour -= 24;
    }

    printf("after >>>  %d  %d %d %d\n", date, hour, minute, second);

    rtc_alarm.AlarmTime.Hours = hour;
    rtc_alarm.AlarmTime.Minutes = minute;
    rtc_alarm.AlarmTime.Seconds = second;
    rtc_alarm.AlarmTime.SubSeconds = 0;
    rtc_alarm.AlarmTime.TimeFormat = RTC_HOURFORMAT12_AM;

    rtc_alarm.AlarmMask = RTC_ALARMMASK_NONE;
    rtc_alarm.AlarmSubSecondMask = RTC_ALARMSUBSECONDMASK_NONE;
    rtc_alarm.AlarmDateWeekDaySel = RTC_ALARMDATEWEEKDAYSEL_WEEKDAY; // RTC_ALARMDATEWEEKDAYSEL_DATE; // RTC_ALARMDATEWEEKDAYSEL_WEEKDAY;
    rtc_alarm.AlarmDateWeekDay = date;
    rtc_alarm.Alarm = RTC_ALARM_A;
    HAL_RTC_SetAlarm_IT(&rtc_handler, &rtc_alarm, RTC_FORMAT_BIN);

    HAL_NVIC_SetPriority(RTC_Alarm_IRQn, 0x01, 0x02);
    HAL_NVIC_EnableIRQ(RTC_Alarm_IRQn);


    // __HAL_PWR_GET_FLAG(PWR_FLAG_WU)


    __HAL_RCC_AHB1_FORCE_RESET();       //复位所有IO口
    __HAL_RCC_PWR_CLK_ENABLE();         //使能PWR时钟

    // __HAL_RCC_BACKUPRESET_FORCE();      //复位备份区域
    HAL_PWR_EnableBkUpAccess();         //后备区域访问使能

    __HAL_PWR_CLEAR_FLAG(PWR_FLAG_SB);
    __HAL_RTC_WRITEPROTECTION_DISABLE(&rtc_handler);//关闭RTC写保护

    //关闭RTC相关中断
    __HAL_RTC_WAKEUPTIMER_DISABLE_IT(&rtc_handler,RTC_IT_WUT);
#if 0
    __HAL_RTC_TIMESTAMP_DISABLE_IT(&rtc_handler,RTC_IT_TS);
    __HAL_RTC_ALARM_DISABLE_IT(&rtc_handler,RTC_IT_ALRA|RTC_IT_ALRB);
#endif

    //清除RTC相关中断标志位
    __HAL_RTC_ALARM_CLEAR_FLAG(&rtc_handler,RTC_FLAG_ALRAF|RTC_FLAG_ALRBF);
    __HAL_RTC_TIMESTAMP_CLEAR_FLAG(&rtc_handler,RTC_FLAG_TSF);
    __HAL_RTC_WAKEUPTIMER_CLEAR_FLAG(&rtc_handler,RTC_FLAG_WUTF);

    // __HAL_RCC_BACKUPRESET_RELEASE();                    //备份区域复位结束
    __HAL_RTC_WRITEPROTECTION_ENABLE(&rtc_handler);     //使能RTC写保护


#ifdef STM32F4
    __HAL_PWR_CLEAR_FLAG(PWR_FLAG_WU);                  //清除Wake_UP标志
#endif

#ifdef STM32F7
    // __HAL_PWR_CLEAR_WAKEUP_FLAG(PWR_WAKEUP_PIN_FLAG1);  //清除Wake_UP标志
#endif

    // HAL_PWR_EnableWakeUpPin(PWR_WAKEUP_PIN1);           //设置WKUP用于唤醒

    return 0;
}

static int tickless_rtc_alarmirq_wkup_alarm_dismiss(void)
{
#if 1
    // __HAL_PWR_GET_FLAG(PWR_FLAG_WU);

    __HAL_PWR_CLEAR_FLAG(PWR_FLAG_WU);

    // __HAL_RTC_WAKEUPTIMER_CLEAR_FLAG(&rtc_handler, RTC_FLAG_ALRAF);

    __HAL_RTC_ALARM_CLEAR_FLAG(&rtc_handler, RTC_FLAG_ALRAF);

#if 0
    if (HAL_RTCEx_DeactivateWakeUpTimer(&rtc_handler) != HAL_OK) {
        return -1;
    }
#endif

    HAL_NVIC_DisableIRQ(RTC_Alarm_IRQn);
    return 0;
#endif
}

static k_time_t tickless_rtc_alarmirq_wkup_alarm_max_delay(void)
{
    return 0xFFFF; // just kidding, I will fix it out. Welcome to tell me, 3ks~ */
}

void RTC_Alarm_IRQHandler(void)
{
    HAL_RTC_AlarmIRQHandler(&rtc_handler);
}

void HAL_RTC_AlarmAEventCallback(RTC_HandleTypeDef *rtc_handler)
{
}

k_tickless_wkup_alarm_t tickless_wkup_alarm_rtc_alarmirq = {
    .init       = tickless_rtc_wkup_alarm_init,
    .setup      = tickless_rtc_alarmirq_wkup_alarm_setup,
    .dismiss    = tickless_rtc_alarmirq_wkup_alarm_dismiss,
    .max_delay  = tickless_rtc_alarmirq_wkup_alarm_max_delay,
};

#endif

