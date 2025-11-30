import struct
import sys
import os

def decode_temperature(encoded_val):
    """
    Decodes temperature from 1 byte (0-255).
    0 is 10C, 255 is 50C.
    Range 10 to 50 = 40 degrees.
    """
    return 10.0 + (encoded_val * 40.0 / 255.0)

def decode_duty_cycle(encoded_val):
    """
    Decodes duty cycle from 1 byte (0-255).
    0 is 0%, 255 is 100%.
    """
    return encoded_val * 100.0 / 255.0

def parse_perf_log(file_path):
    """
    Parses a binary perf log file and prints CSV to stdout.
    """
    record_size = 21
    
    # CSV Header
    print("Timestamp,Fan1_Target%,Fan1_Current%,Fan1_RPM,Fan2_Target%,Fan2_Current%,Fan2_RPM,Fan3_Target%,Fan3_Current%,Fan3_RPM,Fan4_Target%,Fan4_Current%,Fan4_RPM,Temp_Ambient,Temp_Coolant_In,Temp_Coolant_Out")
    
    try:
        with open(file_path, 'rb') as f:
            while True:
                chunk = f.read(record_size)
                if not chunk:
                    break
                if len(chunk) < record_size:
                    sys.stderr.write(f"Warning: Incomplete record at end of file (got {len(chunk)} bytes, expected {record_size})\n")
                    break
                
                # Unpack binary data
                # <H: uint16_t (timestamp)
                # 4 groups of (B B H): uint8_t, uint8_t, uint16_t (Fan data)
                # 3 B: uint8_t (Thermistors)
                # Total format: <H BBH BBH BBH BBH BBB
                
                data = struct.unpack('<HBBHBBHBBHBBHBBB', chunk)
                
                timestamp = data[0]
                
                # Fan 1
                f1_target = decode_duty_cycle(data[1])
                f1_current = decode_duty_cycle(data[2])
                f1_rpm = data[3]
                
                # Fan 2
                f2_target = decode_duty_cycle(data[4])
                f2_current = decode_duty_cycle(data[5])
                f2_rpm = data[6]
                
                # Fan 3
                f3_target = decode_duty_cycle(data[7])
                f3_current = decode_duty_cycle(data[8])
                f3_rpm = data[9]
                
                # Fan 4
                f4_target = decode_duty_cycle(data[10])
                f4_current = decode_duty_cycle(data[11])
                f4_rpm = data[12]
                
                # Thermistors
                t_ambient = decode_temperature(data[13])
                t_coolant_in = decode_temperature(data[14])
                t_coolant_out = decode_temperature(data[15])
                
                print(f"{timestamp},{f1_target:.1f},{f1_current:.1f},{f1_rpm},{f2_target:.1f},{f2_current:.1f},{f2_rpm},{f3_target:.1f},{f3_current:.1f},{f3_rpm},{f4_target:.1f},{f4_current:.1f},{f4_rpm},{t_ambient:.1f},{t_coolant_in:.1f},{t_coolant_out:.1f}")
                
    except FileNotFoundError:
        sys.stderr.write(f"Error: File not found: {file_path}\n")
        sys.exit(1)
    except Exception as e:
        sys.stderr.write(f"Error parsing file: {e}\n")
        sys.exit(1)

if __name__ == "__main__":
    if len(sys.argv) != 2:
        print("Usage: python parse_perf_log.py <path_to_binary_log_file>")
        sys.exit(1)
    
    parse_perf_log(sys.argv[1])
