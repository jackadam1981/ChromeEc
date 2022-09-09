import argparse
import os
import sys

sys.path.insert(0, os.path.join(os.path.dirname(__file__), 'python-devicetree',
                                'src'))
from devicetree import edtlib
import xml.etree.ElementTree as ET

def main():
    args = parse_args()

    try:
        edt = edtlib.EDT(args.dts, args.bindings_dirs)
    except edtlib.EDTError as e:
        sys.exit(f"devicetree error: {e}")

    root = ET.Element("VIF")
    e1 = ET.SubElement(root, remove_special_characters("nordic,nrf-twim"))

    for node in edt.compat2nodes["nordic,nrf-twim"]:
        parse_node(e1, node)

    tree = ET.ElementTree(root)
    tree.write(args.vif_out)

def remove_special_characters(string):
    return ''.join(filter(str.isalnum, string))

def is_simple_data(data):
    if isinstance(data,(str, int, bool)):
        return True
    return False

def parse_controller_and_data(xml_mem, cad):
    xml_mem = ET.SubElement(xml_mem, remove_special_characters(cad.basename))
    for name in cad.data:
        xml_mem_l = ET.SubElement(xml_mem, remove_special_characters(name))
        xml_mem_l.text = str(cad.data[name])
    parse_node(xml_mem, cad.controller)

def parse_arrays(xml_mem, prop):
    for member in prop.val:
        if is_simple_data(member):
            xml_mem_l = ET.SubElement(xml_mem, remove_special_characters(prop.name))
            xml_mem_l.text = str(member)
        elif isinstance(member, list):
            xml_mem_1 = ET.SubElement(xml_mem, remove_special_characters(prop.name))
            parse_arrays(xml_mem_1, member)
        elif isinstance(member, edtlib.Node):
            xml_mem_1 = ET.SubElement(xml_mem, remove_special_characters(prop.name))
            parse_node(xml_mem_1, member)
        elif isinstance(member, edtlib.ControllerAndData):
            xml_mem_1 = ET.SubElement(xml_mem, remove_special_characters(prop.name))
            parse_controller_and_data(xml_mem_1, member)
        else:
            err("Noticed undefined type : " + str(type(member)) + ", with value" + str(member))

def parse_node(xml_mem, node):
    if not isinstance(node, edtlib.Node):
        return
    xml_mem = ET.SubElement(xml_mem, remove_special_characters(node.name))
    for prop in node.props:
        if is_simple_data(node.props[prop].val):
            xml_mem_1 = ET.SubElement(xml_mem, remove_special_characters(node.props[prop].name))
            xml_mem_1.text = str(node.props[prop].val)
        elif isinstance(node.props[prop].val, list):
            parse_arrays(xml_mem, node.props[prop])
        elif isinstance(node.props[prop].val, edtlib.Node):
            xml_mem_1 = ET.SubElement(xml_mem, remove_special_characters(node.props[prop].name))
            parse_node(xml_mem_1, node.props[prop].val)
        elif isinstance(node.props[prop].val, edtlib.ControllerAndData):
            xml_mem_1 = ET.SubElement(xml_mem, remove_special_characters(node.props[prop].name))
            parse_controller_and_data(xml_mem_1, node.props[prop].val)
        else:
            err("Noticed undefined type : " + str(type(node.props[prop].val)) + ", with value" + str(node.props[prop].val))
    for child in node.children:
        xml_mem_1 = ET.SubElement(xml_mem, remove_special_characters(child))
        parse_node(xml_mem_1, node.children[child])


def parse_args():
    # Returns parsed command-line arguments

    parser = argparse.ArgumentParser()
    parser.add_argument("--dts", required=True, help="Aggregated Zephyr project device tree file")
    parser.add_argument("--vif-out", required=True,
                        help="path to write VIF policies to")
    parser.add_argument("--bindings-dirs", nargs='+', required=True,
                        help="directory with bindings in YAML format, we allow multiple")
    return parser.parse_args()

def err(s):
    raise Exception(s)


if __name__ == "__main__":
    main()
