# hello_world_pt
Hello world + pin toggle, the sample outputs a regular stream of messages to the console (J-Link RTT if configured) and toggles the P0.26 GPIO pin (VIO/GND).

### Board target
mtc2n9151/nrf9151/ns

### RTT
To use the Segger RTT Viewer application, include *..\segger-rtt.conf* in the **Extra Kconfig fragments** field on the Build Configuration.

### Debugginhg
To step through your program, set the **Optimization level (size, speed, or debugging)** to Optimize for debugging (-Og)