# GPIO Map — reviewed V0.6.2

The single current assignment table is [MASTER_GPIO_MAP.md](MASTER_GPIO_MAP.md), matching `firmware/libraries/SpaController/src/SpaController.h`.

The older CH1/CH2 LOW/HIGH and GPIO16 SSR-driver proposal is superseded. V0.6.2 uses CH1 safety contactor, CH2 LOW, CH3 SSR control, CH4 HIGH, CH5 UV/ozone, CH6 light. CH1/CH3 are inhibited in both Main and Demo.

GPIO8/9 are the implemented ADS1115 I2C pins. GPIO10–15 remain future panel reservations; GPIO16 is unused. No UI code may write physical outputs. All physical allocation, polarity and boot behavior still require the commissioning checklist.
