# MICROKERNAL-F1

A bare-metal cooperative task scheduler for the STM32F103 (Blue Pill), built from scratch at the register level — no HAL, no CubeMX, no shortcuts.

This project is an exercise in understanding what actually happens under the hood of an RTOS: clock trees, interrupt-driven peripherals, ring buffers, and cooperative task dispatch, all written directly against CMSIS register definitions.

## Why bare-metal?

Most STM32 tutorials lean on ST's HAL or CubeMX-generated code, which hides exactly the details worth understanding — clock configuration, interrupt wiring, register-level timing. This project deliberately avoids both, in favor of writing and reasoning through every register write by hand.

## Features

- **Custom clock initialization** — hand-rolled HSE → PLL (×9) → 72MHz SYSCLK sequence, with timeout guards on every wait loop and correct flash wait-state ordering.
- **Interrupt-driven SysTick** — 1ms system heartbeat, exposed as a clean, non-blocking `systick_get_ms()` API, plus a blocking `delay_ms()` built on the same timebase for occasional use.
- **Interrupt-driven USART1** — full-duplex serial communication with separate RX and TX ring buffers:
  - RX: always-listening interrupt, buffers incoming bytes without blocking.
  - TX: interrupt-driven transmission with overflow protection and a correctly-scoped critical section around shared buffer state.
- **Wraparound-safe timing** — all elapsed-time comparisons use unsigned modular arithmetic, correct even across the ~49-day tick counter overflow.

## Planned

- [ ] Cooperative task scheduler (round-robin, period-based task dispatch)
- [ ] UART command shell (`STATUS`, task enable/disable, live stats)
- [ ] RX buffer overflow protection (mirroring the TX-side implementation)
- [ ] Optional: EXTI-driven event tasks, priority scheduling

## Hardware

- **MCU:** STM32F103C8T6 (Blue Pill)
- **Clock:** 8MHz HSE crystal → PLL ×9 → 72MHz SYSCLK
- **UART:** USART1 (PA9 = TX, PA10 = RX), 115200 baud, 3.3V logic
- **Toolchain:** Keil µVision (ARM Compiler 6), ST-Link V2 for flashing/debug

## Project structure

```
├── SystemInit.c / .h   # Clock tree configuration (HSE, PLL, SystemCoreClock)
├── SysTick.c / .h      # 1ms interrupt-driven tick source + blocking delay
├── USART.c / .h        # Interrupt-driven USART1 driver (RX/TX ring buffers)
└── main.c              # Application entry point
```

## Design principles this project follows

- **ISRs do the minimum possible** — buffer data and return; no parsing, no blocking calls, no unnecessary work in interrupt context.
- **Single source of truth for shared state** — variables modified from an ISR (`g_tick_ms`, ring buffer indices) are `static` to their module and exposed only through functions, never as raw externs.
- **Critical sections are scoped as narrowly as possible** — protecting only the specific shared-state operations that actually race with interrupt context, not blanket-disabling interrupts for convenience.
- **No blocking calls where non-blocking is expected** — the eventual scheduler design assumes every task function returns promptly; blocking delays are reserved for one-off startup/settling waits only.

## Status

Actively in development. SysTick and USART drivers are complete and hardware-verified. Scheduler and shell layer are next.

## License

MIT
