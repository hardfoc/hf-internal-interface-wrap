# Porting

How to bring the interfaces to a new MCU: what to implement, in what order, and how to check it.

## What a port is

A port is a folder `inc/mcu/<platform>/` plus `src/mcu/<platform>/` with one class per interface you need. Each
class derives from a `Base*` class and implements its pure virtual methods with the vendor SDK. Nothing in
`inc/base/` or in the code above it changes. Start with the interfaces your board's drivers use (usually GPIO,
SPI, I2C, UART, then CAN, ADC, PWM and a timer), not all fifteen.

## The methods to implement

<!-- generated: pure-virtuals -->
| Interface | Methods a port must implement |
| --- | --- |
| [`BaseAdc`](../inc/base/BaseAdc.h) (7) | `Initialize()`, `Deinitialize()`, `GetMaxChannels()`, `IsChannelAvailable()`, `ReadChannelV()`, `ReadChannelCount()`, `ReadChannel()` |
| [`BaseBluetooth`](../inc/base/BaseBluetooth.h) (45) | `Initialize()`, `Deinitialize()`, `IsInitialized()`, `Enable()`, `Disable()`, `IsEnabled()`, `SetMode()`, `GetMode()`, `GetLocalAddress()`, `SetDeviceName()`, `GetDeviceName()`, `ConfigureClassic()`, `SetDiscoverable()`, `IsDiscoverable()`, `ConfigureBle()`, `StartAdvertising()`, `StopAdvertising()`, `IsAdvertising()`, `StartScan()`, `StopScan()`, `IsScanning()`, `GetDiscoveredDevices()`, `ClearDiscoveredDevices()`, `Connect()`, `Disconnect()`, `IsConnected()`, `GetConnectedDevices()`, `Pair()`, `Unpair()`, `IsPaired()`, `GetPairedDevices()`, `SendData()`, `GetAvailableData()`, `ReadData()`, `DiscoverServices()`, `DiscoverCharacteristics()`, `ReadCharacteristic()`, `WriteCharacteristic()`, `SubscribeCharacteristic()`, `GetState()`, `GetRssi()`, `RegisterEventCallback()`, `UnregisterEventCallback()`, `RegisterDataCallback()`, `UnregisterDataCallback()` |
| [`BaseCan`](../inc/base/BaseCan.h) (8) | `Initialize()`, `Deinitialize()`, `SendMessage()`, `ReceiveMessage()`, `SetReceiveCallback()`, `SetAcceptanceFilter()`, `GetStatus()`, `Reset()` |
| [`BaseGpio`](../inc/base/BaseGpio.h) (12) | `SetDirectionImpl()`, `GetDirectionImpl()`, `SetOutputModeImpl()`, `GetOutputModeImpl()`, `SetPullModeImpl()`, `GetPullModeImpl()`, `SetPinLevelImpl()`, `GetPinLevelImpl()`, `IsPinAvailable()`, `GetMaxPins()`, `GetDescription()`, `Initialize()` |
| [`BaseI2c`](../inc/base/BaseI2c.h) (6) | `Initialize()`, `Deinitialize()`, `Write()`, `Read()`, `WriteRead()`, `GetDeviceAddress()` |
| [`BaseLogger`](../inc/base/BaseLogger.h) (26) | `Initialize()`, `Deinitialize()`, `IsInitialized()`, `EnsureInitialized()`, `SetLogLevel()`, `GetLogLevel()`, `Error()`, `Warn()`, `Info()`, `Debug()`, `Verbose()`, `Log()`, `LogV()`, `LogWithLocation()`, `Flush()`, `IsLevelEnabled()`, `GetStatistics()`, `GetDiagnostics()`, `ResetStatistics()`, `ResetDiagnostics()`, `IsHealthy()`, `GetLastError()`, `GetLastErrorMessage()`, `PrintStatistics()`, `PrintDiagnostics()`, `PrintStatus()` |
| [`BaseNvs`](../inc/base/BaseNvs.h) (15) | `Initialize()`, `Deinitialize()`, `SetU32()`, `GetU32()`, `SetString()`, `GetString()`, `SetBlob()`, `GetBlob()`, `EraseKey()`, `Commit()`, `KeyExists()`, `GetSize()`, `GetDescription()`, `GetMaxKeyLength()`, `GetMaxValueSize()` |
| [`BasePeriodicTimer`](../inc/base/BasePeriodicTimer.h) (12) | `Initialize()`, `Deinitialize()`, `Start()`, `Stop()`, `SetPeriod()`, `GetPeriod()`, `GetStats()`, `ResetStats()`, `GetDescription()`, `GetMaxPeriod()`, `GetMinPeriod()`, `GetResolution()` |
| [`BasePio`](../inc/base/BasePio.h) (14) | `Initialize()`, `Deinitialize()`, `ConfigureChannel()`, `Transmit()`, `StartReceive()`, `StopReceive()`, `IsChannelBusy()`, `GetChannelStatus()`, `GetCapabilities()`, `SetTransmitCallback()`, `SetReceiveCallback()`, `SetErrorCallback()`, `ClearChannelCallbacks()`, `ClearCallbacks()` |
| [`BasePwm`](../inc/base/BasePwm.h) (15) | `Initialize()`, `Deinitialize()`, `EnableChannel()`, `DisableChannel()`, `IsChannelEnabled()`, `SetDutyCycle()`, `SetDutyCycleRaw()`, `SetFrequency()`, `SetPhaseShift()`, `StartAll()`, `StopAll()`, `UpdateAll()`, `SetComplementaryOutput()`, `GetDutyCycle()`, `GetFrequency()` |
| [`BaseSpi`](../inc/base/BaseSpi.h) (4) | `Initialize()`, `Deinitialize()`, `Transfer()`, `GetDeviceConfig()` |
| [`BaseTemperature`](../inc/base/BaseTemperature.h) (5) | `Initialize()`, `Deinitialize()`, `ReadTemperatureCelsiusImpl()`, `GetSensorInfo()`, `GetCapabilities()` |
| [`BaseUart`](../inc/base/BaseUart.h) (8) | `Initialize()`, `Deinitialize()`, `Write()`, `Read()`, `BytesAvailable()`, `FlushTx()`, `FlushRx()`, `Printf()` |
| [`BaseUsbSerialJtag`](../inc/base/BaseUsbSerialJtag.h) (6) | `Initialize()`, `Deinitialize()`, `Write()`, `Read()`, `FlushTx()`, `IsHostConnected()` |
| [`BaseWifi`](../inc/base/BaseWifi.h) (30) | `Initialize()`, `Deinitialize()`, `IsInitialized()`, `SetMode()`, `GetMode()`, `ConfigureStation()`, `Connect()`, `Disconnect()`, `IsConnected()`, `GetRssi()`, `GetIpInfo()`, `ConfigureAccessPoint()`, `StartAccessPoint()`, `StopAccessPoint()`, `IsAccessPointActive()`, `GetConnectedStationCount()`, `StartScan()`, `GetScanResults()`, `IsScanning()`, `GetState()`, `GetConnectedSsid()`, `GetConnectedBssid()`, `SetPowerSave()`, `GetPowerSave()`, `RegisterEventCallback()`, `UnregisterEventCallback()`, `GetMacAddress()`, `SetMacAddress()`, `GetChannel()`, `SetChannel()` |
<!-- /generated -->

Everything else in an interface has a working default in the base class: lazy initialisation, polarity
handling, statistics, batch helpers, and "not supported" returns for optional features. Override those only to
do better (for example, a hardware `TransferChain()` for SPI, or `GetBusStatus()` for CAN).

## A complete GPIO port

The smallest complete implementation, compiled by the host build from
[`tests/docs/port_gpio.cpp`](../tests/docs/port_gpio.cpp). `BaseGpio` turns `SetActive()` into
`SetPinLevelImpl(HIGH)` or `LOW` depending on the polarity, and validates the object before calling the port.

--8<-- "tests/docs/port_gpio.cpp:port"

## Rules for an implementation

| Rule | Why |
| --- | --- |
| Constructors store configuration only; `Initialize()` touches hardware and sets `initialized_` | Objects are created as globals and members before the scheduler runs |
| Return the interface's error codes, never vendor codes | Callers compare against `*_SUCCESS` and the `*_ERR_*` list |
| Every method is `noexcept`; no exceptions, no RTTI | The component builds with `-fno-exceptions -fno-rtti` |
| Allocate in `Initialize()`, never in an interrupt | Interrupt handlers must not block or allocate |
| Guard shared state with `PlatformMutex` | It becomes a real mutex when a backend is configured (see [Architecture](architecture.md#threads)) |
| Callbacks from interrupts get copies, not pointers into driver buffers | The buffer may be reused before the callback's work runs |
| `timeout_ms = 0` means "do not wait" | `BaseCan::TrySendMessage()` and polling receives rely on it |

## Wire it into the build

1. Add `HF_TARGET_MCU_<NAME>` and its family block to [`inc/utils/McuSelect.h`](../inc/utils/McuSelect.h).
2. Add a monotonic time source `inc/mcu/<platform>/<X>Time.h` and its `#elif` in
   [`inc/utils/HfTime.h`](../inc/utils/HfTime.h).
3. Add a `PlatformMutexBackend.h` for the platform's RTOS where the stack is assembled (hf-core does this for
   FreeRTOS).
4. Build the sources with the platform's toolchain; the ESP-IDF component in the root
   [`CMakeLists.txt`](../CMakeLists.txt) is the model.

## Check a port

| Check | How |
| --- | --- |
| It compiles against the interface | Instantiate every class once, as `port_gpio.cpp` does; an unimplemented pure virtual is a compile error |
| Lifecycle | Call each method before `Initialize()` (expect `*_ERR_NOT_INITIALIZED`), then after |
| Polarity | An active-low `BaseGpio`: `SetActive()` must drive the pin low |
| CAN | Loopback mode, then `TrySendMessage()` until `CAN_ERR_QUEUE_FULL`, then `GetBusStatus()` |
| On hardware | Port the matching ESP32 test application from [`examples/esp32/main`](../examples/esp32/app_config.yml) |

The ESP32 classes are the reference implementation; the [STM32](stm32.md) classes show the CubeMX-handle style.

Next: [Utilities](utilities.md).
