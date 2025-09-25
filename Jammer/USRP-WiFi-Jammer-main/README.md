# USRP WiFi Jammer

Python GUI for controlling the USRP series of SDRs.
Automates control of the USRP, lets user view the spectrum with an RX FFT, and sending basic signals.

---

## Installation

### Ubuntu
```bash
sudo apt update
sudo apt install -y uhd-host
sudo uhd_images_downloader

python3 -m pip install numpy pyqt5 pyqtgraph
```
---

## Usage

1. Connect the USRP over USB
2. Verify detection:
   ```bash
   uhd_find_devices
   ```
3. Start the GUI:
   ```bash
   python3 Jammer.py
   ```
4. In the app:
   - Press **Connect** to initialize the USRP.
   - Use **RX FFT Enable** to monitor activity.
   - Configure waveform options (frequency, waveform, output level).
   - Press **TX Enable** to start transmitting.

---

## Known Issues

- The GUI may freeze while firmware/images are being loaded.
  *Fix:* close the app, unplug/replug the USRP, rerun `uhd_find_devices`, and restart.
- The GUI can sometimes crash and leave the device handle busy.
  *Fix:* unplug/replug the USRP, or reboot if necessary.
--
