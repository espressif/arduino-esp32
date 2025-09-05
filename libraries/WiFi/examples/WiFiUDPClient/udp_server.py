# This python script listens on UDP port 3333
# for messages from the ESP32 board and prints them
import socket
import sys
import subprocess
import platform


def get_interface_ips():
    """Get all available interface IP addresses"""
    interface_ips = []

    # Try using system commands to get interface IPs
    system = platform.system().lower()

    try:
        if system == "darwin" or system == "linux":
            # Use 'ifconfig' on macOS/Linux
            result = subprocess.run(["ifconfig"], capture_output=True, text=True, timeout=5)
            if result.returncode == 0:
                lines = result.stdout.split("\n")
                for line in lines:
                    if "inet " in line and "127.0.0.1" not in line:
                        # Extract IP address from ifconfig output
                        parts = line.strip().split()
                        for i, part in enumerate(parts):
                            if part == "inet":
                                if i + 1 < len(parts):
                                    ip = parts[i + 1]
                                    if ip not in interface_ips and ip != "127.0.0.1":
                                        interface_ips.append(ip)
                                break
        elif system == "windows":
            # Use 'ipconfig' on Windows
            result = subprocess.run(["ipconfig"], capture_output=True, text=True, timeout=5)
            if result.returncode == 0:
                lines = result.stdout.split("\n")
                for line in lines:
                    if "IPv4 Address" in line and "127.0.0.1" not in line:
                        # Extract IP address from ipconfig output
                        if ":" in line:
                            ip = line.split(":")[1].strip()
                            if ip not in interface_ips and ip != "127.0.0.1":
                                interface_ips.append(ip)
    except (subprocess.TimeoutExpired, subprocess.SubprocessError, FileNotFoundError):
        print("Error: Failed to get interface IPs using system commands")
        print("Trying fallback methods...")

    # Fallback: try to get IPs using socket methods
    if not interface_ips:
        try:
            # Get all IP addresses associated with the hostname
            hostname = socket.gethostname()
            ip_list = socket.gethostbyname_ex(hostname)[2]
            for ip in ip_list:
                if ip not in interface_ips and ip != "127.0.0.1":
                    interface_ips.append(ip)
        except socket.gaierror:
            print("Error: Failed to get interface IPs using sockets")

    # Fail if no interfaces found
    if not interface_ips:
        print("Error: No network interfaces found. Please check your network configuration.")
        sys.exit(1)

    return interface_ips


def select_interface(interface_ips):
    """Ask user to select which interface to bind to"""
    if len(interface_ips) == 1:
        print(f"Using interface: {interface_ips[0]}")
        return interface_ips[0]

    print("Multiple network interfaces detected:")
    for i, ip in enumerate(interface_ips, 1):
        print(f"  {i}. {ip}")

    while True:
        try:
            choice = input(f"Select interface (1-{len(interface_ips)}): ").strip()
            choice_idx = int(choice) - 1
            if 0 <= choice_idx < len(interface_ips):
                selected_ip = interface_ips[choice_idx]
                print(f"Selected interface: {selected_ip}")
                return selected_ip
            else:
                print(f"Please enter a number between 1 and {len(interface_ips)}")
        except ValueError:
            print("Please enter a valid number")
        except KeyboardInterrupt:
            print("\nExiting...")
            sys.exit(1)


try:
    s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    s.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
except socket.error as msg:
    print("Failed to create socket. Error Code : " + str(msg[0]) + " Message " + msg[1])
    sys.exit()

# Get available interfaces and let user choose
interface_ips = get_interface_ips()
selected_ip = select_interface(interface_ips)

try:
    s.bind((selected_ip, 3333))
except socket.error as msg:
    print("Bind failed. Error: " + str(msg[0]) + ": " + msg[1])
    sys.exit()

print(f"Server listening on {selected_ip}:3333")

while 1:
    d = s.recvfrom(1024)
    data = d[0]

    if not data:
        break

    print(data.strip())

s.close()
