# Soomfon Stream Controller Support (Ajazz AKP03 Variant)

## Summary

Added USB HID VID/PID recognition for the Soomfon Stream Controller (`VID 0x1500`, `PID 0x3001`) to Chataigne's built-in Ajazz controller module as an `AKP03` variant (6 display keys, 3 rotary knobs with click, 3 extra buttons).

## Changes

1. **`Source/Module/modules/controller/ajazz/AjazzManager.cpp`**
   - Added `{ 0x1500, 0x3001 }` to the `AKP03 Series` device ID table in `AjazzManager::run()`.

2. **`Source/Module/modules/controller/ajazz/AjazzDevice.cpp`**
   - Added `pid == 0x3001` to the `AKP03` branch in `AjazzDevice::AjazzDevice(...)`, configuring `modelName = "AKP03"`, `numRows = 2`, `numColumns = 3`, `numSideKeys = 3`, and `numExtraButtons = 3`.
   - `AjazzDevice::getImageFormat()` was left unchanged; `0x3001` uses the existing `AKP03` fallback (`60x60`, `kRotationNone`).
