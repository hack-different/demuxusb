# DeMuxUSB - Analyze iDevice USB protocols

DeMuxUSB (a play on `usbmuxd`) is a program and libraries designed to allow the analysis of captures of USB sessions
with iDevices.

DeMuxUSB was created because ever major iOS version and new device like the M1 requires reverse engineering of the
recovery / restore process for `idevicerestore`. This toolset lets us take a capture of Configurator and quickly
understand changes between versions and with new devices.

The tool is being extended as a forensics capability to analyze (particularly out-of-band) device restores against a
known baseline for any failures or deviations like would be evident if the technician workstation was attempting to
install a jailbreak like checkra1n.

## Input Handling

Today the only supported front-end input handler is macOS based pcapng format from a program like Wireshark on `XHC20`

In the near future I'd like to add support for both Linux pcapng (very similar but different header to macOS USB
capture)
as well as for hardware based capture systems like the Beagle 5000.

## Protocol Handling

DeMuxUSB handles three different protocols:

* DFU mode
* Recovery Mode
* `usbmuxd` streams and plist services

## Output Format

The output of an analysis is a sequenced series of payloads and commands. We split apart the payloads into files to make
them substantively easier to process after.