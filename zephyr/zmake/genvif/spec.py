"""
VIF specification data types.

These types and their relationships are described in the Vendor Info File
Specification, section 3.1.4.1.
"""

from enum import Enum
from typing import Optional

import dataclasses
from dataclasses import dataclass

from .xmldataclass import XmlDataclass

@dataclass
class App(XmlDataclass):
  Vendor: str = 'Google'
  Name: str = 'Zephyr Genvif'
  Version: str = '1.0.0'

class ProductType(Enum):
  PORT = 0
  CABLE = 1
  REPEATER = 2

class CertificationType(Enum):
  END_PRODUCT = 0
  REFERENCE_PLATFORM = 1
  SILICON = 2

class ConnectorType(Enum):
  A = 0
  B = 1
  C = 2
  MICRO = 3

class PDPortType(Enum):
  CONSUMER_ONLY = 0
  CONSUMER_PROVIDER = 1
  PROVIDER_CONSUMER = 2
  PROVIDER_ONLY = 3
  DRP = 4
  EMARKER = 5

class TypeCStateMachine(Enum):
  SRC = 0
  SNK = 1
  DRP = 2

class BC_1_2_Support(Enum):
  NONE = 0
  PORTABLE_DEVICE = 1
  CHARGING_PORT = 2
  BOTH = 3

class PD_Specification_Revision(Enum):
  REVISION_2 = 1
  REVISION_3 = 2

class Power_Interruption_Available(Enum):
  NONE = 0
  AC_MAINS_ONLY = 1
  DC_ONLY = 2
  AC_MAINS_AND_DC = 3

class ID_Header_Connector_Type_SOP(Enum):
  USB_C_RECEPTABLE = 2
  USB_C_PLUG = 3

class RP_Value(Enum):
  DEFAULT = 0
  1_5A = 1
  3A = 2

class TypeCPowerSource(Enum):
  EXTERNAL = 0
  UFP = 1
  BOTH = 2

class USB4_Max_Speed(Enum):
  GEN_2 = 0
  GEN_3 = 1

@dataclass
class Product(XmlDataclass):
  USB4_DROM_Vendor_ID: Optional[str] = None
  USB4_Dock: bool = False
  USB4_Num_Internal_Host_Controllers: int = 0
  USB4_Num_PCIe_DN_Bridges: int = 0
  USB4_Device_HiFi_Bi_TMU_Mode_Required: bool = False

  # USB4 peripherals and docks: whether the given class is implemented when
  # operating over USB 3.2 or 2.
  USB4_Audio_Supported: Optional[bool] = False
  USB4_HID_Supported: Optional[bool] = False
  USB4_Printer_Supported: Optional[bool] = False
  USB4_Mass_Storage_Supported: Optional[bool] = False
  USB4_Video_Supported: Optional[bool] = False
  USB4_Comms_Networking_Supported: Optional[bool] = False
  USB4_Media_Transfer_Protocol_Supported: Optional[bool] = False
  USB4_Smart_Card_Supported: Optional[bool] = False
  USB4_Still_Image_Capture_Supported: Optional[bool] = False
  USB4_Monitor_Device_Supported: Optional[bool] = None

  # Deprecated in VIF version 3.13
  Product_VID: Optional[int] = None

@dataclass
class Component(XmlDataclass):
  Port_Label: Optional[str]

  Connector_Type: Optional[ConnectorType]

  USB4_Supported: Optional[bool]

  USB_PD_Support: Optional[bool]
  PD_Port_Type: Optional[PDPortType]
  Type_C_State_Machine: Optional[TypeCStateMachine]
  Port_Battery_Powered: Optional[bool]
  BC_1_2_Support: Optional[BC_1_2_Support]

  # Fields required if USB_PD_Support = True
  PD_Spec_Revision_Major: Optional[int]
  PD_Spec_Revision_Minor: Optional[int]
  PD_Spec_Version_Major: Optional[int]
  PD_Spec_Version_Minor: Optional[int]
  PD_Specification_Revision: Optional[PD_Specification_Revision]

  SOP_Capable: Optional[bool]
  SOP_P_Capable: Optional[bool]
  SOP_PP_Capable: Optional[bool]
  SOP_P_Debug_Capable: Optional[bool]
  SOP_PP_Debug_Capable: Optional[bool]
  Manufacturer_Info_Supported_Port: Optional[bool]
  Manufacturer_Info_VID_Port: Optional[int]
  Manufacturer_Info_PID_Port: Optional[int]
  USB_Comms_Capable: Optional[bool]
  DR_Swap_To_DFP_Supported: Optional[bool]
  DR_Swap_To_UFP_Supported: Optional[bool]
  Unconstrained_Power: Optional[bool]
  VCONN_Swap_To_On_Supported: Optional[bool]
  VCONN_Swap_To_Off_Supported: Optional[bool]
  Responds_To_Discov_SOP_UFP: Optional[bool]
  Responds_To_Discov_SOP_DFP: Optional[bool]
  Attempts_Discov_SOP: Optional[bool]
  Power_Interruption_Available: Optional[Power_Interruption_Available]
  Chunking_Implemented_SOP: Optional[bool]
  Data_Reset_Supported: Optional[bool]
  Enter_USB_Supported: Optional[bool]
  Unchunked_Extended_Messages_Support: Optional[bool]ed
  Security_Msgs_Supported_SOP: Optional[bool]
  Num_Fixed_Batteries: Optional[int]
  Num_Swappable_Battery_Slots: Optional[int]
  ID_Header_Connector_Type_SOP: Optional[ID_Header_Connector_Type_SOP]

  Type_C_Can_Act_As_Host: Optional[bool]
  Type_C_Can_Act_As_Device: Optional[bool]
  Type_C_Implements_Try_SRC: Optional[bool]
  Type_C_Implements_Try_SNK: Optional[bool]
  Type_C_Supports_Audio_Accessory: Optional[bool]
  Type_C_Supports_VCONN_Powered_Accessory: Optional[bool]
  Type_C_Is_VCONN_Powered_Accessory: Optional[bool]
  Type_C_Is_Debug_Target_SRC: Optional[bool]
  Type_C_Is_Debug_Target_SNK: Optional[bool]
  Captive_Cable: Optional[bool]
  Captive_Cable_Is_eMarked: Optional[bool]
  RP_Value: Optional[RP_Value]
  Type_C_Port_On_Hub: Optional[bool]
  Type_C_Power_Source: Optional[TypeCPowerSource]
  Type_C_Sources_VCONN: Optional[bool]
  Type_C_Is_Alt_Mode_Controller: Optional[bool]
  Type_C_Is_Alt_Mode_Adapter: Optional[bool]

  USB4_Router_Index: Optional[int]
  USB4_Lane_0_Adapter: Optional[int]
  USB4_Max_Speed: Optional[USB4_Max_Speed]

  USB4_DFP_Supported: Optional[bool]
  USB4_UFP_Supported: Optional[bool]

  USB4_USB3_Tunneling_Supported: Optional[bool]
  USB4_DP_Tunneling_Supported: Optional[bool]
  USB4_PCIe_Tunneling_Supported: Optional[bool]
  USB4_TBT3_Compatibility_Supported: Optional[bool]
  USB4_CL1_State_Supported: Optional[bool]
  USB4_CL2_State_Supported: Optional[bool]

  USB4_Num_Retimers: Optional[int]
  USB4_DP_Bit_Rate: Optional[int]
  USB4_Num_DP_Lanes: Optional[int]

  Host_Supports_USB_Data: Optional[bool]
  Host_Speed: Optional[int]
  Host_Contains_Captive_Retimer: Optional[bool]
  Host_Truncates_DP_For_tDHPResponse: Optional[bool]
  Host_Gen1x1_tLinkTurnaround: Optional[int]
  Host_Gen2x1_tLinkTurnaround: Optional[int]
  Host_Is_Embedded: Optional[bool]
  Host_Suspend_Supported: Optional[bool]
  Is_DFP_On_Hub: Optional[bool]
  Hub_Port_Number: Optional[int]

  Device_Supports_USB_Data: Optional[bool]
  Device_Speed: Optional[int]
  Device_Max_USB2_Speed: Optional[int]
  Device_Contains_Captive_Retimer: Optional[bool]
  Device_Truncates_DP_For_tDHPResponse: Optional[bool]
  Device_Gen1x1_tLinkTurnaround: Optional[int]
  Device_Gen2x1_tLinkTurnaround: Optional[int]

  BC_1_2_Charging_Port_Type: Optional[int]

  PD_Power_As_Source: Optional[int]
  EPR_Supported_As_Src: Optional[bool]
  USB_Suspend_May_Be_Cleared: Optional[bool]
  Sends_Pings: Optional[bool]
  FR_Swap_Type_C_Current_Capability_As_Initial_Sink: Optional[int]
  Master_Port: Optional[bool]
  Num_Src_PDOs: Optional[int]
  PD_OC_Protection: Optional[bool]
  PD_OCP_Method: Optional[int]

  SrcPdoList: List[SrcPdo]

  PD_Power_As_Sink: Optional[int]
  EPR_Supported_As_Snk: Optional[bool]
  No_USB_Suspend_May_Be_Set: Optional[bool]
  GiveBack_May_Be_Set: Optional[bool]
  Higher_Capability_Set: Optional[bool]
  FR_Swap_Reqd_Type_C_Current_As_Initial_Source: Optional[int]
  Num_Snk_PDOs: Optional[int]

  SnkPdoList: List[SnkPdo]

  Accepts_PR_Swap_As_Src: Optional[bool]
  Accepts_PR_Swap_As_Snk: Optional[bool]
  Requests_PR_Swap_As_Src: Optional[bool]
  Requests_PR_Swap_As_Snk: Optional[bool]
  FR_Swap_Supported_As_Initial_Sink: Optional[bool]

  XID_SOP: Optional[int]
  Data_Capable_As_USB_Host_SOP: Optional[bool]
  Data_Capable_As_USB_Device_SOP: Optional[bool]
  Product_Type_UFP_SOP: Optional[int]
  Product_Type_DFP_SOP: Optional[int]
  DFP_VDO_Port_Number: Optional[int]
  Modal_Operation_Supported_SOP: Optional[bool]
  USB_VID_SOP: Optional[int]
  PID_SOP: Optional[int]
  bcdDevice_SOP: Optional[int]

  SVID_Fixed_SOP: Optional[bool]
  Num_SVIDs_Min_SOP: Optional[int]
  Num_SVIDs_Max_SOP: Optional[int]

  SOPSVIDList: List[SopSVID]

  AMA_HW_Vers: Optional[int]
  AMA_FW_Vers: Optional[int]
  AMA_VCONN_Reqd: Optional[bool]
  AMA_VCONN_Power: Optional[int]
  AMA_VBUS_Reqd: Optional[bool]
  AMA_Superspeed_Support: Optional[int]

  Product_Total_Source_Power_mW: Optional[int]
  Port_Source_Power_Type: Optional[int]
  Port_Source_Power_Gang: Optional[str]
  Port_Source_Power_Gang_Max_Power: Optional[int]

  XID: Optional[int]
  Data_Capable_As_USB_Host: Optional[bool]
  Data_Capable_As_USB_Device: Optional[bool]
  Product_Type: Optional[int]
  Modal_Operation_Supported: Optional[bool]
  USB_VID: Optional[int]
  PID: Optional[int]
  bcdDevice: Optional[int]
  Cable_HW_Vers: Optional[int]
  Cable_FW_Vers: Optional[int]
  Type_C_To_Type_A_B_C: Optional[int]
  Type_C_To_Type_C_Capt_Vdm_V2: Optional[int]
  Cable_Latency: Optional[int]
  Cable_Termination_Type: Optional[int]
  VBUS_Through_Cable: Optional[bool]
  Cable_VBUS_Current: Optional[int]
  Cable_Superspeed_Support: Optional[int]
  Cable_USB_Highest_Speed: Optional[int]
  EPR_Mode_Capable: Optional[bool]
  Max_VBUS_Voltage_Vdm_V2: Optional[int]
  Manufacturer_Info_Supported: Optional[bool]
  Manufacturer_Info_VID: Optional[int]
  Manufacturer_Info_PID: Optional[int]
  Chunking_Implemented: Optional[bool]
  Security_Msgs_Supported: Optional[bool]
  ID_Header_Connector_Type: Optional[int]

  SVID_Fixed: Optional[bool]
  Cable_Num_SVIDs_Min: Optional[int]
  Cable_Num_SVIDs_Max: Optional[int]

  CableSVIDList: List[CableSVID]

  VPD_HW_Vers: Optional[int]
  VPD_FW_Vers: Optional[int]
  VPD_Max_VBUS_Voltage: Optional[int]
  VPD_Charge_Through_Support: Optional[bool]
  VPD_Charge_Through_Current: Optional[int]
  VPD_VBUS_Impedance: Optional[int]
  VPD_Ground_Impedance: Optional[int]

  Cable_SOP_PP_Controller: Optional[bool]
  SBU_Supported: Optional[bool]
  SBU_Type: Optional[int]
  Active_Cable_Operating_Temp_Support: Optional[bool]
  Active_Cable_Max_Operating_Temp: Optional[int]
  Active_Cable_Shutdown_Temp_Support: Optional[bool]
  Active_Cable_Shutdown_Temp: Optional[int]
  Active_Cable_U3_CLd_Power: Optional[int]
  Active_Cable_U3_U0_Trans_Mode: Optional[int]
  Active_Cable_Physical_Connection: Optional[int]
  Active_Cable_Active_Element: Optional[int]
  Active_Cable_USB4_Support: Optional[bool]
  Active_Cable_USB2_Hub_Hops_Consumed: Optional[int]
  Active_Cable_USB2_Supported: Optional[bool]
  Active_Cable_USB32_Supported: Optional[bool]
  Active_Cable_USB_Lanes: Optional[int]: Optional[int]
  Active_Cable_Optically_Isolated: Optional[bool]
  Active_Cable_USB_Gen: Optional[int]: Optional[int]

  Repeater_One_Type: Optional[int]
  Repeater_Two_Type: Optional[int]
  # Continues in https://compliance.usb.org/cv/VendorInfoFile/Schemas/Current/VendorInfoFile.xsd

@dataclass
class VIF(XmlDataclass):
  Component: list[Component]

  VIF_Specification: str = '3.12'
  VIF_App: App = App()

  Vendor_Name: Optional[str] = 'Google'
  Model_Part_Number: Optional[str] = None
  Product_Revision: Optional[str] = None
  TID: Optional[str] = None
  VIF_Product_Type: ProductType = ProductType.PORT
  Certification_Type: CertificationType = CertificationType.END_PRODUCT

  Product: Product = Product()


