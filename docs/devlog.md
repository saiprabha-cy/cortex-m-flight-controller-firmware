# Development log (problems found and how they were fixed)

* **Renode timers ran 8x slow** - the stock platform clocks `STM32_Timer` at 10 MHz; firmware computes prescalers for the real 84 MHz. Fixed in `renode/stm32f407.repl` (84/168 MHz). Symptom: simulated attitude period was 8x too long.
* **Renode SysTick = 72 MHz** in the stock platform; set to 168 MHz so the FreeRTOS tick is 1 ms.
* **Stock platform needs internet** (`ApplySVD` downloads a file) - made a local copy without it.
* **No DWT in Renode** - `dwt_init()` probes the counter; falls back to TIM2-derived cycles; report flags it.
* **Bootloader updater took ~0.4 s per frame in Renode** - tight polling of the UART status register starved Renode's UART model lock. A ~1 us back-off in the idle branch fixed it (round trip 0.4 s -> 0.08 s); harmless on silicon.
* **Real bug: reset cause lost.** The bootloader read and cleared `RCC->CSR`, so the application could never see IWDG/BOR resets on real hardware. Fix: the bootloader latches the cause in the shared RAM block.
* **Renode RCC never sets IWDG/software flags** - RAM clean-reset marker infers the cause (`FC_EMU_RESET_INFERENCE`).
* **Design change vs. the first plan:** TIM2 is the timebase (`time_us()`); SysTick belongs to FreeRTOS. Stage-0 blinky code was superseded by the full BSP.
