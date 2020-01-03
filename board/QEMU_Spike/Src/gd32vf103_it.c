#include "tos_k.h"
void SysTick_IRQHandler() {
    port_systick_config((uint32_t)k_cpu_cycle_per_tick);
    if (tos_knl_is_running()) {
        tos_knl_irq_enter();
        tos_tick_handler();
        tos_knl_irq_leave();
    }
}
