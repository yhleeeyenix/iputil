import socket
import time
import struct
import threading
from PyQt5 import QtWidgets, QtCore
import webbrowser
from PyQt5.QtCore import Qt
import ipaddress
from PyQt5.QtWidgets import QDialog, QLineEdit, QFormLayout, QDialogButtonBox, QHBoxLayout, QLabel, QWidget
from PyQt5.QtGui import QIntValidator

# Define constants
MSG_IP_SEARCH = 1
MSG_CAM_ACK = 2
MSG_IP_ACK = 3
MSG_IP_SET = 4
MSG_CAM_SET = 5
MSG_IPUTIL_SEARCH = 6
MSG_IPUTIL_RUN = 7
MSG_IP_REBOOT = 8
MSG_IP_UPDATE = 9
MSG_PTZ_UPDATE = 10
MSG_IP_MACUPDATE = 11
MSG_CAM_AUTOIP = 12
MSG_IP_DHCP = 13
MSG_OPCODE_MAX = 14

MAGIC = 0x73747278
SEND_PORT = 59163
LISTEN_PORT = 59164
MULTICAST_ADDRESS = '239.255.255.250'

# Camera Status Flg
FLAG_STATUS_AUTOIP = 0
FLAG_STATUS_DHCP = 1
FLAG_STATUS_MAX = 2

file_paths = []

# Functions to format MAC address, IP address and running time
def format_mac_address(mac):
    return ':'.join(f"{b:02x}" for b in mac[:6])

def format_ip_address(ip):
    return f"{ip & 0xff}.{(ip >> 8) & 0xff}.{(ip >> 16) & 0xff}.{(ip >> 24) & 0xff}"

def format_runtime(seconds):
    days = seconds // 86400
    hours = (seconds % 86400) // 3600
    minutes = (seconds % 3600) // 60
    secs = seconds % 60
    return f"{days} Day ( {hours:02}:{minutes:02}:{secs:02} )"

# Function to send multicast message
def send_multicast_message(sock, msg):
    host_name = socket.gethostname()
    try:
        host_info = socket.gethostbyname_ex(host_name)
    except socket.gaierror:
        return

    ip_list = host_info[2]

    for ip in ip_list:
        sock.setsockopt(socket.IPPROTO_IP, socket.IP_MULTICAST_IF, socket.inet_aton(ip))
        sock.sendto(msg, (MULTICAST_ADDRESS, SEND_PORT))

# Function to unpack received netMsg
def unpack_net_msg(data):
    unpacked_data = struct.unpack(
        '!BBHIII16sIIIIIIIII128sI20sBI6sI',
        data[:235]
    )

    return {
        "opcode": unpacked_data[0],
        "nettype": unpacked_data[1],
        "secs": unpacked_data[2],
        "xid": unpacked_data[3],
        "magic": unpacked_data[4],
        "ciaddr": socket.ntohl(unpacked_data[5]),
        "chaddr": unpacked_data[6],
        "yiaddr": socket.ntohl(unpacked_data[7]),
        "miaddr": socket.ntohl(unpacked_data[8]),
        "giaddr": socket.ntohl(unpacked_data[9]),
        "d1iaddr": socket.ntohl(unpacked_data[10]),
        "d2iaddr": socket.ntohl(unpacked_data[11]),
        "d3iaddr": socket.ntohl(unpacked_data[12]),
        "siaddr": socket.ntohl(unpacked_data[13]),
        "sport": socket.ntohl(unpacked_data[14]),
        "http": socket.ntohl(unpacked_data[15]),
        "vend": unpacked_data[16],
        "stream": socket.ntohl(unpacked_data[17]),
        "fw_ver": unpacked_data[18],
        "vidstd": unpacked_data[19],
        "flag": socket.ntohl(unpacked_data[20]),
        "chaddr2": unpacked_data[21],
        "runningtime": unpacked_data[22],
    }

def pack_net_msg(net_msg):
    return struct.pack('!BBHIII16sIIIIIIIII128sI20sBI6sI',
                       MSG_IP_ACK,
                       0,
                       0,
                       net_msg['xid'],
                       net_msg['magic'],
                       net_msg['ciaddr'],
                       net_msg['chaddr'],
                       net_msg['yiaddr'],
                       net_msg['miaddr'],
                       net_msg['giaddr'],
                       net_msg['d1iaddr'],
                       net_msg['d2iaddr'],
                       net_msg['d3iaddr'],
                       net_msg['siaddr'],
                       net_msg['sport'],
                       net_msg['http'],
                       net_msg['vend'],
                       net_msg['stream'],
                       net_msg['fw_ver'],
                       net_msg['vidstd'],
                       net_msg['flag'],
                       net_msg['chaddr2'],
                       net_msg['runningtime'])


# Function to listen for responses and print them
def listen_for_responses(sock, add_to_table, update_table, stop_event, seen_devices, received_packets):
    sock.settimeout(1)

    number = 0

    while not stop_event.is_set():
        try:
            data, address = sock.recvfrom(512)
            net_msg = unpack_net_msg(data)

            if net_msg:
                if net_msg['magic'] == MAGIC:
                    device_id = (net_msg['ciaddr'], net_msg['chaddr'])
                    if device_id in seen_devices:
                        received_packets[device_id] = net_msg
                        update_table(device_id, net_msg)
                        ack_msg = pack_net_msg(net_msg)
                        send_multicast_message(sock, ack_msg)
                        continue

                    seen_devices.add(device_id)

                    number += 1
                    mac_addr = format_mac_address(net_msg['chaddr'])
                    ip_addr = format_ip_address(net_msg['ciaddr'])
                    gateway = format_ip_address(net_msg['giaddr'])
                    subnet_mask = format_ip_address(net_msg['miaddr'])
                    dns1 = format_ip_address(net_msg['d1iaddr'])
                    dns2 = format_ip_address(net_msg['d2iaddr'])
                    http_port = socket.ntohl(net_msg['http'])
                    rtsp_port = socket.ntohl(net_msg['stream'])
                    autoip_status = 'ON' if socket.ntohl(net_msg['flag']) & (0x1 << 0) else 'OFF'
                    dhcp_status = 'ON' if socket.ntohl(net_msg['flag']) & (0x1 << 1) else 'OFF'
                    runtime = format_runtime(net_msg['runningtime'])

                    add_to_table((number,mac_addr, ip_addr, gateway, subnet_mask, dns1, dns2, http_port,
                                  runtime, autoip_status, dhcp_status))

                    received_packets[device_id] = net_msg
                    ack_msg = pack_net_msg(net_msg)
                    send_multicast_message(sock, ack_msg)

                else:
                    # print("magic error")
                    return

        except socket.timeout:
            continue

class IPInputWidget(QWidget):
    def __init__(self, parent=None):
        super().__init__(parent)
        self.initUI()

    def initUI(self):
        layout = QHBoxLayout()
        self.line_edits = []

        for i in range(4):
            line_edit = QLineEdit()
            line_edit.setMaxLength(3)
            line_edit.setValidator(QIntValidator(0, 255))
            line_edit.setFixedWidth(40)
            line_edit.setAlignment(Qt.AlignCenter)
            self.line_edits.append(line_edit)
            layout.addWidget(line_edit)

            if i < 3:
                dot_label = QLabel(".")
                layout.addWidget(dot_label)

        self.setLayout(layout)

    def text(self):
        return ".".join([le.text() for le in self.line_edits])

    def setText(self, text):
        parts = text.split('.')
        for i in range(4):
            self.line_edits[i].setText(parts[i])

class ChangeIPDialog(QDialog):
    def __init__(self, ip_addr='', netmask='', gateway='', dns1='', dns2='', parent=None):
        super().__init__(parent)
        self.setWindowTitle("Change IP Settings")

        layout = QFormLayout()

        self.ip_input = IPInputWidget()
        self.ip_input.setText(ip_addr)

        self.netmask_input = IPInputWidget()
        self.netmask_input.setText(netmask)

        self.gateway_input = IPInputWidget()
        self.gateway_input.setText(gateway)

        self.dns1_input = IPInputWidget()
        self.dns1_input.setText(dns1)

        self.dns2_input = IPInputWidget()
        self.dns2_input.setText(dns2)

        layout.addRow("New IP Address:", self.ip_input)
        layout.addRow("New Netmask:", self.netmask_input)
        layout.addRow("New Gateway:", self.gateway_input)
        layout.addRow("New DNS1:", self.dns1_input)
        layout.addRow("New DNS2:", self.dns2_input)

        self.button_box = QDialogButtonBox(QDialogButtonBox.Ok | QDialogButtonBox.Cancel)
        self.button_box.accepted.connect(self.accept)
        self.button_box.rejected.connect(self.reject)

        layout.addWidget(self.button_box)
        self.setLayout(layout)

    def getInputs(self):
        return (self.ip_input.text(), self.netmask_input.text(), self.gateway_input.text(),
                self.dns1_input.text(), self.dns2_input.text())

    def update_ip_format(self, text):
        sender = self.sender()
        parts = text.split('.')

        for i in range(4):
            if i >= len(parts) or not parts[i].isdigit():
                parts.append('0')

        sender.setText('.'.join(parts[:4]))

    def validate_ip(self, ip, netmask):
        try:
            octets = ip.split('.')
            if int(octets[0]) > 223 or octets[0] == '127' or octets[-1] == '255' or octets[-1] == '0':
                print("octet 조건 불만족")
                return False

            return True
        except ValueError:
            print("0~255")
            return False

    def validate_netmask(self, netmask):
        try:
            octets = netmask.split('.')

            if len(octets) != 4:
                return False

            for octet in octets:
                if not 0 <= int(octet) <= 255:
                    return False

            binary_str = ''.join(f'{int(octet):08b}' for octet in octets)

            if '01' in binary_str:
                return False

            return True

        except ValueError:
            return False

    def validate_gateway(self, gateway, ip, netmask):
        try:
            gateway_ip = ipaddress.ip_address(gateway)

            ip_network = ipaddress.ip_network(f"{ip}/{netmask}", strict=False)

            if gateway_ip == ip_network.network_address or gateway_ip == ip_network.broadcast_address:
                return False

            if gateway_ip not in ip_network:
                return False

            if gateway_ip.is_multicast:
                return False

            return True
        except ValueError:
            return False

    def validate_dns(self, dns):
        try:
            ipaddress.ip_address(dns)
            return True
        except ValueError:
            return False

    def validate_inputs(self):
        ip = self.ip_input.text()
        netmask = self.netmask_input.text()
        gateway = self.gateway_input.text()
        dns1 = self.dns1_input.text()
        dns2 = self.dns2_input.text()

        if not self.validate_netmask(netmask):
            QtWidgets.QMessageBox.warning(self, "Invalid Input", "Invalid Netmask")
            return False
        if not self.validate_ip(ip, netmask):
            QtWidgets.QMessageBox.warning(self, "Invalid Input", "Invalid IP Address")
            return False
        if not self.validate_gateway(gateway, ip, netmask):
            QtWidgets.QMessageBox.warning(self, "Invalid Input", "Invalid Gateway")
            return False
        if dns1 and not self.validate_dns(dns1):
            QtWidgets.QMessageBox.warning(self, "Invalid Input", "Invalid DNS1")
            return False
        if dns2 and not self.validate_dns(dns2):
            QtWidgets.QMessageBox.warning(self, "Invalid Input", "Invalid DNS2")
            return False
        return True

class IPCameraSearchUtility(QtWidgets.QWidget):
    def __init__(self):
        super().__init__()

        self.selected_ip = None
        self.selected_http = None
        self.sock = None  # Initialize the socket attribute
        self.initUI()
        self.selected_device_id = None

        # Connect aboutToQuit signal to stop function
        app.aboutToQuit.connect(self.stop)

    def initUI(self):
        self.setWindowTitle("IP Camera Search Utility")
        self.setFixedSize(1610, 800)

        # Create main layout
        main_layout = QtWidgets.QVBoxLayout()

        # Create top layout for START button, progress bar, and file selection
        top_layout = QtWidgets.QHBoxLayout()

        # Start button
        self.start_button = QtWidgets.QPushButton("START")
        self.start_button.setMinimumHeight(50)
        self.start_button.setMinimumWidth(200)
        self.start_button.clicked.connect(self.on_start_click)
        self.start_button.setStyleSheet("""
            QPushButton {
                font-size: 18px;
                padding: 10px;
                border-radius: 5px;
                background-color: #4CAF50;
                color: white;
            }
            QPushButton:hover {
                background-color: #45a049;
            }
            QPushButton:pressed {
                background-color: #3e8e41;
            }
        """)
        top_layout.addWidget(self.start_button)

        # Progress bar
        self.progress = QtWidgets.QProgressBar()
        self.progress.setValue(0)

        self.progress.setFixedWidth(600)
        self.progress.setStyleSheet("""
            QProgressBar {
                height: 30px;
                font-size: 18px;
                text-align: center;
            }
            QProgressBar::chunk {
                background-color: #4CAF50;
                width: 20px;
            }
        """)

        self.progress.setAlignment(Qt.AlignCenter)
        top_layout.addWidget(self.progress)
        top_layout.addStretch(1)

        main_layout.addLayout(top_layout)

        # Create button layout for other buttons
        button_layout = QtWidgets.QHBoxLayout()
        buttons = [
            ("IP-CHANGE", self.on_ip_change_click),
            ("ALL CLEAR", self.on_clear_click),
            ("WEB", self.open_web_browser),
            ("REBOOT", self.reboot)
        ]

        for text, command in buttons:
            button = QtWidgets.QPushButton(text)
            button.setMinimumHeight(40)  # Set button height
            button.clicked.connect(command)
            button.setStyleSheet("""
                QPushButton {
                    font-size: 18px;
                    padding: 10px;
                    border-radius: 5px;
                    background-color: #4CAF50;
                    color: white;
                }
                QPushButton:hover {
                    background-color: #45a049;
                }
                QPushButton:pressed {
                    background-color: #3e8e41;
                }
            """)
            button_layout.addWidget(button)

        # Create table
        self.table = QtWidgets.QTableWidget()
        self.table.setColumnCount(11)
        self.table.setHorizontalHeaderLabels(
            ["No.", "MAC Address", "IP Address", "Gateway", "Net Mask", "DNS1", "DNS2", "HTTP(P)", "Run Time", "AUTO", "DHCP"])
        self.table.verticalHeader().setVisible(False)

        self.table.setSelectionBehavior(QtWidgets.QTableWidget.SelectRows)

        # Set initial widths based on expected data
        initial_data = {
            "No.": 100,
            "MAC Address": 180,
            "IP Address": 150,
            "Gateway": 150,
            "Net Mask": 150,
            "DNS1": 150,
            "DNS2": 150,
            "HTTP(P)": 100,
            "Run Time": 220,
            "AUTO": 100,
            "DHCP": 100
        }

        for i in range(self.table.columnCount()):
            header = self.table.horizontalHeaderItem(i).text()
            self.table.setColumnWidth(i, initial_data.get(header, 100))

        self.table.setStyleSheet("""
            QTableWidget {
                font-size: 18px;
            }
            QHeaderView::section {
                background-color: #f0f0f0;
                padding: 5px;
                border: 1px solid #d0d0d0;
                font-size: 16px;
            }
            QTableWidget::item {
                padding: 5px;
            }
        """)

        self.table.cellDoubleClicked.connect(self.on_row_double_clicked)
        self.table.cellClicked.connect(self.on_row_clicked)

        self.table.setVerticalScrollBarPolicy(Qt.ScrollBarAlwaysOn)

        # Start and Stop Event for response thread
        self.stop_event = threading.Event()
        self.received_packets = {}

        # Apply additional stylesheet for main window
        self.setStyleSheet("""
            QWidget {
                background-color: #f5f5f5;
            }
        """)

        main_layout.addWidget(self.table)
        main_layout.addLayout(button_layout)
        self.setLayout(main_layout)

    def on_row_double_clicked(self, row, column):
        self.on_row_clicked(row, column)  # Ensure the row is selected
        self.open_web_browser()

    def add_to_table(self, data):
        # Check for duplicates before adding
        for row in range(self.table.rowCount()):
            if self.table.item(row, 2).text() == data[2] and self.table.item(row, 3).text() == data[3]:
                return  # Skip adding duplicate entry
        row_position = self.table.rowCount()
        self.table.insertRow(row_position)
        for column, value in enumerate(data):
            item = QtWidgets.QTableWidgetItem(str(value))
            item.setTextAlignment(Qt.AlignCenter)
            self.table.setItem(row_position, column, item)

    def on_start_click(self):
        try:
            # Stop existing threads if they are running
            self.stop_event.set()
            if hasattr(self, 'send_thread') and self.send_thread.is_alive():
                self.send_thread.join()
            if hasattr(self, 'response_thread') and self.response_thread.is_alive():
                self.response_thread.join()

            # Reinitialize socket
            if self.sock is not None:
                self.sock.close()

            self.sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM, socket.IPPROTO_UDP)
            # self.sock.setsockopt(socket.IPPROTO_IP, socket.IP_MULTICAST_TTL, 2)
            self.sock.setsockopt(socket.SOL_SOCKET, socket.SO_BROADCAST, 1)
            self.sock.bind(('', LISTEN_PORT))

            # Clear the stop event for new operation
            self.stop_event.clear()

            # Initialize the received_packets dictionary and clear the table
            self.received_packets.clear()
            self.table.setRowCount(0)

            # Reset the progress bar
            self.progress.setValue(0)

            # Record the start time
            self.start_time = time.time()

            # Initialize seen_devices set
            self.seen_devices = set()

            # Start the send_messages thread
            self.send_thread = threading.Thread(target=self.send_messages)
            self.send_thread.start()

            # Start the response listening thread
            self.response_thread = threading.Thread(target=listen_for_responses, args=(
                self.sock, self.add_to_table, self.update_table, self.stop_event, self.seen_devices,
                self.received_packets))
            self.response_thread.start()

            # Update the progress bar
            self.update_progress()

        except Exception as e:
            QtWidgets.QMessageBox.critical(self, "Error", f"Failed to start: {e}")
            # Ensure the START button is re-enabled in case of an exception
            self.start_button.setEnabled(True)

    def send_messages(self):
        self.start_button.setDisabled(True)
        self.start_button.setStyleSheet("""
                    QPushButton {
                        font-size: 18px;
                        padding: 10px;
                        border-radius: 5px;
                        background-color: #9E9E9E;
                        color: #E0E0E0;
                    }
                """)
        while not self.stop_event.is_set() and time.time() - self.start_time < 10:
            dwTick = int(time.time())
            net_msg = struct.pack('!BBHIII16sIIIIIIIII128sI20sBI6sI',
                                  MSG_IP_SEARCH,
                                  0,
                                  0,
                                  socket.htonl(dwTick),
                                  MAGIC,
                                  0,
                                  b'\x00' * 16,
                                  0,
                                  0,
                                  0,
                                  0,
                                  0,
                                  0,
                                  0,
                                  0,
                                  0,
                                  b'\x00' * 128,
                                  0,
                                  b'\x00' * 20,
                                  0,
                                  0,
                                  b'\x00' * 6,
                                  0
                                  )

            send_multicast_message(self.sock, net_msg)
            time.sleep(0.2)

        # Enable the START button on the main thread
        QtCore.QMetaObject.invokeMethod(self.start_button, "setEnabled", QtCore.Q_ARG(bool, True))
        self.start_button.setStyleSheet("""
                    QPushButton {
                        font-size: 18px;
                        padding: 10px;
                        border-radius: 5px;
                        background-color: #4CAF50;
                        color: white;
                    }
                    QPushButton:hover {
                        background-color: #45a049;
                    }
                    QPushButton:pressed {
                        background-color: #3e8e41;
                    }
                """)
        self.stop_event.set()
        QtCore.QMetaObject.invokeMethod(self.progress, "setValue", QtCore.Qt.QueuedConnection, QtCore.Q_ARG(int, 0))

        # Wait for response thread to complete
        if hasattr(self, 'response_thread') and self.response_thread.is_alive():
            self.response_thread.join()

        # Close the socket
        if self.sock:
            self.sock.close()

    def update_table(self, device_id, net_msg):
        for row in range(self.table.rowCount()):
            if self.table.item(row, 2).text() == format_mac_address(net_msg['chaddr']):
                self.table.setItem(row, 1, self.create_centered_item(
                    str(f"{int(''.join(filter(str.isdigit, net_msg['fw_ver'].decode('utf-8', 'ignore').strip()))):x}")))
                self.table.setItem(row, 3, self.create_centered_item(format_ip_address(net_msg['ciaddr'])))
                self.table.setItem(row, 4, self.create_centered_item(format_ip_address(net_msg['giaddr'])))
                self.table.setItem(row, 5, self.create_centered_item(format_ip_address(net_msg['miaddr'])))
                self.table.setItem(row, 6, self.create_centered_item(format_ip_address(net_msg['d1iaddr'])))
                self.table.setItem(row, 7, self.create_centered_item(format_ip_address(net_msg['d2iaddr'])))
                self.table.setItem(row, 8, self.create_centered_item(str(socket.ntohl(net_msg['http']))))
                # self.table.setItem(row, 9, self.create_centered_item(str(socket.ntohl(net_msg['stream']))))
                self.table.setItem(row, 10, self.create_centered_item(format_runtime(net_msg['runningtime'])))
                # self.table.setItem(row, 11, self.create_centered_item("Ready"))
                self.table.setItem(row, 12, self.create_centered_item(
                    (net_msg['flag'])))
                self.table.setItem(row, 13, self.create_centered_item(
                    'ON' if socket.ntohl(net_msg['flag']) & (0x1 << 1) else 'OFF'))

    def create_centered_item(self, text):
        item = QtWidgets.QTableWidgetItem(text)
        item.setTextAlignment(Qt.AlignCenter)
        return item
    def on_clear_click(self):
        self.table.setRowCount(0)

    def stop(self):
        self.stop_event.set()
        if self.send_thread.is_alive():
            self.send_thread.join()
        if self.response_thread.is_alive():
            self.response_thread.join()
        if self.sock:
            self.sock.close()  # Close the socket
            self.sock = None  # Reset the socket attribute
        self.progress.setValue(0)

    def update_progress(self):
        if not self.stop_event.is_set():
            elapsed_time = time.time() - self.start_time
            self.progress.setValue(min(100, int(elapsed_time * 10)))
            QtCore.QTimer.singleShot(100, self.update_progress)


    def on_row_clicked(self, row, column):
        self.selected_ip = self.table.item(row, 2).text()  # Assuming the IP address is in the 4th column
        self.selected_http = self.table.item(row, 7).text()
        # Store the selected device ID
        for device_id, packet in self.received_packets.items():
            if format_ip_address(packet['ciaddr']) == self.selected_ip:
                self.selected_device_id = device_id
                break

    def on_ip_change_click(self):
        if self.selected_ip:
            for device_id, packet in self.received_packets.items():
                if format_ip_address(packet['ciaddr']) == self.selected_ip:
                    self.selected_device_id = device_id
                    ip_addr = format_ip_address(packet['ciaddr'])
                    gateway = format_ip_address(packet['giaddr'])
                    subnet_mask = format_ip_address(packet['miaddr'])
                    dns1 = format_ip_address(packet['d1iaddr'])
                    dns2 = format_ip_address(packet['d2iaddr'])

                    dialog = ChangeIPDialog(ip_addr, subnet_mask, gateway, dns1, dns2, self)
                    if dialog.exec_() == QtWidgets.QDialog.Accepted:
                        if dialog.validate_inputs():
                            new_ip, new_netmask, new_gateway, new_dns1, new_dns2 = dialog.getInputs()
                            if all([new_ip, new_netmask, new_gateway, new_dns1, new_dns2]):
                                self.send_change_ip_message(new_ip, new_netmask, new_gateway, new_dns1, new_dns2)
                                # self.send_change_ip_message(new_ip, "255.255.0.0", "192.168.0.1", new_dns1, new_dns2)
                    return
        else:
            QtWidgets.QMessageBox.warning(self, "No Selection", "No row selected. Please select a row first.")

    def send_change_ip_message(self, new_ip, new_netmask, new_gateway, new_dns1, new_dns2):

        if self.sock is None or self.sock.fileno() == -1:  # Check if the socket is None or closed
            self.sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM, socket.IPPROTO_UDP)
            self.sock.setsockopt(socket.IPPROTO_IP, socket.IP_MULTICAST_TTL, 2)
            self.sock.bind(('', LISTEN_PORT))

        try:
            new_ip_bytes = socket.inet_aton(new_ip)
            new_netmask_bytes = socket.inet_aton(new_netmask)
            new_gateway_bytes = socket.inet_aton(new_gateway)
            new_dns1_bytes = socket.inet_aton(new_dns1)
            new_dns2_bytes = socket.inet_aton(new_dns2)

            if self.selected_device_id:
                packet = self.received_packets.get(self.selected_device_id)
                if packet:
                    change_ip_msg = struct.pack('!BBHIII16sIIIIIIIII128sI20sBI6sI',
                                                MSG_IP_SET,
                                                1,
                                                packet['secs'],
                                                packet['xid'],
                                                packet['magic'],
                                                packet['ciaddr'],
                                                packet['chaddr'],
                                                struct.unpack('!I', new_ip_bytes)[0],
                                                struct.unpack('!I', new_netmask_bytes)[0],
                                                struct.unpack('!I', new_gateway_bytes)[0],
                                                struct.unpack('!I', new_dns1_bytes)[0],
                                                struct.unpack('!I', new_dns2_bytes)[0],
                                                packet['d3iaddr'],
                                                packet['siaddr'],
                                                packet['sport'],
                                                packet['http'],
                                                packet['vend'],
                                                packet['stream'],
                                                packet['fw_ver'],
                                                packet['vidstd'],
                                                packet['flag'],
                                                packet['chaddr2'],
                                                packet['runningtime']
                                                )
                    send_multicast_message(self.sock, change_ip_msg)
                    self.on_start_click()  # Refresh the device list
                    return

            raise ValueError("Selected IP address not found in received packets.")
            self.selected_device_id = None


        except Exception as e:
            QtWidgets.QMessageBox.critical(self, "Error", f"Failed to send change IP message: {e}")

    def open_web_browser(self):
        if self.selected_ip:
            # webbrowser.open(f'http://{self.selected_ip}')
            webbrowser.open(f'http://{self.selected_ip}:{self.selected_http}')
        else:
            QtWidgets.QMessageBox.warning(self, "No Selection", "No row selected. Please select a row first.")

    def reboot(self):
        try:
            # Check if socket is valid and open
            if self.sock is None or self.sock.fileno() == -1:
                self.sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM, socket.IPPROTO_UDP)
                self.sock.setsockopt(socket.IPPROTO_IP, socket.IP_MULTICAST_TTL, 2)
                self.sock.bind(('', LISTEN_PORT))

            if self.selected_device_id:
                packet = self.received_packets.get(self.selected_device_id)
                if packet:
                    reboot_msg = struct.pack('!BBHIII16sIIIIIIIII128sI20sBI6sI',
                                             MSG_IP_REBOOT,
                                             1,
                                             packet['secs'],
                                             packet['xid'],
                                             packet['magic'],
                                             packet['ciaddr'],
                                             packet['chaddr'],
                                             packet['yiaddr'],
                                             packet['miaddr'],
                                             packet['giaddr'],
                                             packet['d1iaddr'],
                                             packet['d2iaddr'],
                                             packet['d3iaddr'],
                                             packet['siaddr'],
                                             packet['sport'],
                                             packet['http'],
                                             packet['vend'],
                                             packet['stream'],
                                             packet['fw_ver'],
                                             packet['vidstd'],
                                             packet['flag'],
                                             packet['chaddr2'],
                                             packet['runningtime']
                                             )
                    send_multicast_message(self.sock, reboot_msg)
                    self.on_start_click()  # Refresh the device list after reboot
                    self.selected_device_id = None
            else:
                QtWidgets.QMessageBox.warning(self, "No Selection", "No row selected. Please select a row first.")


        except Exception as e:
            QtWidgets.QMessageBox.critical(self, "Error", f"Failed to reboot message: {e}")



if __name__ == '__main__':
    import sys

    app = QtWidgets.QApplication(sys.argv)
    ex = IPCameraSearchUtility()
    ex.show()
    sys.exit(app.exec_())
