# edge

Firmware (STM32L4/ESP32-S3 + SX1262) and gateway (concentrator + relay + LTE uplink).

**Key constraint:** `firmware/src/arbiter.c` and `gateway/src/arbiter.c` are the SAME file.
Do not diverge. CI runs both against `protocol/vectors/*.json`.
