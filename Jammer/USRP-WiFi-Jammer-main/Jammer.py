

#!/usr/bin/env python3

"""
USRP WiFi jammer (patched for precomputed buffers).

Changes:
- Precompute large TX buffers for: white_noise, gated_white_noise (aka pulsed), and pulse_stream.
- Reuse buffers in the TX loop to avoid per-iteration allocations.
- Align send sizes to the TX streamer's max packet size for stability.
- Keep CPU dtype as complex64 (fc32) and on-the-wire sc16 for performance + compatibility.
- Added waveform "pulse_stream" and alias "gated_white_noise".

Safety note: Transmitting may disrupt 2.4 GHz devices. Use responsibly and within regulations.
"""

import sys, threading, time, queue
import numpy as np
from PyQt5 import QtCore, QtGui, QtWidgets
import pyqtgraph as pg
import uhd

# --- Parameters ---

FS = 3_260_000  # Consider 20_000_000 on USB3, lower for USB2
P_MAX_DBM = 7.5
DEFAULT_CH = 1
DEFAULT_PRF = 25_000.0   # Pulse repetition frequency (Hz)
DEFAULT_DUTY = 0.20      # Duty cycle 0..1
# Size target for precomputed buffers (samples). At FS=20e6, 8_388_608 samples ≈ 0.42 s and ~64 MB as complex64.
PRECOMP_SAMPS_TARGET = 8_388_608

RX_FFT_SIZE = 1024
RX_FFT_UPDATES_HZ = 6
RX_AVG_ALPHA = 0.3


# --- Helpers ---

def channel_to_freq(ch: int) -> float:
    return 2.407e9 + 5e6 * ch

def hann_window(n):
    return np.hanning(n).astype(np.float32)

def _pulsed_gate(n, fs, prf_hz, duty):
    """Return float32 gate of length n with ones during 'on' portion of each period."""
    prf_hz = max(1.0, float(prf_hz))
    duty = float(np.clip(duty, 0.0, 1.0))
    # Period in samples (>=1), on-length in samples (>=0)
    T = max(1, int(round(fs / prf_hz)))
    on = int(round(duty * T))
    g = np.zeros(n, dtype=np.float32)
    # Vectorized fill via slicing
    if on > 0:
        # For large n, loop stepping by T is fine; n/T iterations only.
        for i in range(0, n, T):
            g[i:i+on] = 1.0
    return g

def _choose_precomp_len(fs: int, max_samps_per_pkt: int) -> int:
    """
    Choose a large buffer length that's:
    - at least one full second if memory allows (capped by target),
    - a multiple of the packet size for zero-fragment sends.
    """
    # Start from target; ensure at least max_samps_per_pkt and make it a multiple.
    N = max(PRECOMP_SAMPS_TARGET, max_samps_per_pkt)
    remainder = N % max_samps_per_pkt
    if remainder:
        N += (max_samps_per_pkt - remainder)
    return int(N)

# --- SDR interface ---

class USRPRadio(QtCore.QObject):
    log_msg = QtCore.pyqtSignal(str)
    tx_state_changed = QtCore.pyqtSignal(bool)

    def __init__(self, parent=None):
        super().__init__(parent)
        self.usrp = None
        self.max_gain = 0.0
        self.center_freq_hz = None
        self._tx_running = False
        self._rx_running = False
        self._tx_thread = None
        self._rx_thread = None
        self._rx_psd_q = queue.Queue(maxsize=2)

    @property
    def tx_running(self): return self._tx_running
    @property
    def rx_running(self): return self._rx_running

    def connect(self, fs=FS, tx_freq=None, rx_freq=None):
        try:
            # Consider adding device args like num_send_frames/size if you still underrun at very high rates.
            self.usrp = uhd.usrp.MultiUSRP("type=b200")
            # Good practice: set master clock to a clean multiple of Wi-Fi-ish rates
            try:
                self.usrp.set_master_clock_rate(30.72e6)
            except Exception:
                pass

            self.usrp.set_tx_rate(fs)
            self.usrp.set_rx_rate(fs)

            if tx_freq is not None:
                self.usrp.set_tx_freq(float(tx_freq))
            if rx_freq is None and tx_freq is not None:
                rx_freq = tx_freq
            if rx_freq is not None:
                self.usrp.set_rx_freq(float(rx_freq))

            # Set a wide-enough analog bandwidth
            try:
                self.usrp.set_tx_bandwidth(min(fs*0.9, fs))
                self.usrp.set_rx_bandwidth(min(fs*0.9, fs))
            except Exception:
                pass

            self.center_freq_hz = float(rx_freq if rx_freq is not None else (tx_freq or 0.0))

            gr = self.usrp.get_tx_gain_range()
            self.max_gain = float(gr.stop())
            self.usrp.set_tx_gain(self.max_gain / 2.0)
            try:
                self.usrp.set_rx_gain(20.0)
            except Exception:
                pass

            # Try to read a serial number (best-effort)
            serial = "unknown"
            for name in self.usrp.get_mboard_sensor_names(0):
                if "serial" in name.lower():
                    serial = str(self.usrp.get_mboard_sensor(name, 0))
                    break

            self.log_msg.emit(f"Connected @ {fs/1e6:.1f} MS/s  SN: {serial}")
            return True
        except Exception as e:
            self.usrp = None
            self.log_msg.emit(f"Connect error: {e}")
            return False

    # --- TX ---

    def start_tx(self, wf_name:str, prf_hz=DEFAULT_PRF, duty=DEFAULT_DUTY):
        if self.usrp is None or self._tx_running:
            return

        # Normalize/alias names
        name = wf_name.strip().lower()
        if name == "gated_white_noise":
            name = "pulsed_white_noise"

        self._tx_running = True
        self.tx_state_changed.emit(True)

        def _tx_loop():
            try:
                # Use fc32 on CPU (numpy.complex64), sc16 on-wire.
                sa = uhd.usrp.StreamArgs("fc32", "sc16")
                sa.channels = [0]
                tx_stream = self.usrp.get_tx_stream(sa)
                max_pkt = int(tx_stream.get_max_num_samps() or 4096)

                # Decide precomputed buffer length
                N_buf = _choose_precomp_len(int(self.usrp.get_tx_rate()), max_pkt)

                # Precompute according to waveform
                if name == "white_noise":
                    # Complex white noise, ~0.1 RMS per component
                    sigma = 0.1
                    buf = (np.random.randn(N_buf).astype(np.float32) + 1j*np.random.randn(N_buf).astype(np.float32)) * sigma
                    buf = buf.astype(np.complex64, copy=False)

                elif name == "pulsed_white_noise":
                    sigma = 0.1
                    noise = (np.random.randn(N_buf).astype(np.float32) + 1j*np.random.randn(N_buf).astype(np.float32)) * sigma
                    gate = _pulsed_gate(N_buf, int(self.usrp.get_tx_rate()), prf_hz, duty)
                    buf = (noise.astype(np.complex64) * gate.astype(np.float32)).astype(np.complex64, copy=False)

                elif name == "pulse_stream":
                    # Baseband rectangular pulses at constant amplitude (complex)
                    amp = 0.3
                    gate = _pulsed_gate(N_buf, int(self.usrp.get_tx_rate()), prf_hz, duty)
                    buf = (gate.astype(np.float32) * amp).astype(np.complex64)

                else:
                    # Fallback: continuous white noise
                    sigma = 0.1
                    buf = (np.random.randn(N_buf).astype(np.float32) + 1j*np.random.randn(N_buf).astype(np.float32)) * sigma
                    buf = buf.astype(np.complex64, copy=False)
                    self.log_msg.emit(f"Unknown waveform '{wf_name}', defaulting to white_noise.")

                # Metadata
                meta = uhd.types.TXMetadata()
                meta.start_of_burst = True
                meta.end_of_burst = False
                meta.has_time_spec = False

                # Send in aligned chunks, wrapping around the precomputed circular buffer
                idx = 0
                while self._tx_running:
                    end = idx + max_pkt
                    if end <= N_buf:
                        chunk = buf[idx:end]
                        idx = end
                        if idx == N_buf:
                            idx = 0
                    else:
                        # Wrap: concatenate tail and head without allocating large temps
                        tail = buf[idx:N_buf]
                        head = buf[0:(end - N_buf)]
                        # Create a small contiguous view for the packet send
                        chunk = np.empty(max_pkt, dtype=np.complex64)
                        chunk[:tail.size] = tail
                        chunk[tail.size:] = head
                        idx = (end - N_buf)

                    # First packet: start_of_burst True, then False
                    tx_stream.send(chunk, meta)
                    meta.start_of_burst = False

                # End burst cleanly
                meta.end_of_burst = True
                try:
                    tx_stream.send(np.zeros(0, dtype=np.complex64), meta)
                except Exception:
                    pass

            except Exception as e:
                self.log_msg.emit(f"TX error: {e}")
            finally:
                self._tx_running = False
                self.tx_state_changed.emit(False)

        self._tx_thread = threading.Thread(target=_tx_loop, daemon=True)
        self._tx_thread.start()

        if name == "pulsed_white_noise":
            self.log_msg.emit(f"TX started: gated_white_noise (PRF={prf_hz:.0f} Hz, Duty={duty:.2f})")
        elif name == "pulse_stream":
            self.log_msg.emit(f"TX started: pulse_stream (PRF={prf_hz:.0f} Hz, Duty={duty:.2f})")
        else:
            self.log_msg.emit("TX started: white_noise")

    def stop_tx(self):
        self._tx_running = False

    def set_tx_gain(self, gain_db):
        if self.usrp is None: return
        try:
            self.usrp.set_tx_gain(float(gain_db))
        except Exception as e:
            self.log_msg.emit(f"TX gain error: {e}")

    def set_tx_freq(self, freq_hz):
        if self.usrp is None: return
        try:
            self.usrp.set_tx_freq(float(freq_hz))
        except Exception as e:
            self.log_msg.emit(f"TX freq error: {e}")

    def set_rx_freq(self, freq_hz):
        if self.usrp is None: return
        try:
            self.usrp.set_rx_freq(float(freq_hz))
            self.center_freq_hz = float(freq_hz)
        except Exception as e:
            self.log_msg.emit(f"RX freq error: {e}")

    def tune_both(self, freq_hz):
        self.set_tx_freq(freq_hz)
        self.set_rx_freq(freq_hz)

    # --- RX and FFT ---

    def start_rx_fft(self, fft_size=RX_FFT_SIZE, updates_hz=RX_FFT_UPDATES_HZ, avg_alpha=RX_AVG_ALPHA):
        if self.usrp is None or self._rx_running:
            return

        self._rx_running = True

        def _rx_loop():
            try:
                sa = uhd.usrp.StreamArgs("fc32", "sc16")
                sa.channels = [0]
                rx_stream = self.usrp.get_rx_stream(sa)
                md = uhd.types.RXMetadata()

                scmd = uhd.types.StreamCMD(uhd.types.StreamMode.start_cont)
                scmd.stream_now = True
                scmd.time_spec = uhd.types.TimeSpec(0)
                scmd.num_samps = 0
                rx_stream.issue_stream_cmd(scmd)

                w = hann_window(fft_size)
                U = np.sum(w**2)
                fs = float(self.usrp.get_rx_rate())
                base_f_axis = np.fft.fftshift(np.fft.fftfreq(fft_size, d=1.0/fs)) / 1e6  # MHz (baseband)
                psd_avg = None

                samps = np.empty(fft_size, dtype=np.complex64)
                last_update = time.time()
                update_period = 1.0 / max(1.0, updates_hz)

                while self._rx_running:
                    num_rx = rx_stream.recv(samps, md, timeout=0.25)
                    if num_rx <= 0:
                        continue

                    err = md.error_code
                    if err != uhd.types.RXMetadataErrorCode.none:
                        if err in (uhd.types.RXMetadataErrorCode.overflow,
                                   uhd.types.RXMetadataErrorCode.timeout):
                            continue
                        self.log_msg.emit(f"RX meta: {getattr(err, 'name', str(err))}")
                        continue

                    x = samps[:min(num_rx, fft_size)]
                    if x.size < fft_size:
                        xb = np.zeros(fft_size, dtype=np.complex64)
                        xb[-x.size:] = x
                        x = xb

                    xw = x * w
                    X = np.fft.fftshift(np.fft.fft(xw, n=fft_size))
                    Pxx = (np.abs(X)**2) / (U * fs)
                    Pxx_db = 10*np.log10(np.maximum(Pxx, 1e-20))

                    psd_avg = Pxx_db if psd_avg is None else (1.0 - avg_alpha) * psd_avg + avg_alpha * Pxx_db

                    now = time.time()
                    if now - last_update >= update_period:
                        last_update = now
                        cf_mhz = (self.center_freq_hz or 0.0) / 1e6
                        f_abs = base_f_axis + cf_mhz
                        try:
                            if not self._rx_psd_q.full():
                                self._rx_psd_q.put((f_abs, psd_avg.copy()), block=False)
                        except Exception:
                            pass

                rx_stream.issue_stream_cmd(uhd.types.StreamCMD(uhd.types.StreamMode.stop_cont))

            except Exception as e:
                self.log_msg.emit(f"RX error: {e}")
            finally:
                self._rx_running = False

        self._rx_thread = threading.Thread(target=_rx_loop, daemon=True)
        self._rx_thread.start()
        self.log_msg.emit("RX FFT started")

    def stop_rx(self):
        self._rx_running = False

    def poll_psd(self):
        try:
            return self._rx_psd_q.get_nowait()
        except queue.Empty:
            return None


# --- GUI ---

class MainWindow(QtWidgets.QMainWindow):
    def __init__(self):
        super().__init__()
        self.setWindowTitle("USRP WiFi Jammer")
        self.setMinimumSize(1000, 650)

        self.radio = USRPRadio()
        self.radio.log_msg.connect(self._append_log)
        self.radio.tx_state_changed.connect(self._update_tx_status)

        cw = QtWidgets.QWidget(self); self.setCentralWidget(cw)
        main = QtWidgets.QVBoxLayout(cw)

        ctrl = QtWidgets.QGroupBox("Radio Control"); main.addWidget(ctrl)
        grid = QtWidgets.QGridLayout(ctrl)

        self.btn_connect = QtWidgets.QPushButton("Connect")
        self.btn_connect.clicked.connect(self.on_connect)
        grid.addWidget(self.btn_connect, 0, 0)

        grid.addWidget(QtWidgets.QLabel("Wi-Fi Channel:"), 0, 1)
        self.cb_chan = QtWidgets.QComboBox()
        self.cb_chan.addItems([str(i) for i in range(1, 12)])
        self.cb_chan.setCurrentText(str(DEFAULT_CH))
        self.cb_chan.currentIndexChanged.connect(self.on_channel_changed)
        grid.addWidget(self.cb_chan, 0, 2)

        grid.addWidget(QtWidgets.QLabel("Waveform:"), 0, 3)
        self.cb_wf = QtWidgets.QComboBox()
        # Added "pulse_stream" and alias "gated_white_noise"
        self.cb_wf.addItems(["white_noise", "pulsed_white_noise", "gated_white_noise", "pulse_stream"])
        grid.addWidget(self.cb_wf, 0, 4)

        grid.addWidget(QtWidgets.QLabel("PRF (Hz):"), 1, 1)
        self.ed_prf = QtWidgets.QLineEdit(f"{DEFAULT_PRF:.0f}")
        grid.addWidget(self.ed_prf, 1, 2)
        grid.addWidget(QtWidgets.QLabel("Duty (0..1):"), 1, 3)
        self.ed_duty = QtWidgets.QLineEdit(f"{DEFAULT_DUTY:.2f}")
        grid.addWidget(self.ed_duty, 1, 4)

        grid.addWidget(QtWidgets.QLabel("TX Gain (dB):"), 2, 0)
        self.s_gain = QtWidgets.QSlider(QtCore.Qt.Horizontal)
        self.s_gain.setEnabled(False); self.s_gain.setMinimum(0); self.s_gain.setMaximum(1000)
        self.s_gain.valueChanged.connect(self.on_gain_change)
        grid.addWidget(self.s_gain, 2, 1, 1, 3)
        self.lbl_gain = QtWidgets.QLabel("— dB (— dBm)")
        grid.addWidget(self.lbl_gain, 2, 4)

        self.btn_tx = QtWidgets.QPushButton("TX Enable"); self.btn_tx.setEnabled(False)
        self.btn_tx.clicked.connect(self.on_toggle_tx)
        grid.addWidget(self.btn_tx, 3, 0)

        self.btn_rx = QtWidgets.QPushButton("RX FFT Enable"); self.btn_rx.setEnabled(False)
        self.btn_rx.clicked.connect(self.on_toggle_rx)
        grid.addWidget(self.btn_rx, 3, 1)

        self.lbl_status = QtWidgets.QLabel("Disconnected"); grid.addWidget(self.lbl_status, 3, 2, 1, 3)

        fft_group = QtWidgets.QGroupBox("RX FFT (PSD)"); main.addWidget(fft_group)
        v_fft = QtWidgets.QVBoxLayout(fft_group)
        self.plot = pg.PlotWidget()
        self.plot.setLabel('left', 'PSD', units='dB/Hz')
        self.plot.setLabel('bottom', 'Frequency', units='MHz')
        self.plot.showGrid(x=True, y=True)
        self.curve = self.plot.plot(pen='w')
        v_fft.addWidget(self.plot)

        log_group = QtWidgets.QGroupBox("Log"); main.addWidget(log_group)
        v_log = QtWidgets.QVBoxLayout(log_group)
        self.txt_log = QtWidgets.QPlainTextEdit(); self.txt_log.setReadOnly(True)
        v_log.addWidget(self.txt_log)

        self.timer = QtCore.QTimer(self)
        self.timer.setInterval(int(1000 / RX_FFT_UPDATES_HZ))
        self.timer.timeout.connect(self.on_timer)
        self.timer.start()

        self._tx_gain_max = 1.0
        self._closing = False

    # --- UI Handlers ---

    def on_connect(self):
        ch = int(self.cb_chan.currentText())
        freq = channel_to_freq(ch)
        ok = self.radio.connect(fs=FS, tx_freq=freq, rx_freq=freq)
        if ok:
            self.btn_tx.setEnabled(True); self.btn_rx.setEnabled(True)
            self._tx_gain_max = max(self.radio.max_gain, 1.0)
            self.s_gain.setEnabled(True)
            try: cur = self.radio.usrp.get_tx_gain()
            except Exception: cur = self._tx_gain_max / 2
            self.s_gain.setValue(int(1000 * cur / self._tx_gain_max))
            self._update_gain_label(cur)
            self.lbl_status.setText(f"TX OFF | f={freq/1e6:.3f} MHz")
        else:
            self.lbl_status.setText("Connect failed (see log)")

    def on_channel_changed(self, _idx):
        if self.radio.usrp is None:
            return
        ch = int(self.cb_chan.currentText())
        freq = channel_to_freq(ch)
        self.radio.tune_both(freq)
        self._append_log(f"Tuned TX/RX to ch {ch}: {freq/1e6:.3f} MHz")
        prefix = "TX ON | " if self.radio.tx_running else "TX OFF | "
        self.lbl_status.setText(f"{prefix}f={freq/1e6:.3f} MHz")

    def on_gain_change(self, slider_val):
        gain = (float(slider_val) / 1000.0) * self._tx_gain_max
        self.radio.set_tx_gain(gain)
        self._update_gain_label(gain)

    def _update_gain_label(self, gain_db):
        power_dbm = (gain_db / max(self._tx_gain_max, 1e-9)) * P_MAX_DBM
        self.lbl_gain.setText(f"{gain_db:.1f} dB   (~{power_dbm:.1f} dBm, uncal)")

    def on_toggle_tx(self):
        if not self.radio.tx_running:
            wf = self.cb_wf.currentText()
            try:
                prf = float(self.ed_prf.text()); duty = float(self.ed_duty.text())
            except ValueError:
                prf, duty = DEFAULT_PRF, DEFAULT_DUTY
                self._append_log("Invalid PRF/Duty; using defaults.")
            self.radio.start_tx(wf, prf_hz=prf, duty=duty)
            self.btn_tx.setText("TX Disable")
            freq = (self.radio.center_freq_hz or 0.0) / 1e6
            self.lbl_status.setText(f"TX ON | f={freq:.3f} MHz")
        else:
            self.radio.stop_tx()
            self.btn_tx.setText("TX Enable")
            freq = (self.radio.center_freq_hz or 0.0) / 1e6
            self.lbl_status.setText(f"TX OFF | f={freq:.3f} MHz")

    def on_toggle_rx(self):
        if not self.radio.rx_running:
            self.radio.start_rx_fft()
            self.btn_rx.setText("RX FFT Disable")
        else:
            self.radio.stop_rx()
            self.btn_rx.setText("RX FFT Enable")

    def on_timer(self):
        psd = self.radio.poll_psd()
        if psd is not None:
            f_mhz, psd_db = psd
            self.curve.setData(f_mhz, psd_db)

    def _append_log(self, s: str):
        self.txt_log.appendPlainText(s)

    def _update_tx_status(self, running: bool):
        freq = (self.radio.center_freq_hz or 0.0) / 1e6
        self.lbl_status.setText(("TX ON | " if running else "TX OFF | ") + f"f={freq:.3f} MHz")

    # Clean exit
    def closeEvent(self, event: QtGui.QCloseEvent):
        if getattr(self, "_closing", False):
            event.accept(); return
        self._closing = True
        try:
            self.radio.stop_tx(); self.radio.stop_rx()
        except Exception:
            pass
        QtCore.QTimer.singleShot(150, self._final_close)
        event.ignore()

    def _final_close(self):
        self.timer.stop()
        self.deleteLater()
        QtWidgets.qApp.quit()

# --- Main ---

def main():
    app = QtWidgets.QApplication(sys.argv)
    try:
        QtWidgets.QApplication.setStyle('Fusion')
    except Exception:
        pass
    win = MainWindow(); win.show()
    try:
        sys.exit(app.exec_())
    finally:
        try: win.radio.stop_tx(); win.radio.stop_rx()
        except Exception: pass

if __name__ == "__main__":
    main()