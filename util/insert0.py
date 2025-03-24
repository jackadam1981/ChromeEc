#!/usr/bin/env python3
import sys

def insert_zeroes_to_hex_file(insert_position, num_zeroes):
    if len(sys.argv) > 1:
        file_path = sys.argv[1]
        print(f"Process file: {file_path}")
    else:
        print("Please specify file name.")
        return

    # Read the hex file
    with open(file_path, 'rb') as file:
        hex_data = file.read()

    # Insert zeroes at the specified position
    zeroes = b'\x00' * num_zeroes
    modified_hex_data = hex_data[:insert_position] + zeroes + hex_data[insert_position:]

    # Write the modified data back to the hex file
    with open(file_path, 'wb') as file:
        file.write(modified_hex_data)

# Example usage
insert_position = 0x600  # Position to insert zeroes
num_zeroes = 2560        # Number of zeroes to insert

insert_zeroes_to_hex_file(insert_position, num_zeroes)
