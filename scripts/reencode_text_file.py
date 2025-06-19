import argparse
import codecs

parser = argparse.ArgumentParser(description='Converts a UTF-8 text file into a UTF-16 LE text file')
parser.add_argument('src_file', help='Source UTF-8 file')
parser.add_argument('dst_file', help='Destination UTF-16 LE file')

args = parser.parse_args()

with open(args.src_file, 'r', encoding='utf-8') as src_f:
    with open(args.dst_file, 'wb') as dst_f:
        dst_f.write(codecs.BOM_UTF16_LE)
        for line in src_f:
            dst_f.write(line.encode('utf-16-le'))
