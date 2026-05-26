# Hot Swap Controller Update Daemon

This daemon implements the update process for hot swap controllers attached via
I2C/PMBus bus.

## Configuration Example

This example shows an addition to configuration for the Texas Instruments 
TPS25990 target. It only shows the DBUS related information provided by the
configuration.

### EntityManager configuration

```json
{
  "Address": "0x47",
  "Bus": 8,
  "FirmwareInfo": {
    "CompatibleHardware": "com.meta.Hardware.Yosemite5.HSC.TPS25990_p12v_aux",
    "VendorIANA": 40981
  },
  "Name": "Yosemite5_Paddle_HSC_P12V_AUX",
  "Type": "TPS25990Firmware"
}
```
