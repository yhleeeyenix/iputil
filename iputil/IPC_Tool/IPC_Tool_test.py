import socket
import time
import struct
import threading
from PyQt5 import QtWidgets, QtCore
from datetime import datetime

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

# MAGIC = 0x71727374
MAGIC = 0x73747278
SEND_PORT = 59163
LISTEN_PORT = 59164
MULTICAST_ADDRESS = '239.255.255.250'


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
        "runningtime": socket.ntohl(unpacked_data[22]),
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
                    fw_ver_str = net_msg['fw_ver'].decode('utf-8', 'ignore').strip()
                    version_str = ''.join(filter(str.isdigit, fw_ver_str))
                    version = int(version_str) if version_str else 0
                    fw_ver_hex = f"{version:x}"
                    mac_addr = format_mac_address(net_msg['chaddr'])
                    ip_addr = format_ip_address(net_msg['ciaddr'])
                    gateway = format_ip_address(net_msg['giaddr'])
                    subnet_mask = format_ip_address(net_msg['miaddr'])
                    dns1 = format_ip_address(net_msg['d1iaddr'])
                    dns2 = format_ip_address(net_msg['d2iaddr'])
                    http_port = socket.ntohl(net_msg['http'])
                    rtsp_port = socket.ntohl(net_msg['stream'])
                    update_status = "Ready"
                    autoip_status = 'ON' if socket.ntohl(net_msg['flag']) & (0x1 << 0) else 'OFF'
                    dhcp_status = 'ON' if socket.ntohl(net_msg['flag']) & (0x1 << 1) else 'OFF'
                    runtime = format_runtime(net_msg['runningtime'])

                    add_to_table((number, fw_ver_hex, mac_addr, ip_addr, gateway, subnet_mask, dns1, dns2, http_port,
                                  rtsp_port, runtime, update_status, autoip_status, dhcp_status))

                    received_packets[device_id] = net_msg
                    ack_msg = pack_net_msg(net_msg)
                    send_multicast_message(sock, ack_msg)

                else:
                    print("magic error")
                    return

        except socket.timeout:
            continue
        except Exception as e:
            print(f"Error in listen_for_responses: {e}")

class ChangeMACDialog(QtWidgets.QDialog):
    def __init__(self, current_mac='', parent=None):
        super().__init__(parent)
        self.setWindowTitle("Change MAC Address")

        layout = QtWidgets.QFormLayout()

        self.mac_input = QtWidgets.QLineEdit(current_mac)

        layout.addRow("New MAC Address:", self.mac_input)

        self.button_box = QtWidgets.QDialogButtonBox(QtWidgets.QDialogButtonBox.Ok | QtWidgets.QDialogButtonBox.Cancel)
        self.button_box.accepted.connect(self.accept)
        self.button_box.rejected.connect(self.reject)

        layout.addWidget(self.button_box)
        self.setLayout(layout)

    def get_mac(self):
        return self.mac_input.text()


# 커스텀 입력 다이얼로그 클래스 정의
class ChangeIPDialog(QtWidgets.QDialog):
    def __init__(self, ip_addr='', netmask='', gateway='', dns1='', dns2='', parent=None):
        super().__init__(parent)
        self.setWindowTitle("Change IP Settings")

        layout = QtWidgets.QFormLayout()

        self.ip_input = QtWidgets.QLineEdit(ip_addr)
        self.netmask_input = QtWidgets.QLineEdit(netmask)
        self.gateway_input = QtWidgets.QLineEdit(gateway)
        self.dns1_input = QtWidgets.QLineEdit(dns1)
        self.dns2_input = QtWidgets.QLineEdit(dns2)

        layout.addRow("New IP Address:", self.ip_input)
        layout.addRow("New Netmask:", self.netmask_input)
        layout.addRow("New Gateway:", self.gateway_input)
        layout.addRow("New DNS1:", self.dns1_input)
        layout.addRow("New DNS2:", self.dns2_input)

        self.button_box = QtWidgets.QDialogButtonBox(QtWidgets.QDialogButtonBox.Ok | QtWidgets.QDialogButtonBox.Cancel)
        self.button_box.accepted.connect(self.accept)
        self.button_box.rejected.connect(self.reject)

        layout.addWidget(self.button_box)
        self.setLayout(layout)

    def getInputs(self):
        return (self.ip_input.text(), self.netmask_input.text(), self.gateway_input.text(),
                self.dns1_input.text(), self.dns2_input.text())


class IPCameraSearchUtility(QtWidgets.QWidget):
    def __init__(self):
        super().__init__()

        self.selected_ip = None
        self.sock = None  # Initialize the socket attribute
        self.initUI()
        self.selected_device_id = None
        self.sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM, socket.IPPROTO_UDP)
        self.sock.setsockopt(socket.IPPROTO_IP, socket.IP_MULTICAST_TTL, 2)
        self.sock.bind(('', LISTEN_PORT))

        # Connect aboutToQuit signal to stop function
        app.aboutToQuit.connect(self.stop)

    def initUI(self):
        self.setWindowTitle("IP Camera Search Utility")
        # self.resize(2200, 1100)  # Set initial window size
        self.setGeometry(100, 100, 2000, 1000)  # Set window size

        # Create layout
        layout = QtWidgets.QVBoxLayout()

        # Create table
        self.table = QtWidgets.QTableWidget()
        self.table.setColumnCount(14)
        self.table.setHorizontalHeaderLabels(
            ["No.", "FW Ver.", "MAC Address", "IP Address", "Gateway", "Net Mask", "DNS1", "DNS2", "HTTP(P)", "RTSP(P)",
             "Run Time", "UPDATE", "AUTO", "DHCP"])
        self.table.horizontalHeader().setSectionResizeMode(QtWidgets.QHeaderView.Stretch)
        layout.addWidget(self.table)

        # Connect cell clicked signal to the handler
        self.table.cellClicked.connect(self.on_row_clicked)
        self.table.verticalHeader().setVisible(False)  # 수직 헤더 숨기기

        # Create buttons
        button_layout = QtWidgets.QGridLayout()
        buttons = [
            ("START", self.on_start_click),
            ("IP-CHANGE", self.on_ip_change_click),
            ("UPDATE", self.send_update_message),
            ("ALL CLEAR", self.on_clear_click),
            ("DELETE", self.on_delete_click),
            ("WEB", lambda: print("WEB Clicked")),
            ("DHCP", self.send_DHCP_message),
            ("REBOOT", self.reboot),
            ("AUTO-IP", self.send_auto_message),
            ("MAC", self.on_mac_change_click),
        ]

        for i, (text, command) in enumerate(buttons):
            button = QtWidgets.QPushButton(text)
            button.setMinimumHeight(40)  # Set button height
            button.clicked.connect(command)
            button_layout.addWidget(button, i // 5, i % 5)

        layout.addLayout(button_layout)

        # Create progress bar
        self.progress = QtWidgets.QProgressBar()
        self.progress.setValue(0)
        layout.addWidget(self.progress)

        self.setLayout(layout)

        # Start and Stop Event for response thread
        self.stop_event = threading.Event()
        self.received_packets = {}

        # Apply stylesheet
        self.setStyleSheet("""
            QPushButton {
                font-size: 20px;
                padding: 5px;
                margin: 20px;
            }
            QTableWidget {
                font-size: 20px;
            }
            QProgressBar {
                height: 30px;
                font-size: 20px;
            }
        """)

    def add_to_table(self, data):
        # Check for duplicates before adding
        for row in range(self.table.rowCount()):
            if self.table.item(row, 2).text() == data[2] and self.table.item(row, 3).text() == data[3]:
                return  # Skip adding duplicate entry
        row_position = self.table.rowCount()
        self.table.insertRow(row_position)
        for column, value in enumerate(data):
            self.table.setItem(row_position, column, QtWidgets.QTableWidgetItem(str(value)))

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
            self.sock.setsockopt(socket.IPPROTO_IP, socket.IP_MULTICAST_TTL, 2)
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

    def send_messages(self):
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

    def update_table(self, device_id, net_msg):
        for row in range(self.table.rowCount()):
            if self.table.item(row, 2).text() == format_mac_address(net_msg['chaddr']):
                self.table.setItem(row, 1, QtWidgets.QTableWidgetItem(
                    str(f"{int(''.join(filter(str.isdigit, net_msg['fw_ver'].decode('utf-8', 'ignore').strip()))):x}")))
                self.table.setItem(row, 3, QtWidgets.QTableWidgetItem(format_ip_address(net_msg['ciaddr'])))
                self.table.setItem(row, 4, QtWidgets.QTableWidgetItem(format_ip_address(net_msg['giaddr'])))
                self.table.setItem(row, 5, QtWidgets.QTableWidgetItem(format_ip_address(net_msg['miaddr'])))
                self.table.setItem(row, 6, QtWidgets.QTableWidgetItem(format_ip_address(net_msg['d1iaddr'])))
                self.table.setItem(row, 7, QtWidgets.QTableWidgetItem(format_ip_address(net_msg['d2iaddr'])))
                self.table.setItem(row, 8, QtWidgets.QTableWidgetItem(str(socket.ntohl(net_msg['http']))))
                self.table.setItem(row, 9, QtWidgets.QTableWidgetItem(str(socket.ntohl(net_msg['stream']))))
                self.table.setItem(row, 10, QtWidgets.QTableWidgetItem(format_runtime(net_msg['runningtime'])))
                self.table.setItem(row, 11, QtWidgets.QTableWidgetItem("Ready"))
                self.table.setItem(row, 12, QtWidgets.QTableWidgetItem(
                    'ON' if socket.ntohl(net_msg['flag']) & (0x1 << 0) else 'OFF'))
                self.table.setItem(row, 13, QtWidgets.QTableWidgetItem(
                    'ON' if socket.ntohl(net_msg['flag']) & (0x1 << 1) else 'OFF'))

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
        self.selected_ip = self.table.item(row, 3).text()  # Assuming the IP address is in the 4th column
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
                        new_ip, new_netmask, new_gateway, new_dns1, new_dns2 = dialog.getInputs()
                        if all([new_ip, new_netmask, new_gateway, new_dns1, new_dns2]):
                            self.send_change_ip_message(new_ip, new_netmask, new_gateway, new_dns1,new_dns2)
                    return
        else:
            QtWidgets.QMessageBox.warning(self, "No Selection", "No row selected. Please select a row first.")

    def send_change_ip_message(self, new_ip, new_netmask, new_gateway, new_dns1, new_dns2):
        if self.sock is None:
            QtWidgets.QMessageBox.critical(self, "Error", "Socket is not initialized. Please start the search first.")
            return

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

    def reboot(self):
        try:
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

    def on_mac_change_click(self):
        if self.selected_ip:
            for device_id, packet in self.received_packets.items():
                if format_ip_address(packet['ciaddr']) == self.selected_ip:
                    current_mac = format_mac_address(packet['chaddr'])
                    dialog = ChangeMACDialog(current_mac, self)
                    if dialog.exec_() == QtWidgets.QDialog.Accepted:
                        new_mac = dialog.get_mac()
                        if new_mac:
                            self.send_change_mac_message(new_mac)
                    return
        else:
            QtWidgets.QMessageBox.warning(self, "No Selection", "No row selected. Please select a row first.")

    def send_change_mac_message(self, new_mac):
        if self.sock is None:
            QtWidgets.QMessageBox.critical(self, "Error", "Please start the search first.")
            return

        try:
            new_mac_bytes = bytes.fromhex(new_mac.replace(':', ''))

            if len(new_mac_bytes) != 6:
                raise ValueError("Invalid MAC address format. Must be 6 bytes.")

            if self.selected_device_id:
                packet = self.received_packets.get(self.selected_device_id)
                if packet:
                    change_mac_msg = struct.pack('!BBHIII16sIIIIIIIII128sI20sBI6sI',
                                                 MSG_IP_MACUPDATE,
                                                 1,
                                                 packet['secs'],
                                                 packet['xid'],
                                                 packet['magic'],
                                                 socket.htonl(packet['ciaddr']),
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
                                                 new_mac_bytes.ljust(16, b'\x00'),
                                                 packet['runningtime']
                                                 )

                    send_multicast_message(self.sock, change_mac_msg)
                    self.on_start_click()  # Refresh the device list
                    return

                raise ValueError("Selected device not found in received packets.")
        except Exception as e:
            QtWidgets.QMessageBox.critical(self, "Error", f"Failed to send change MAC message: {e}")

    def send_update_message(self):
        try:
            if self.selected_device_id:
                packet = self.received_packets.get(self.selected_device_id)
                if packet:
                    update_msg = struct.pack('!BBHIII16sIIIIIIIII128sI20sBI6sI',
                                             MSG_IP_UPDATE,
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
                    send_multicast_message(self.sock, update_msg)
                    self.on_start_click()  # Refresh the device list after reboot
                    self.selected_device_id = None
            else:
                QtWidgets.QMessageBox.warning(self, "No Selection", "No row selected. Please select a row first.")

        except Exception as e:
            QtWidgets.QMessageBox.critical(self, "Error", f"Failed to reboot message: {e}")

    def send_auto_message(self):
        try:
            if self.selected_device_id:
                packet = self.received_packets.get(self.selected_device_id)
                if packet:
                    auto_msg = struct.pack('!BBHIII16sIIIIIIIII128sI20sBI6sI',
                                         MSG_CAM_AUTOIP,
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
                    send_multicast_message(self.sock, auto_msg)
                    self.on_start_click()  # Refresh the device list after reboot
                    self.selected_device_id = None
            else:
                QtWidgets.QMessageBox.warning(self, "No Selection", "No row selected. Please select a row first.")


        except Exception as e:
            QtWidgets.QMessageBox.critical(self, "Error", f"Failed to reboot message: {e}")

    def send_DHCP_message(self):
        try:
            if self.selected_device_id:
                packet = self.received_packets.get(self.selected_device_id)
                if packet:
                    DHCP_msg = struct.pack('!BBHIII16sIIIIIIIII128sI20sBI6sI',
                                         MSG_IP_DHCP,
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
                    send_multicast_message(self.sock, DHCP_msg)
                    self.on_start_click()  # Refresh the device list after reboot
                    self.selected_device_id = None
            else:
                QtWidgets.QMessageBox.warning(self, "No Selection", "No row selected. Please select a row first.")


        except Exception as e:
            QtWidgets.QMessageBox.critical(self, "Error", f"Failed to reboot message: {e}")

    def on_delete_click(self):
        if self.selected_ip:
            for row in range(self.table.rowCount()):
                if self.table.item(row, 3).text() == self.selected_ip:
                    self.table.removeRow(row)
                    self.selected_ip = None  # Reset the selected IP
                    return
        else:
            QtWidgets.QMessageBox.warning(self, "No Selection", "No row selected. Please select a row first.")


if __name__ == '__main__':
    import sys

    app = QtWidgets.QApplication(sys.argv)
    ex = IPCameraSearchUtility()
    ex.show()
    sys.exit(app.exec_())
