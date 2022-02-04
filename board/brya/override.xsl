<?xml version="1.0" encoding="utf-8"?>
<xsl:stylesheet version="1.0"
		xmlns:xsl="http://www.w3.org/1999/XSL/Transform"
		xmlns:vif="http://usb.org/VendorInfoFile.xsd">

    <xsl:output method="xml" indent="yes" encoding="utf-8"/>

    <xsl:template match="@* | node()">
        <xsl:copy>
            <xsl:apply-templates select="@* | node()"/>
        </xsl:copy>
    </xsl:template>

    <!-- Make changes below this to override any parameter in baseline.xml -->
    <!-- match field is used to identify the XML node in the hierarchy -->

    <xsl:template match="vif:Product_Revision">
      <vif:Product_Revision>2</vif:Product_Revision>
    </xsl:template>

    <xsl:template match="vif:TID">
      <vif:TID>0</vif:TID>
    </xsl:template>

    <xsl:template match="vif:Certification_Type">
      <xsl:copy>
	<xsl:copy-of select="@*"/>
	<xsl:copy-of select="node()"/>
      </xsl:copy>
      <vif:Product>
	<xsl:comment>Product Level Content:</xsl:comment>
	<xsl:comment>;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;</xsl:comment>
	<xsl:comment>;USB4™ Product</xsl:comment>
	<xsl:comment>;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;</xsl:comment>
	<vif:USB4_Num_Internal_Host_Controllers value="1" />
	<vif:USB4_Num_PCIe_DN_Bridges value="0" />
	<xsl:comment>Bundle: USB4RouterList</xsl:comment>
	<vif:USB4RouterList>
	  <vif:Usb4Router>
            <xsl:comment>USB4 Router 0</xsl:comment>
            <vif:USB4_Router_ID value="0" />
            <vif:USB4_Silicon_VID value="32903">8087</vif:USB4_Silicon_VID>
            <vif:USB4_Num_Lane_Adapters value="4" />
            <vif:USB4_Num_USB3_DN_Adapters value="1" />
            <vif:USB4_Num_DP_IN_Adapters value="1" />
            <vif:USB4_Num_DP_OUT_Adapters value="0" />
            <vif:USB4_Num_PCIe_DN_Adapters value="4" />
            <vif:USB4_TBT3_Not_Supported value="0">TBT3 Compatible</vif:USB4_TBT3_Not_Supported>
            <vif:USB4_PCIe_Wake_Supported value="true" />
            <vif:USB4_USB3_Wake_Supported value="false" />
            <vif:USB4_Num_Unused_Adapters value="0" />
            <vif:USB4_TBT3_VID value="32903">8087</vif:USB4_TBT3_VID>
            <vif:USB4_PCIe_Switch_Vendor_ID value="32902">8086</vif:USB4_PCIe_Switch_Vendor_ID>
            <vif:USB4_PCIe_Switch_Device_ID value="39451">9A1B</vif:USB4_PCIe_Switch_Device_ID>
	  </vif:Usb4Router>
	</vif:USB4RouterList>
      </vif:Product>
    </xsl:template>

    <!-- all components -->

    <xsl:template match="vif:Component/vif:USB_Suspend_May_Be_Cleared">
      <vif:USB_Suspend_May_Be_Cleared value="true"/>
    </xsl:template>

    <xsl:template match="vif:Component/vif:Host_Is_Embedded">
      <xsl:copy>
	<xsl:copy-of select="@*"/>
	<xsl:copy-of select="node()"/>
      </xsl:copy>
      <vif:Host_Suspend_Supported value="true" />
    </xsl:template>

    <xsl:template match="vif:Component/vif:FR_Swap_Type_C_Current_Capability_As_Initial_Sink">
      <vif:FR_Swap_Type_C_Current_Capability_As_Initial_Sink value="3">3A @ 5V</vif:FR_Swap_Type_C_Current_Capability_As_Initial_Sink>
    </xsl:template>

    <!-- component 1 -->

    <xsl:template match="vif:Component[1]/vif:USB4_Max_Speed">
      <vif:USB4_Lane_0_Adapter value="1" />
      <xsl:copy>
	<xsl:copy-of select="@*"/>
	<xsl:copy-of select="node()"/>
      </xsl:copy>
      <vif:USB4_DFP_Supported value="true" />
      <vif:USB4_UFP_Supported value="false" />
      <vif:USB4_USB3_Tunneling_Supported value="true" />
      <vif:USB4_DP_Tunneling_Supported value="true" />
    </xsl:template>
    <xsl:template match="vif:Component[1]/vif:USB4_TBT3_Compatibility_Supported">
      <xsl:copy>
	<xsl:copy-of select="@*"/>
	<xsl:copy-of select="node()"/>
      </xsl:copy>
      <vif:USB4_CL1_State_Supported value="true" />
      <vif:USB4_CL2_State_Supported value="true" />
      <vif:USB4_Num_Retimers value="1" />
      <vif:USB4_DP_Bit_Rate value="3">HBR3</vif:USB4_DP_Bit_Rate>
      <vif:USB4_Num_DP_Lanes value="4">4 Lanes</vif:USB4_Num_DP_Lanes>
    </xsl:template>

    <xsl:template
	match="vif:Component[1]/vif:Host_Speed"
	>
      <vif:Host_Speed value="4">USB 3.2 Gen 2x2</vif:Host_Speed>
    </xsl:template>

    <xsl:template match="vif:Component[1]/vif:Host_Contains_Captive_Retimer">
      <vif:Host_Contains_Captive_Retimer value="true" />
      <vif:Host_Truncates_DP_For_tDHPResponse value="false" />
    </xsl:template>

    <xsl:template match="vif:Component[1]/vif:Data_Capable_As_USB_Device_SOP">
      <xsl:copy>
	<xsl:copy-of select="@*"/>
	<xsl:copy-of select="node()"/>
      </xsl:copy>
      <vif:Product_Type_UFP_SOP value="2">PDUSB Peripheral</vif:Product_Type_UFP_SOP>
      <vif:Product_Type_DFP_SOP value="2">PDUSB Host</vif:Product_Type_DFP_SOP>
    </xsl:template>

    <xsl:template match="vif:Component[1]/vif:SrcPdoList/vif:SrcPDO[1]">
      <xsl:copy>
	<xsl:copy-of select="@*"/>
	<xsl:copy-of select="node()"/>
	<vif:Src_PD_OCP_OC_Debounce value="50">50 msec</vif:Src_PD_OCP_OC_Debounce>
	<vif:Src_PD_OCP_OC_Threshold value="360">3600 mA</vif:Src_PD_OCP_OC_Threshold>
      </xsl:copy>
    </xsl:template>

    <!-- component 2 -->

    <xsl:template match="vif:Component[2]/vif:Host_Speed">
      <vif:Host_Speed value="2">USB 3.2 Gen 2x1</vif:Host_Speed>
    </xsl:template>

    <xsl:template match="vif:Component[2]/vif:Host_Contains_Captive_Retimer">
      <vif:Host_Contains_Captive_Retimer value="true" />
      <vif:Host_Truncates_DP_For_tDHPResponse value="false" />
    </xsl:template>

    <xsl:template match="vif:Component[2]/vif:Data_Capable_As_USB_Device_SOP">
      <xsl:copy>
	<xsl:copy-of select="@*"/>
	<xsl:copy-of select="node()"/>
      </xsl:copy>
      <vif:Product_Type_UFP_SOP value="3">PSD</vif:Product_Type_UFP_SOP>
      <vif:Product_Type_DFP_SOP value="2">PDUSB Host</vif:Product_Type_DFP_SOP>
    </xsl:template>

    <xsl:template match="vif:Component[2]/vif:SrcPdoList/vif:SrcPDO[1]">
      <xsl:copy>
	<xsl:copy-of select="@*"/>
	<xsl:copy-of select="node()"/>
	<vif:Src_PD_OCP_OC_Debounce value="50">50 msec</vif:Src_PD_OCP_OC_Debounce>
	<vif:Src_PD_OCP_OC_Threshold value="360">3600 mA</vif:Src_PD_OCP_OC_Threshold>
      </xsl:copy>
    </xsl:template>

    <!-- component 3 -->

    <xsl:template match="vif:Component[3]/vif:USB4_Max_Speed">
      <vif:USB4_Lane_0_Adapter value="1" />
      <xsl:copy>
	<xsl:copy-of select="@*"/>
	<xsl:copy-of select="node()"/>
      </xsl:copy>
      <vif:USB4_DFP_Supported value="true" />
      <vif:USB4_UFP_Supported value="false" />
      <vif:USB4_USB3_Tunneling_Supported value="true" />
      <vif:USB4_DP_Tunneling_Supported value="true" />
    </xsl:template>
    <xsl:template match="vif:Component[3]/vif:USB4_TBT3_Compatibility_Supported">
      <xsl:copy>
	<xsl:copy-of select="@*"/>
	<xsl:copy-of select="node()"/>
      </xsl:copy>
      <vif:USB4_CL1_State_Supported value="true" />
      <vif:USB4_CL2_State_Supported value="true" />
      <vif:USB4_Num_Retimers value="1" />
      <vif:USB4_DP_Bit_Rate value="3">HBR3</vif:USB4_DP_Bit_Rate>
      <vif:USB4_Num_DP_Lanes value="4">4 Lanes</vif:USB4_Num_DP_Lanes>
    </xsl:template>

    <xsl:template match="vif:Component[3]/vif:Host_Speed">
      <vif:Host_Speed value="4">USB 3.2 Gen 2x2</vif:Host_Speed>
    </xsl:template>

    <xsl:template match="vif:Component[3]/vif:Host_Contains_Captive_Retimer">
      <vif:Host_Contains_Captive_Retimer value="true" />
      <vif:Host_Truncates_DP_For_tDHPResponse value="false" />
    </xsl:template>

    <xsl:template match="vif:Component[3]/vif:Data_Capable_As_USB_Device_SOP">
      <xsl:copy>
	<xsl:copy-of select="@*"/>
	<xsl:copy-of select="node()"/>
      </xsl:copy>
      <vif:Product_Type_UFP_SOP value="2">PDUSB Peripheral</vif:Product_Type_UFP_SOP>
      <vif:Product_Type_DFP_SOP value="2">PDUSB Host</vif:Product_Type_DFP_SOP>
    </xsl:template>

    <xsl:template match="vif:FR_Swap_Supported_As_Initial_Sink">
      <vif:FR_Swap_Supported_As_Initial_Sink value="true"/>
    </xsl:template>

    <xsl:template match="vif:Component[3]/vif:SrcPdoList/vif:SrcPDO[1]">
      <xsl:copy>
	<xsl:copy-of select="@*"/>
	<xsl:copy-of select="node()"/>
	<vif:Src_PD_OCP_OC_Debounce value="50">50 msec</vif:Src_PD_OCP_OC_Debounce>
	<vif:Src_PD_OCP_OC_Threshold value="360">3600 mA</vif:Src_PD_OCP_OC_Threshold>
      </xsl:copy>
    </xsl:template>

</xsl:stylesheet>
