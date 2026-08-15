#!/usr/bin/env python3
"""
CIPHER Desktop GUI
A native chat window for CIPHER LoRa mesh nodes.

Connect to any powered-on CIPHER node over its WiFi AP, or over your LAN
if the node is on the same network.

Requirements:
    pip install PySide6 websocket-client

Usage:
    python cipher_gui.py
    python cipher_gui.py --host 192.168.4.1 --port 80
"""

import sys
import json
import threading
import argparse
from datetime import datetime

from PySide6.QtWidgets import (
    QApplication, QMainWindow, QWidget, QVBoxLayout, QHBoxLayout,
    QLineEdit, QPushButton, QScrollArea, QLabel, QFrame, QSizePolicy,
    QDialog, QFormLayout, QDialogButtonBox, QStatusBar, QSystemTrayIcon,
    QMenu, QStyle,
)
from PySide6.QtCore import Qt, Signal, QObject, QTimer, QSize, QThread
from PySide6.QtGui import (
    QFont, QPalette, QColor, QIcon, QKeyEvent, QPixmap, QPainter,
)

import websocket  # pip install websocket-client

# ─── Colour palette (matches the embedded web UI) ─────────────────────────
C_BG      = "#0d0d0d"
C_SURFACE = "#1a1a1a"
C_BORDER  = "#2a2a2a"
C_ACCENT  = "#00ff88"
C_TEXT    = "#e0e0e0"
C_DIM     = "#888888"
C_OWN_BG  = "#004d29"
C_OWN_FG  = "#b0ffd0"
C_OTHER   = "#1e1e1e"

# ─── WebSocket worker ─────────────────────────────────────────────────────

class WsWorker(QObject):
    """Runs the WebSocket connection in a background thread."""

    message_received = Signal(dict)   # dict from parsed JSON
    connected        = Signal()
    disconnected     = Signal()

    def __init__(self, host: str, port: int):
        super().__init__()
        self.host = host
        self.port = port
        self._ws: websocket.WebSocketApp | None = None
        self._stop = False

    def start(self):
        t = threading.Thread(target=self._run, daemon=True)
        t.start()

    def _run(self):
        url = f"ws://{self.host}:{self.port}/ws"
        while not self._stop:
            try:
                self._ws = websocket.WebSocketApp(
                    url,
                    on_open    = self._on_open,
                    on_message = self._on_message,
                    on_close   = self._on_close,
                    on_error   = self._on_error,
                )
                self._ws.run_forever(ping_interval=20, ping_timeout=10)
            except Exception as e:
                print(f"[WS] Error: {e}")
            if not self._stop:
                import time; time.sleep(3)

    def _on_open(self, ws):
        self.connected.emit()

    def _on_message(self, ws, message):
        try:
            data = json.loads(message)
            self.message_received.emit(data)
        except Exception:
            pass

    def _on_close(self, ws, code, msg):
        self.disconnected.emit()

    def _on_error(self, ws, error):
        print(f"[WS] {error}")

    def send(self, data: dict):
        if self._ws:
            try:
                self._ws.send(json.dumps(data))
            except Exception as e:
                print(f"[WS] Send error: {e}")

    def stop(self):
        self._stop = True
        if self._ws:
            self._ws.close()


# ─── Message bubble widget ────────────────────────────────────────────────

class Bubble(QFrame):
    def __init__(self, sender: str, text: str, rssi: int | None,
                 own: bool, parent=None):
        super().__init__(parent)

        layout = QVBoxLayout(self)
        layout.setContentsMargins(0, 0, 0, 0)
        layout.setSpacing(2)

        # Sender label (only for received messages)
        if not own:
            sender_lbl = QLabel(sender)
            sender_lbl.setStyleSheet(f"color:{C_DIM};font-size:10px;padding:0 4px;")
            layout.addWidget(sender_lbl)

        # Message text
        text_lbl = QLabel(text)
        text_lbl.setWordWrap(True)
        text_lbl.setTextInteractionFlags(Qt.TextSelectableByMouse)
        text_lbl.setStyleSheet(
            f"padding:8px 14px;border-radius:16px;font-size:13px;"
            f"background:{'%s;color:%s;border-bottom-right-radius:4px' % (C_OWN_BG, C_OWN_FG) if own else '%s;color:%s;border:1px solid %s;border-bottom-left-radius:4px' % (C_OTHER, C_TEXT, C_BORDER)};"
        )
        text_lbl.setSizePolicy(QSizePolicy.Preferred, QSizePolicy.Minimum)
        layout.addWidget(text_lbl)

        # Meta line: time + RSSI
        now = datetime.now().strftime("%H:%M")
        meta = now + (f"  ● {rssi} dBm" if rssi and rssi != -1 else "")
        meta_lbl = QLabel(meta)
        meta_lbl.setStyleSheet(f"color:{C_DIM};font-size:9px;padding:0 4px;")
        layout.addWidget(meta_lbl)

        self.setStyleSheet("background:transparent;border:none;")


# ─── Settings dialog ──────────────────────────────────────────────────────

class SettingsDialog(QDialog):
    def __init__(self, host: str, port: int, parent=None):
        super().__init__(parent)
        self.setWindowTitle("CIPHER — Settings")
        self.setModal(True)
        self.setMinimumWidth(320)
        self.setStyleSheet(f"background:{C_SURFACE};color:{C_TEXT};")

        form = QFormLayout(self)
        form.setContentsMargins(16, 16, 16, 16)
        form.setSpacing(10)

        self.host_edit = QLineEdit(host)
        self.host_edit.setStyleSheet(
            f"background:{C_BG};color:{C_TEXT};border:1px solid {C_BORDER};"
            f"border-radius:4px;padding:6px;"
        )
        self.port_edit = QLineEdit(str(port))
        self.port_edit.setStyleSheet(self.host_edit.styleSheet())

        form.addRow(QLabel("Node IP / Host:"), self.host_edit)
        form.addRow(QLabel("Port:"), self.port_edit)

        buttons = QDialogButtonBox(
            QDialogButtonBox.Ok | QDialogButtonBox.Cancel
        )
        buttons.setStyleSheet(
            f"QPushButton{{background:{C_ACCENT};color:#000;border:none;"
            f"border-radius:4px;padding:6px 18px;font-weight:bold;}}"
            f"QPushButton:hover{{background:{C_ACCENT}cc;}}"
            f"QPushButton[text='Cancel']{{background:{C_SURFACE};"
            f"color:{C_TEXT};border:1px solid {C_BORDER};}}"
        )
        buttons.accepted.connect(self.accept)
        buttons.rejected.connect(self.reject)
        form.addRow(buttons)

    def values(self):
        return self.host_edit.text().strip(), int(self.port_edit.text().strip())


# ─── Send input field (Enter = send) ─────────────────────────────────────

class SendInput(QLineEdit):
    send_requested = Signal()

    def keyPressEvent(self, event: QKeyEvent):
        if event.key() in (Qt.Key_Return, Qt.Key_Enter):
            self.send_requested.emit()
        else:
            super().keyPressEvent(event)


# ─── Main window ──────────────────────────────────────────────────────────

class CipherWindow(QMainWindow):
    def __init__(self, host: str, port: int):
        super().__init__()
        self.host = host
        self.port = port
        self.node_name = "..."
        self.connected = False

        self.setWindowTitle("CIPHER")
        self.setMinimumSize(420, 620)
        self.resize(480, 700)
        self._apply_palette()
        self._build_ui()
        self._build_tray()
        self._start_ws()

    # ── Palette ──────────────────────────────────────────────────────────
    def _apply_palette(self):
        self.setStyleSheet(f"""
            QMainWindow, QWidget {{ background:{C_BG}; color:{C_TEXT}; }}
            QScrollArea {{ border:none; background:{C_BG}; }}
            QScrollBar:vertical {{
                background:{C_SURFACE}; width:6px; border-radius:3px;
            }}
            QScrollBar::handle:vertical {{
                background:{C_BORDER}; border-radius:3px; min-height:20px;
            }}
            QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical {{
                height:0;
            }}
        """)

    # ── UI layout ─────────────────────────────────────────────────────────
    def _build_ui(self):
        central = QWidget()
        self.setCentralWidget(central)
        root = QVBoxLayout(central)
        root.setContentsMargins(0, 0, 0, 0)
        root.setSpacing(0)

        # ── Header ──
        header = QWidget()
        header.setFixedHeight(52)
        header.setStyleSheet(
            f"background:{C_SURFACE};border-bottom:1px solid {C_BORDER};"
        )
        hlay = QHBoxLayout(header)
        hlay.setContentsMargins(14, 0, 14, 0)
        hlay.setSpacing(8)

        logo = QLabel("CIPHER")
        logo.setStyleSheet(
            f"color:{C_ACCENT};font-weight:700;font-size:15px;letter-spacing:3px;"
        )
        self.node_lbl = QLabel("connecting...")
        self.node_lbl.setStyleSheet(f"color:{C_DIM};font-size:12px;")

        self.rssi_lbl = QLabel("")
        self.rssi_lbl.setStyleSheet(f"color:{C_DIM};font-size:11px;")

        self.status_dot = QLabel("●")
        self.status_dot.setStyleSheet(f"color:#333;font-size:14px;")

        settings_btn = QPushButton("⚙")
        settings_btn.setFixedSize(30, 30)
        settings_btn.setToolTip("Settings")
        settings_btn.setCursor(Qt.PointingHandCursor)
        settings_btn.setStyleSheet(
            f"QPushButton{{background:transparent;color:{C_DIM};"
            f"border:none;font-size:16px;}}"
            f"QPushButton:hover{{color:{C_ACCENT};}}"
        )
        settings_btn.clicked.connect(self._open_settings)

        hlay.addWidget(logo)
        hlay.addWidget(self.node_lbl)
        hlay.addStretch()
        hlay.addWidget(self.rssi_lbl)
        hlay.addWidget(self.status_dot)
        hlay.addWidget(settings_btn)
        root.addWidget(header)

        # ── Messages scroll area ──
        self.scroll = QScrollArea()
        self.scroll.setWidgetResizable(True)
        self.scroll.setHorizontalScrollBarPolicy(Qt.ScrollBarAlwaysOff)

        self.msg_container = QWidget()
        self.msg_container.setStyleSheet(f"background:{C_BG};")
        self.msg_layout = QVBoxLayout(self.msg_container)
        self.msg_layout.setContentsMargins(10, 10, 10, 10)
        self.msg_layout.setSpacing(6)
        self.msg_layout.addStretch()   # pushes messages to bottom

        self.scroll.setWidget(self.msg_container)
        root.addWidget(self.scroll, 1)

        # ── Input bar ──
        bar = QWidget()
        bar.setFixedHeight(64)
        bar.setStyleSheet(
            f"background:{C_SURFACE};border-top:1px solid {C_BORDER};"
        )
        blay = QHBoxLayout(bar)
        blay.setContentsMargins(10, 10, 10, 10)
        blay.setSpacing(8)

        self.input = SendInput()
        self.input.setPlaceholderText("Type a message...")
        self.input.setMaxLength(187)
        self.input.setEnabled(False)
        self.input.setStyleSheet(
            f"QLineEdit{{background:{C_BG};color:{C_TEXT};"
            f"border:1px solid {C_BORDER};border-radius:20px;"
            f"padding:8px 16px;font-size:13px;}}"
            f"QLineEdit:focus{{border-color:{C_ACCENT};}}"
            f"QLineEdit:disabled{{color:{C_DIM};}}"
        )
        self.input.send_requested.connect(self._send)

        self.send_btn = QPushButton("➤")
        self.send_btn.setFixedSize(42, 42)
        self.send_btn.setCursor(Qt.PointingHandCursor)
        self.send_btn.setEnabled(False)
        self.send_btn.setStyleSheet(
            f"QPushButton{{background:{C_ACCENT};color:#000;border:none;"
            f"border-radius:21px;font-size:16px;font-weight:bold;}}"
            f"QPushButton:hover{{background:{C_ACCENT}cc;}}"
            f"QPushButton:disabled{{background:#2a2a2a;color:#555;}}"
        )
        self.send_btn.clicked.connect(self._send)

        blay.addWidget(self.input)
        blay.addWidget(self.send_btn)
        root.addWidget(bar)

        # ── Status bar ──
        self.status_bar = QStatusBar()
        self.status_bar.setStyleSheet(
            f"background:{C_SURFACE};color:{C_DIM};font-size:11px;"
            f"border-top:1px solid {C_BORDER};"
        )
        self.setStatusBar(self.status_bar)
        self.status_bar.showMessage(f"Connecting to {self.host}:{self.port}...")

    # ── System tray ───────────────────────────────────────────────────────
    def _build_tray(self):
        if not QSystemTrayIcon.isSystemTrayAvailable():
            return

        # Create a simple coloured icon
        px = QPixmap(16, 16)
        px.fill(Qt.transparent)
        p = QPainter(px)
        p.setBrush(QColor(C_ACCENT))
        p.setPen(Qt.NoPen)
        p.drawEllipse(0, 0, 16, 16)
        p.end()

        self.tray = QSystemTrayIcon(QIcon(px), self)
        menu = QMenu()
        menu.addAction("Show", self.show)
        menu.addAction("Quit", QApplication.quit)
        self.tray.setContextMenu(menu)
        self.tray.activated.connect(lambda reason: self.show()
                                    if reason == QSystemTrayIcon.Trigger else None)
        self.tray.show()

    # ── WebSocket ─────────────────────────────────────────────────────────
    def _start_ws(self):
        self.worker = WsWorker(self.host, self.port)
        self.worker.connected.connect(self._on_connected)
        self.worker.disconnected.connect(self._on_disconnected)
        self.worker.message_received.connect(self._on_message)
        self.worker.start()

    def _on_connected(self):
        self.connected = True
        self.status_dot.setStyleSheet(f"color:{C_ACCENT};font-size:14px;")
        self.input.setEnabled(True)
        self.send_btn.setEnabled(True)
        self.status_bar.showMessage("Connected")
        self.input.setFocus()

    def _on_disconnected(self):
        self.connected = False
        self.status_dot.setStyleSheet("color:#333;font-size:14px;")
        self.node_lbl.setText("reconnecting...")
        self.rssi_lbl.setText("")
        self.input.setEnabled(False)
        self.send_btn.setEnabled(False)
        self.status_bar.showMessage(f"Disconnected — retrying {self.host}:{self.port}...")

    def _on_message(self, data: dict):
        msg_type = data.get("type", "")

        if msg_type == "status":
            self.node_name = data.get("node", "?")
            self.node_lbl.setText(self.node_name)
            rssi = data.get("rssi")
            self.rssi_lbl.setText(f"{rssi} dBm" if rssi else "")
            self.setWindowTitle(f"CIPHER — {self.node_name}")
            self.status_bar.showMessage(
                f"Connected to {self.node_name} @ {self.host}"
            )

        elif msg_type == "history":
            # Clear existing messages (below the stretch item)
            while self.msg_layout.count() > 1:
                item = self.msg_layout.takeAt(1)
                if item.widget():
                    item.widget().deleteLater()
            for m in data.get("messages", []):
                self._add_bubble(
                    m["from"], m["text"],
                    m.get("rssi"), m.get("own", False)
                )

        elif msg_type == "msg":
            self._add_bubble(
                data["from"], data["text"],
                data.get("rssi"), data.get("own", False)
            )
            # Tray notification for background messages
            if not self.isVisible() or not self.isActiveWindow():
                if hasattr(self, "tray"):
                    self.tray.showMessage(
                        f"CIPHER — {data['from']}",
                        data["text"],
                        QSystemTrayIcon.Information,
                        3000,
                    )

    def _add_bubble(self, sender: str, text: str,
                    rssi: int | None, own: bool):
        bubble = Bubble(sender, text, rssi, own)

        wrapper = QWidget()
        wrapper.setStyleSheet("background:transparent;")
        wlay = QHBoxLayout(wrapper)
        wlay.setContentsMargins(0, 0, 0, 0)

        bubble.setSizePolicy(QSizePolicy.Maximum, QSizePolicy.Minimum)
        bubble.setMaximumWidth(int(self.width() * 0.78))

        if own:
            wlay.addStretch()
            wlay.addWidget(bubble)
        else:
            wlay.addWidget(bubble)
            wlay.addStretch()

        # Insert before the trailing stretch
        self.msg_layout.insertWidget(self.msg_layout.count() - 1, wrapper)

        # Scroll to bottom
        QTimer.singleShot(50, lambda: self.scroll.verticalScrollBar()
                          .setValue(self.scroll.verticalScrollBar().maximum()))

    # ── Send ──────────────────────────────────────────────────────────────
    def _send(self):
        text = self.input.text().strip()
        if not text or not self.connected:
            return
        self.worker.send({"type": "send", "text": text})
        self.input.clear()

    # ── Settings ──────────────────────────────────────────────────────────
    def _open_settings(self):
        dlg = SettingsDialog(self.host, self.port, self)
        if dlg.exec() == QDialog.Accepted:
            host, port = dlg.values()
            if host != self.host or port != self.port:
                self.host = host
                self.port = port
                self.worker.stop()
                self._start_ws()
                self.status_bar.showMessage(f"Reconnecting to {host}:{port}...")

    def closeEvent(self, event):
        # Minimise to tray instead of quitting (if tray is available)
        if hasattr(self, "tray") and self.tray.isVisible():
            self.hide()
            event.ignore()
        else:
            self.worker.stop()
            event.accept()


# ─── Entry point ──────────────────────────────────────────────────────────

def main():
    parser = argparse.ArgumentParser(description="CIPHER Desktop Chat GUI")
    parser.add_argument("--host", default="192.168.4.1",
                        help="CIPHER node IP address (default: 192.168.4.1)")
    parser.add_argument("--port", default=80, type=int,
                        help="CIPHER node web port (default: 80)")
    args = parser.parse_args()

    app = QApplication(sys.argv)
    app.setApplicationName("CIPHER")
    app.setQuitOnLastWindowClosed(False)

    win = CipherWindow(args.host, args.port)
    win.show()

    sys.exit(app.exec())


if __name__ == "__main__":
    main()
