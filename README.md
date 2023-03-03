# Govee Bluetooth logger

Simple display logger for Govee 5075 temperature & Humidity monitor. 

 ⌂      Music 🌡  16.4°C 💧   49%  - 🔋  67% 📶-71db - GVH5075_159E - a4c13832159e  
 ⌂        SDB 🌡  18.3°C 💧 39.1%  - 🔋 100% 📶-73db - GVH5075_9448 - a4c138409448  
 ⌂      Salon 🌡  18.9°C 💧 36.6%  - 🔋 100% 📶-71db - GVH5075_A1B2 - a4c138d0a1b2  
 ⌂    Chambre 🌡  18.5°C 💧 37.6%  - 🔋  61% 📶-63db - GVH5075_4416 - a4c138e84416  

## Install

Require a C++20 compiler + [ninja](https://ninja-build.org/) + bluez dev stack

From this repo.
```bash
git clone git@github.com:yduf/Govee-monitor.git

sudo apt-get install bluetooth bluez libbluetooth-dev

ninja # build
```

run `sudo build/goveescan`

## Room naming

Edit `.govee_logger` or `.conf/govee_logger` which is a Toml file to name the room in which a device is.
see example config.

## Notes

Dependencies included in this repo, for easy conf & args parsing.
- [toml++](https://github.com/juzzlin/Argengine) - to read config file
- [Argengine](https://github.com/juzzlin/Argengine) - for cli options


## Thanks to
- [wcbonner](https://github.com/wcbonner/GoveeBTTempLogger), that's the project that got me started using bluez on my own. Contains a ton of info and links about Govee devices as well.
  
## Notes from [btmon source](https://github.com/bluez/bluez/blob/293d670fb0ec51b69cdd0b9bf625b1e4d3a7975f/monitor/analyze.c#L749)

- [analyze_trace](https://github.com/bluez/bluez/blob/293d670fb0ec51b69cdd0b9bf625b1e4d3a7975f/monitor/analyze.c#L749)
- [btsnoop_read_hci](https://github.com/bluez/bluez/blob/8fe1e5e165ad7b4f7c318f507aa85cd747401b81/src/shared/btsnoop.c#L491)
- [event_pkt](https://github.com/bluez/bluez/blob/293d670fb0ec51b69cdd0b9bf625b1e4d3a7975f/monitor/analyze.c#L517)
- [bt_hci_evt_hdr](https://github.com/bluez/bluez/blob/d84ce72a543e090665a33ecac64b604805d2ec4c/monitor/bt.h#L511)
- [PKTLOG_HDR_SIZE](https://github.com/bluez/bluez/blob/9be85f867856195e16c9b94b605f65f6389eda33/tools/hcidump.c#L78)