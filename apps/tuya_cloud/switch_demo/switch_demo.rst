Tuya Cloud Switch Demo
======================

.. contents:: Table of Contents
   :depth: 3

Overview
--------
This is a multi-connection switch demo based on Tuya Cloud service, supporting:

- Remote control via Tuya Cloud
- LAN control
- Bluetooth control

Main features include:

- QR code generation for device binding
- Command line interface for control and debugging
- Network configuration reset
- Multiple connection modes

Working Principle
-----------------
The demo works as follows:

1. **Initialization Phase**:
   - Initialize logging system, key-value storage, timer, work queue
   - Read device authorization info (uuid and authkey)
   - Initialize Tuya IoT client
   - Initialize network connection (WiFi or wired)

2. **Running Phase**:
   - Start Tuya IoT task
   - Enter main loop to process MQTT messages and events
   - Handle various events (binding, connection, OTA, datapoint receive)

Workflow
--------
.. graphviz::

   digraph workflow {
       rankdir=LR;
       node [shape=box];
       
       Start -> Init;
       Init -> ReadConfig;
       ReadConfig -> InitIoT;
       InitIoT -> InitNetwork;
       InitNetwork -> StartIoT;
       StartIoT -> MainLoop;
       MainLoop -> HandleEvents;
       HandleEvents -> MainLoop [label="Continue"];
       HandleEvents -> Stop [label="Exit"];
   }

Key Components
--------------
1. **QR Code Generation (qrencode_print.c)**
   - Uses libqrencode library
   - Supports UTF-8 terminal output
   - Configurable parameters:
     * Version
     * Error correction level
     * Margin size
     * Color inversion

2. **Command Line Interface (cli_cmd.c)**
   Available commands:

   ======== ===========================================
   Command  Description
   ======== ===========================================
   switch   Control switch state (on/off)
   kv       Key-value storage operations
   sys      Execute system commands
   reset    Reset IoT service
   start    Start IoT service
   stop     Stop IoT service
   mem      Show memory usage
   netmgr   Network management commands
   ======== ===========================================

3. **Network Configuration Reset (reset_netcfg.c)**
   - Uses key-value storage to save reset counter
   - Reset logic:
     * Counter increases on each startup
     * Automatically clears after 5 seconds
     * Triggers network reset if counter >= 3

Usage Instructions
-----------------
1. **Initial Setup**:
   - Flash the firmware to device
   - Connect to device via serial console

2. **Device Binding**:
   - Run command to generate QR code:
     ```
     qrcode "binding:device_uuid"
     ```
   - Scan QR code using Tuya Smart app

3. **Control Switch**:
   - Turn on:
     ```
     switch on
     ```
   - Turn off:
     ```
     switch off
     ```

4. **Debugging**:
   - Check memory usage:
     ```
     mem
     ```
   - Reset network config (if needed):
     ```
     reset
     ```

API Reference
------------
1. **QR Code Generation**:
   .. code-block:: c

      void example_qrcode_string(const char *string, void (*fputs)(const char *str), int invert);

2. **Switch Control**:
   .. code-block:: c

      static void switch_test(int argc, char *argv[]);

3. **Network Reset**:
   .. code-block:: c

      void reset_netconfig_start(void);