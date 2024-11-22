# OTA

The `tkl_ota.c` firmware OTA (Over-The-Air) includes handling various stages of the firmware upgrade process: obtaining firmware upgrade capabilities, OTA start notification, OTA data processing, and OTA end notification. This file is automatically generated by the TuyaOS and allows developers to add their own implementations in the provided user-defined areas.

## API Description

### tkl_ota_get_ability

```c
OPERATE_RET tkl_ota_get_ability(uint32_t *image_size, TUYA_OTA_TYPE_E *type);
```

#### Functionality

Obtains the OTA capability information of the current device, including the maximum firmware image size and the OTA type.

#### Parameters

- `image_size`: Output parameter, used to store the maximum size of the firmware image.
- `type`: Output parameter, OTA type, can be a full package or a differential package, use `TUYA_OTA_FULL` to indicate a full package, and `TUYA_OTA_DIFF` to indicate a differential package.
    ```c
        typedef enum {
            TUYA_OTA_FULL        = 1,            ///< AB area switch, full package upgrade
            TUYA_OTA_DIFF        = 2,            ///< fixed area, difference package upgrade
        } TUYA_OTA_TYPE_E;
    ```

#### Return Value

A return value of `OPRT_OK` indicates success, other values indicate an error, please refer to `tuya_error_code.h`.

### tkl_ota_start_notify

```c
OPERATE_RET tkl_ota_start_notify(uint32_t image_size, TUYA_OTA_TYPE_E type, TUYA_OTA_PATH_E path);
```

#### Functionality

Notifies the start of the OTA upgrade, this function initializes the necessary variables and states for the OTA process.

#### Parameters

- `image_size`: Input parameter, the size of the firmware image to be upgraded.
- `type`: Input parameter, OTA type.
- `path`: Input parameter, OTA path, the channel through which the data is transmitted.
    ```c
        typedef enum {
        TUYA_OTA_PATH_AIR    = 0,            ///< OTA from Wired/Wi-Fi/Cellular/NBIoT
        TUYA_OTA_PATH_UART   = 1,            ///< OTA from uart for MF
        TUYA_OTA_PATH_BLE    = 2,            ///< OTA from BLE protocol for subdev
        TUYA_OTA_PATH_ZIGBEE = 3,            ///< OTA from Zigbee protocol for subdev
        TUYA_OTA_PATH_SEC_A = 4,             ///< OTA from multi-section A
        TUYA_OTA_PATH_SEC_B = 5,             ///< OTA from multi-section B
        TUYA_OTA_PATH_INVALID = 255          ///< OTA from multi-section invalid
        }TUYA_OTA_PATH_E;
    ```
#### Return Value

A return value of `OPRT_OK` indicates the operation was successful, other return values indicate an error, please refer to `tuya_error_code.h` for error information.

### tkl_ota_data_process

```c
OPERATE_RET tkl_ota_data_process(TUYA_OTA_DATA_T *pack, uint32_t* remain_len);
```

#### Functionality

Processes the received OTA data packet, this function will perform different upgrade operations according to the state.

#### Parameters

- `pack`: Input parameter, a pointer to the OTA data packet.
- `remain_len`: Output parameter, indicates the length of the remaining unprocessed data in the OTA data packet.

#### Return Value

If the data packet is processed successfully, the return value is `OPRT_OK`, otherwise, an error code is returned. For specific error information, please see `tuya_error_code.h`.

### tkl_ota_end_notify

```c
OPERATE_RET tkl_ota_end_notify(BOOL_T reset);
```

#### Functionality

Notifies the end of the OTA upgrade, performs verification and follow-up processing, and will reset the device if necessary.

#### Parameters

- `reset`: Input parameter, indicates whether the device needs to be reset after the OTA ends.

#### Return Value

A return value of `OPRT_OK` indicates the operation was successful, if the verification fails or other reasons cause failure, an error code is returned. For error details, please refer to `tuya_error_code.h`.

### tkl_ota_get_old_firmware_info

```c
OPERATE_RET tkl_ota_get_old_firmware_info(TUYA_OTA_FIRMWARE_INFO_T **info);
```

#### Functionality

Obtains information about the old firmware, this function is typically used in breakpoint resume scenarios.

#### Parameters

- `info`: Output parameter, a pointer to a pointer to the old firmware information structure.

#### Return Value

If the old firmware information is successfully obtained, the return value is `OPRT_OK`, otherwise, an error code is returned. For specific error information, please see `tuya_error_code.h`.