# LooUQ MTC.2 Samples
Today, all of the samples here are for the MTC2-N9151 device. This will change as other tecnologies make there way into the LooUQ MTC.2 lineup. The folder organization will likely change at that time to reflect how samples map to individual devices, stay tuned.

## MTC2-N9151
Samples are organized by folders, each folder is a single sample. Samples normally have a readme.md file to describe what it does, specify any dependencies, and provide instructions on building. In addition to the sample folders, there are some useful files that can be applied to most samples. These are specified in the build configurations and are described individually below.

**segger-rtt.conf** This extension *configuration* file diverts application logging to the Segger J-Link RTT channel. RTT Viewer is included in the Segger J-Link distribution files and allow log output to be viewed outside of VS Code in a separate window. RTT has the advantage of being very fast and low impact on the application being developed. It only requires a small memory buffer on the nRF9151 side and utilizes the speed of the SWD and USB links to the computer workstation.

**hostExtension.overlay** This devicetree overlay file, supplements the MTC2-N9151 board devicetree board definitions and provides support for the LooUQ MTC.2 *Host Extensions*. One of these extensions: the RGB LED found on the UXplor board and future LooUQ MTC.2 products. The LED is controlled by a TI LED driver over I2C.