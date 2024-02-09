 /*
* The MIT License (MIT)
*
* Copyright (c) 2019 Microchip Technology Inc.
* Copyright (c) 2018, hathach (tinyusb.org)
* Copyright (c) 2021, HiFiPhile
*
* Permission is hereby granted, free of charge, to any person obtaining a copy
* of this software and associated documentation files (the "Software"), to deal
* in the Software without restriction, including without limitation the rights
* to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
* copies of the Software, and to permit persons to whom the Software is
* furnished to do so, subject to the following conditions:
*
* The above copyright notice and this permission notice shall be included in
* all copies or substantial portions of the Software.
*
* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
* IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
* FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
* AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
* LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
* OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
* THE SOFTWARE.
*
* This file is part of the TinyUSB stack.
*/

#ifndef _COMMON_USB_REGS_H_
#define _COMMON_USB_REGS_H_

#if CFG_TUSB_MCU == OPT_MCU_SAMX7X

/* -------- DEVDMANXTDSC : (USBHS Offset: 0x00) (R/W 32) Device DMA Channel Next Descriptor Address Register -------- */

#define DEVDMANXTDSC_OFFSET           (0x00)                                        /**<  (DEVDMANXTDSC) Device DMA Channel Next Descriptor Address Register  Offset */

#define DEVDMANXTDSC_NXT_DSC_ADD_Pos  0                                              /**< (DEVDMANXTDSC) Next Descriptor Address Position */
#define DEVDMANXTDSC_NXT_DSC_ADD      (_U_(0xFFFFFFFF) << DEVDMANXTDSC_NXT_DSC_ADD_Pos)  /**< (DEVDMANXTDSC) Next Descriptor Address Mask */
#define DEVDMANXTDSC_Msk              _U_(0xFFFFFFFF)                                /**< (DEVDMANXTDSC) Register Mask  */


/* -------- DEVDMAADDRESS : (USBHS Offset: 0x04) (R/W 32) Device DMA Channel Address Register -------- */

#define DEVDMAADDRESS_OFFSET          (0x04)                                        /**<  (DEVDMAADDRESS) Device DMA Channel Address Register  Offset */

#define DEVDMAADDRESS_BUFF_ADD_Pos    0                                              /**< (DEVDMAADDRESS) Buffer Address Position */
#define DEVDMAADDRESS_BUFF_ADD        (_U_(0xFFFFFFFF) << DEVDMAADDRESS_BUFF_ADD_Pos)  /**< (DEVDMAADDRESS) Buffer Address Mask */
#define DEVDMAADDRESS_Msk             _U_(0xFFFFFFFF)                                /**< (DEVDMAADDRESS) Register Mask  */


/* -------- DEVDMACONTROL : (USBHS Offset: 0x08) (R/W 32) Device DMA Channel Control Register -------- */

#define DEVDMACONTROL_OFFSET          (0x08)                                        /**<  (DEVDMACONTROL) Device DMA Channel Control Register  Offset */

#define DEVDMACONTROL_CHANN_ENB_Pos   0                                              /**< (DEVDMACONTROL) Channel Enable Command Position */
#define DEVDMACONTROL_CHANN_ENB       (_U_(0x1) << DEVDMACONTROL_CHANN_ENB_Pos)  /**< (DEVDMACONTROL) Channel Enable Command Mask */
#define DEVDMACONTROL_LDNXT_DSC_Pos   1                                              /**< (DEVDMACONTROL) Load Next Channel Transfer Descriptor Enable Command Position */
#define DEVDMACONTROL_LDNXT_DSC       (_U_(0x1) << DEVDMACONTROL_LDNXT_DSC_Pos)  /**< (DEVDMACONTROL) Load Next Channel Transfer Descriptor Enable Command Mask */
#define DEVDMACONTROL_END_TR_EN_Pos   2                                              /**< (DEVDMACONTROL) End of Transfer Enable Control (OUT transfers only) Position */
#define DEVDMACONTROL_END_TR_EN       (_U_(0x1) << DEVDMACONTROL_END_TR_EN_Pos)  /**< (DEVDMACONTROL) End of Transfer Enable Control (OUT transfers only) Mask */
#define DEVDMACONTROL_END_B_EN_Pos    3                                              /**< (DEVDMACONTROL) End of Buffer Enable Control Position */
#define DEVDMACONTROL_END_B_EN        (_U_(0x1) << DEVDMACONTROL_END_B_EN_Pos)  /**< (DEVDMACONTROL) End of Buffer Enable Control Mask */
#define DEVDMACONTROL_END_TR_IT_Pos   4                                              /**< (DEVDMACONTROL) End of Transfer Interrupt Enable Position */
#define DEVDMACONTROL_END_TR_IT       (_U_(0x1) << DEVDMACONTROL_END_TR_IT_Pos)  /**< (DEVDMACONTROL) End of Transfer Interrupt Enable Mask */
#define DEVDMACONTROL_END_BUFFIT_Pos  5                                              /**< (DEVDMACONTROL) End of Buffer Interrupt Enable Position */
#define DEVDMACONTROL_END_BUFFIT      (_U_(0x1) << DEVDMACONTROL_END_BUFFIT_Pos)  /**< (DEVDMACONTROL) End of Buffer Interrupt Enable Mask */
#define DEVDMACONTROL_DESC_LD_IT_Pos  6                                              /**< (DEVDMACONTROL) Descriptor Loaded Interrupt Enable Position */
#define DEVDMACONTROL_DESC_LD_IT      (_U_(0x1) << DEVDMACONTROL_DESC_LD_IT_Pos)  /**< (DEVDMACONTROL) Descriptor Loaded Interrupt Enable Mask */
#define DEVDMACONTROL_BURST_LCK_Pos   7                                              /**< (DEVDMACONTROL) Burst Lock Enable Position */
#define DEVDMACONTROL_BURST_LCK       (_U_(0x1) << DEVDMACONTROL_BURST_LCK_Pos)  /**< (DEVDMACONTROL) Burst Lock Enable Mask */
#define DEVDMACONTROL_BUFF_LENGTH_Pos 16                                             /**< (DEVDMACONTROL) Buffer Byte Length (Write-only) Position */
#define DEVDMACONTROL_BUFF_LENGTH     (_U_(0xFFFF) << DEVDMACONTROL_BUFF_LENGTH_Pos)  /**< (DEVDMACONTROL) Buffer Byte Length (Write-only) Mask */
#define DEVDMACONTROL_Msk             _U_(0xFFFF00FF)                                /**< (DEVDMACONTROL) Register Mask  */


/* -------- DEVDMASTATUS : (USBHS Offset: 0x0c) (R/W 32) Device DMA Channel Status Register -------- */

#define DEVDMASTATUS_OFFSET           (0x0C)                                        /**<  (DEVDMASTATUS) Device DMA Channel Status Register  Offset */

#define DEVDMASTATUS_CHANN_ENB_Pos    0                                              /**< (DEVDMASTATUS) Channel Enable Status Position */
#define DEVDMASTATUS_CHANN_ENB        (_U_(0x1) << DEVDMASTATUS_CHANN_ENB_Pos)  /**< (DEVDMASTATUS) Channel Enable Status Mask */
#define DEVDMASTATUS_CHANN_ACT_Pos    1                                              /**< (DEVDMASTATUS) Channel Active Status Position */
#define DEVDMASTATUS_CHANN_ACT        (_U_(0x1) << DEVDMASTATUS_CHANN_ACT_Pos)  /**< (DEVDMASTATUS) Channel Active Status Mask */
#define DEVDMASTATUS_END_TR_ST_Pos    4                                              /**< (DEVDMASTATUS) End of Channel Transfer Status Position */
#define DEVDMASTATUS_END_TR_ST        (_U_(0x1) << DEVDMASTATUS_END_TR_ST_Pos)  /**< (DEVDMASTATUS) End of Channel Transfer Status Mask */
#define DEVDMASTATUS_END_BF_ST_Pos    5                                              /**< (DEVDMASTATUS) End of Channel Buffer Status Position */
#define DEVDMASTATUS_END_BF_ST        (_U_(0x1) << DEVDMASTATUS_END_BF_ST_Pos)  /**< (DEVDMASTATUS) End of Channel Buffer Status Mask */
#define DEVDMASTATUS_DESC_LDST_Pos    6                                              /**< (DEVDMASTATUS) Descriptor Loaded Status Position */
#define DEVDMASTATUS_DESC_LDST        (_U_(0x1) << DEVDMASTATUS_DESC_LDST_Pos)  /**< (DEVDMASTATUS) Descriptor Loaded Status Mask */
#define DEVDMASTATUS_BUFF_COUNT_Pos   16                                             /**< (DEVDMASTATUS) Buffer Byte Count Position */
#define DEVDMASTATUS_BUFF_COUNT       (_U_(0xFFFF) << DEVDMASTATUS_BUFF_COUNT_Pos)  /**< (DEVDMASTATUS) Buffer Byte Count Mask */
#define DEVDMASTATUS_Msk              _U_(0xFFFF0073)                                /**< (DEVDMASTATUS) Register Mask  */


/* -------- HSTDMANXTDSC : (USBHS Offset: 0x00) (R/W 32) Host DMA Channel Next Descriptor Address Register -------- */

#define HSTDMANXTDSC_OFFSET           (0x00)                                        /**<  (HSTDMANXTDSC) Host DMA Channel Next Descriptor Address Register  Offset */

#define HSTDMANXTDSC_NXT_DSC_ADD_Pos  0                                              /**< (HSTDMANXTDSC) Next Descriptor Address Position */
#define HSTDMANXTDSC_NXT_DSC_ADD      (_U_(0xFFFFFFFF) << HSTDMANXTDSC_NXT_DSC_ADD_Pos)  /**< (HSTDMANXTDSC) Next Descriptor Address Mask */
#define HSTDMANXTDSC_Msk              _U_(0xFFFFFFFF)                                /**< (HSTDMANXTDSC) Register Mask  */


/* -------- HSTDMAADDRESS : (USBHS Offset: 0x04) (R/W 32) Host DMA Channel Address Register -------- */

#define HSTDMAADDRESS_OFFSET          (0x04)                                        /**<  (HSTDMAADDRESS) Host DMA Channel Address Register  Offset */

#define HSTDMAADDRESS_BUFF_ADD_Pos    0                                              /**< (HSTDMAADDRESS) Buffer Address Position */
#define HSTDMAADDRESS_BUFF_ADD        (_U_(0xFFFFFFFF) << HSTDMAADDRESS_BUFF_ADD_Pos)  /**< (HSTDMAADDRESS) Buffer Address Mask */
#define HSTDMAADDRESS_Msk             _U_(0xFFFFFFFF)                                /**< (HSTDMAADDRESS) Register Mask  */


/* -------- HSTDMACONTROL : (USBHS Offset: 0x08) (R/W 32) Host DMA Channel Control Register -------- */

#define HSTDMACONTROL_OFFSET          (0x08)                                        /**<  (HSTDMACONTROL) Host DMA Channel Control Register  Offset */

#define HSTDMACONTROL_CHANN_ENB_Pos   0                                              /**< (HSTDMACONTROL) Channel Enable Command Position */
#define HSTDMACONTROL_CHANN_ENB       (_U_(0x1) << HSTDMACONTROL_CHANN_ENB_Pos)  /**< (HSTDMACONTROL) Channel Enable Command Mask */
#define HSTDMACONTROL_LDNXT_DSC_Pos   1                                              /**< (HSTDMACONTROL) Load Next Channel Transfer Descriptor Enable Command Position */
#define HSTDMACONTROL_LDNXT_DSC       (_U_(0x1) << HSTDMACONTROL_LDNXT_DSC_Pos)  /**< (HSTDMACONTROL) Load Next Channel Transfer Descriptor Enable Command Mask */
#define HSTDMACONTROL_END_TR_EN_Pos   2                                              /**< (HSTDMACONTROL) End of Transfer Enable Control (OUT transfers only) Position */
#define HSTDMACONTROL_END_TR_EN       (_U_(0x1) << HSTDMACONTROL_END_TR_EN_Pos)  /**< (HSTDMACONTROL) End of Transfer Enable Control (OUT transfers only) Mask */
#define HSTDMACONTROL_END_B_EN_Pos    3                                              /**< (HSTDMACONTROL) End of Buffer Enable Control Position */
#define HSTDMACONTROL_END_B_EN        (_U_(0x1) << HSTDMACONTROL_END_B_EN_Pos)  /**< (HSTDMACONTROL) End of Buffer Enable Control Mask */
#define HSTDMACONTROL_END_TR_IT_Pos   4                                              /**< (HSTDMACONTROL) End of Transfer Interrupt Enable Position */
#define HSTDMACONTROL_END_TR_IT       (_U_(0x1) << HSTDMACONTROL_END_TR_IT_Pos)  /**< (HSTDMACONTROL) End of Transfer Interrupt Enable Mask */
#define HSTDMACONTROL_END_BUFFIT_Pos  5                                              /**< (HSTDMACONTROL) End of Buffer Interrupt Enable Position */
#define HSTDMACONTROL_END_BUFFIT      (_U_(0x1) << HSTDMACONTROL_END_BUFFIT_Pos)  /**< (HSTDMACONTROL) End of Buffer Interrupt Enable Mask */
#define HSTDMACONTROL_DESC_LD_IT_Pos  6                                              /**< (HSTDMACONTROL) Descriptor Loaded Interrupt Enable Position */
#define HSTDMACONTROL_DESC_LD_IT      (_U_(0x1) << HSTDMACONTROL_DESC_LD_IT_Pos)  /**< (HSTDMACONTROL) Descriptor Loaded Interrupt Enable Mask */
#define HSTDMACONTROL_BURST_LCK_Pos   7                                              /**< (HSTDMACONTROL) Burst Lock Enable Position */
#define HSTDMACONTROL_BURST_LCK       (_U_(0x1) << HSTDMACONTROL_BURST_LCK_Pos)  /**< (HSTDMACONTROL) Burst Lock Enable Mask */
#define HSTDMACONTROL_BUFF_LENGTH_Pos 16                                             /**< (HSTDMACONTROL) Buffer Byte Length (Write-only) Position */
#define HSTDMACONTROL_BUFF_LENGTH     (_U_(0xFFFF) << HSTDMACONTROL_BUFF_LENGTH_Pos)  /**< (HSTDMACONTROL) Buffer Byte Length (Write-only) Mask */
#define HSTDMACONTROL_Msk             _U_(0xFFFF00FF)                                /**< (HSTDMACONTROL) Register Mask  */


/* -------- HSTDMASTATUS : (USBHS Offset: 0x0c) (R/W 32) Host DMA Channel Status Register -------- */

#define HSTDMASTATUS_OFFSET           (0x0C)                                        /**<  (HSTDMASTATUS) Host DMA Channel Status Register  Offset */

#define HSTDMASTATUS_CHANN_ENB_Pos    0                                              /**< (HSTDMASTATUS) Channel Enable Status Position */
#define HSTDMASTATUS_CHANN_ENB        (_U_(0x1) << HSTDMASTATUS_CHANN_ENB_Pos)  /**< (HSTDMASTATUS) Channel Enable Status Mask */
#define HSTDMASTATUS_CHANN_ACT_Pos    1                                              /**< (HSTDMASTATUS) Channel Active Status Position */
#define HSTDMASTATUS_CHANN_ACT        (_U_(0x1) << HSTDMASTATUS_CHANN_ACT_Pos)  /**< (HSTDMASTATUS) Channel Active Status Mask */
#define HSTDMASTATUS_END_TR_ST_Pos    4                                              /**< (HSTDMASTATUS) End of Channel Transfer Status Position */
#define HSTDMASTATUS_END_TR_ST        (_U_(0x1) << HSTDMASTATUS_END_TR_ST_Pos)  /**< (HSTDMASTATUS) End of Channel Transfer Status Mask */
#define HSTDMASTATUS_END_BF_ST_Pos    5                                              /**< (HSTDMASTATUS) End of Channel Buffer Status Position */
#define HSTDMASTATUS_END_BF_ST        (_U_(0x1) << HSTDMASTATUS_END_BF_ST_Pos)  /**< (HSTDMASTATUS) End of Channel Buffer Status Mask */
#define HSTDMASTATUS_DESC_LDST_Pos    6                                              /**< (HSTDMASTATUS) Descriptor Loaded Status Position */
#define HSTDMASTATUS_DESC_LDST        (_U_(0x1) << HSTDMASTATUS_DESC_LDST_Pos)  /**< (HSTDMASTATUS) Descriptor Loaded Status Mask */
#define HSTDMASTATUS_BUFF_COUNT_Pos   16                                             /**< (HSTDMASTATUS) Buffer Byte Count Position */
#define HSTDMASTATUS_BUFF_COUNT       (_U_(0xFFFF) << HSTDMASTATUS_BUFF_COUNT_Pos)  /**< (HSTDMASTATUS) Buffer Byte Count Mask */
#define HSTDMASTATUS_Msk              _U_(0xFFFF0073)                                /**< (HSTDMASTATUS) Register Mask  */


/* -------- DEVCTRL : (USBHS Offset: 0x00) (R/W 32) Device General Control Register -------- */

#define DEVCTRL_OFFSET                (0x00)                                        /**<  (DEVCTRL) Device General Control Register  Offset */

#define DEVCTRL_UADD_Pos              0                                              /**< (DEVCTRL) USB Address Position */
#define DEVCTRL_UADD                  (_U_(0x7F) << DEVCTRL_UADD_Pos)          /**< (DEVCTRL) USB Address Mask */
#define DEVCTRL_ADDEN_Pos             7                                              /**< (DEVCTRL) Address Enable Position */
#define DEVCTRL_ADDEN                 (_U_(0x1) << DEVCTRL_ADDEN_Pos)          /**< (DEVCTRL) Address Enable Mask */
#define DEVCTRL_DETACH_Pos            8                                              /**< (DEVCTRL) Detach Position */
#define DEVCTRL_DETACH                (_U_(0x1) << DEVCTRL_DETACH_Pos)         /**< (DEVCTRL) Detach Mask */
#define DEVCTRL_RMWKUP_Pos            9                                              /**< (DEVCTRL) Remote Wake-Up Position */
#define DEVCTRL_RMWKUP                (_U_(0x1) << DEVCTRL_RMWKUP_Pos)         /**< (DEVCTRL) Remote Wake-Up Mask */
#define DEVCTRL_SPDCONF_Pos           10                                             /**< (DEVCTRL) Mode Configuration Position */
#define DEVCTRL_SPDCONF               (_U_(0x3) << DEVCTRL_SPDCONF_Pos)        /**< (DEVCTRL) Mode Configuration Mask */
#define   DEVCTRL_SPDCONF_NORMAL_Val  _U_(0x0)                                       /**< (DEVCTRL) The peripheral starts in Full-speed mode and performs a high-speed reset to switch to High-speed mode if the host is high-speed-capable.  */
#define   DEVCTRL_SPDCONF_LOW_POWER_Val _U_(0x1)                                       /**< (DEVCTRL) For a better consumption, if high speed is not needed.  */
#define   DEVCTRL_SPDCONF_HIGH_SPEED_Val _U_(0x2)                                       /**< (DEVCTRL) Forced high speed.  */
#define   DEVCTRL_SPDCONF_FORCED_FS_Val _U_(0x3)                                       /**< (DEVCTRL) The peripheral remains in Full-speed mode whatever the host speed capability.  */
#define DEVCTRL_SPDCONF_NORMAL        (DEVCTRL_SPDCONF_NORMAL_Val << DEVCTRL_SPDCONF_Pos)  /**< (DEVCTRL) The peripheral starts in Full-speed mode and performs a high-speed reset to switch to High-speed mode if the host is high-speed-capable. Position  */
#define DEVCTRL_SPDCONF_LOW_POWER     (DEVCTRL_SPDCONF_LOW_POWER_Val << DEVCTRL_SPDCONF_Pos)  /**< (DEVCTRL) For a better consumption, if high speed is not needed. Position  */
#define DEVCTRL_SPDCONF_HIGH_SPEED    (DEVCTRL_SPDCONF_HIGH_SPEED_Val << DEVCTRL_SPDCONF_Pos)  /**< (DEVCTRL) Forced high speed. Position  */
#define DEVCTRL_SPDCONF_FORCED_FS     (DEVCTRL_SPDCONF_FORCED_FS_Val << DEVCTRL_SPDCONF_Pos)  /**< (DEVCTRL) The peripheral remains in Full-speed mode whatever the host speed capability. Position  */
#define DEVCTRL_LS_Pos                12                                             /**< (DEVCTRL) Low-Speed Mode Force Position */
#define DEVCTRL_LS                    (_U_(0x1) << DEVCTRL_LS_Pos)             /**< (DEVCTRL) Low-Speed Mode Force Mask */
#define DEVCTRL_TSTJ_Pos              13                                             /**< (DEVCTRL) Test mode J Position */
#define DEVCTRL_TSTJ                  (_U_(0x1) << DEVCTRL_TSTJ_Pos)           /**< (DEVCTRL) Test mode J Mask */
#define DEVCTRL_TSTK_Pos              14                                             /**< (DEVCTRL) Test mode K Position */
#define DEVCTRL_TSTK                  (_U_(0x1) << DEVCTRL_TSTK_Pos)           /**< (DEVCTRL) Test mode K Mask */
#define DEVCTRL_TSTPCKT_Pos           15                                             /**< (DEVCTRL) Test packet mode Position */
#define DEVCTRL_TSTPCKT               (_U_(0x1) << DEVCTRL_TSTPCKT_Pos)        /**< (DEVCTRL) Test packet mode Mask */
#define DEVCTRL_OPMODE2_Pos           16                                             /**< (DEVCTRL) Specific Operational mode Position */
#define DEVCTRL_OPMODE2               (_U_(0x1) << DEVCTRL_OPMODE2_Pos)        /**< (DEVCTRL) Specific Operational mode Mask */
#define DEVCTRL_Msk                   _U_(0x1FFFF)                                   /**< (DEVCTRL) Register Mask  */

#define DEVCTRL_OPMODE_Pos            16                                             /**< (DEVCTRL Position) Specific Operational mode */
#define DEVCTRL_OPMODE                (_U_(0x1) << DEVCTRL_OPMODE_Pos)         /**< (DEVCTRL Mask) OPMODE */

/* -------- DEVISR : (USBHS Offset: 0x04) (R/ 32) Device Global Interrupt Status Register -------- */

#define DEVISR_OFFSET                 (0x04)                                        /**<  (DEVISR) Device Global Interrupt Status Register  Offset */

#define DEVISR_SUSP_Pos               0                                              /**< (DEVISR) Suspend Interrupt Position */
#define DEVISR_SUSP                   (_U_(0x1) << DEVISR_SUSP_Pos)            /**< (DEVISR) Suspend Interrupt Mask */
#define DEVISR_MSOF_Pos               1                                              /**< (DEVISR) Micro Start of Frame Interrupt Position */
#define DEVISR_MSOF                   (_U_(0x1) << DEVISR_MSOF_Pos)            /**< (DEVISR) Micro Start of Frame Interrupt Mask */
#define DEVISR_SOF_Pos                2                                              /**< (DEVISR) Start of Frame Interrupt Position */
#define DEVISR_SOF                    (_U_(0x1) << DEVISR_SOF_Pos)             /**< (DEVISR) Start of Frame Interrupt Mask */
#define DEVISR_EORST_Pos              3                                              /**< (DEVISR) End of Reset Interrupt Position */
#define DEVISR_EORST                  (_U_(0x1) << DEVISR_EORST_Pos)           /**< (DEVISR) End of Reset Interrupt Mask */
#define DEVISR_WAKEUP_Pos             4                                              /**< (DEVISR) Wake-Up Interrupt Position */
#define DEVISR_WAKEUP                 (_U_(0x1) << DEVISR_WAKEUP_Pos)          /**< (DEVISR) Wake-Up Interrupt Mask */
#define DEVISR_EORSM_Pos              5                                              /**< (DEVISR) End of Resume Interrupt Position */
#define DEVISR_EORSM                  (_U_(0x1) << DEVISR_EORSM_Pos)           /**< (DEVISR) End of Resume Interrupt Mask */
#define DEVISR_UPRSM_Pos              6                                              /**< (DEVISR) Upstream Resume Interrupt Position */
#define DEVISR_UPRSM                  (_U_(0x1) << DEVISR_UPRSM_Pos)           /**< (DEVISR) Upstream Resume Interrupt Mask */
#define DEVISR_PEP_0_Pos              12                                             /**< (DEVISR) Endpoint 0 Interrupt Position */
#define DEVISR_PEP_0                  (_U_(0x1) << DEVISR_PEP_0_Pos)           /**< (DEVISR) Endpoint 0 Interrupt Mask */
#define DEVISR_PEP_1_Pos              13                                             /**< (DEVISR) Endpoint 1 Interrupt Position */
#define DEVISR_PEP_1                  (_U_(0x1) << DEVISR_PEP_1_Pos)           /**< (DEVISR) Endpoint 1 Interrupt Mask */
#define DEVISR_PEP_2_Pos              14                                             /**< (DEVISR) Endpoint 2 Interrupt Position */
#define DEVISR_PEP_2                  (_U_(0x1) << DEVISR_PEP_2_Pos)           /**< (DEVISR) Endpoint 2 Interrupt Mask */
#define DEVISR_PEP_3_Pos              15                                             /**< (DEVISR) Endpoint 3 Interrupt Position */
#define DEVISR_PEP_3                  (_U_(0x1) << DEVISR_PEP_3_Pos)           /**< (DEVISR) Endpoint 3 Interrupt Mask */
#define DEVISR_PEP_4_Pos              16                                             /**< (DEVISR) Endpoint 4 Interrupt Position */
#define DEVISR_PEP_4                  (_U_(0x1) << DEVISR_PEP_4_Pos)           /**< (DEVISR) Endpoint 4 Interrupt Mask */
#define DEVISR_PEP_5_Pos              17                                             /**< (DEVISR) Endpoint 5 Interrupt Position */
#define DEVISR_PEP_5                  (_U_(0x1) << DEVISR_PEP_5_Pos)           /**< (DEVISR) Endpoint 5 Interrupt Mask */
#define DEVISR_PEP_6_Pos              18                                             /**< (DEVISR) Endpoint 6 Interrupt Position */
#define DEVISR_PEP_6                  (_U_(0x1) << DEVISR_PEP_6_Pos)           /**< (DEVISR) Endpoint 6 Interrupt Mask */
#define DEVISR_PEP_7_Pos              19                                             /**< (DEVISR) Endpoint 7 Interrupt Position */
#define DEVISR_PEP_7                  (_U_(0x1) << DEVISR_PEP_7_Pos)           /**< (DEVISR) Endpoint 7 Interrupt Mask */
#define DEVISR_PEP_8_Pos              20                                             /**< (DEVISR) Endpoint 8 Interrupt Position */
#define DEVISR_PEP_8                  (_U_(0x1) << DEVISR_PEP_8_Pos)           /**< (DEVISR) Endpoint 8 Interrupt Mask */
#define DEVISR_PEP_9_Pos              21                                             /**< (DEVISR) Endpoint 9 Interrupt Position */
#define DEVISR_PEP_9                  (_U_(0x1) << DEVISR_PEP_9_Pos)           /**< (DEVISR) Endpoint 9 Interrupt Mask */
#define DEVISR_DMA_1_Pos              25                                             /**< (DEVISR) DMA Channel 1 Interrupt Position */
#define DEVISR_DMA_1                  (_U_(0x1) << DEVISR_DMA_1_Pos)           /**< (DEVISR) DMA Channel 1 Interrupt Mask */
#define DEVISR_DMA_2_Pos              26                                             /**< (DEVISR) DMA Channel 2 Interrupt Position */
#define DEVISR_DMA_2                  (_U_(0x1) << DEVISR_DMA_2_Pos)           /**< (DEVISR) DMA Channel 2 Interrupt Mask */
#define DEVISR_DMA_3_Pos              27                                             /**< (DEVISR) DMA Channel 3 Interrupt Position */
#define DEVISR_DMA_3                  (_U_(0x1) << DEVISR_DMA_3_Pos)           /**< (DEVISR) DMA Channel 3 Interrupt Mask */
#define DEVISR_DMA_4_Pos              28                                             /**< (DEVISR) DMA Channel 4 Interrupt Position */
#define DEVISR_DMA_4                  (_U_(0x1) << DEVISR_DMA_4_Pos)           /**< (DEVISR) DMA Channel 4 Interrupt Mask */
#define DEVISR_DMA_5_Pos              29                                             /**< (DEVISR) DMA Channel 5 Interrupt Position */
#define DEVISR_DMA_5                  (_U_(0x1) << DEVISR_DMA_5_Pos)           /**< (DEVISR) DMA Channel 5 Interrupt Mask */
#define DEVISR_DMA_6_Pos              30                                             /**< (DEVISR) DMA Channel 6 Interrupt Position */
#define DEVISR_DMA_6                  (_U_(0x1) << DEVISR_DMA_6_Pos)           /**< (DEVISR) DMA Channel 6 Interrupt Mask */
#define DEVISR_DMA_7_Pos              31                                             /**< (DEVISR) DMA Channel 7 Interrupt Position */
#define DEVISR_DMA_7                  (_U_(0x1) << DEVISR_DMA_7_Pos)           /**< (DEVISR) DMA Channel 7 Interrupt Mask */
#define DEVISR_Msk                    _U_(0xFE3FF07F)                                /**< (DEVISR) Register Mask  */

#define DEVISR_PEP__Pos               12                                             /**< (DEVISR Position) Endpoint x Interrupt */
#define DEVISR_PEP_                   (_U_(0x3FF) << DEVISR_PEP__Pos)          /**< (DEVISR Mask) PEP_ */
#define DEVISR_DMA__Pos               25                                             /**< (DEVISR Position) DMA Channel 7 Interrupt */
#define DEVISR_DMA_                   (_U_(0x7F) << DEVISR_DMA__Pos)           /**< (DEVISR Mask) DMA_ */

/* -------- DEVICR : (USBHS Offset: 0x08) (/W 32) Device Global Interrupt Clear Register -------- */

#define DEVICR_OFFSET                 (0x08)                                        /**<  (DEVICR) Device Global Interrupt Clear Register  Offset */

#define DEVICR_SUSPC_Pos              0                                              /**< (DEVICR) Suspend Interrupt Clear Position */
#define DEVICR_SUSPC                  (_U_(0x1) << DEVICR_SUSPC_Pos)           /**< (DEVICR) Suspend Interrupt Clear Mask */
#define DEVICR_MSOFC_Pos              1                                              /**< (DEVICR) Micro Start of Frame Interrupt Clear Position */
#define DEVICR_MSOFC                  (_U_(0x1) << DEVICR_MSOFC_Pos)           /**< (DEVICR) Micro Start of Frame Interrupt Clear Mask */
#define DEVICR_SOFC_Pos               2                                              /**< (DEVICR) Start of Frame Interrupt Clear Position */
#define DEVICR_SOFC                   (_U_(0x1) << DEVICR_SOFC_Pos)            /**< (DEVICR) Start of Frame Interrupt Clear Mask */
#define DEVICR_EORSTC_Pos             3                                              /**< (DEVICR) End of Reset Interrupt Clear Position */
#define DEVICR_EORSTC                 (_U_(0x1) << DEVICR_EORSTC_Pos)          /**< (DEVICR) End of Reset Interrupt Clear Mask */
#define DEVICR_WAKEUPC_Pos            4                                              /**< (DEVICR) Wake-Up Interrupt Clear Position */
#define DEVICR_WAKEUPC                (_U_(0x1) << DEVICR_WAKEUPC_Pos)         /**< (DEVICR) Wake-Up Interrupt Clear Mask */
#define DEVICR_EORSMC_Pos             5                                              /**< (DEVICR) End of Resume Interrupt Clear Position */
#define DEVICR_EORSMC                 (_U_(0x1) << DEVICR_EORSMC_Pos)          /**< (DEVICR) End of Resume Interrupt Clear Mask */
#define DEVICR_UPRSMC_Pos             6                                              /**< (DEVICR) Upstream Resume Interrupt Clear Position */
#define DEVICR_UPRSMC                 (_U_(0x1) << DEVICR_UPRSMC_Pos)          /**< (DEVICR) Upstream Resume Interrupt Clear Mask */
#define DEVICR_Msk                    _U_(0x7F)                                      /**< (DEVICR) Register Mask  */


/* -------- DEVIFR : (USBHS Offset: 0x0c) (/W 32) Device Global Interrupt Set Register -------- */

#define DEVIFR_OFFSET                 (0x0C)                                        /**<  (DEVIFR) Device Global Interrupt Set Register  Offset */

#define DEVIFR_SUSPS_Pos              0                                              /**< (DEVIFR) Suspend Interrupt Set Position */
#define DEVIFR_SUSPS                  (_U_(0x1) << DEVIFR_SUSPS_Pos)           /**< (DEVIFR) Suspend Interrupt Set Mask */
#define DEVIFR_MSOFS_Pos              1                                              /**< (DEVIFR) Micro Start of Frame Interrupt Set Position */
#define DEVIFR_MSOFS                  (_U_(0x1) << DEVIFR_MSOFS_Pos)           /**< (DEVIFR) Micro Start of Frame Interrupt Set Mask */
#define DEVIFR_SOFS_Pos               2                                              /**< (DEVIFR) Start of Frame Interrupt Set Position */
#define DEVIFR_SOFS                   (_U_(0x1) << DEVIFR_SOFS_Pos)            /**< (DEVIFR) Start of Frame Interrupt Set Mask */
#define DEVIFR_EORSTS_Pos             3                                              /**< (DEVIFR) End of Reset Interrupt Set Position */
#define DEVIFR_EORSTS                 (_U_(0x1) << DEVIFR_EORSTS_Pos)          /**< (DEVIFR) End of Reset Interrupt Set Mask */
#define DEVIFR_WAKEUPS_Pos            4                                              /**< (DEVIFR) Wake-Up Interrupt Set Position */
#define DEVIFR_WAKEUPS                (_U_(0x1) << DEVIFR_WAKEUPS_Pos)         /**< (DEVIFR) Wake-Up Interrupt Set Mask */
#define DEVIFR_EORSMS_Pos             5                                              /**< (DEVIFR) End of Resume Interrupt Set Position */
#define DEVIFR_EORSMS                 (_U_(0x1) << DEVIFR_EORSMS_Pos)          /**< (DEVIFR) End of Resume Interrupt Set Mask */
#define DEVIFR_UPRSMS_Pos             6                                              /**< (DEVIFR) Upstream Resume Interrupt Set Position */
#define DEVIFR_UPRSMS                 (_U_(0x1) << DEVIFR_UPRSMS_Pos)          /**< (DEVIFR) Upstream Resume Interrupt Set Mask */
#define DEVIFR_DMA_1_Pos              25                                             /**< (DEVIFR) DMA Channel 1 Interrupt Set Position */
#define DEVIFR_DMA_1                  (_U_(0x1) << DEVIFR_DMA_1_Pos)           /**< (DEVIFR) DMA Channel 1 Interrupt Set Mask */
#define DEVIFR_DMA_2_Pos              26                                             /**< (DEVIFR) DMA Channel 2 Interrupt Set Position */
#define DEVIFR_DMA_2                  (_U_(0x1) << DEVIFR_DMA_2_Pos)           /**< (DEVIFR) DMA Channel 2 Interrupt Set Mask */
#define DEVIFR_DMA_3_Pos              27                                             /**< (DEVIFR) DMA Channel 3 Interrupt Set Position */
#define DEVIFR_DMA_3                  (_U_(0x1) << DEVIFR_DMA_3_Pos)           /**< (DEVIFR) DMA Channel 3 Interrupt Set Mask */
#define DEVIFR_DMA_4_Pos              28                                             /**< (DEVIFR) DMA Channel 4 Interrupt Set Position */
#define DEVIFR_DMA_4                  (_U_(0x1) << DEVIFR_DMA_4_Pos)           /**< (DEVIFR) DMA Channel 4 Interrupt Set Mask */
#define DEVIFR_DMA_5_Pos              29                                             /**< (DEVIFR) DMA Channel 5 Interrupt Set Position */
#define DEVIFR_DMA_5                  (_U_(0x1) << DEVIFR_DMA_5_Pos)           /**< (DEVIFR) DMA Channel 5 Interrupt Set Mask */
#define DEVIFR_DMA_6_Pos              30                                             /**< (DEVIFR) DMA Channel 6 Interrupt Set Position */
#define DEVIFR_DMA_6                  (_U_(0x1) << DEVIFR_DMA_6_Pos)           /**< (DEVIFR) DMA Channel 6 Interrupt Set Mask */
#define DEVIFR_DMA_7_Pos              31                                             /**< (DEVIFR) DMA Channel 7 Interrupt Set Position */
#define DEVIFR_DMA_7                  (_U_(0x1) << DEVIFR_DMA_7_Pos)           /**< (DEVIFR) DMA Channel 7 Interrupt Set Mask */
#define DEVIFR_Msk                    _U_(0xFE00007F)                                /**< (DEVIFR) Register Mask  */

#define DEVIFR_DMA__Pos               25                                             /**< (DEVIFR Position) DMA Channel 7 Interrupt Set */
#define DEVIFR_DMA_                   (_U_(0x7F) << DEVIFR_DMA__Pos)           /**< (DEVIFR Mask) DMA_ */

/* -------- DEVIMR : (USBHS Offset: 0x10) (R/ 32) Device Global Interrupt Mask Register -------- */

#define DEVIMR_OFFSET                 (0x10)                                        /**<  (DEVIMR) Device Global Interrupt Mask Register  Offset */

#define DEVIMR_SUSPE_Pos              0                                              /**< (DEVIMR) Suspend Interrupt Mask Position */
#define DEVIMR_SUSPE                  (_U_(0x1) << DEVIMR_SUSPE_Pos)           /**< (DEVIMR) Suspend Interrupt Mask Mask */
#define DEVIMR_MSOFE_Pos              1                                              /**< (DEVIMR) Micro Start of Frame Interrupt Mask Position */
#define DEVIMR_MSOFE                  (_U_(0x1) << DEVIMR_MSOFE_Pos)           /**< (DEVIMR) Micro Start of Frame Interrupt Mask Mask */
#define DEVIMR_SOFE_Pos               2                                              /**< (DEVIMR) Start of Frame Interrupt Mask Position */
#define DEVIMR_SOFE                   (_U_(0x1) << DEVIMR_SOFE_Pos)            /**< (DEVIMR) Start of Frame Interrupt Mask Mask */
#define DEVIMR_EORSTE_Pos             3                                              /**< (DEVIMR) End of Reset Interrupt Mask Position */
#define DEVIMR_EORSTE                 (_U_(0x1) << DEVIMR_EORSTE_Pos)          /**< (DEVIMR) End of Reset Interrupt Mask Mask */
#define DEVIMR_WAKEUPE_Pos            4                                              /**< (DEVIMR) Wake-Up Interrupt Mask Position */
#define DEVIMR_WAKEUPE                (_U_(0x1) << DEVIMR_WAKEUPE_Pos)         /**< (DEVIMR) Wake-Up Interrupt Mask Mask */
#define DEVIMR_EORSME_Pos             5                                              /**< (DEVIMR) End of Resume Interrupt Mask Position */
#define DEVIMR_EORSME                 (_U_(0x1) << DEVIMR_EORSME_Pos)          /**< (DEVIMR) End of Resume Interrupt Mask Mask */
#define DEVIMR_UPRSME_Pos             6                                              /**< (DEVIMR) Upstream Resume Interrupt Mask Position */
#define DEVIMR_UPRSME                 (_U_(0x1) << DEVIMR_UPRSME_Pos)          /**< (DEVIMR) Upstream Resume Interrupt Mask Mask */
#define DEVIMR_PEP_0_Pos              12                                             /**< (DEVIMR) Endpoint 0 Interrupt Mask Position */
#define DEVIMR_PEP_0                  (_U_(0x1) << DEVIMR_PEP_0_Pos)           /**< (DEVIMR) Endpoint 0 Interrupt Mask Mask */
#define DEVIMR_PEP_1_Pos              13                                             /**< (DEVIMR) Endpoint 1 Interrupt Mask Position */
#define DEVIMR_PEP_1                  (_U_(0x1) << DEVIMR_PEP_1_Pos)           /**< (DEVIMR) Endpoint 1 Interrupt Mask Mask */
#define DEVIMR_PEP_2_Pos              14                                             /**< (DEVIMR) Endpoint 2 Interrupt Mask Position */
#define DEVIMR_PEP_2                  (_U_(0x1) << DEVIMR_PEP_2_Pos)           /**< (DEVIMR) Endpoint 2 Interrupt Mask Mask */
#define DEVIMR_PEP_3_Pos              15                                             /**< (DEVIMR) Endpoint 3 Interrupt Mask Position */
#define DEVIMR_PEP_3                  (_U_(0x1) << DEVIMR_PEP_3_Pos)           /**< (DEVIMR) Endpoint 3 Interrupt Mask Mask */
#define DEVIMR_PEP_4_Pos              16                                             /**< (DEVIMR) Endpoint 4 Interrupt Mask Position */
#define DEVIMR_PEP_4                  (_U_(0x1) << DEVIMR_PEP_4_Pos)           /**< (DEVIMR) Endpoint 4 Interrupt Mask Mask */
#define DEVIMR_PEP_5_Pos              17                                             /**< (DEVIMR) Endpoint 5 Interrupt Mask Position */
#define DEVIMR_PEP_5                  (_U_(0x1) << DEVIMR_PEP_5_Pos)           /**< (DEVIMR) Endpoint 5 Interrupt Mask Mask */
#define DEVIMR_PEP_6_Pos              18                                             /**< (DEVIMR) Endpoint 6 Interrupt Mask Position */
#define DEVIMR_PEP_6                  (_U_(0x1) << DEVIMR_PEP_6_Pos)           /**< (DEVIMR) Endpoint 6 Interrupt Mask Mask */
#define DEVIMR_PEP_7_Pos              19                                             /**< (DEVIMR) Endpoint 7 Interrupt Mask Position */
#define DEVIMR_PEP_7                  (_U_(0x1) << DEVIMR_PEP_7_Pos)           /**< (DEVIMR) Endpoint 7 Interrupt Mask Mask */
#define DEVIMR_PEP_8_Pos              20                                             /**< (DEVIMR) Endpoint 8 Interrupt Mask Position */
#define DEVIMR_PEP_8                  (_U_(0x1) << DEVIMR_PEP_8_Pos)           /**< (DEVIMR) Endpoint 8 Interrupt Mask Mask */
#define DEVIMR_PEP_9_Pos              21                                             /**< (DEVIMR) Endpoint 9 Interrupt Mask Position */
#define DEVIMR_PEP_9                  (_U_(0x1) << DEVIMR_PEP_9_Pos)           /**< (DEVIMR) Endpoint 9 Interrupt Mask Mask */
#define DEVIMR_DMA_1_Pos              25                                             /**< (DEVIMR) DMA Channel 1 Interrupt Mask Position */
#define DEVIMR_DMA_1                  (_U_(0x1) << DEVIMR_DMA_1_Pos)           /**< (DEVIMR) DMA Channel 1 Interrupt Mask Mask */
#define DEVIMR_DMA_2_Pos              26                                             /**< (DEVIMR) DMA Channel 2 Interrupt Mask Position */
#define DEVIMR_DMA_2                  (_U_(0x1) << DEVIMR_DMA_2_Pos)           /**< (DEVIMR) DMA Channel 2 Interrupt Mask Mask */
#define DEVIMR_DMA_3_Pos              27                                             /**< (DEVIMR) DMA Channel 3 Interrupt Mask Position */
#define DEVIMR_DMA_3                  (_U_(0x1) << DEVIMR_DMA_3_Pos)           /**< (DEVIMR) DMA Channel 3 Interrupt Mask Mask */
#define DEVIMR_DMA_4_Pos              28                                             /**< (DEVIMR) DMA Channel 4 Interrupt Mask Position */
#define DEVIMR_DMA_4                  (_U_(0x1) << DEVIMR_DMA_4_Pos)           /**< (DEVIMR) DMA Channel 4 Interrupt Mask Mask */
#define DEVIMR_DMA_5_Pos              29                                             /**< (DEVIMR) DMA Channel 5 Interrupt Mask Position */
#define DEVIMR_DMA_5                  (_U_(0x1) << DEVIMR_DMA_5_Pos)           /**< (DEVIMR) DMA Channel 5 Interrupt Mask Mask */
#define DEVIMR_DMA_6_Pos              30                                             /**< (DEVIMR) DMA Channel 6 Interrupt Mask Position */
#define DEVIMR_DMA_6                  (_U_(0x1) << DEVIMR_DMA_6_Pos)           /**< (DEVIMR) DMA Channel 6 Interrupt Mask Mask */
#define DEVIMR_DMA_7_Pos              31                                             /**< (DEVIMR) DMA Channel 7 Interrupt Mask Position */
#define DEVIMR_DMA_7                  (_U_(0x1) << DEVIMR_DMA_7_Pos)           /**< (DEVIMR) DMA Channel 7 Interrupt Mask Mask */
#define DEVIMR_Msk                    _U_(0xFE3FF07F)                                /**< (DEVIMR) Register Mask  */

#define DEVIMR_PEP__Pos               12                                             /**< (DEVIMR Position) Endpoint x Interrupt Mask */
#define DEVIMR_PEP_                   (_U_(0x3FF) << DEVIMR_PEP__Pos)          /**< (DEVIMR Mask) PEP_ */
#define DEVIMR_DMA__Pos               25                                             /**< (DEVIMR Position) DMA Channel 7 Interrupt Mask */
#define DEVIMR_DMA_                   (_U_(0x7F) << DEVIMR_DMA__Pos)           /**< (DEVIMR Mask) DMA_ */

/* -------- DEVIDR : (USBHS Offset: 0x14) (/W 32) Device Global Interrupt Disable Register -------- */

#define DEVIDR_OFFSET                 (0x14)                                        /**<  (DEVIDR) Device Global Interrupt Disable Register  Offset */

#define DEVIDR_SUSPEC_Pos             0                                              /**< (DEVIDR) Suspend Interrupt Disable Position */
#define DEVIDR_SUSPEC                 (_U_(0x1) << DEVIDR_SUSPEC_Pos)          /**< (DEVIDR) Suspend Interrupt Disable Mask */
#define DEVIDR_MSOFEC_Pos             1                                              /**< (DEVIDR) Micro Start of Frame Interrupt Disable Position */
#define DEVIDR_MSOFEC                 (_U_(0x1) << DEVIDR_MSOFEC_Pos)          /**< (DEVIDR) Micro Start of Frame Interrupt Disable Mask */
#define DEVIDR_SOFEC_Pos              2                                              /**< (DEVIDR) Start of Frame Interrupt Disable Position */
#define DEVIDR_SOFEC                  (_U_(0x1) << DEVIDR_SOFEC_Pos)           /**< (DEVIDR) Start of Frame Interrupt Disable Mask */
#define DEVIDR_EORSTEC_Pos            3                                              /**< (DEVIDR) End of Reset Interrupt Disable Position */
#define DEVIDR_EORSTEC                (_U_(0x1) << DEVIDR_EORSTEC_Pos)         /**< (DEVIDR) End of Reset Interrupt Disable Mask */
#define DEVIDR_WAKEUPEC_Pos           4                                              /**< (DEVIDR) Wake-Up Interrupt Disable Position */
#define DEVIDR_WAKEUPEC               (_U_(0x1) << DEVIDR_WAKEUPEC_Pos)        /**< (DEVIDR) Wake-Up Interrupt Disable Mask */
#define DEVIDR_EORSMEC_Pos            5                                              /**< (DEVIDR) End of Resume Interrupt Disable Position */
#define DEVIDR_EORSMEC                (_U_(0x1) << DEVIDR_EORSMEC_Pos)         /**< (DEVIDR) End of Resume Interrupt Disable Mask */
#define DEVIDR_UPRSMEC_Pos            6                                              /**< (DEVIDR) Upstream Resume Interrupt Disable Position */
#define DEVIDR_UPRSMEC                (_U_(0x1) << DEVIDR_UPRSMEC_Pos)         /**< (DEVIDR) Upstream Resume Interrupt Disable Mask */
#define DEVIDR_PEP_0_Pos              12                                             /**< (DEVIDR) Endpoint 0 Interrupt Disable Position */
#define DEVIDR_PEP_0                  (_U_(0x1) << DEVIDR_PEP_0_Pos)           /**< (DEVIDR) Endpoint 0 Interrupt Disable Mask */
#define DEVIDR_PEP_1_Pos              13                                             /**< (DEVIDR) Endpoint 1 Interrupt Disable Position */
#define DEVIDR_PEP_1                  (_U_(0x1) << DEVIDR_PEP_1_Pos)           /**< (DEVIDR) Endpoint 1 Interrupt Disable Mask */
#define DEVIDR_PEP_2_Pos              14                                             /**< (DEVIDR) Endpoint 2 Interrupt Disable Position */
#define DEVIDR_PEP_2                  (_U_(0x1) << DEVIDR_PEP_2_Pos)           /**< (DEVIDR) Endpoint 2 Interrupt Disable Mask */
#define DEVIDR_PEP_3_Pos              15                                             /**< (DEVIDR) Endpoint 3 Interrupt Disable Position */
#define DEVIDR_PEP_3                  (_U_(0x1) << DEVIDR_PEP_3_Pos)           /**< (DEVIDR) Endpoint 3 Interrupt Disable Mask */
#define DEVIDR_PEP_4_Pos              16                                             /**< (DEVIDR) Endpoint 4 Interrupt Disable Position */
#define DEVIDR_PEP_4                  (_U_(0x1) << DEVIDR_PEP_4_Pos)           /**< (DEVIDR) Endpoint 4 Interrupt Disable Mask */
#define DEVIDR_PEP_5_Pos              17                                             /**< (DEVIDR) Endpoint 5 Interrupt Disable Position */
#define DEVIDR_PEP_5                  (_U_(0x1) << DEVIDR_PEP_5_Pos)           /**< (DEVIDR) Endpoint 5 Interrupt Disable Mask */
#define DEVIDR_PEP_6_Pos              18                                             /**< (DEVIDR) Endpoint 6 Interrupt Disable Position */
#define DEVIDR_PEP_6                  (_U_(0x1) << DEVIDR_PEP_6_Pos)           /**< (DEVIDR) Endpoint 6 Interrupt Disable Mask */
#define DEVIDR_PEP_7_Pos              19                                             /**< (DEVIDR) Endpoint 7 Interrupt Disable Position */
#define DEVIDR_PEP_7                  (_U_(0x1) << DEVIDR_PEP_7_Pos)           /**< (DEVIDR) Endpoint 7 Interrupt Disable Mask */
#define DEVIDR_PEP_8_Pos              20                                             /**< (DEVIDR) Endpoint 8 Interrupt Disable Position */
#define DEVIDR_PEP_8                  (_U_(0x1) << DEVIDR_PEP_8_Pos)           /**< (DEVIDR) Endpoint 8 Interrupt Disable Mask */
#define DEVIDR_PEP_9_Pos              21                                             /**< (DEVIDR) Endpoint 9 Interrupt Disable Position */
#define DEVIDR_PEP_9                  (_U_(0x1) << DEVIDR_PEP_9_Pos)           /**< (DEVIDR) Endpoint 9 Interrupt Disable Mask */
#define DEVIDR_DMA_1_Pos              25                                             /**< (DEVIDR) DMA Channel 1 Interrupt Disable Position */
#define DEVIDR_DMA_1                  (_U_(0x1) << DEVIDR_DMA_1_Pos)           /**< (DEVIDR) DMA Channel 1 Interrupt Disable Mask */
#define DEVIDR_DMA_2_Pos              26                                             /**< (DEVIDR) DMA Channel 2 Interrupt Disable Position */
#define DEVIDR_DMA_2                  (_U_(0x1) << DEVIDR_DMA_2_Pos)           /**< (DEVIDR) DMA Channel 2 Interrupt Disable Mask */
#define DEVIDR_DMA_3_Pos              27                                             /**< (DEVIDR) DMA Channel 3 Interrupt Disable Position */
#define DEVIDR_DMA_3                  (_U_(0x1) << DEVIDR_DMA_3_Pos)           /**< (DEVIDR) DMA Channel 3 Interrupt Disable Mask */
#define DEVIDR_DMA_4_Pos              28                                             /**< (DEVIDR) DMA Channel 4 Interrupt Disable Position */
#define DEVIDR_DMA_4                  (_U_(0x1) << DEVIDR_DMA_4_Pos)           /**< (DEVIDR) DMA Channel 4 Interrupt Disable Mask */
#define DEVIDR_DMA_5_Pos              29                                             /**< (DEVIDR) DMA Channel 5 Interrupt Disable Position */
#define DEVIDR_DMA_5                  (_U_(0x1) << DEVIDR_DMA_5_Pos)           /**< (DEVIDR) DMA Channel 5 Interrupt Disable Mask */
#define DEVIDR_DMA_6_Pos              30                                             /**< (DEVIDR) DMA Channel 6 Interrupt Disable Position */
#define DEVIDR_DMA_6                  (_U_(0x1) << DEVIDR_DMA_6_Pos)           /**< (DEVIDR) DMA Channel 6 Interrupt Disable Mask */
#define DEVIDR_DMA_7_Pos              31                                             /**< (DEVIDR) DMA Channel 7 Interrupt Disable Position */
#define DEVIDR_DMA_7                  (_U_(0x1) << DEVIDR_DMA_7_Pos)           /**< (DEVIDR) DMA Channel 7 Interrupt Disable Mask */
#define DEVIDR_Msk                    _U_(0xFE3FF07F)                                /**< (DEVIDR) Register Mask  */

#define DEVIDR_PEP__Pos               12                                             /**< (DEVIDR Position) Endpoint x Interrupt Disable */
#define DEVIDR_PEP_                   (_U_(0x3FF) << DEVIDR_PEP__Pos)          /**< (DEVIDR Mask) PEP_ */
#define DEVIDR_DMA__Pos               25                                             /**< (DEVIDR Position) DMA Channel 7 Interrupt Disable */
#define DEVIDR_DMA_                   (_U_(0x7F) << DEVIDR_DMA__Pos)           /**< (DEVIDR Mask) DMA_ */

/* -------- DEVIER : (USBHS Offset: 0x18) (/W 32) Device Global Interrupt Enable Register -------- */

#define DEVIER_OFFSET                 (0x18)                                        /**<  (DEVIER) Device Global Interrupt Enable Register  Offset */

#define DEVIER_SUSPES_Pos             0                                              /**< (DEVIER) Suspend Interrupt Enable Position */
#define DEVIER_SUSPES                 (_U_(0x1) << DEVIER_SUSPES_Pos)          /**< (DEVIER) Suspend Interrupt Enable Mask */
#define DEVIER_MSOFES_Pos             1                                              /**< (DEVIER) Micro Start of Frame Interrupt Enable Position */
#define DEVIER_MSOFES                 (_U_(0x1) << DEVIER_MSOFES_Pos)          /**< (DEVIER) Micro Start of Frame Interrupt Enable Mask */
#define DEVIER_SOFES_Pos              2                                              /**< (DEVIER) Start of Frame Interrupt Enable Position */
#define DEVIER_SOFES                  (_U_(0x1) << DEVIER_SOFES_Pos)           /**< (DEVIER) Start of Frame Interrupt Enable Mask */
#define DEVIER_EORSTES_Pos            3                                              /**< (DEVIER) End of Reset Interrupt Enable Position */
#define DEVIER_EORSTES                (_U_(0x1) << DEVIER_EORSTES_Pos)         /**< (DEVIER) End of Reset Interrupt Enable Mask */
#define DEVIER_WAKEUPES_Pos           4                                              /**< (DEVIER) Wake-Up Interrupt Enable Position */
#define DEVIER_WAKEUPES               (_U_(0x1) << DEVIER_WAKEUPES_Pos)        /**< (DEVIER) Wake-Up Interrupt Enable Mask */
#define DEVIER_EORSMES_Pos            5                                              /**< (DEVIER) End of Resume Interrupt Enable Position */
#define DEVIER_EORSMES                (_U_(0x1) << DEVIER_EORSMES_Pos)         /**< (DEVIER) End of Resume Interrupt Enable Mask */
#define DEVIER_UPRSMES_Pos            6                                              /**< (DEVIER) Upstream Resume Interrupt Enable Position */
#define DEVIER_UPRSMES                (_U_(0x1) << DEVIER_UPRSMES_Pos)         /**< (DEVIER) Upstream Resume Interrupt Enable Mask */
#define DEVIER_PEP_0_Pos              12                                             /**< (DEVIER) Endpoint 0 Interrupt Enable Position */
#define DEVIER_PEP_0                  (_U_(0x1) << DEVIER_PEP_0_Pos)           /**< (DEVIER) Endpoint 0 Interrupt Enable Mask */
#define DEVIER_PEP_1_Pos              13                                             /**< (DEVIER) Endpoint 1 Interrupt Enable Position */
#define DEVIER_PEP_1                  (_U_(0x1) << DEVIER_PEP_1_Pos)           /**< (DEVIER) Endpoint 1 Interrupt Enable Mask */
#define DEVIER_PEP_2_Pos              14                                             /**< (DEVIER) Endpoint 2 Interrupt Enable Position */
#define DEVIER_PEP_2                  (_U_(0x1) << DEVIER_PEP_2_Pos)           /**< (DEVIER) Endpoint 2 Interrupt Enable Mask */
#define DEVIER_PEP_3_Pos              15                                             /**< (DEVIER) Endpoint 3 Interrupt Enable Position */
#define DEVIER_PEP_3                  (_U_(0x1) << DEVIER_PEP_3_Pos)           /**< (DEVIER) Endpoint 3 Interrupt Enable Mask */
#define DEVIER_PEP_4_Pos              16                                             /**< (DEVIER) Endpoint 4 Interrupt Enable Position */
#define DEVIER_PEP_4                  (_U_(0x1) << DEVIER_PEP_4_Pos)           /**< (DEVIER) Endpoint 4 Interrupt Enable Mask */
#define DEVIER_PEP_5_Pos              17                                             /**< (DEVIER) Endpoint 5 Interrupt Enable Position */
#define DEVIER_PEP_5                  (_U_(0x1) << DEVIER_PEP_5_Pos)           /**< (DEVIER) Endpoint 5 Interrupt Enable Mask */
#define DEVIER_PEP_6_Pos              18                                             /**< (DEVIER) Endpoint 6 Interrupt Enable Position */
#define DEVIER_PEP_6                  (_U_(0x1) << DEVIER_PEP_6_Pos)           /**< (DEVIER) Endpoint 6 Interrupt Enable Mask */
#define DEVIER_PEP_7_Pos              19                                             /**< (DEVIER) Endpoint 7 Interrupt Enable Position */
#define DEVIER_PEP_7                  (_U_(0x1) << DEVIER_PEP_7_Pos)           /**< (DEVIER) Endpoint 7 Interrupt Enable Mask */
#define DEVIER_PEP_8_Pos              20                                             /**< (DEVIER) Endpoint 8 Interrupt Enable Position */
#define DEVIER_PEP_8                  (_U_(0x1) << DEVIER_PEP_8_Pos)           /**< (DEVIER) Endpoint 8 Interrupt Enable Mask */
#define DEVIER_PEP_9_Pos              21                                             /**< (DEVIER) Endpoint 9 Interrupt Enable Position */
#define DEVIER_PEP_9                  (_U_(0x1) << DEVIER_PEP_9_Pos)           /**< (DEVIER) Endpoint 9 Interrupt Enable Mask */
#define DEVIER_DMA_1_Pos              25                                             /**< (DEVIER) DMA Channel 1 Interrupt Enable Position */
#define DEVIER_DMA_1                  (_U_(0x1) << DEVIER_DMA_1_Pos)           /**< (DEVIER) DMA Channel 1 Interrupt Enable Mask */
#define DEVIER_DMA_2_Pos              26                                             /**< (DEVIER) DMA Channel 2 Interrupt Enable Position */
#define DEVIER_DMA_2                  (_U_(0x1) << DEVIER_DMA_2_Pos)           /**< (DEVIER) DMA Channel 2 Interrupt Enable Mask */
#define DEVIER_DMA_3_Pos              27                                             /**< (DEVIER) DMA Channel 3 Interrupt Enable Position */
#define DEVIER_DMA_3                  (_U_(0x1) << DEVIER_DMA_3_Pos)           /**< (DEVIER) DMA Channel 3 Interrupt Enable Mask */
#define DEVIER_DMA_4_Pos              28                                             /**< (DEVIER) DMA Channel 4 Interrupt Enable Position */
#define DEVIER_DMA_4                  (_U_(0x1) << DEVIER_DMA_4_Pos)           /**< (DEVIER) DMA Channel 4 Interrupt Enable Mask */
#define DEVIER_DMA_5_Pos              29                                             /**< (DEVIER) DMA Channel 5 Interrupt Enable Position */
#define DEVIER_DMA_5                  (_U_(0x1) << DEVIER_DMA_5_Pos)           /**< (DEVIER) DMA Channel 5 Interrupt Enable Mask */
#define DEVIER_DMA_6_Pos              30                                             /**< (DEVIER) DMA Channel 6 Interrupt Enable Position */
#define DEVIER_DMA_6                  (_U_(0x1) << DEVIER_DMA_6_Pos)           /**< (DEVIER) DMA Channel 6 Interrupt Enable Mask */
#define DEVIER_DMA_7_Pos              31                                             /**< (DEVIER) DMA Channel 7 Interrupt Enable Position */
#define DEVIER_DMA_7                  (_U_(0x1) << DEVIER_DMA_7_Pos)           /**< (DEVIER) DMA Channel 7 Interrupt Enable Mask */
#define DEVIER_Msk                    _U_(0xFE3FF07F)                                /**< (DEVIER) Register Mask  */

#define DEVIER_PEP__Pos               12                                             /**< (DEVIER Position) Endpoint x Interrupt Enable */
#define DEVIER_PEP_                   (_U_(0x3FF) << DEVIER_PEP__Pos)          /**< (DEVIER Mask) PEP_ */
#define DEVIER_DMA__Pos               25                                             /**< (DEVIER Position) DMA Channel 7 Interrupt Enable */
#define DEVIER_DMA_                   (_U_(0x7F) << DEVIER_DMA__Pos)           /**< (DEVIER Mask) DMA_ */

/* -------- DEVEPT : (USBHS Offset: 0x1c) (R/W 32) Device Endpoint Register -------- */

#define DEVEPT_OFFSET                 (0x1C)                                        /**<  (DEVEPT) Device Endpoint Register  Offset */

#define DEVEPT_EPEN0_Pos              0                                              /**< (DEVEPT) Endpoint 0 Enable Position */
#define DEVEPT_EPEN0                  (_U_(0x1) << DEVEPT_EPEN0_Pos)           /**< (DEVEPT) Endpoint 0 Enable Mask */
#define DEVEPT_EPEN1_Pos              1                                              /**< (DEVEPT) Endpoint 1 Enable Position */
#define DEVEPT_EPEN1                  (_U_(0x1) << DEVEPT_EPEN1_Pos)           /**< (DEVEPT) Endpoint 1 Enable Mask */
#define DEVEPT_EPEN2_Pos              2                                              /**< (DEVEPT) Endpoint 2 Enable Position */
#define DEVEPT_EPEN2                  (_U_(0x1) << DEVEPT_EPEN2_Pos)           /**< (DEVEPT) Endpoint 2 Enable Mask */
#define DEVEPT_EPEN3_Pos              3                                              /**< (DEVEPT) Endpoint 3 Enable Position */
#define DEVEPT_EPEN3                  (_U_(0x1) << DEVEPT_EPEN3_Pos)           /**< (DEVEPT) Endpoint 3 Enable Mask */
#define DEVEPT_EPEN4_Pos              4                                              /**< (DEVEPT) Endpoint 4 Enable Position */
#define DEVEPT_EPEN4                  (_U_(0x1) << DEVEPT_EPEN4_Pos)           /**< (DEVEPT) Endpoint 4 Enable Mask */
#define DEVEPT_EPEN5_Pos              5                                              /**< (DEVEPT) Endpoint 5 Enable Position */
#define DEVEPT_EPEN5                  (_U_(0x1) << DEVEPT_EPEN5_Pos)           /**< (DEVEPT) Endpoint 5 Enable Mask */
#define DEVEPT_EPEN6_Pos              6                                              /**< (DEVEPT) Endpoint 6 Enable Position */
#define DEVEPT_EPEN6                  (_U_(0x1) << DEVEPT_EPEN6_Pos)           /**< (DEVEPT) Endpoint 6 Enable Mask */
#define DEVEPT_EPEN7_Pos              7                                              /**< (DEVEPT) Endpoint 7 Enable Position */
#define DEVEPT_EPEN7                  (_U_(0x1) << DEVEPT_EPEN7_Pos)           /**< (DEVEPT) Endpoint 7 Enable Mask */
#define DEVEPT_EPEN8_Pos              8                                              /**< (DEVEPT) Endpoint 8 Enable Position */
#define DEVEPT_EPEN8                  (_U_(0x1) << DEVEPT_EPEN8_Pos)           /**< (DEVEPT) Endpoint 8 Enable Mask */
#define DEVEPT_EPEN9_Pos              9                                              /**< (DEVEPT) Endpoint 9 Enable Position */
#define DEVEPT_EPEN9                  (_U_(0x1) << DEVEPT_EPEN9_Pos)           /**< (DEVEPT) Endpoint 9 Enable Mask */
#define DEVEPT_EPRST0_Pos             16                                             /**< (DEVEPT) Endpoint 0 Reset Position */
#define DEVEPT_EPRST0                 (_U_(0x1) << DEVEPT_EPRST0_Pos)          /**< (DEVEPT) Endpoint 0 Reset Mask */
#define DEVEPT_EPRST1_Pos             17                                             /**< (DEVEPT) Endpoint 1 Reset Position */
#define DEVEPT_EPRST1                 (_U_(0x1) << DEVEPT_EPRST1_Pos)          /**< (DEVEPT) Endpoint 1 Reset Mask */
#define DEVEPT_EPRST2_Pos             18                                             /**< (DEVEPT) Endpoint 2 Reset Position */
#define DEVEPT_EPRST2                 (_U_(0x1) << DEVEPT_EPRST2_Pos)          /**< (DEVEPT) Endpoint 2 Reset Mask */
#define DEVEPT_EPRST3_Pos             19                                             /**< (DEVEPT) Endpoint 3 Reset Position */
#define DEVEPT_EPRST3                 (_U_(0x1) << DEVEPT_EPRST3_Pos)          /**< (DEVEPT) Endpoint 3 Reset Mask */
#define DEVEPT_EPRST4_Pos             20                                             /**< (DEVEPT) Endpoint 4 Reset Position */
#define DEVEPT_EPRST4                 (_U_(0x1) << DEVEPT_EPRST4_Pos)          /**< (DEVEPT) Endpoint 4 Reset Mask */
#define DEVEPT_EPRST5_Pos             21                                             /**< (DEVEPT) Endpoint 5 Reset Position */
#define DEVEPT_EPRST5                 (_U_(0x1) << DEVEPT_EPRST5_Pos)          /**< (DEVEPT) Endpoint 5 Reset Mask */
#define DEVEPT_EPRST6_Pos             22                                             /**< (DEVEPT) Endpoint 6 Reset Position */
#define DEVEPT_EPRST6                 (_U_(0x1) << DEVEPT_EPRST6_Pos)          /**< (DEVEPT) Endpoint 6 Reset Mask */
#define DEVEPT_EPRST7_Pos             23                                             /**< (DEVEPT) Endpoint 7 Reset Position */
#define DEVEPT_EPRST7                 (_U_(0x1) << DEVEPT_EPRST7_Pos)          /**< (DEVEPT) Endpoint 7 Reset Mask */
#define DEVEPT_EPRST8_Pos             24                                             /**< (DEVEPT) Endpoint 8 Reset Position */
#define DEVEPT_EPRST8                 (_U_(0x1) << DEVEPT_EPRST8_Pos)          /**< (DEVEPT) Endpoint 8 Reset Mask */
#define DEVEPT_EPRST9_Pos             25                                             /**< (DEVEPT) Endpoint 9 Reset Position */
#define DEVEPT_EPRST9                 (_U_(0x1) << DEVEPT_EPRST9_Pos)          /**< (DEVEPT) Endpoint 9 Reset Mask */
#define DEVEPT_Msk                    _U_(0x3FF03FF)                                 /**< (DEVEPT) Register Mask  */

#define DEVEPT_EPEN_Pos               0                                              /**< (DEVEPT Position) Endpoint x Enable */
#define DEVEPT_EPEN                   (_U_(0x3FF) << DEVEPT_EPEN_Pos)          /**< (DEVEPT Mask) EPEN */
#define DEVEPT_EPRST_Pos              16                                             /**< (DEVEPT Position) Endpoint 9 Reset */
#define DEVEPT_EPRST                  (_U_(0x3FF) << DEVEPT_EPRST_Pos)         /**< (DEVEPT Mask) EPRST */

/* -------- DEVFNUM : (USBHS Offset: 0x20) (R/ 32) Device Frame Number Register -------- */

#define DEVFNUM_OFFSET                (0x20)                                        /**<  (DEVFNUM) Device Frame Number Register  Offset */

#define DEVFNUM_MFNUM_Pos             0                                              /**< (DEVFNUM) Micro Frame Number Position */
#define DEVFNUM_MFNUM                 (_U_(0x7) << DEVFNUM_MFNUM_Pos)          /**< (DEVFNUM) Micro Frame Number Mask */
#define DEVFNUM_FNUM_Pos              3                                              /**< (DEVFNUM) Frame Number Position */
#define DEVFNUM_FNUM                  (_U_(0x7FF) << DEVFNUM_FNUM_Pos)         /**< (DEVFNUM) Frame Number Mask */
#define DEVFNUM_FNCERR_Pos            15                                             /**< (DEVFNUM) Frame Number CRC Error Position */
#define DEVFNUM_FNCERR                (_U_(0x1) << DEVFNUM_FNCERR_Pos)         /**< (DEVFNUM) Frame Number CRC Error Mask */
#define DEVFNUM_Msk                   _U_(0xBFFF)                                    /**< (DEVFNUM) Register Mask  */


/* -------- DEVEPTCFG : (USBHS Offset: 0x100) (R/W 32) Device Endpoint Configuration Register -------- */

#define DEVEPTCFG_OFFSET              (0x100)                                       /**<  (DEVEPTCFG) Device Endpoint Configuration Register  Offset */

#define DEVEPTCFG_ALLOC_Pos           1                                              /**< (DEVEPTCFG) Endpoint Memory Allocate Position */
#define DEVEPTCFG_ALLOC               (_U_(0x1) << DEVEPTCFG_ALLOC_Pos)        /**< (DEVEPTCFG) Endpoint Memory Allocate Mask */
#define DEVEPTCFG_EPBK_Pos            2                                              /**< (DEVEPTCFG) Endpoint Banks Position */
#define DEVEPTCFG_EPBK                (_U_(0x3) << DEVEPTCFG_EPBK_Pos)         /**< (DEVEPTCFG) Endpoint Banks Mask */
#define   DEVEPTCFG_EPBK_1_BANK_Val   _U_(0x0)                                       /**< (DEVEPTCFG) Single-bank endpoint  */
#define   DEVEPTCFG_EPBK_2_BANK_Val   _U_(0x1)                                       /**< (DEVEPTCFG) Double-bank endpoint  */
#define   DEVEPTCFG_EPBK_3_BANK_Val   _U_(0x2)                                       /**< (DEVEPTCFG) Triple-bank endpoint  */
#define DEVEPTCFG_EPBK_1_BANK         (DEVEPTCFG_EPBK_1_BANK_Val << DEVEPTCFG_EPBK_Pos)  /**< (DEVEPTCFG) Single-bank endpoint Position  */
#define DEVEPTCFG_EPBK_2_BANK         (DEVEPTCFG_EPBK_2_BANK_Val << DEVEPTCFG_EPBK_Pos)  /**< (DEVEPTCFG) Double-bank endpoint Position  */
#define DEVEPTCFG_EPBK_3_BANK         (DEVEPTCFG_EPBK_3_BANK_Val << DEVEPTCFG_EPBK_Pos)  /**< (DEVEPTCFG) Triple-bank endpoint Position  */
#define DEVEPTCFG_EPSIZE_Pos          4                                              /**< (DEVEPTCFG) Endpoint Size Position */
#define DEVEPTCFG_EPSIZE              (_U_(0x7) << DEVEPTCFG_EPSIZE_Pos)       /**< (DEVEPTCFG) Endpoint Size Mask */
#define   DEVEPTCFG_EPSIZE_8_BYTE_Val _U_(0x0)                                       /**< (DEVEPTCFG) 8 bytes  */
#define   DEVEPTCFG_EPSIZE_16_BYTE_Val _U_(0x1)                                       /**< (DEVEPTCFG) 16 bytes  */
#define   DEVEPTCFG_EPSIZE_32_BYTE_Val _U_(0x2)                                       /**< (DEVEPTCFG) 32 bytes  */
#define   DEVEPTCFG_EPSIZE_64_BYTE_Val _U_(0x3)                                       /**< (DEVEPTCFG) 64 bytes  */
#define   DEVEPTCFG_EPSIZE_128_BYTE_Val _U_(0x4)                                       /**< (DEVEPTCFG) 128 bytes  */
#define   DEVEPTCFG_EPSIZE_256_BYTE_Val _U_(0x5)                                       /**< (DEVEPTCFG) 256 bytes  */
#define   DEVEPTCFG_EPSIZE_512_BYTE_Val _U_(0x6)                                       /**< (DEVEPTCFG) 512 bytes  */
#define   DEVEPTCFG_EPSIZE_1024_BYTE_Val _U_(0x7)                                       /**< (DEVEPTCFG) 1024 bytes  */
#define DEVEPTCFG_EPSIZE_8_BYTE       (DEVEPTCFG_EPSIZE_8_BYTE_Val << DEVEPTCFG_EPSIZE_Pos)  /**< (DEVEPTCFG) 8 bytes Position  */
#define DEVEPTCFG_EPSIZE_16_BYTE      (DEVEPTCFG_EPSIZE_16_BYTE_Val << DEVEPTCFG_EPSIZE_Pos)  /**< (DEVEPTCFG) 16 bytes Position  */
#define DEVEPTCFG_EPSIZE_32_BYTE      (DEVEPTCFG_EPSIZE_32_BYTE_Val << DEVEPTCFG_EPSIZE_Pos)  /**< (DEVEPTCFG) 32 bytes Position  */
#define DEVEPTCFG_EPSIZE_64_BYTE      (DEVEPTCFG_EPSIZE_64_BYTE_Val << DEVEPTCFG_EPSIZE_Pos)  /**< (DEVEPTCFG) 64 bytes Position  */
#define DEVEPTCFG_EPSIZE_128_BYTE     (DEVEPTCFG_EPSIZE_128_BYTE_Val << DEVEPTCFG_EPSIZE_Pos)  /**< (DEVEPTCFG) 128 bytes Position  */
#define DEVEPTCFG_EPSIZE_256_BYTE     (DEVEPTCFG_EPSIZE_256_BYTE_Val << DEVEPTCFG_EPSIZE_Pos)  /**< (DEVEPTCFG) 256 bytes Position  */
#define DEVEPTCFG_EPSIZE_512_BYTE     (DEVEPTCFG_EPSIZE_512_BYTE_Val << DEVEPTCFG_EPSIZE_Pos)  /**< (DEVEPTCFG) 512 bytes Position  */
#define DEVEPTCFG_EPSIZE_1024_BYTE    (DEVEPTCFG_EPSIZE_1024_BYTE_Val << DEVEPTCFG_EPSIZE_Pos)  /**< (DEVEPTCFG) 1024 bytes Position  */
#define DEVEPTCFG_EPDIR_Pos           8                                              /**< (DEVEPTCFG) Endpoint Direction Position */
#define DEVEPTCFG_EPDIR               (_U_(0x1) << DEVEPTCFG_EPDIR_Pos)        /**< (DEVEPTCFG) Endpoint Direction Mask */
#define   DEVEPTCFG_EPDIR_OUT_Val     _U_(0x0)                                       /**< (DEVEPTCFG) The endpoint direction is OUT.  */
#define   DEVEPTCFG_EPDIR_IN_Val      _U_(0x1)                                       /**< (DEVEPTCFG) The endpoint direction is IN (nor for control endpoints).  */
#define DEVEPTCFG_EPDIR_OUT           (DEVEPTCFG_EPDIR_OUT_Val << DEVEPTCFG_EPDIR_Pos)  /**< (DEVEPTCFG) The endpoint direction is OUT. Position  */
#define DEVEPTCFG_EPDIR_IN            (DEVEPTCFG_EPDIR_IN_Val << DEVEPTCFG_EPDIR_Pos)  /**< (DEVEPTCFG) The endpoint direction is IN (nor for control endpoints). Position  */
#define DEVEPTCFG_AUTOSW_Pos          9                                              /**< (DEVEPTCFG) Automatic Switch Position */
#define DEVEPTCFG_AUTOSW              (_U_(0x1) << DEVEPTCFG_AUTOSW_Pos)       /**< (DEVEPTCFG) Automatic Switch Mask */
#define DEVEPTCFG_EPTYPE_Pos          11                                             /**< (DEVEPTCFG) Endpoint Type Position */
#define DEVEPTCFG_EPTYPE              (_U_(0x3) << DEVEPTCFG_EPTYPE_Pos)       /**< (DEVEPTCFG) Endpoint Type Mask */
#define   DEVEPTCFG_EPTYPE_CTRL_Val   _U_(0x0)                                       /**< (DEVEPTCFG) Control  */
#define   DEVEPTCFG_EPTYPE_ISO_Val    _U_(0x1)                                       /**< (DEVEPTCFG) Isochronous  */
#define   DEVEPTCFG_EPTYPE_BLK_Val    _U_(0x2)                                       /**< (DEVEPTCFG) Bulk  */
#define   DEVEPTCFG_EPTYPE_INTRPT_Val _U_(0x3)                                       /**< (DEVEPTCFG) Interrupt  */
#define DEVEPTCFG_EPTYPE_CTRL         (DEVEPTCFG_EPTYPE_CTRL_Val << DEVEPTCFG_EPTYPE_Pos)  /**< (DEVEPTCFG) Control Position  */
#define DEVEPTCFG_EPTYPE_ISO          (DEVEPTCFG_EPTYPE_ISO_Val << DEVEPTCFG_EPTYPE_Pos)  /**< (DEVEPTCFG) Isochronous Position  */
#define DEVEPTCFG_EPTYPE_BLK          (DEVEPTCFG_EPTYPE_BLK_Val << DEVEPTCFG_EPTYPE_Pos)  /**< (DEVEPTCFG) Bulk Position  */
#define DEVEPTCFG_EPTYPE_INTRPT       (DEVEPTCFG_EPTYPE_INTRPT_Val << DEVEPTCFG_EPTYPE_Pos)  /**< (DEVEPTCFG) Interrupt Position  */
#define DEVEPTCFG_NBTRANS_Pos         13                                             /**< (DEVEPTCFG) Number of transactions per microframe for isochronous endpoint Position */
#define DEVEPTCFG_NBTRANS             (_U_(0x3) << DEVEPTCFG_NBTRANS_Pos)      /**< (DEVEPTCFG) Number of transactions per microframe for isochronous endpoint Mask */
#define   DEVEPTCFG_NBTRANS_0_TRANS_Val _U_(0x0)                                       /**< (DEVEPTCFG) Reserved to endpoint that does not have the high-bandwidth isochronous capability.  */
#define   DEVEPTCFG_NBTRANS_1_TRANS_Val _U_(0x1)                                       /**< (DEVEPTCFG) Default value: one transaction per microframe.  */
#define   DEVEPTCFG_NBTRANS_2_TRANS_Val _U_(0x2)                                       /**< (DEVEPTCFG) Two transactions per microframe. This endpoint should be configured as double-bank.  */
#define   DEVEPTCFG_NBTRANS_3_TRANS_Val _U_(0x3)                                       /**< (DEVEPTCFG) Three transactions per microframe. This endpoint should be configured as triple-bank.  */
#define DEVEPTCFG_NBTRANS_0_TRANS     (DEVEPTCFG_NBTRANS_0_TRANS_Val << DEVEPTCFG_NBTRANS_Pos)  /**< (DEVEPTCFG) Reserved to endpoint that does not have the high-bandwidth isochronous capability. Position  */
#define DEVEPTCFG_NBTRANS_1_TRANS     (DEVEPTCFG_NBTRANS_1_TRANS_Val << DEVEPTCFG_NBTRANS_Pos)  /**< (DEVEPTCFG) Default value: one transaction per microframe. Position  */
#define DEVEPTCFG_NBTRANS_2_TRANS     (DEVEPTCFG_NBTRANS_2_TRANS_Val << DEVEPTCFG_NBTRANS_Pos)  /**< (DEVEPTCFG) Two transactions per microframe. This endpoint should be configured as double-bank. Position  */
#define DEVEPTCFG_NBTRANS_3_TRANS     (DEVEPTCFG_NBTRANS_3_TRANS_Val << DEVEPTCFG_NBTRANS_Pos)  /**< (DEVEPTCFG) Three transactions per microframe. This endpoint should be configured as triple-bank. Position  */
#define DEVEPTCFG_Msk                 _U_(0x7B7E)                                    /**< (DEVEPTCFG) Register Mask  */


/* -------- DEVEPTISR : (USBHS Offset: 0x130) (R/ 32) Device Endpoint Interrupt Status Register -------- */

#define DEVEPTISR_OFFSET              (0x130)                                       /**<  (DEVEPTISR) Device Endpoint Interrupt Status Register  Offset */

#define DEVEPTISR_TXINI_Pos           0                                              /**< (DEVEPTISR) Transmitted IN Data Interrupt Position */
#define DEVEPTISR_TXINI               (_U_(0x1) << DEVEPTISR_TXINI_Pos)        /**< (DEVEPTISR) Transmitted IN Data Interrupt Mask */
#define DEVEPTISR_RXOUTI_Pos          1                                              /**< (DEVEPTISR) Received OUT Data Interrupt Position */
#define DEVEPTISR_RXOUTI              (_U_(0x1) << DEVEPTISR_RXOUTI_Pos)       /**< (DEVEPTISR) Received OUT Data Interrupt Mask */
#define DEVEPTISR_OVERFI_Pos          5                                              /**< (DEVEPTISR) Overflow Interrupt Position */
#define DEVEPTISR_OVERFI              (_U_(0x1) << DEVEPTISR_OVERFI_Pos)       /**< (DEVEPTISR) Overflow Interrupt Mask */
#define DEVEPTISR_SHORTPACKET_Pos     7                                              /**< (DEVEPTISR) Short Packet Interrupt Position */
#define DEVEPTISR_SHORTPACKET         (_U_(0x1) << DEVEPTISR_SHORTPACKET_Pos)  /**< (DEVEPTISR) Short Packet Interrupt Mask */
#define DEVEPTISR_DTSEQ_Pos           8                                              /**< (DEVEPTISR) Data Toggle Sequence Position */
#define DEVEPTISR_DTSEQ               (_U_(0x3) << DEVEPTISR_DTSEQ_Pos)        /**< (DEVEPTISR) Data Toggle Sequence Mask */
#define   DEVEPTISR_DTSEQ_DATA0_Val   _U_(0x0)                                       /**< (DEVEPTISR) Data0 toggle sequence  */
#define   DEVEPTISR_DTSEQ_DATA1_Val   _U_(0x1)                                       /**< (DEVEPTISR) Data1 toggle sequence  */
#define   DEVEPTISR_DTSEQ_DATA2_Val   _U_(0x2)                                       /**< (DEVEPTISR) Reserved for high-bandwidth isochronous endpoint  */
#define   DEVEPTISR_DTSEQ_MDATA_Val   _U_(0x3)                                       /**< (DEVEPTISR) Reserved for high-bandwidth isochronous endpoint  */
#define DEVEPTISR_DTSEQ_DATA0         (DEVEPTISR_DTSEQ_DATA0_Val << DEVEPTISR_DTSEQ_Pos)  /**< (DEVEPTISR) Data0 toggle sequence Position  */
#define DEVEPTISR_DTSEQ_DATA1         (DEVEPTISR_DTSEQ_DATA1_Val << DEVEPTISR_DTSEQ_Pos)  /**< (DEVEPTISR) Data1 toggle sequence Position  */
#define DEVEPTISR_DTSEQ_DATA2         (DEVEPTISR_DTSEQ_DATA2_Val << DEVEPTISR_DTSEQ_Pos)  /**< (DEVEPTISR) Reserved for high-bandwidth isochronous endpoint Position  */
#define DEVEPTISR_DTSEQ_MDATA         (DEVEPTISR_DTSEQ_MDATA_Val << DEVEPTISR_DTSEQ_Pos)  /**< (DEVEPTISR) Reserved for high-bandwidth isochronous endpoint Position  */
#define DEVEPTISR_NBUSYBK_Pos         12                                             /**< (DEVEPTISR) Number of Busy Banks Position */
#define DEVEPTISR_NBUSYBK             (_U_(0x3) << DEVEPTISR_NBUSYBK_Pos)      /**< (DEVEPTISR) Number of Busy Banks Mask */
#define   DEVEPTISR_NBUSYBK_0_BUSY_Val _U_(0x0)                                       /**< (DEVEPTISR) 0 busy bank (all banks free)  */
#define   DEVEPTISR_NBUSYBK_1_BUSY_Val _U_(0x1)                                       /**< (DEVEPTISR) 1 busy bank  */
#define   DEVEPTISR_NBUSYBK_2_BUSY_Val _U_(0x2)                                       /**< (DEVEPTISR) 2 busy banks  */
#define   DEVEPTISR_NBUSYBK_3_BUSY_Val _U_(0x3)                                       /**< (DEVEPTISR) 3 busy banks  */
#define DEVEPTISR_NBUSYBK_0_BUSY      (DEVEPTISR_NBUSYBK_0_BUSY_Val << DEVEPTISR_NBUSYBK_Pos)  /**< (DEVEPTISR) 0 busy bank (all banks free) Position  */
#define DEVEPTISR_NBUSYBK_1_BUSY      (DEVEPTISR_NBUSYBK_1_BUSY_Val << DEVEPTISR_NBUSYBK_Pos)  /**< (DEVEPTISR) 1 busy bank Position  */
#define DEVEPTISR_NBUSYBK_2_BUSY      (DEVEPTISR_NBUSYBK_2_BUSY_Val << DEVEPTISR_NBUSYBK_Pos)  /**< (DEVEPTISR) 2 busy banks Position  */
#define DEVEPTISR_NBUSYBK_3_BUSY      (DEVEPTISR_NBUSYBK_3_BUSY_Val << DEVEPTISR_NBUSYBK_Pos)  /**< (DEVEPTISR) 3 busy banks Position  */
#define DEVEPTISR_CURRBK_Pos          14                                             /**< (DEVEPTISR) Current Bank Position */
#define DEVEPTISR_CURRBK              (_U_(0x3) << DEVEPTISR_CURRBK_Pos)       /**< (DEVEPTISR) Current Bank Mask */
#define   DEVEPTISR_CURRBK_BANK0_Val  _U_(0x0)                                       /**< (DEVEPTISR) Current bank is bank0  */
#define   DEVEPTISR_CURRBK_BANK1_Val  _U_(0x1)                                       /**< (DEVEPTISR) Current bank is bank1  */
#define   DEVEPTISR_CURRBK_BANK2_Val  _U_(0x2)                                       /**< (DEVEPTISR) Current bank is bank2  */
#define DEVEPTISR_CURRBK_BANK0        (DEVEPTISR_CURRBK_BANK0_Val << DEVEPTISR_CURRBK_Pos)  /**< (DEVEPTISR) Current bank is bank0 Position  */
#define DEVEPTISR_CURRBK_BANK1        (DEVEPTISR_CURRBK_BANK1_Val << DEVEPTISR_CURRBK_Pos)  /**< (DEVEPTISR) Current bank is bank1 Position  */
#define DEVEPTISR_CURRBK_BANK2        (DEVEPTISR_CURRBK_BANK2_Val << DEVEPTISR_CURRBK_Pos)  /**< (DEVEPTISR) Current bank is bank2 Position  */
#define DEVEPTISR_RWALL_Pos           16                                             /**< (DEVEPTISR) Read/Write Allowed Position */
#define DEVEPTISR_RWALL               (_U_(0x1) << DEVEPTISR_RWALL_Pos)        /**< (DEVEPTISR) Read/Write Allowed Mask */
#define DEVEPTISR_CFGOK_Pos           18                                             /**< (DEVEPTISR) Configuration OK Status Position */
#define DEVEPTISR_CFGOK               (_U_(0x1) << DEVEPTISR_CFGOK_Pos)        /**< (DEVEPTISR) Configuration OK Status Mask */
#define DEVEPTISR_BYCT_Pos            20                                             /**< (DEVEPTISR) Byte Count Position */
#define DEVEPTISR_BYCT                (_U_(0x7FF) << DEVEPTISR_BYCT_Pos)       /**< (DEVEPTISR) Byte Count Mask */
#define DEVEPTISR_Msk                 _U_(0x7FF5F3A3)                                /**< (DEVEPTISR) Register Mask  */

/* CTRL mode */
#define DEVEPTISR_CTRL_RXSTPI_Pos     2                                              /**< (DEVEPTISR) Received SETUP Interrupt Position */
#define DEVEPTISR_CTRL_RXSTPI         (_U_(0x1) << DEVEPTISR_CTRL_RXSTPI_Pos)  /**< (DEVEPTISR) Received SETUP Interrupt Mask */
#define DEVEPTISR_CTRL_NAKOUTI_Pos    3                                              /**< (DEVEPTISR) NAKed OUT Interrupt Position */
#define DEVEPTISR_CTRL_NAKOUTI        (_U_(0x1) << DEVEPTISR_CTRL_NAKOUTI_Pos)  /**< (DEVEPTISR) NAKed OUT Interrupt Mask */
#define DEVEPTISR_CTRL_NAKINI_Pos     4                                              /**< (DEVEPTISR) NAKed IN Interrupt Position */
#define DEVEPTISR_CTRL_NAKINI         (_U_(0x1) << DEVEPTISR_CTRL_NAKINI_Pos)  /**< (DEVEPTISR) NAKed IN Interrupt Mask */
#define DEVEPTISR_CTRL_STALLEDI_Pos   6                                              /**< (DEVEPTISR) STALLed Interrupt Position */
#define DEVEPTISR_CTRL_STALLEDI       (_U_(0x1) << DEVEPTISR_CTRL_STALLEDI_Pos)  /**< (DEVEPTISR) STALLed Interrupt Mask */
#define DEVEPTISR_CTRL_CTRLDIR_Pos    17                                             /**< (DEVEPTISR) Control Direction Position */
#define DEVEPTISR_CTRL_CTRLDIR        (_U_(0x1) << DEVEPTISR_CTRL_CTRLDIR_Pos)  /**< (DEVEPTISR) Control Direction Mask */
#define DEVEPTISR_CTRL_Msk            _U_(0x2005C)                                   /**< (DEVEPTISR_CTRL) Register Mask  */

/* ISO mode */
#define DEVEPTISR_ISO_UNDERFI_Pos     2                                              /**< (DEVEPTISR) Underflow Interrupt Position */
#define DEVEPTISR_ISO_UNDERFI         (_U_(0x1) << DEVEPTISR_ISO_UNDERFI_Pos)  /**< (DEVEPTISR) Underflow Interrupt Mask */
#define DEVEPTISR_ISO_HBISOINERRI_Pos 3                                              /**< (DEVEPTISR) High Bandwidth Isochronous IN Underflow Error Interrupt Position */
#define DEVEPTISR_ISO_HBISOINERRI     (_U_(0x1) << DEVEPTISR_ISO_HBISOINERRI_Pos)  /**< (DEVEPTISR) High Bandwidth Isochronous IN Underflow Error Interrupt Mask */
#define DEVEPTISR_ISO_HBISOFLUSHI_Pos 4                                              /**< (DEVEPTISR) High Bandwidth Isochronous IN Flush Interrupt Position */
#define DEVEPTISR_ISO_HBISOFLUSHI     (_U_(0x1) << DEVEPTISR_ISO_HBISOFLUSHI_Pos)  /**< (DEVEPTISR) High Bandwidth Isochronous IN Flush Interrupt Mask */
#define DEVEPTISR_ISO_CRCERRI_Pos     6                                              /**< (DEVEPTISR) CRC Error Interrupt Position */
#define DEVEPTISR_ISO_CRCERRI         (_U_(0x1) << DEVEPTISR_ISO_CRCERRI_Pos)  /**< (DEVEPTISR) CRC Error Interrupt Mask */
#define DEVEPTISR_ISO_ERRORTRANS_Pos  10                                             /**< (DEVEPTISR) High-bandwidth Isochronous OUT Endpoint Transaction Error Interrupt Position */
#define DEVEPTISR_ISO_ERRORTRANS      (_U_(0x1) << DEVEPTISR_ISO_ERRORTRANS_Pos)  /**< (DEVEPTISR) High-bandwidth Isochronous OUT Endpoint Transaction Error Interrupt Mask */
#define DEVEPTISR_ISO_Msk             _U_(0x45C)                                     /**< (DEVEPTISR_ISO) Register Mask  */

/* BLK mode */
#define DEVEPTISR_BLK_RXSTPI_Pos      2                                              /**< (DEVEPTISR) Received SETUP Interrupt Position */
#define DEVEPTISR_BLK_RXSTPI          (_U_(0x1) << DEVEPTISR_BLK_RXSTPI_Pos)   /**< (DEVEPTISR) Received SETUP Interrupt Mask */
#define DEVEPTISR_BLK_NAKOUTI_Pos     3                                              /**< (DEVEPTISR) NAKed OUT Interrupt Position */
#define DEVEPTISR_BLK_NAKOUTI         (_U_(0x1) << DEVEPTISR_BLK_NAKOUTI_Pos)  /**< (DEVEPTISR) NAKed OUT Interrupt Mask */
#define DEVEPTISR_BLK_NAKINI_Pos      4                                              /**< (DEVEPTISR) NAKed IN Interrupt Position */
#define DEVEPTISR_BLK_NAKINI          (_U_(0x1) << DEVEPTISR_BLK_NAKINI_Pos)   /**< (DEVEPTISR) NAKed IN Interrupt Mask */
#define DEVEPTISR_BLK_STALLEDI_Pos    6                                              /**< (DEVEPTISR) STALLed Interrupt Position */
#define DEVEPTISR_BLK_STALLEDI        (_U_(0x1) << DEVEPTISR_BLK_STALLEDI_Pos)  /**< (DEVEPTISR) STALLed Interrupt Mask */
#define DEVEPTISR_BLK_CTRLDIR_Pos     17                                             /**< (DEVEPTISR) Control Direction Position */
#define DEVEPTISR_BLK_CTRLDIR         (_U_(0x1) << DEVEPTISR_BLK_CTRLDIR_Pos)  /**< (DEVEPTISR) Control Direction Mask */
#define DEVEPTISR_BLK_Msk             _U_(0x2005C)                                   /**< (DEVEPTISR_BLK) Register Mask  */

/* INTRPT mode */
#define DEVEPTISR_INTRPT_RXSTPI_Pos   2                                              /**< (DEVEPTISR) Received SETUP Interrupt Position */
#define DEVEPTISR_INTRPT_RXSTPI       (_U_(0x1) << DEVEPTISR_INTRPT_RXSTPI_Pos)  /**< (DEVEPTISR) Received SETUP Interrupt Mask */
#define DEVEPTISR_INTRPT_NAKOUTI_Pos  3                                              /**< (DEVEPTISR) NAKed OUT Interrupt Position */
#define DEVEPTISR_INTRPT_NAKOUTI      (_U_(0x1) << DEVEPTISR_INTRPT_NAKOUTI_Pos)  /**< (DEVEPTISR) NAKed OUT Interrupt Mask */
#define DEVEPTISR_INTRPT_NAKINI_Pos   4                                              /**< (DEVEPTISR) NAKed IN Interrupt Position */
#define DEVEPTISR_INTRPT_NAKINI       (_U_(0x1) << DEVEPTISR_INTRPT_NAKINI_Pos)  /**< (DEVEPTISR) NAKed IN Interrupt Mask */
#define DEVEPTISR_INTRPT_STALLEDI_Pos 6                                              /**< (DEVEPTISR) STALLed Interrupt Position */
#define DEVEPTISR_INTRPT_STALLEDI     (_U_(0x1) << DEVEPTISR_INTRPT_STALLEDI_Pos)  /**< (DEVEPTISR) STALLed Interrupt Mask */
#define DEVEPTISR_INTRPT_CTRLDIR_Pos  17                                             /**< (DEVEPTISR) Control Direction Position */
#define DEVEPTISR_INTRPT_CTRLDIR      (_U_(0x1) << DEVEPTISR_INTRPT_CTRLDIR_Pos)  /**< (DEVEPTISR) Control Direction Mask */
#define DEVEPTISR_INTRPT_Msk          _U_(0x2005C)                                   /**< (DEVEPTISR_INTRPT) Register Mask  */


/* -------- DEVEPTICR : (USBHS Offset: 0x160) (/W 32) Device Endpoint Interrupt Clear Register -------- */

#define DEVEPTICR_OFFSET              (0x160)                                       /**<  (DEVEPTICR) Device Endpoint Interrupt Clear Register  Offset */

#define DEVEPTICR_TXINIC_Pos          0                                              /**< (DEVEPTICR) Transmitted IN Data Interrupt Clear Position */
#define DEVEPTICR_TXINIC              (_U_(0x1) << DEVEPTICR_TXINIC_Pos)       /**< (DEVEPTICR) Transmitted IN Data Interrupt Clear Mask */
#define DEVEPTICR_RXOUTIC_Pos         1                                              /**< (DEVEPTICR) Received OUT Data Interrupt Clear Position */
#define DEVEPTICR_RXOUTIC             (_U_(0x1) << DEVEPTICR_RXOUTIC_Pos)      /**< (DEVEPTICR) Received OUT Data Interrupt Clear Mask */
#define DEVEPTICR_OVERFIC_Pos         5                                              /**< (DEVEPTICR) Overflow Interrupt Clear Position */
#define DEVEPTICR_OVERFIC             (_U_(0x1) << DEVEPTICR_OVERFIC_Pos)      /**< (DEVEPTICR) Overflow Interrupt Clear Mask */
#define DEVEPTICR_SHORTPACKETC_Pos    7                                              /**< (DEVEPTICR) Short Packet Interrupt Clear Position */
#define DEVEPTICR_SHORTPACKETC        (_U_(0x1) << DEVEPTICR_SHORTPACKETC_Pos)  /**< (DEVEPTICR) Short Packet Interrupt Clear Mask */
#define DEVEPTICR_Msk                 _U_(0xA3)                                      /**< (DEVEPTICR) Register Mask  */

/* CTRL mode */
#define DEVEPTICR_CTRL_RXSTPIC_Pos    2                                              /**< (DEVEPTICR) Received SETUP Interrupt Clear Position */
#define DEVEPTICR_CTRL_RXSTPIC        (_U_(0x1) << DEVEPTICR_CTRL_RXSTPIC_Pos)  /**< (DEVEPTICR) Received SETUP Interrupt Clear Mask */
#define DEVEPTICR_CTRL_NAKOUTIC_Pos   3                                              /**< (DEVEPTICR) NAKed OUT Interrupt Clear Position */
#define DEVEPTICR_CTRL_NAKOUTIC       (_U_(0x1) << DEVEPTICR_CTRL_NAKOUTIC_Pos)  /**< (DEVEPTICR) NAKed OUT Interrupt Clear Mask */
#define DEVEPTICR_CTRL_NAKINIC_Pos    4                                              /**< (DEVEPTICR) NAKed IN Interrupt Clear Position */
#define DEVEPTICR_CTRL_NAKINIC        (_U_(0x1) << DEVEPTICR_CTRL_NAKINIC_Pos)  /**< (DEVEPTICR) NAKed IN Interrupt Clear Mask */
#define DEVEPTICR_CTRL_STALLEDIC_Pos  6                                              /**< (DEVEPTICR) STALLed Interrupt Clear Position */
#define DEVEPTICR_CTRL_STALLEDIC      (_U_(0x1) << DEVEPTICR_CTRL_STALLEDIC_Pos)  /**< (DEVEPTICR) STALLed Interrupt Clear Mask */
#define DEVEPTICR_CTRL_Msk            _U_(0x5C)                                      /**< (DEVEPTICR_CTRL) Register Mask  */

/* ISO mode */
#define DEVEPTICR_ISO_UNDERFIC_Pos    2                                              /**< (DEVEPTICR) Underflow Interrupt Clear Position */
#define DEVEPTICR_ISO_UNDERFIC        (_U_(0x1) << DEVEPTICR_ISO_UNDERFIC_Pos)  /**< (DEVEPTICR) Underflow Interrupt Clear Mask */
#define DEVEPTICR_ISO_HBISOINERRIC_Pos 3                                              /**< (DEVEPTICR) High Bandwidth Isochronous IN Underflow Error Interrupt Clear Position */
#define DEVEPTICR_ISO_HBISOINERRIC     (_U_(0x1) << DEVEPTICR_ISO_HBISOINERRIC_Pos)  /**< (DEVEPTICR) High Bandwidth Isochronous IN Underflow Error Interrupt Clear Mask */
#define DEVEPTICR_ISO_HBISOFLUSHIC_Pos 4                                              /**< (DEVEPTICR) High Bandwidth Isochronous IN Flush Interrupt Clear Position */
#define DEVEPTICR_ISO_HBISOFLUSHIC     (_U_(0x1) << DEVEPTICR_ISO_HBISOFLUSHIC_Pos)  /**< (DEVEPTICR) High Bandwidth Isochronous IN Flush Interrupt Clear Mask */
#define DEVEPTICR_ISO_CRCERRIC_Pos    6                                              /**< (DEVEPTICR) CRC Error Interrupt Clear Position */
#define DEVEPTICR_ISO_CRCERRIC        (_U_(0x1) << DEVEPTICR_ISO_CRCERRIC_Pos)  /**< (DEVEPTICR) CRC Error Interrupt Clear Mask */
#define DEVEPTICR_ISO_Msk             _U_(0x5C)                                      /**< (DEVEPTICR_ISO) Register Mask  */

/* BLK mode */
#define DEVEPTICR_BLK_RXSTPIC_Pos     2                                              /**< (DEVEPTICR) Received SETUP Interrupt Clear Position */
#define DEVEPTICR_BLK_RXSTPIC         (_U_(0x1) << DEVEPTICR_BLK_RXSTPIC_Pos)  /**< (DEVEPTICR) Received SETUP Interrupt Clear Mask */
#define DEVEPTICR_BLK_NAKOUTIC_Pos    3                                              /**< (DEVEPTICR) NAKed OUT Interrupt Clear Position */
#define DEVEPTICR_BLK_NAKOUTIC        (_U_(0x1) << DEVEPTICR_BLK_NAKOUTIC_Pos)  /**< (DEVEPTICR) NAKed OUT Interrupt Clear Mask */
#define DEVEPTICR_BLK_NAKINIC_Pos     4                                              /**< (DEVEPTICR) NAKed IN Interrupt Clear Position */
#define DEVEPTICR_BLK_NAKINIC         (_U_(0x1) << DEVEPTICR_BLK_NAKINIC_Pos)  /**< (DEVEPTICR) NAKed IN Interrupt Clear Mask */
#define DEVEPTICR_BLK_STALLEDIC_Pos   6                                              /**< (DEVEPTICR) STALLed Interrupt Clear Position */
#define DEVEPTICR_BLK_STALLEDIC       (_U_(0x1) << DEVEPTICR_BLK_STALLEDIC_Pos)  /**< (DEVEPTICR) STALLed Interrupt Clear Mask */
#define DEVEPTICR_BLK_Msk             _U_(0x5C)                                      /**< (DEVEPTICR_BLK) Register Mask  */

/* INTRPT mode */
#define DEVEPTICR_INTRPT_RXSTPIC_Pos  2                                              /**< (DEVEPTICR) Received SETUP Interrupt Clear Position */
#define DEVEPTICR_INTRPT_RXSTPIC      (_U_(0x1) << DEVEPTICR_INTRPT_RXSTPIC_Pos)  /**< (DEVEPTICR) Received SETUP Interrupt Clear Mask */
#define DEVEPTICR_INTRPT_NAKOUTIC_Pos 3                                              /**< (DEVEPTICR) NAKed OUT Interrupt Clear Position */
#define DEVEPTICR_INTRPT_NAKOUTIC     (_U_(0x1) << DEVEPTICR_INTRPT_NAKOUTIC_Pos)  /**< (DEVEPTICR) NAKed OUT Interrupt Clear Mask */
#define DEVEPTICR_INTRPT_NAKINIC_Pos  4                                              /**< (DEVEPTICR) NAKed IN Interrupt Clear Position */
#define DEVEPTICR_INTRPT_NAKINIC      (_U_(0x1) << DEVEPTICR_INTRPT_NAKINIC_Pos)  /**< (DEVEPTICR) NAKed IN Interrupt Clear Mask */
#define DEVEPTICR_INTRPT_STALLEDIC_Pos 6                                              /**< (DEVEPTICR) STALLed Interrupt Clear Position */
#define DEVEPTICR_INTRPT_STALLEDIC     (_U_(0x1) << DEVEPTICR_INTRPT_STALLEDIC_Pos)  /**< (DEVEPTICR) STALLed Interrupt Clear Mask */
#define DEVEPTICR_INTRPT_Msk          _U_(0x5C)                                      /**< (DEVEPTICR_INTRPT) Register Mask  */


/* -------- DEVEPTIFR : (USBHS Offset: 0x190) (/W 32) Device Endpoint Interrupt Set Register -------- */

#define DEVEPTIFR_OFFSET              (0x190)                                       /**<  (DEVEPTIFR) Device Endpoint Interrupt Set Register  Offset */

#define DEVEPTIFR_TXINIS_Pos          0                                              /**< (DEVEPTIFR) Transmitted IN Data Interrupt Set Position */
#define DEVEPTIFR_TXINIS              (_U_(0x1) << DEVEPTIFR_TXINIS_Pos)       /**< (DEVEPTIFR) Transmitted IN Data Interrupt Set Mask */
#define DEVEPTIFR_RXOUTIS_Pos         1                                              /**< (DEVEPTIFR) Received OUT Data Interrupt Set Position */
#define DEVEPTIFR_RXOUTIS             (_U_(0x1) << DEVEPTIFR_RXOUTIS_Pos)      /**< (DEVEPTIFR) Received OUT Data Interrupt Set Mask */
#define DEVEPTIFR_OVERFIS_Pos         5                                              /**< (DEVEPTIFR) Overflow Interrupt Set Position */
#define DEVEPTIFR_OVERFIS             (_U_(0x1) << DEVEPTIFR_OVERFIS_Pos)      /**< (DEVEPTIFR) Overflow Interrupt Set Mask */
#define DEVEPTIFR_SHORTPACKETS_Pos    7                                              /**< (DEVEPTIFR) Short Packet Interrupt Set Position */
#define DEVEPTIFR_SHORTPACKETS        (_U_(0x1) << DEVEPTIFR_SHORTPACKETS_Pos)  /**< (DEVEPTIFR) Short Packet Interrupt Set Mask */
#define DEVEPTIFR_NBUSYBKS_Pos        12                                             /**< (DEVEPTIFR) Number of Busy Banks Interrupt Set Position */
#define DEVEPTIFR_NBUSYBKS            (_U_(0x1) << DEVEPTIFR_NBUSYBKS_Pos)     /**< (DEVEPTIFR) Number of Busy Banks Interrupt Set Mask */
#define DEVEPTIFR_Msk                 _U_(0x10A3)                                    /**< (DEVEPTIFR) Register Mask  */

/* CTRL mode */
#define DEVEPTIFR_CTRL_RXSTPIS_Pos    2                                              /**< (DEVEPTIFR) Received SETUP Interrupt Set Position */
#define DEVEPTIFR_CTRL_RXSTPIS        (_U_(0x1) << DEVEPTIFR_CTRL_RXSTPIS_Pos)  /**< (DEVEPTIFR) Received SETUP Interrupt Set Mask */
#define DEVEPTIFR_CTRL_NAKOUTIS_Pos   3                                              /**< (DEVEPTIFR) NAKed OUT Interrupt Set Position */
#define DEVEPTIFR_CTRL_NAKOUTIS       (_U_(0x1) << DEVEPTIFR_CTRL_NAKOUTIS_Pos)  /**< (DEVEPTIFR) NAKed OUT Interrupt Set Mask */
#define DEVEPTIFR_CTRL_NAKINIS_Pos    4                                              /**< (DEVEPTIFR) NAKed IN Interrupt Set Position */
#define DEVEPTIFR_CTRL_NAKINIS        (_U_(0x1) << DEVEPTIFR_CTRL_NAKINIS_Pos)  /**< (DEVEPTIFR) NAKed IN Interrupt Set Mask */
#define DEVEPTIFR_CTRL_STALLEDIS_Pos  6                                              /**< (DEVEPTIFR) STALLed Interrupt Set Position */
#define DEVEPTIFR_CTRL_STALLEDIS      (_U_(0x1) << DEVEPTIFR_CTRL_STALLEDIS_Pos)  /**< (DEVEPTIFR) STALLed Interrupt Set Mask */
#define DEVEPTIFR_CTRL_Msk            _U_(0x5C)                                      /**< (DEVEPTIFR_CTRL) Register Mask  */

/* ISO mode */
#define DEVEPTIFR_ISO_UNDERFIS_Pos    2                                              /**< (DEVEPTIFR) Underflow Interrupt Set Position */
#define DEVEPTIFR_ISO_UNDERFIS        (_U_(0x1) << DEVEPTIFR_ISO_UNDERFIS_Pos)  /**< (DEVEPTIFR) Underflow Interrupt Set Mask */
#define DEVEPTIFR_ISO_HBISOINERRIS_Pos 3                                              /**< (DEVEPTIFR) High Bandwidth Isochronous IN Underflow Error Interrupt Set Position */
#define DEVEPTIFR_ISO_HBISOINERRIS     (_U_(0x1) << DEVEPTIFR_ISO_HBISOINERRIS_Pos)  /**< (DEVEPTIFR) High Bandwidth Isochronous IN Underflow Error Interrupt Set Mask */
#define DEVEPTIFR_ISO_HBISOFLUSHIS_Pos 4                                              /**< (DEVEPTIFR) High Bandwidth Isochronous IN Flush Interrupt Set Position */
#define DEVEPTIFR_ISO_HBISOFLUSHIS     (_U_(0x1) << DEVEPTIFR_ISO_HBISOFLUSHIS_Pos)  /**< (DEVEPTIFR) High Bandwidth Isochronous IN Flush Interrupt Set Mask */
#define DEVEPTIFR_ISO_CRCERRIS_Pos    6                                              /**< (DEVEPTIFR) CRC Error Interrupt Set Position */
#define DEVEPTIFR_ISO_CRCERRIS        (_U_(0x1) << DEVEPTIFR_ISO_CRCERRIS_Pos)  /**< (DEVEPTIFR) CRC Error Interrupt Set Mask */
#define DEVEPTIFR_ISO_Msk             _U_(0x5C)                                      /**< (DEVEPTIFR_ISO) Register Mask  */

/* BLK mode */
#define DEVEPTIFR_BLK_RXSTPIS_Pos     2                                              /**< (DEVEPTIFR) Received SETUP Interrupt Set Position */
#define DEVEPTIFR_BLK_RXSTPIS         (_U_(0x1) << DEVEPTIFR_BLK_RXSTPIS_Pos)  /**< (DEVEPTIFR) Received SETUP Interrupt Set Mask */
#define DEVEPTIFR_BLK_NAKOUTIS_Pos    3                                              /**< (DEVEPTIFR) NAKed OUT Interrupt Set Position */
#define DEVEPTIFR_BLK_NAKOUTIS        (_U_(0x1) << DEVEPTIFR_BLK_NAKOUTIS_Pos)  /**< (DEVEPTIFR) NAKed OUT Interrupt Set Mask */
#define DEVEPTIFR_BLK_NAKINIS_Pos     4                                              /**< (DEVEPTIFR) NAKed IN Interrupt Set Position */
#define DEVEPTIFR_BLK_NAKINIS         (_U_(0x1) << DEVEPTIFR_BLK_NAKINIS_Pos)  /**< (DEVEPTIFR) NAKed IN Interrupt Set Mask */
#define DEVEPTIFR_BLK_STALLEDIS_Pos   6                                              /**< (DEVEPTIFR) STALLed Interrupt Set Position */
#define DEVEPTIFR_BLK_STALLEDIS       (_U_(0x1) << DEVEPTIFR_BLK_STALLEDIS_Pos)  /**< (DEVEPTIFR) STALLed Interrupt Set Mask */
#define DEVEPTIFR_BLK_Msk             _U_(0x5C)                                      /**< (DEVEPTIFR_BLK) Register Mask  */

/* INTRPT mode */
#define DEVEPTIFR_INTRPT_RXSTPIS_Pos  2                                              /**< (DEVEPTIFR) Received SETUP Interrupt Set Position */
#define DEVEPTIFR_INTRPT_RXSTPIS      (_U_(0x1) << DEVEPTIFR_INTRPT_RXSTPIS_Pos)  /**< (DEVEPTIFR) Received SETUP Interrupt Set Mask */
#define DEVEPTIFR_INTRPT_NAKOUTIS_Pos 3                                              /**< (DEVEPTIFR) NAKed OUT Interrupt Set Position */
#define DEVEPTIFR_INTRPT_NAKOUTIS     (_U_(0x1) << DEVEPTIFR_INTRPT_NAKOUTIS_Pos)  /**< (DEVEPTIFR) NAKed OUT Interrupt Set Mask */
#define DEVEPTIFR_INTRPT_NAKINIS_Pos  4                                              /**< (DEVEPTIFR) NAKed IN Interrupt Set Position */
#define DEVEPTIFR_INTRPT_NAKINIS      (_U_(0x1) << DEVEPTIFR_INTRPT_NAKINIS_Pos)  /**< (DEVEPTIFR) NAKed IN Interrupt Set Mask */
#define DEVEPTIFR_INTRPT_STALLEDIS_Pos 6                                              /**< (DEVEPTIFR) STALLed Interrupt Set Position */
#define DEVEPTIFR_INTRPT_STALLEDIS     (_U_(0x1) << DEVEPTIFR_INTRPT_STALLEDIS_Pos)  /**< (DEVEPTIFR) STALLed Interrupt Set Mask */
#define DEVEPTIFR_INTRPT_Msk          _U_(0x5C)                                      /**< (DEVEPTIFR_INTRPT) Register Mask  */


/* -------- DEVEPTIMR : (USBHS Offset: 0x1c0) (R/ 32) Device Endpoint Interrupt Mask Register -------- */

#define DEVEPTIMR_OFFSET              (0x1C0)                                       /**<  (DEVEPTIMR) Device Endpoint Interrupt Mask Register  Offset */

#define DEVEPTIMR_TXINE_Pos           0                                              /**< (DEVEPTIMR) Transmitted IN Data Interrupt Position */
#define DEVEPTIMR_TXINE               (_U_(0x1) << DEVEPTIMR_TXINE_Pos)        /**< (DEVEPTIMR) Transmitted IN Data Interrupt Mask */
#define DEVEPTIMR_RXOUTE_Pos          1                                              /**< (DEVEPTIMR) Received OUT Data Interrupt Position */
#define DEVEPTIMR_RXOUTE              (_U_(0x1) << DEVEPTIMR_RXOUTE_Pos)       /**< (DEVEPTIMR) Received OUT Data Interrupt Mask */
#define DEVEPTIMR_OVERFE_Pos          5                                              /**< (DEVEPTIMR) Overflow Interrupt Position */
#define DEVEPTIMR_OVERFE              (_U_(0x1) << DEVEPTIMR_OVERFE_Pos)       /**< (DEVEPTIMR) Overflow Interrupt Mask */
#define DEVEPTIMR_SHORTPACKETE_Pos    7                                              /**< (DEVEPTIMR) Short Packet Interrupt Position */
#define DEVEPTIMR_SHORTPACKETE        (_U_(0x1) << DEVEPTIMR_SHORTPACKETE_Pos)  /**< (DEVEPTIMR) Short Packet Interrupt Mask */
#define DEVEPTIMR_NBUSYBKE_Pos        12                                             /**< (DEVEPTIMR) Number of Busy Banks Interrupt Position */
#define DEVEPTIMR_NBUSYBKE            (_U_(0x1) << DEVEPTIMR_NBUSYBKE_Pos)     /**< (DEVEPTIMR) Number of Busy Banks Interrupt Mask */
#define DEVEPTIMR_KILLBK_Pos          13                                             /**< (DEVEPTIMR) Kill IN Bank Position */
#define DEVEPTIMR_KILLBK              (_U_(0x1) << DEVEPTIMR_KILLBK_Pos)       /**< (DEVEPTIMR) Kill IN Bank Mask */
#define DEVEPTIMR_FIFOCON_Pos         14                                             /**< (DEVEPTIMR) FIFO Control Position */
#define DEVEPTIMR_FIFOCON             (_U_(0x1) << DEVEPTIMR_FIFOCON_Pos)      /**< (DEVEPTIMR) FIFO Control Mask */
#define DEVEPTIMR_EPDISHDMA_Pos       16                                             /**< (DEVEPTIMR) Endpoint Interrupts Disable HDMA Request Position */
#define DEVEPTIMR_EPDISHDMA           (_U_(0x1) << DEVEPTIMR_EPDISHDMA_Pos)    /**< (DEVEPTIMR) Endpoint Interrupts Disable HDMA Request Mask */
#define DEVEPTIMR_RSTDT_Pos           18                                             /**< (DEVEPTIMR) Reset Data Toggle Position */
#define DEVEPTIMR_RSTDT               (_U_(0x1) << DEVEPTIMR_RSTDT_Pos)        /**< (DEVEPTIMR) Reset Data Toggle Mask */
#define DEVEPTIMR_Msk                 _U_(0x570A3)                                   /**< (DEVEPTIMR) Register Mask  */

/* CTRL mode */
#define DEVEPTIMR_CTRL_RXSTPE_Pos     2                                              /**< (DEVEPTIMR) Received SETUP Interrupt Position */
#define DEVEPTIMR_CTRL_RXSTPE         (_U_(0x1) << DEVEPTIMR_CTRL_RXSTPE_Pos)  /**< (DEVEPTIMR) Received SETUP Interrupt Mask */
#define DEVEPTIMR_CTRL_NAKOUTE_Pos    3                                              /**< (DEVEPTIMR) NAKed OUT Interrupt Position */
#define DEVEPTIMR_CTRL_NAKOUTE        (_U_(0x1) << DEVEPTIMR_CTRL_NAKOUTE_Pos)  /**< (DEVEPTIMR) NAKed OUT Interrupt Mask */
#define DEVEPTIMR_CTRL_NAKINE_Pos     4                                              /**< (DEVEPTIMR) NAKed IN Interrupt Position */
#define DEVEPTIMR_CTRL_NAKINE         (_U_(0x1) << DEVEPTIMR_CTRL_NAKINE_Pos)  /**< (DEVEPTIMR) NAKed IN Interrupt Mask */
#define DEVEPTIMR_CTRL_STALLEDE_Pos   6                                              /**< (DEVEPTIMR) STALLed Interrupt Position */
#define DEVEPTIMR_CTRL_STALLEDE       (_U_(0x1) << DEVEPTIMR_CTRL_STALLEDE_Pos)  /**< (DEVEPTIMR) STALLed Interrupt Mask */
#define DEVEPTIMR_CTRL_NYETDIS_Pos    17                                             /**< (DEVEPTIMR) NYET Token Disable Position */
#define DEVEPTIMR_CTRL_NYETDIS        (_U_(0x1) << DEVEPTIMR_CTRL_NYETDIS_Pos)  /**< (DEVEPTIMR) NYET Token Disable Mask */
#define DEVEPTIMR_CTRL_STALLRQ_Pos    19                                             /**< (DEVEPTIMR) STALL Request Position */
#define DEVEPTIMR_CTRL_STALLRQ        (_U_(0x1) << DEVEPTIMR_CTRL_STALLRQ_Pos)  /**< (DEVEPTIMR) STALL Request Mask */
#define DEVEPTIMR_CTRL_Msk            _U_(0xA005C)                                   /**< (DEVEPTIMR_CTRL) Register Mask  */

/* ISO mode */
#define DEVEPTIMR_ISO_UNDERFE_Pos     2                                              /**< (DEVEPTIMR) Underflow Interrupt Position */
#define DEVEPTIMR_ISO_UNDERFE         (_U_(0x1) << DEVEPTIMR_ISO_UNDERFE_Pos)  /**< (DEVEPTIMR) Underflow Interrupt Mask */
#define DEVEPTIMR_ISO_HBISOINERRE_Pos 3                                              /**< (DEVEPTIMR) High Bandwidth Isochronous IN Underflow Error Interrupt Position */
#define DEVEPTIMR_ISO_HBISOINERRE     (_U_(0x1) << DEVEPTIMR_ISO_HBISOINERRE_Pos)  /**< (DEVEPTIMR) High Bandwidth Isochronous IN Underflow Error Interrupt Mask */
#define DEVEPTIMR_ISO_HBISOFLUSHE_Pos 4                                              /**< (DEVEPTIMR) High Bandwidth Isochronous IN Flush Interrupt Position */
#define DEVEPTIMR_ISO_HBISOFLUSHE     (_U_(0x1) << DEVEPTIMR_ISO_HBISOFLUSHE_Pos)  /**< (DEVEPTIMR) High Bandwidth Isochronous IN Flush Interrupt Mask */
#define DEVEPTIMR_ISO_CRCERRE_Pos     6                                              /**< (DEVEPTIMR) CRC Error Interrupt Position */
#define DEVEPTIMR_ISO_CRCERRE         (_U_(0x1) << DEVEPTIMR_ISO_CRCERRE_Pos)  /**< (DEVEPTIMR) CRC Error Interrupt Mask */
#define DEVEPTIMR_ISO_MDATAE_Pos      8                                              /**< (DEVEPTIMR) MData Interrupt Position */
#define DEVEPTIMR_ISO_MDATAE          (_U_(0x1) << DEVEPTIMR_ISO_MDATAE_Pos)   /**< (DEVEPTIMR) MData Interrupt Mask */
#define DEVEPTIMR_ISO_DATAXE_Pos      9                                              /**< (DEVEPTIMR) DataX Interrupt Position */
#define DEVEPTIMR_ISO_DATAXE          (_U_(0x1) << DEVEPTIMR_ISO_DATAXE_Pos)   /**< (DEVEPTIMR) DataX Interrupt Mask */
#define DEVEPTIMR_ISO_ERRORTRANSE_Pos 10                                             /**< (DEVEPTIMR) Transaction Error Interrupt Position */
#define DEVEPTIMR_ISO_ERRORTRANSE     (_U_(0x1) << DEVEPTIMR_ISO_ERRORTRANSE_Pos)  /**< (DEVEPTIMR) Transaction Error Interrupt Mask */
#define DEVEPTIMR_ISO_Msk             _U_(0x75C)                                     /**< (DEVEPTIMR_ISO) Register Mask  */

/* BLK mode */
#define DEVEPTIMR_BLK_RXSTPE_Pos      2                                              /**< (DEVEPTIMR) Received SETUP Interrupt Position */
#define DEVEPTIMR_BLK_RXSTPE          (_U_(0x1) << DEVEPTIMR_BLK_RXSTPE_Pos)   /**< (DEVEPTIMR) Received SETUP Interrupt Mask */
#define DEVEPTIMR_BLK_NAKOUTE_Pos     3                                              /**< (DEVEPTIMR) NAKed OUT Interrupt Position */
#define DEVEPTIMR_BLK_NAKOUTE         (_U_(0x1) << DEVEPTIMR_BLK_NAKOUTE_Pos)  /**< (DEVEPTIMR) NAKed OUT Interrupt Mask */
#define DEVEPTIMR_BLK_NAKINE_Pos      4                                              /**< (DEVEPTIMR) NAKed IN Interrupt Position */
#define DEVEPTIMR_BLK_NAKINE          (_U_(0x1) << DEVEPTIMR_BLK_NAKINE_Pos)   /**< (DEVEPTIMR) NAKed IN Interrupt Mask */
#define DEVEPTIMR_BLK_STALLEDE_Pos    6                                              /**< (DEVEPTIMR) STALLed Interrupt Position */
#define DEVEPTIMR_BLK_STALLEDE        (_U_(0x1) << DEVEPTIMR_BLK_STALLEDE_Pos)  /**< (DEVEPTIMR) STALLed Interrupt Mask */
#define DEVEPTIMR_BLK_NYETDIS_Pos     17                                             /**< (DEVEPTIMR) NYET Token Disable Position */
#define DEVEPTIMR_BLK_NYETDIS         (_U_(0x1) << DEVEPTIMR_BLK_NYETDIS_Pos)  /**< (DEVEPTIMR) NYET Token Disable Mask */
#define DEVEPTIMR_BLK_STALLRQ_Pos     19                                             /**< (DEVEPTIMR) STALL Request Position */
#define DEVEPTIMR_BLK_STALLRQ         (_U_(0x1) << DEVEPTIMR_BLK_STALLRQ_Pos)  /**< (DEVEPTIMR) STALL Request Mask */
#define DEVEPTIMR_BLK_Msk             _U_(0xA005C)                                   /**< (DEVEPTIMR_BLK) Register Mask  */

/* INTRPT mode */
#define DEVEPTIMR_INTRPT_RXSTPE_Pos   2                                              /**< (DEVEPTIMR) Received SETUP Interrupt Position */
#define DEVEPTIMR_INTRPT_RXSTPE       (_U_(0x1) << DEVEPTIMR_INTRPT_RXSTPE_Pos)  /**< (DEVEPTIMR) Received SETUP Interrupt Mask */
#define DEVEPTIMR_INTRPT_NAKOUTE_Pos  3                                              /**< (DEVEPTIMR) NAKed OUT Interrupt Position */
#define DEVEPTIMR_INTRPT_NAKOUTE      (_U_(0x1) << DEVEPTIMR_INTRPT_NAKOUTE_Pos)  /**< (DEVEPTIMR) NAKed OUT Interrupt Mask */
#define DEVEPTIMR_INTRPT_NAKINE_Pos   4                                              /**< (DEVEPTIMR) NAKed IN Interrupt Position */
#define DEVEPTIMR_INTRPT_NAKINE       (_U_(0x1) << DEVEPTIMR_INTRPT_NAKINE_Pos)  /**< (DEVEPTIMR) NAKed IN Interrupt Mask */
#define DEVEPTIMR_INTRPT_STALLEDE_Pos 6                                              /**< (DEVEPTIMR) STALLed Interrupt Position */
#define DEVEPTIMR_INTRPT_STALLEDE     (_U_(0x1) << DEVEPTIMR_INTRPT_STALLEDE_Pos)  /**< (DEVEPTIMR) STALLed Interrupt Mask */
#define DEVEPTIMR_INTRPT_NYETDIS_Pos  17                                             /**< (DEVEPTIMR) NYET Token Disable Position */
#define DEVEPTIMR_INTRPT_NYETDIS      (_U_(0x1) << DEVEPTIMR_INTRPT_NYETDIS_Pos)  /**< (DEVEPTIMR) NYET Token Disable Mask */
#define DEVEPTIMR_INTRPT_STALLRQ_Pos  19                                             /**< (DEVEPTIMR) STALL Request Position */
#define DEVEPTIMR_INTRPT_STALLRQ      (_U_(0x1) << DEVEPTIMR_INTRPT_STALLRQ_Pos)  /**< (DEVEPTIMR) STALL Request Mask */
#define DEVEPTIMR_INTRPT_Msk          _U_(0xA005C)                                   /**< (DEVEPTIMR_INTRPT) Register Mask  */


/* -------- DEVEPTIER : (USBHS Offset: 0x1f0) (/W 32) Device Endpoint Interrupt Enable Register -------- */

#define DEVEPTIER_OFFSET              (0x1F0)                                       /**<  (DEVEPTIER) Device Endpoint Interrupt Enable Register  Offset */

#define DEVEPTIER_TXINES_Pos          0                                              /**< (DEVEPTIER) Transmitted IN Data Interrupt Enable Position */
#define DEVEPTIER_TXINES              (_U_(0x1) << DEVEPTIER_TXINES_Pos)       /**< (DEVEPTIER) Transmitted IN Data Interrupt Enable Mask */
#define DEVEPTIER_RXOUTES_Pos         1                                              /**< (DEVEPTIER) Received OUT Data Interrupt Enable Position */
#define DEVEPTIER_RXOUTES             (_U_(0x1) << DEVEPTIER_RXOUTES_Pos)      /**< (DEVEPTIER) Received OUT Data Interrupt Enable Mask */
#define DEVEPTIER_OVERFES_Pos         5                                              /**< (DEVEPTIER) Overflow Interrupt Enable Position */
#define DEVEPTIER_OVERFES             (_U_(0x1) << DEVEPTIER_OVERFES_Pos)      /**< (DEVEPTIER) Overflow Interrupt Enable Mask */
#define DEVEPTIER_SHORTPACKETES_Pos   7                                              /**< (DEVEPTIER) Short Packet Interrupt Enable Position */
#define DEVEPTIER_SHORTPACKETES       (_U_(0x1) << DEVEPTIER_SHORTPACKETES_Pos)  /**< (DEVEPTIER) Short Packet Interrupt Enable Mask */
#define DEVEPTIER_NBUSYBKES_Pos       12                                             /**< (DEVEPTIER) Number of Busy Banks Interrupt Enable Position */
#define DEVEPTIER_NBUSYBKES           (_U_(0x1) << DEVEPTIER_NBUSYBKES_Pos)    /**< (DEVEPTIER) Number of Busy Banks Interrupt Enable Mask */
#define DEVEPTIER_KILLBKS_Pos         13                                             /**< (DEVEPTIER) Kill IN Bank Position */
#define DEVEPTIER_KILLBKS             (_U_(0x1) << DEVEPTIER_KILLBKS_Pos)      /**< (DEVEPTIER) Kill IN Bank Mask */
#define DEVEPTIER_FIFOCONS_Pos        14                                             /**< (DEVEPTIER) FIFO Control Position */
#define DEVEPTIER_FIFOCONS            (_U_(0x1) << DEVEPTIER_FIFOCONS_Pos)     /**< (DEVEPTIER) FIFO Control Mask */
#define DEVEPTIER_EPDISHDMAS_Pos      16                                             /**< (DEVEPTIER) Endpoint Interrupts Disable HDMA Request Enable Position */
#define DEVEPTIER_EPDISHDMAS          (_U_(0x1) << DEVEPTIER_EPDISHDMAS_Pos)   /**< (DEVEPTIER) Endpoint Interrupts Disable HDMA Request Enable Mask */
#define DEVEPTIER_RSTDTS_Pos          18                                             /**< (DEVEPTIER) Reset Data Toggle Enable Position */
#define DEVEPTIER_RSTDTS              (_U_(0x1) << DEVEPTIER_RSTDTS_Pos)       /**< (DEVEPTIER) Reset Data Toggle Enable Mask */
#define DEVEPTIER_Msk                 _U_(0x570A3)                                   /**< (DEVEPTIER) Register Mask  */

/* CTRL mode */
#define DEVEPTIER_CTRL_RXSTPES_Pos    2                                              /**< (DEVEPTIER) Received SETUP Interrupt Enable Position */
#define DEVEPTIER_CTRL_RXSTPES        (_U_(0x1) << DEVEPTIER_CTRL_RXSTPES_Pos)  /**< (DEVEPTIER) Received SETUP Interrupt Enable Mask */
#define DEVEPTIER_CTRL_NAKOUTES_Pos   3                                              /**< (DEVEPTIER) NAKed OUT Interrupt Enable Position */
#define DEVEPTIER_CTRL_NAKOUTES       (_U_(0x1) << DEVEPTIER_CTRL_NAKOUTES_Pos)  /**< (DEVEPTIER) NAKed OUT Interrupt Enable Mask */
#define DEVEPTIER_CTRL_NAKINES_Pos    4                                              /**< (DEVEPTIER) NAKed IN Interrupt Enable Position */
#define DEVEPTIER_CTRL_NAKINES        (_U_(0x1) << DEVEPTIER_CTRL_NAKINES_Pos)  /**< (DEVEPTIER) NAKed IN Interrupt Enable Mask */
#define DEVEPTIER_CTRL_STALLEDES_Pos  6                                              /**< (DEVEPTIER) STALLed Interrupt Enable Position */
#define DEVEPTIER_CTRL_STALLEDES      (_U_(0x1) << DEVEPTIER_CTRL_STALLEDES_Pos)  /**< (DEVEPTIER) STALLed Interrupt Enable Mask */
#define DEVEPTIER_CTRL_NYETDISS_Pos   17                                             /**< (DEVEPTIER) NYET Token Disable Enable Position */
#define DEVEPTIER_CTRL_NYETDISS       (_U_(0x1) << DEVEPTIER_CTRL_NYETDISS_Pos)  /**< (DEVEPTIER) NYET Token Disable Enable Mask */
#define DEVEPTIER_CTRL_STALLRQS_Pos   19                                             /**< (DEVEPTIER) STALL Request Enable Position */
#define DEVEPTIER_CTRL_STALLRQS       (_U_(0x1) << DEVEPTIER_CTRL_STALLRQS_Pos)  /**< (DEVEPTIER) STALL Request Enable Mask */
#define DEVEPTIER_CTRL_Msk            _U_(0xA005C)                                   /**< (DEVEPTIER_CTRL) Register Mask  */

/* ISO mode */
#define DEVEPTIER_ISO_UNDERFES_Pos    2                                              /**< (DEVEPTIER) Underflow Interrupt Enable Position */
#define DEVEPTIER_ISO_UNDERFES        (_U_(0x1) << DEVEPTIER_ISO_UNDERFES_Pos)  /**< (DEVEPTIER) Underflow Interrupt Enable Mask */
#define DEVEPTIER_ISO_HBISOINERRES_Pos 3                                              /**< (DEVEPTIER) High Bandwidth Isochronous IN Underflow Error Interrupt Enable Position */
#define DEVEPTIER_ISO_HBISOINERRES     (_U_(0x1) << DEVEPTIER_ISO_HBISOINERRES_Pos)  /**< (DEVEPTIER) High Bandwidth Isochronous IN Underflow Error Interrupt Enable Mask */
#define DEVEPTIER_ISO_HBISOFLUSHES_Pos 4                                              /**< (DEVEPTIER) High Bandwidth Isochronous IN Flush Interrupt Enable Position */
#define DEVEPTIER_ISO_HBISOFLUSHES     (_U_(0x1) << DEVEPTIER_ISO_HBISOFLUSHES_Pos)  /**< (DEVEPTIER) High Bandwidth Isochronous IN Flush Interrupt Enable Mask */
#define DEVEPTIER_ISO_CRCERRES_Pos    6                                              /**< (DEVEPTIER) CRC Error Interrupt Enable Position */
#define DEVEPTIER_ISO_CRCERRES        (_U_(0x1) << DEVEPTIER_ISO_CRCERRES_Pos)  /**< (DEVEPTIER) CRC Error Interrupt Enable Mask */
#define DEVEPTIER_ISO_MDATAES_Pos     8                                              /**< (DEVEPTIER) MData Interrupt Enable Position */
#define DEVEPTIER_ISO_MDATAES         (_U_(0x1) << DEVEPTIER_ISO_MDATAES_Pos)  /**< (DEVEPTIER) MData Interrupt Enable Mask */
#define DEVEPTIER_ISO_DATAXES_Pos     9                                              /**< (DEVEPTIER) DataX Interrupt Enable Position */
#define DEVEPTIER_ISO_DATAXES         (_U_(0x1) << DEVEPTIER_ISO_DATAXES_Pos)  /**< (DEVEPTIER) DataX Interrupt Enable Mask */
#define DEVEPTIER_ISO_ERRORTRANSES_Pos 10                                             /**< (DEVEPTIER) Transaction Error Interrupt Enable Position */
#define DEVEPTIER_ISO_ERRORTRANSES     (_U_(0x1) << DEVEPTIER_ISO_ERRORTRANSES_Pos)  /**< (DEVEPTIER) Transaction Error Interrupt Enable Mask */
#define DEVEPTIER_ISO_Msk             _U_(0x75C)                                     /**< (DEVEPTIER_ISO) Register Mask  */

/* BLK mode */
#define DEVEPTIER_BLK_RXSTPES_Pos     2                                              /**< (DEVEPTIER) Received SETUP Interrupt Enable Position */
#define DEVEPTIER_BLK_RXSTPES         (_U_(0x1) << DEVEPTIER_BLK_RXSTPES_Pos)  /**< (DEVEPTIER) Received SETUP Interrupt Enable Mask */
#define DEVEPTIER_BLK_NAKOUTES_Pos    3                                              /**< (DEVEPTIER) NAKed OUT Interrupt Enable Position */
#define DEVEPTIER_BLK_NAKOUTES        (_U_(0x1) << DEVEPTIER_BLK_NAKOUTES_Pos)  /**< (DEVEPTIER) NAKed OUT Interrupt Enable Mask */
#define DEVEPTIER_BLK_NAKINES_Pos     4                                              /**< (DEVEPTIER) NAKed IN Interrupt Enable Position */
#define DEVEPTIER_BLK_NAKINES         (_U_(0x1) << DEVEPTIER_BLK_NAKINES_Pos)  /**< (DEVEPTIER) NAKed IN Interrupt Enable Mask */
#define DEVEPTIER_BLK_STALLEDES_Pos   6                                              /**< (DEVEPTIER) STALLed Interrupt Enable Position */
#define DEVEPTIER_BLK_STALLEDES       (_U_(0x1) << DEVEPTIER_BLK_STALLEDES_Pos)  /**< (DEVEPTIER) STALLed Interrupt Enable Mask */
#define DEVEPTIER_BLK_NYETDISS_Pos    17                                             /**< (DEVEPTIER) NYET Token Disable Enable Position */
#define DEVEPTIER_BLK_NYETDISS        (_U_(0x1) << DEVEPTIER_BLK_NYETDISS_Pos)  /**< (DEVEPTIER) NYET Token Disable Enable Mask */
#define DEVEPTIER_BLK_STALLRQS_Pos    19                                             /**< (DEVEPTIER) STALL Request Enable Position */
#define DEVEPTIER_BLK_STALLRQS        (_U_(0x1) << DEVEPTIER_BLK_STALLRQS_Pos)  /**< (DEVEPTIER) STALL Request Enable Mask */
#define DEVEPTIER_BLK_Msk             _U_(0xA005C)                                   /**< (DEVEPTIER_BLK) Register Mask  */

/* INTRPT mode */
#define DEVEPTIER_INTRPT_RXSTPES_Pos  2                                              /**< (DEVEPTIER) Received SETUP Interrupt Enable Position */
#define DEVEPTIER_INTRPT_RXSTPES      (_U_(0x1) << DEVEPTIER_INTRPT_RXSTPES_Pos)  /**< (DEVEPTIER) Received SETUP Interrupt Enable Mask */
#define DEVEPTIER_INTRPT_NAKOUTES_Pos 3                                              /**< (DEVEPTIER) NAKed OUT Interrupt Enable Position */
#define DEVEPTIER_INTRPT_NAKOUTES     (_U_(0x1) << DEVEPTIER_INTRPT_NAKOUTES_Pos)  /**< (DEVEPTIER) NAKed OUT Interrupt Enable Mask */
#define DEVEPTIER_INTRPT_NAKINES_Pos  4                                              /**< (DEVEPTIER) NAKed IN Interrupt Enable Position */
#define DEVEPTIER_INTRPT_NAKINES      (_U_(0x1) << DEVEPTIER_INTRPT_NAKINES_Pos)  /**< (DEVEPTIER) NAKed IN Interrupt Enable Mask */
#define DEVEPTIER_INTRPT_STALLEDES_Pos 6                                              /**< (DEVEPTIER) STALLed Interrupt Enable Position */
#define DEVEPTIER_INTRPT_STALLEDES     (_U_(0x1) << DEVEPTIER_INTRPT_STALLEDES_Pos)  /**< (DEVEPTIER) STALLed Interrupt Enable Mask */
#define DEVEPTIER_INTRPT_NYETDISS_Pos 17                                             /**< (DEVEPTIER) NYET Token Disable Enable Position */
#define DEVEPTIER_INTRPT_NYETDISS     (_U_(0x1) << DEVEPTIER_INTRPT_NYETDISS_Pos)  /**< (DEVEPTIER) NYET Token Disable Enable Mask */
#define DEVEPTIER_INTRPT_STALLRQS_Pos 19                                             /**< (DEVEPTIER) STALL Request Enable Position */
#define DEVEPTIER_INTRPT_STALLRQS     (_U_(0x1) << DEVEPTIER_INTRPT_STALLRQS_Pos)  /**< (DEVEPTIER) STALL Request Enable Mask */
#define DEVEPTIER_INTRPT_Msk          _U_(0xA005C)                                   /**< (DEVEPTIER_INTRPT) Register Mask  */


/* -------- DEVEPTIDR : (USBHS Offset: 0x220) (/W 32) Device Endpoint Interrupt Disable Register -------- */

#define DEVEPTIDR_OFFSET              (0x220)                                       /**<  (DEVEPTIDR) Device Endpoint Interrupt Disable Register  Offset */

#define DEVEPTIDR_TXINEC_Pos          0                                              /**< (DEVEPTIDR) Transmitted IN Interrupt Clear Position */
#define DEVEPTIDR_TXINEC              (_U_(0x1) << DEVEPTIDR_TXINEC_Pos)       /**< (DEVEPTIDR) Transmitted IN Interrupt Clear Mask */
#define DEVEPTIDR_RXOUTEC_Pos         1                                              /**< (DEVEPTIDR) Received OUT Data Interrupt Clear Position */
#define DEVEPTIDR_RXOUTEC             (_U_(0x1) << DEVEPTIDR_RXOUTEC_Pos)      /**< (DEVEPTIDR) Received OUT Data Interrupt Clear Mask */
#define DEVEPTIDR_OVERFEC_Pos         5                                              /**< (DEVEPTIDR) Overflow Interrupt Clear Position */
#define DEVEPTIDR_OVERFEC             (_U_(0x1) << DEVEPTIDR_OVERFEC_Pos)      /**< (DEVEPTIDR) Overflow Interrupt Clear Mask */
#define DEVEPTIDR_SHORTPACKETEC_Pos   7                                              /**< (DEVEPTIDR) Shortpacket Interrupt Clear Position */
#define DEVEPTIDR_SHORTPACKETEC       (_U_(0x1) << DEVEPTIDR_SHORTPACKETEC_Pos)  /**< (DEVEPTIDR) Shortpacket Interrupt Clear Mask */
#define DEVEPTIDR_NBUSYBKEC_Pos       12                                             /**< (DEVEPTIDR) Number of Busy Banks Interrupt Clear Position */
#define DEVEPTIDR_NBUSYBKEC           (_U_(0x1) << DEVEPTIDR_NBUSYBKEC_Pos)    /**< (DEVEPTIDR) Number of Busy Banks Interrupt Clear Mask */
#define DEVEPTIDR_FIFOCONC_Pos        14                                             /**< (DEVEPTIDR) FIFO Control Clear Position */
#define DEVEPTIDR_FIFOCONC            (_U_(0x1) << DEVEPTIDR_FIFOCONC_Pos)     /**< (DEVEPTIDR) FIFO Control Clear Mask */
#define DEVEPTIDR_EPDISHDMAC_Pos      16                                             /**< (DEVEPTIDR) Endpoint Interrupts Disable HDMA Request Clear Position */
#define DEVEPTIDR_EPDISHDMAC          (_U_(0x1) << DEVEPTIDR_EPDISHDMAC_Pos)   /**< (DEVEPTIDR) Endpoint Interrupts Disable HDMA Request Clear Mask */
#define DEVEPTIDR_Msk                 _U_(0x150A3)                                   /**< (DEVEPTIDR) Register Mask  */

/* CTRL mode */
#define DEVEPTIDR_CTRL_RXSTPEC_Pos    2                                              /**< (DEVEPTIDR) Received SETUP Interrupt Clear Position */
#define DEVEPTIDR_CTRL_RXSTPEC        (_U_(0x1) << DEVEPTIDR_CTRL_RXSTPEC_Pos)  /**< (DEVEPTIDR) Received SETUP Interrupt Clear Mask */
#define DEVEPTIDR_CTRL_NAKOUTEC_Pos   3                                              /**< (DEVEPTIDR) NAKed OUT Interrupt Clear Position */
#define DEVEPTIDR_CTRL_NAKOUTEC       (_U_(0x1) << DEVEPTIDR_CTRL_NAKOUTEC_Pos)  /**< (DEVEPTIDR) NAKed OUT Interrupt Clear Mask */
#define DEVEPTIDR_CTRL_NAKINEC_Pos    4                                              /**< (DEVEPTIDR) NAKed IN Interrupt Clear Position */
#define DEVEPTIDR_CTRL_NAKINEC        (_U_(0x1) << DEVEPTIDR_CTRL_NAKINEC_Pos)  /**< (DEVEPTIDR) NAKed IN Interrupt Clear Mask */
#define DEVEPTIDR_CTRL_STALLEDEC_Pos  6                                              /**< (DEVEPTIDR) STALLed Interrupt Clear Position */
#define DEVEPTIDR_CTRL_STALLEDEC      (_U_(0x1) << DEVEPTIDR_CTRL_STALLEDEC_Pos)  /**< (DEVEPTIDR) STALLed Interrupt Clear Mask */
#define DEVEPTIDR_CTRL_NYETDISC_Pos   17                                             /**< (DEVEPTIDR) NYET Token Disable Clear Position */
#define DEVEPTIDR_CTRL_NYETDISC       (_U_(0x1) << DEVEPTIDR_CTRL_NYETDISC_Pos)  /**< (DEVEPTIDR) NYET Token Disable Clear Mask */
#define DEVEPTIDR_CTRL_STALLRQC_Pos   19                                             /**< (DEVEPTIDR) STALL Request Clear Position */
#define DEVEPTIDR_CTRL_STALLRQC       (_U_(0x1) << DEVEPTIDR_CTRL_STALLRQC_Pos)  /**< (DEVEPTIDR) STALL Request Clear Mask */
#define DEVEPTIDR_CTRL_Msk            _U_(0xA005C)                                   /**< (DEVEPTIDR_CTRL) Register Mask  */

/* ISO mode */
#define DEVEPTIDR_ISO_UNDERFEC_Pos    2                                              /**< (DEVEPTIDR) Underflow Interrupt Clear Position */
#define DEVEPTIDR_ISO_UNDERFEC        (_U_(0x1) << DEVEPTIDR_ISO_UNDERFEC_Pos)  /**< (DEVEPTIDR) Underflow Interrupt Clear Mask */
#define DEVEPTIDR_ISO_HBISOINERREC_Pos 3                                              /**< (DEVEPTIDR) High Bandwidth Isochronous IN Underflow Error Interrupt Clear Position */
#define DEVEPTIDR_ISO_HBISOINERREC     (_U_(0x1) << DEVEPTIDR_ISO_HBISOINERREC_Pos)  /**< (DEVEPTIDR) High Bandwidth Isochronous IN Underflow Error Interrupt Clear Mask */
#define DEVEPTIDR_ISO_HBISOFLUSHEC_Pos 4                                              /**< (DEVEPTIDR) High Bandwidth Isochronous IN Flush Interrupt Clear Position */
#define DEVEPTIDR_ISO_HBISOFLUSHEC     (_U_(0x1) << DEVEPTIDR_ISO_HBISOFLUSHEC_Pos)  /**< (DEVEPTIDR) High Bandwidth Isochronous IN Flush Interrupt Clear Mask */
#define DEVEPTIDR_ISO_MDATAEC_Pos     8                                              /**< (DEVEPTIDR) MData Interrupt Clear Position */
#define DEVEPTIDR_ISO_MDATAEC         (_U_(0x1) << DEVEPTIDR_ISO_MDATAEC_Pos)  /**< (DEVEPTIDR) MData Interrupt Clear Mask */
#define DEVEPTIDR_ISO_DATAXEC_Pos     9                                              /**< (DEVEPTIDR) DataX Interrupt Clear Position */
#define DEVEPTIDR_ISO_DATAXEC         (_U_(0x1) << DEVEPTIDR_ISO_DATAXEC_Pos)  /**< (DEVEPTIDR) DataX Interrupt Clear Mask */
#define DEVEPTIDR_ISO_ERRORTRANSEC_Pos 10                                             /**< (DEVEPTIDR) Transaction Error Interrupt Clear Position */
#define DEVEPTIDR_ISO_ERRORTRANSEC     (_U_(0x1) << DEVEPTIDR_ISO_ERRORTRANSEC_Pos)  /**< (DEVEPTIDR) Transaction Error Interrupt Clear Mask */
#define DEVEPTIDR_ISO_Msk             _U_(0x71C)                                     /**< (DEVEPTIDR_ISO) Register Mask  */

/* BLK mode */
#define DEVEPTIDR_BLK_RXSTPEC_Pos     2                                              /**< (DEVEPTIDR) Received SETUP Interrupt Clear Position */
#define DEVEPTIDR_BLK_RXSTPEC         (_U_(0x1) << DEVEPTIDR_BLK_RXSTPEC_Pos)  /**< (DEVEPTIDR) Received SETUP Interrupt Clear Mask */
#define DEVEPTIDR_BLK_NAKOUTEC_Pos    3                                              /**< (DEVEPTIDR) NAKed OUT Interrupt Clear Position */
#define DEVEPTIDR_BLK_NAKOUTEC        (_U_(0x1) << DEVEPTIDR_BLK_NAKOUTEC_Pos)  /**< (DEVEPTIDR) NAKed OUT Interrupt Clear Mask */
#define DEVEPTIDR_BLK_NAKINEC_Pos     4                                              /**< (DEVEPTIDR) NAKed IN Interrupt Clear Position */
#define DEVEPTIDR_BLK_NAKINEC         (_U_(0x1) << DEVEPTIDR_BLK_NAKINEC_Pos)  /**< (DEVEPTIDR) NAKed IN Interrupt Clear Mask */
#define DEVEPTIDR_BLK_STALLEDEC_Pos   6                                              /**< (DEVEPTIDR) STALLed Interrupt Clear Position */
#define DEVEPTIDR_BLK_STALLEDEC       (_U_(0x1) << DEVEPTIDR_BLK_STALLEDEC_Pos)  /**< (DEVEPTIDR) STALLed Interrupt Clear Mask */
#define DEVEPTIDR_BLK_NYETDISC_Pos    17                                             /**< (DEVEPTIDR) NYET Token Disable Clear Position */
#define DEVEPTIDR_BLK_NYETDISC        (_U_(0x1) << DEVEPTIDR_BLK_NYETDISC_Pos)  /**< (DEVEPTIDR) NYET Token Disable Clear Mask */
#define DEVEPTIDR_BLK_STALLRQC_Pos    19                                             /**< (DEVEPTIDR) STALL Request Clear Position */
#define DEVEPTIDR_BLK_STALLRQC        (_U_(0x1) << DEVEPTIDR_BLK_STALLRQC_Pos)  /**< (DEVEPTIDR) STALL Request Clear Mask */
#define DEVEPTIDR_BLK_Msk             _U_(0xA005C)                                   /**< (DEVEPTIDR_BLK) Register Mask  */

/* INTRPT mode */
#define DEVEPTIDR_INTRPT_RXSTPEC_Pos  2                                              /**< (DEVEPTIDR) Received SETUP Interrupt Clear Position */
#define DEVEPTIDR_INTRPT_RXSTPEC      (_U_(0x1) << DEVEPTIDR_INTRPT_RXSTPEC_Pos)  /**< (DEVEPTIDR) Received SETUP Interrupt Clear Mask */
#define DEVEPTIDR_INTRPT_NAKOUTEC_Pos 3                                              /**< (DEVEPTIDR) NAKed OUT Interrupt Clear Position */
#define DEVEPTIDR_INTRPT_NAKOUTEC     (_U_(0x1) << DEVEPTIDR_INTRPT_NAKOUTEC_Pos)  /**< (DEVEPTIDR) NAKed OUT Interrupt Clear Mask */
#define DEVEPTIDR_INTRPT_NAKINEC_Pos  4                                              /**< (DEVEPTIDR) NAKed IN Interrupt Clear Position */
#define DEVEPTIDR_INTRPT_NAKINEC      (_U_(0x1) << DEVEPTIDR_INTRPT_NAKINEC_Pos)  /**< (DEVEPTIDR) NAKed IN Interrupt Clear Mask */
#define DEVEPTIDR_INTRPT_STALLEDEC_Pos 6                                              /**< (DEVEPTIDR) STALLed Interrupt Clear Position */
#define DEVEPTIDR_INTRPT_STALLEDEC     (_U_(0x1) << DEVEPTIDR_INTRPT_STALLEDEC_Pos)  /**< (DEVEPTIDR) STALLed Interrupt Clear Mask */
#define DEVEPTIDR_INTRPT_NYETDISC_Pos 17                                             /**< (DEVEPTIDR) NYET Token Disable Clear Position */
#define DEVEPTIDR_INTRPT_NYETDISC     (_U_(0x1) << DEVEPTIDR_INTRPT_NYETDISC_Pos)  /**< (DEVEPTIDR) NYET Token Disable Clear Mask */
#define DEVEPTIDR_INTRPT_STALLRQC_Pos 19                                             /**< (DEVEPTIDR) STALL Request Clear Position */
#define DEVEPTIDR_INTRPT_STALLRQC     (_U_(0x1) << DEVEPTIDR_INTRPT_STALLRQC_Pos)  /**< (DEVEPTIDR) STALL Request Clear Mask */
#define DEVEPTIDR_INTRPT_Msk          _U_(0xA005C)                                   /**< (DEVEPTIDR_INTRPT) Register Mask  */


/* -------- HSTCTRL : (USBHS Offset: 0x400) (R/W 32) Host General Control Register -------- */

#define HSTCTRL_OFFSET                (0x400)                                       /**<  (HSTCTRL) Host General Control Register  Offset */

#define HSTCTRL_SOFE_Pos              8                                              /**< (HSTCTRL) Start of Frame Generation Enable Position */
#define HSTCTRL_SOFE                  (_U_(0x1) << HSTCTRL_SOFE_Pos)           /**< (HSTCTRL) Start of Frame Generation Enable Mask */
#define HSTCTRL_RESET_Pos             9                                              /**< (HSTCTRL) Send USB Reset Position */
#define HSTCTRL_RESET                 (_U_(0x1) << HSTCTRL_RESET_Pos)          /**< (HSTCTRL) Send USB Reset Mask */
#define HSTCTRL_RESUME_Pos            10                                             /**< (HSTCTRL) Send USB Resume Position */
#define HSTCTRL_RESUME                (_U_(0x1) << HSTCTRL_RESUME_Pos)         /**< (HSTCTRL) Send USB Resume Mask */
#define HSTCTRL_SPDCONF_Pos           12                                             /**< (HSTCTRL) Mode Configuration Position */
#define HSTCTRL_SPDCONF               (_U_(0x3) << HSTCTRL_SPDCONF_Pos)        /**< (HSTCTRL) Mode Configuration Mask */
#define   HSTCTRL_SPDCONF_NORMAL_Val  _U_(0x0)                                       /**< (HSTCTRL) The host starts in Full-speed mode and performs a high-speed reset to switch to High-speed mode if the downstream peripheral is high-speed capable.  */
#define   HSTCTRL_SPDCONF_LOW_POWER_Val _U_(0x1)                                       /**< (HSTCTRL) For a better consumption, if high speed is not needed.  */
#define   HSTCTRL_SPDCONF_HIGH_SPEED_Val _U_(0x2)                                       /**< (HSTCTRL) Forced high speed.  */
#define   HSTCTRL_SPDCONF_FORCED_FS_Val _U_(0x3)                                       /**< (HSTCTRL) The host remains in Full-speed mode whatever the peripheral speed capability.  */
#define HSTCTRL_SPDCONF_NORMAL        (HSTCTRL_SPDCONF_NORMAL_Val << HSTCTRL_SPDCONF_Pos)  /**< (HSTCTRL) The host starts in Full-speed mode and performs a high-speed reset to switch to High-speed mode if the downstream peripheral is high-speed capable. Position  */
#define HSTCTRL_SPDCONF_LOW_POWER     (HSTCTRL_SPDCONF_LOW_POWER_Val << HSTCTRL_SPDCONF_Pos)  /**< (HSTCTRL) For a better consumption, if high speed is not needed. Position  */
#define HSTCTRL_SPDCONF_HIGH_SPEED    (HSTCTRL_SPDCONF_HIGH_SPEED_Val << HSTCTRL_SPDCONF_Pos)  /**< (HSTCTRL) Forced high speed. Position  */
#define HSTCTRL_SPDCONF_FORCED_FS     (HSTCTRL_SPDCONF_FORCED_FS_Val << HSTCTRL_SPDCONF_Pos)  /**< (HSTCTRL) The host remains in Full-speed mode whatever the peripheral speed capability. Position  */
#define HSTCTRL_Msk                   _U_(0x3700)                                    /**< (HSTCTRL) Register Mask  */


/* -------- HSTISR : (USBHS Offset: 0x404) (R/ 32) Host Global Interrupt Status Register -------- */

#define HSTISR_OFFSET                 (0x404)                                       /**<  (HSTISR) Host Global Interrupt Status Register  Offset */

#define HSTISR_DCONNI_Pos             0                                              /**< (HSTISR) Device Connection Interrupt Position */
#define HSTISR_DCONNI                 (_U_(0x1) << HSTISR_DCONNI_Pos)          /**< (HSTISR) Device Connection Interrupt Mask */
#define HSTISR_DDISCI_Pos             1                                              /**< (HSTISR) Device Disconnection Interrupt Position */
#define HSTISR_DDISCI                 (_U_(0x1) << HSTISR_DDISCI_Pos)          /**< (HSTISR) Device Disconnection Interrupt Mask */
#define HSTISR_RSTI_Pos               2                                              /**< (HSTISR) USB Reset Sent Interrupt Position */
#define HSTISR_RSTI                   (_U_(0x1) << HSTISR_RSTI_Pos)            /**< (HSTISR) USB Reset Sent Interrupt Mask */
#define HSTISR_RSMEDI_Pos             3                                              /**< (HSTISR) Downstream Resume Sent Interrupt Position */
#define HSTISR_RSMEDI                 (_U_(0x1) << HSTISR_RSMEDI_Pos)          /**< (HSTISR) Downstream Resume Sent Interrupt Mask */
#define HSTISR_RXRSMI_Pos             4                                              /**< (HSTISR) Upstream Resume Received Interrupt Position */
#define HSTISR_RXRSMI                 (_U_(0x1) << HSTISR_RXRSMI_Pos)          /**< (HSTISR) Upstream Resume Received Interrupt Mask */
#define HSTISR_HSOFI_Pos              5                                              /**< (HSTISR) Host Start of Frame Interrupt Position */
#define HSTISR_HSOFI                  (_U_(0x1) << HSTISR_HSOFI_Pos)           /**< (HSTISR) Host Start of Frame Interrupt Mask */
#define HSTISR_HWUPI_Pos              6                                              /**< (HSTISR) Host Wake-Up Interrupt Position */
#define HSTISR_HWUPI                  (_U_(0x1) << HSTISR_HWUPI_Pos)           /**< (HSTISR) Host Wake-Up Interrupt Mask */
#define HSTISR_PEP_0_Pos              8                                              /**< (HSTISR) Pipe 0 Interrupt Position */
#define HSTISR_PEP_0                  (_U_(0x1) << HSTISR_PEP_0_Pos)           /**< (HSTISR) Pipe 0 Interrupt Mask */
#define HSTISR_PEP_1_Pos              9                                              /**< (HSTISR) Pipe 1 Interrupt Position */
#define HSTISR_PEP_1                  (_U_(0x1) << HSTISR_PEP_1_Pos)           /**< (HSTISR) Pipe 1 Interrupt Mask */
#define HSTISR_PEP_2_Pos              10                                             /**< (HSTISR) Pipe 2 Interrupt Position */
#define HSTISR_PEP_2                  (_U_(0x1) << HSTISR_PEP_2_Pos)           /**< (HSTISR) Pipe 2 Interrupt Mask */
#define HSTISR_PEP_3_Pos              11                                             /**< (HSTISR) Pipe 3 Interrupt Position */
#define HSTISR_PEP_3                  (_U_(0x1) << HSTISR_PEP_3_Pos)           /**< (HSTISR) Pipe 3 Interrupt Mask */
#define HSTISR_PEP_4_Pos              12                                             /**< (HSTISR) Pipe 4 Interrupt Position */
#define HSTISR_PEP_4                  (_U_(0x1) << HSTISR_PEP_4_Pos)           /**< (HSTISR) Pipe 4 Interrupt Mask */
#define HSTISR_PEP_5_Pos              13                                             /**< (HSTISR) Pipe 5 Interrupt Position */
#define HSTISR_PEP_5                  (_U_(0x1) << HSTISR_PEP_5_Pos)           /**< (HSTISR) Pipe 5 Interrupt Mask */
#define HSTISR_PEP_6_Pos              14                                             /**< (HSTISR) Pipe 6 Interrupt Position */
#define HSTISR_PEP_6                  (_U_(0x1) << HSTISR_PEP_6_Pos)           /**< (HSTISR) Pipe 6 Interrupt Mask */
#define HSTISR_PEP_7_Pos              15                                             /**< (HSTISR) Pipe 7 Interrupt Position */
#define HSTISR_PEP_7                  (_U_(0x1) << HSTISR_PEP_7_Pos)           /**< (HSTISR) Pipe 7 Interrupt Mask */
#define HSTISR_PEP_8_Pos              16                                             /**< (HSTISR) Pipe 8 Interrupt Position */
#define HSTISR_PEP_8                  (_U_(0x1) << HSTISR_PEP_8_Pos)           /**< (HSTISR) Pipe 8 Interrupt Mask */
#define HSTISR_PEP_9_Pos              17                                             /**< (HSTISR) Pipe 9 Interrupt Position */
#define HSTISR_PEP_9                  (_U_(0x1) << HSTISR_PEP_9_Pos)           /**< (HSTISR) Pipe 9 Interrupt Mask */
#define HSTISR_DMA_0_Pos              25                                             /**< (HSTISR) DMA Channel 0 Interrupt Position */
#define HSTISR_DMA_0                  (_U_(0x1) << HSTISR_DMA_0_Pos)           /**< (HSTISR) DMA Channel 0 Interrupt Mask */
#define HSTISR_DMA_1_Pos              26                                             /**< (HSTISR) DMA Channel 1 Interrupt Position */
#define HSTISR_DMA_1                  (_U_(0x1) << HSTISR_DMA_1_Pos)           /**< (HSTISR) DMA Channel 1 Interrupt Mask */
#define HSTISR_DMA_2_Pos              27                                             /**< (HSTISR) DMA Channel 2 Interrupt Position */
#define HSTISR_DMA_2                  (_U_(0x1) << HSTISR_DMA_2_Pos)           /**< (HSTISR) DMA Channel 2 Interrupt Mask */
#define HSTISR_DMA_3_Pos              28                                             /**< (HSTISR) DMA Channel 3 Interrupt Position */
#define HSTISR_DMA_3                  (_U_(0x1) << HSTISR_DMA_3_Pos)           /**< (HSTISR) DMA Channel 3 Interrupt Mask */
#define HSTISR_DMA_4_Pos              29                                             /**< (HSTISR) DMA Channel 4 Interrupt Position */
#define HSTISR_DMA_4                  (_U_(0x1) << HSTISR_DMA_4_Pos)           /**< (HSTISR) DMA Channel 4 Interrupt Mask */
#define HSTISR_DMA_5_Pos              30                                             /**< (HSTISR) DMA Channel 5 Interrupt Position */
#define HSTISR_DMA_5                  (_U_(0x1) << HSTISR_DMA_5_Pos)           /**< (HSTISR) DMA Channel 5 Interrupt Mask */
#define HSTISR_DMA_6_Pos              31                                             /**< (HSTISR) DMA Channel 6 Interrupt Position */
#define HSTISR_DMA_6                  (_U_(0x1) << HSTISR_DMA_6_Pos)           /**< (HSTISR) DMA Channel 6 Interrupt Mask */
#define HSTISR_Msk                    _U_(0xFE03FF7F)                                /**< (HSTISR) Register Mask  */

#define HSTISR_PEP__Pos               8                                              /**< (HSTISR Position) Pipe x Interrupt */
#define HSTISR_PEP_                   (_U_(0x3FF) << HSTISR_PEP__Pos)          /**< (HSTISR Mask) PEP_ */
#define HSTISR_DMA__Pos               25                                             /**< (HSTISR Position) DMA Channel 6 Interrupt */
#define HSTISR_DMA_                   (_U_(0x7F) << HSTISR_DMA__Pos)           /**< (HSTISR Mask) DMA_ */

/* -------- HSTICR : (USBHS Offset: 0x408) (/W 32) Host Global Interrupt Clear Register -------- */

#define HSTICR_OFFSET                 (0x408)                                       /**<  (HSTICR) Host Global Interrupt Clear Register  Offset */

#define HSTICR_DCONNIC_Pos            0                                              /**< (HSTICR) Device Connection Interrupt Clear Position */
#define HSTICR_DCONNIC                (_U_(0x1) << HSTICR_DCONNIC_Pos)         /**< (HSTICR) Device Connection Interrupt Clear Mask */
#define HSTICR_DDISCIC_Pos            1                                              /**< (HSTICR) Device Disconnection Interrupt Clear Position */
#define HSTICR_DDISCIC                (_U_(0x1) << HSTICR_DDISCIC_Pos)         /**< (HSTICR) Device Disconnection Interrupt Clear Mask */
#define HSTICR_RSTIC_Pos              2                                              /**< (HSTICR) USB Reset Sent Interrupt Clear Position */
#define HSTICR_RSTIC                  (_U_(0x1) << HSTICR_RSTIC_Pos)           /**< (HSTICR) USB Reset Sent Interrupt Clear Mask */
#define HSTICR_RSMEDIC_Pos            3                                              /**< (HSTICR) Downstream Resume Sent Interrupt Clear Position */
#define HSTICR_RSMEDIC                (_U_(0x1) << HSTICR_RSMEDIC_Pos)         /**< (HSTICR) Downstream Resume Sent Interrupt Clear Mask */
#define HSTICR_RXRSMIC_Pos            4                                              /**< (HSTICR) Upstream Resume Received Interrupt Clear Position */
#define HSTICR_RXRSMIC                (_U_(0x1) << HSTICR_RXRSMIC_Pos)         /**< (HSTICR) Upstream Resume Received Interrupt Clear Mask */
#define HSTICR_HSOFIC_Pos             5                                              /**< (HSTICR) Host Start of Frame Interrupt Clear Position */
#define HSTICR_HSOFIC                 (_U_(0x1) << HSTICR_HSOFIC_Pos)          /**< (HSTICR) Host Start of Frame Interrupt Clear Mask */
#define HSTICR_HWUPIC_Pos             6                                              /**< (HSTICR) Host Wake-Up Interrupt Clear Position */
#define HSTICR_HWUPIC                 (_U_(0x1) << HSTICR_HWUPIC_Pos)          /**< (HSTICR) Host Wake-Up Interrupt Clear Mask */
#define HSTICR_Msk                    _U_(0x7F)                                      /**< (HSTICR) Register Mask  */


/* -------- HSTIFR : (USBHS Offset: 0x40c) (/W 32) Host Global Interrupt Set Register -------- */

#define HSTIFR_OFFSET                 (0x40C)                                       /**<  (HSTIFR) Host Global Interrupt Set Register  Offset */

#define HSTIFR_DCONNIS_Pos            0                                              /**< (HSTIFR) Device Connection Interrupt Set Position */
#define HSTIFR_DCONNIS                (_U_(0x1) << HSTIFR_DCONNIS_Pos)         /**< (HSTIFR) Device Connection Interrupt Set Mask */
#define HSTIFR_DDISCIS_Pos            1                                              /**< (HSTIFR) Device Disconnection Interrupt Set Position */
#define HSTIFR_DDISCIS                (_U_(0x1) << HSTIFR_DDISCIS_Pos)         /**< (HSTIFR) Device Disconnection Interrupt Set Mask */
#define HSTIFR_RSTIS_Pos              2                                              /**< (HSTIFR) USB Reset Sent Interrupt Set Position */
#define HSTIFR_RSTIS                  (_U_(0x1) << HSTIFR_RSTIS_Pos)           /**< (HSTIFR) USB Reset Sent Interrupt Set Mask */
#define HSTIFR_RSMEDIS_Pos            3                                              /**< (HSTIFR) Downstream Resume Sent Interrupt Set Position */
#define HSTIFR_RSMEDIS                (_U_(0x1) << HSTIFR_RSMEDIS_Pos)         /**< (HSTIFR) Downstream Resume Sent Interrupt Set Mask */
#define HSTIFR_RXRSMIS_Pos            4                                              /**< (HSTIFR) Upstream Resume Received Interrupt Set Position */
#define HSTIFR_RXRSMIS                (_U_(0x1) << HSTIFR_RXRSMIS_Pos)         /**< (HSTIFR) Upstream Resume Received Interrupt Set Mask */
#define HSTIFR_HSOFIS_Pos             5                                              /**< (HSTIFR) Host Start of Frame Interrupt Set Position */
#define HSTIFR_HSOFIS                 (_U_(0x1) << HSTIFR_HSOFIS_Pos)          /**< (HSTIFR) Host Start of Frame Interrupt Set Mask */
#define HSTIFR_HWUPIS_Pos             6                                              /**< (HSTIFR) Host Wake-Up Interrupt Set Position */
#define HSTIFR_HWUPIS                 (_U_(0x1) << HSTIFR_HWUPIS_Pos)          /**< (HSTIFR) Host Wake-Up Interrupt Set Mask */
#define HSTIFR_DMA_0_Pos              25                                             /**< (HSTIFR) DMA Channel 0 Interrupt Set Position */
#define HSTIFR_DMA_0                  (_U_(0x1) << HSTIFR_DMA_0_Pos)           /**< (HSTIFR) DMA Channel 0 Interrupt Set Mask */
#define HSTIFR_DMA_1_Pos              26                                             /**< (HSTIFR) DMA Channel 1 Interrupt Set Position */
#define HSTIFR_DMA_1                  (_U_(0x1) << HSTIFR_DMA_1_Pos)           /**< (HSTIFR) DMA Channel 1 Interrupt Set Mask */
#define HSTIFR_DMA_2_Pos              27                                             /**< (HSTIFR) DMA Channel 2 Interrupt Set Position */
#define HSTIFR_DMA_2                  (_U_(0x1) << HSTIFR_DMA_2_Pos)           /**< (HSTIFR) DMA Channel 2 Interrupt Set Mask */
#define HSTIFR_DMA_3_Pos              28                                             /**< (HSTIFR) DMA Channel 3 Interrupt Set Position */
#define HSTIFR_DMA_3                  (_U_(0x1) << HSTIFR_DMA_3_Pos)           /**< (HSTIFR) DMA Channel 3 Interrupt Set Mask */
#define HSTIFR_DMA_4_Pos              29                                             /**< (HSTIFR) DMA Channel 4 Interrupt Set Position */
#define HSTIFR_DMA_4                  (_U_(0x1) << HSTIFR_DMA_4_Pos)           /**< (HSTIFR) DMA Channel 4 Interrupt Set Mask */
#define HSTIFR_DMA_5_Pos              30                                             /**< (HSTIFR) DMA Channel 5 Interrupt Set Position */
#define HSTIFR_DMA_5                  (_U_(0x1) << HSTIFR_DMA_5_Pos)           /**< (HSTIFR) DMA Channel 5 Interrupt Set Mask */
#define HSTIFR_DMA_6_Pos              31                                             /**< (HSTIFR) DMA Channel 6 Interrupt Set Position */
#define HSTIFR_DMA_6                  (_U_(0x1) << HSTIFR_DMA_6_Pos)           /**< (HSTIFR) DMA Channel 6 Interrupt Set Mask */
#define HSTIFR_Msk                    _U_(0xFE00007F)                                /**< (HSTIFR) Register Mask  */

#define HSTIFR_DMA__Pos               25                                             /**< (HSTIFR Position) DMA Channel 6 Interrupt Set */
#define HSTIFR_DMA_                   (_U_(0x7F) << HSTIFR_DMA__Pos)           /**< (HSTIFR Mask) DMA_ */

/* -------- HSTIMR : (USBHS Offset: 0x410) (R/ 32) Host Global Interrupt Mask Register -------- */

#define HSTIMR_OFFSET                 (0x410)                                       /**<  (HSTIMR) Host Global Interrupt Mask Register  Offset */

#define HSTIMR_DCONNIE_Pos            0                                              /**< (HSTIMR) Device Connection Interrupt Enable Position */
#define HSTIMR_DCONNIE                (_U_(0x1) << HSTIMR_DCONNIE_Pos)         /**< (HSTIMR) Device Connection Interrupt Enable Mask */
#define HSTIMR_DDISCIE_Pos            1                                              /**< (HSTIMR) Device Disconnection Interrupt Enable Position */
#define HSTIMR_DDISCIE                (_U_(0x1) << HSTIMR_DDISCIE_Pos)         /**< (HSTIMR) Device Disconnection Interrupt Enable Mask */
#define HSTIMR_RSTIE_Pos              2                                              /**< (HSTIMR) USB Reset Sent Interrupt Enable Position */
#define HSTIMR_RSTIE                  (_U_(0x1) << HSTIMR_RSTIE_Pos)           /**< (HSTIMR) USB Reset Sent Interrupt Enable Mask */
#define HSTIMR_RSMEDIE_Pos            3                                              /**< (HSTIMR) Downstream Resume Sent Interrupt Enable Position */
#define HSTIMR_RSMEDIE                (_U_(0x1) << HSTIMR_RSMEDIE_Pos)         /**< (HSTIMR) Downstream Resume Sent Interrupt Enable Mask */
#define HSTIMR_RXRSMIE_Pos            4                                              /**< (HSTIMR) Upstream Resume Received Interrupt Enable Position */
#define HSTIMR_RXRSMIE                (_U_(0x1) << HSTIMR_RXRSMIE_Pos)         /**< (HSTIMR) Upstream Resume Received Interrupt Enable Mask */
#define HSTIMR_HSOFIE_Pos             5                                              /**< (HSTIMR) Host Start of Frame Interrupt Enable Position */
#define HSTIMR_HSOFIE                 (_U_(0x1) << HSTIMR_HSOFIE_Pos)          /**< (HSTIMR) Host Start of Frame Interrupt Enable Mask */
#define HSTIMR_HWUPIE_Pos             6                                              /**< (HSTIMR) Host Wake-Up Interrupt Enable Position */
#define HSTIMR_HWUPIE                 (_U_(0x1) << HSTIMR_HWUPIE_Pos)          /**< (HSTIMR) Host Wake-Up Interrupt Enable Mask */
#define HSTIMR_PEP_0_Pos              8                                              /**< (HSTIMR) Pipe 0 Interrupt Enable Position */
#define HSTIMR_PEP_0                  (_U_(0x1) << HSTIMR_PEP_0_Pos)           /**< (HSTIMR) Pipe 0 Interrupt Enable Mask */
#define HSTIMR_PEP_1_Pos              9                                              /**< (HSTIMR) Pipe 1 Interrupt Enable Position */
#define HSTIMR_PEP_1                  (_U_(0x1) << HSTIMR_PEP_1_Pos)           /**< (HSTIMR) Pipe 1 Interrupt Enable Mask */
#define HSTIMR_PEP_2_Pos              10                                             /**< (HSTIMR) Pipe 2 Interrupt Enable Position */
#define HSTIMR_PEP_2                  (_U_(0x1) << HSTIMR_PEP_2_Pos)           /**< (HSTIMR) Pipe 2 Interrupt Enable Mask */
#define HSTIMR_PEP_3_Pos              11                                             /**< (HSTIMR) Pipe 3 Interrupt Enable Position */
#define HSTIMR_PEP_3                  (_U_(0x1) << HSTIMR_PEP_3_Pos)           /**< (HSTIMR) Pipe 3 Interrupt Enable Mask */
#define HSTIMR_PEP_4_Pos              12                                             /**< (HSTIMR) Pipe 4 Interrupt Enable Position */
#define HSTIMR_PEP_4                  (_U_(0x1) << HSTIMR_PEP_4_Pos)           /**< (HSTIMR) Pipe 4 Interrupt Enable Mask */
#define HSTIMR_PEP_5_Pos              13                                             /**< (HSTIMR) Pipe 5 Interrupt Enable Position */
#define HSTIMR_PEP_5                  (_U_(0x1) << HSTIMR_PEP_5_Pos)           /**< (HSTIMR) Pipe 5 Interrupt Enable Mask */
#define HSTIMR_PEP_6_Pos              14                                             /**< (HSTIMR) Pipe 6 Interrupt Enable Position */
#define HSTIMR_PEP_6                  (_U_(0x1) << HSTIMR_PEP_6_Pos)           /**< (HSTIMR) Pipe 6 Interrupt Enable Mask */
#define HSTIMR_PEP_7_Pos              15                                             /**< (HSTIMR) Pipe 7 Interrupt Enable Position */
#define HSTIMR_PEP_7                  (_U_(0x1) << HSTIMR_PEP_7_Pos)           /**< (HSTIMR) Pipe 7 Interrupt Enable Mask */
#define HSTIMR_PEP_8_Pos              16                                             /**< (HSTIMR) Pipe 8 Interrupt Enable Position */
#define HSTIMR_PEP_8                  (_U_(0x1) << HSTIMR_PEP_8_Pos)           /**< (HSTIMR) Pipe 8 Interrupt Enable Mask */
#define HSTIMR_PEP_9_Pos              17                                             /**< (HSTIMR) Pipe 9 Interrupt Enable Position */
#define HSTIMR_PEP_9                  (_U_(0x1) << HSTIMR_PEP_9_Pos)           /**< (HSTIMR) Pipe 9 Interrupt Enable Mask */
#define HSTIMR_DMA_0_Pos              25                                             /**< (HSTIMR) DMA Channel 0 Interrupt Enable Position */
#define HSTIMR_DMA_0                  (_U_(0x1) << HSTIMR_DMA_0_Pos)           /**< (HSTIMR) DMA Channel 0 Interrupt Enable Mask */
#define HSTIMR_DMA_1_Pos              26                                             /**< (HSTIMR) DMA Channel 1 Interrupt Enable Position */
#define HSTIMR_DMA_1                  (_U_(0x1) << HSTIMR_DMA_1_Pos)           /**< (HSTIMR) DMA Channel 1 Interrupt Enable Mask */
#define HSTIMR_DMA_2_Pos              27                                             /**< (HSTIMR) DMA Channel 2 Interrupt Enable Position */
#define HSTIMR_DMA_2                  (_U_(0x1) << HSTIMR_DMA_2_Pos)           /**< (HSTIMR) DMA Channel 2 Interrupt Enable Mask */
#define HSTIMR_DMA_3_Pos              28                                             /**< (HSTIMR) DMA Channel 3 Interrupt Enable Position */
#define HSTIMR_DMA_3                  (_U_(0x1) << HSTIMR_DMA_3_Pos)           /**< (HSTIMR) DMA Channel 3 Interrupt Enable Mask */
#define HSTIMR_DMA_4_Pos              29                                             /**< (HSTIMR) DMA Channel 4 Interrupt Enable Position */
#define HSTIMR_DMA_4                  (_U_(0x1) << HSTIMR_DMA_4_Pos)           /**< (HSTIMR) DMA Channel 4 Interrupt Enable Mask */
#define HSTIMR_DMA_5_Pos              30                                             /**< (HSTIMR) DMA Channel 5 Interrupt Enable Position */
#define HSTIMR_DMA_5                  (_U_(0x1) << HSTIMR_DMA_5_Pos)           /**< (HSTIMR) DMA Channel 5 Interrupt Enable Mask */
#define HSTIMR_DMA_6_Pos              31                                             /**< (HSTIMR) DMA Channel 6 Interrupt Enable Position */
#define HSTIMR_DMA_6                  (_U_(0x1) << HSTIMR_DMA_6_Pos)           /**< (HSTIMR) DMA Channel 6 Interrupt Enable Mask */
#define HSTIMR_Msk                    _U_(0xFE03FF7F)                                /**< (HSTIMR) Register Mask  */

#define HSTIMR_PEP__Pos               8                                              /**< (HSTIMR Position) Pipe x Interrupt Enable */
#define HSTIMR_PEP_                   (_U_(0x3FF) << HSTIMR_PEP__Pos)          /**< (HSTIMR Mask) PEP_ */
#define HSTIMR_DMA__Pos               25                                             /**< (HSTIMR Position) DMA Channel 6 Interrupt Enable */
#define HSTIMR_DMA_                   (_U_(0x7F) << HSTIMR_DMA__Pos)           /**< (HSTIMR Mask) DMA_ */

/* -------- HSTIDR : (USBHS Offset: 0x414) (/W 32) Host Global Interrupt Disable Register -------- */

#define HSTIDR_OFFSET                 (0x414)                                       /**<  (HSTIDR) Host Global Interrupt Disable Register  Offset */

#define HSTIDR_DCONNIEC_Pos           0                                              /**< (HSTIDR) Device Connection Interrupt Disable Position */
#define HSTIDR_DCONNIEC               (_U_(0x1) << HSTIDR_DCONNIEC_Pos)        /**< (HSTIDR) Device Connection Interrupt Disable Mask */
#define HSTIDR_DDISCIEC_Pos           1                                              /**< (HSTIDR) Device Disconnection Interrupt Disable Position */
#define HSTIDR_DDISCIEC               (_U_(0x1) << HSTIDR_DDISCIEC_Pos)        /**< (HSTIDR) Device Disconnection Interrupt Disable Mask */
#define HSTIDR_RSTIEC_Pos             2                                              /**< (HSTIDR) USB Reset Sent Interrupt Disable Position */
#define HSTIDR_RSTIEC                 (_U_(0x1) << HSTIDR_RSTIEC_Pos)          /**< (HSTIDR) USB Reset Sent Interrupt Disable Mask */
#define HSTIDR_RSMEDIEC_Pos           3                                              /**< (HSTIDR) Downstream Resume Sent Interrupt Disable Position */
#define HSTIDR_RSMEDIEC               (_U_(0x1) << HSTIDR_RSMEDIEC_Pos)        /**< (HSTIDR) Downstream Resume Sent Interrupt Disable Mask */
#define HSTIDR_RXRSMIEC_Pos           4                                              /**< (HSTIDR) Upstream Resume Received Interrupt Disable Position */
#define HSTIDR_RXRSMIEC               (_U_(0x1) << HSTIDR_RXRSMIEC_Pos)        /**< (HSTIDR) Upstream Resume Received Interrupt Disable Mask */
#define HSTIDR_HSOFIEC_Pos            5                                              /**< (HSTIDR) Host Start of Frame Interrupt Disable Position */
#define HSTIDR_HSOFIEC                (_U_(0x1) << HSTIDR_HSOFIEC_Pos)         /**< (HSTIDR) Host Start of Frame Interrupt Disable Mask */
#define HSTIDR_HWUPIEC_Pos            6                                              /**< (HSTIDR) Host Wake-Up Interrupt Disable Position */
#define HSTIDR_HWUPIEC                (_U_(0x1) << HSTIDR_HWUPIEC_Pos)         /**< (HSTIDR) Host Wake-Up Interrupt Disable Mask */
#define HSTIDR_PEP_0_Pos              8                                              /**< (HSTIDR) Pipe 0 Interrupt Disable Position */
#define HSTIDR_PEP_0                  (_U_(0x1) << HSTIDR_PEP_0_Pos)           /**< (HSTIDR) Pipe 0 Interrupt Disable Mask */
#define HSTIDR_PEP_1_Pos              9                                              /**< (HSTIDR) Pipe 1 Interrupt Disable Position */
#define HSTIDR_PEP_1                  (_U_(0x1) << HSTIDR_PEP_1_Pos)           /**< (HSTIDR) Pipe 1 Interrupt Disable Mask */
#define HSTIDR_PEP_2_Pos              10                                             /**< (HSTIDR) Pipe 2 Interrupt Disable Position */
#define HSTIDR_PEP_2                  (_U_(0x1) << HSTIDR_PEP_2_Pos)           /**< (HSTIDR) Pipe 2 Interrupt Disable Mask */
#define HSTIDR_PEP_3_Pos              11                                             /**< (HSTIDR) Pipe 3 Interrupt Disable Position */
#define HSTIDR_PEP_3                  (_U_(0x1) << HSTIDR_PEP_3_Pos)           /**< (HSTIDR) Pipe 3 Interrupt Disable Mask */
#define HSTIDR_PEP_4_Pos              12                                             /**< (HSTIDR) Pipe 4 Interrupt Disable Position */
#define HSTIDR_PEP_4                  (_U_(0x1) << HSTIDR_PEP_4_Pos)           /**< (HSTIDR) Pipe 4 Interrupt Disable Mask */
#define HSTIDR_PEP_5_Pos              13                                             /**< (HSTIDR) Pipe 5 Interrupt Disable Position */
#define HSTIDR_PEP_5                  (_U_(0x1) << HSTIDR_PEP_5_Pos)           /**< (HSTIDR) Pipe 5 Interrupt Disable Mask */
#define HSTIDR_PEP_6_Pos              14                                             /**< (HSTIDR) Pipe 6 Interrupt Disable Position */
#define HSTIDR_PEP_6                  (_U_(0x1) << HSTIDR_PEP_6_Pos)           /**< (HSTIDR) Pipe 6 Interrupt Disable Mask */
#define HSTIDR_PEP_7_Pos              15                                             /**< (HSTIDR) Pipe 7 Interrupt Disable Position */
#define HSTIDR_PEP_7                  (_U_(0x1) << HSTIDR_PEP_7_Pos)           /**< (HSTIDR) Pipe 7 Interrupt Disable Mask */
#define HSTIDR_PEP_8_Pos              16                                             /**< (HSTIDR) Pipe 8 Interrupt Disable Position */
#define HSTIDR_PEP_8                  (_U_(0x1) << HSTIDR_PEP_8_Pos)           /**< (HSTIDR) Pipe 8 Interrupt Disable Mask */
#define HSTIDR_PEP_9_Pos              17                                             /**< (HSTIDR) Pipe 9 Interrupt Disable Position */
#define HSTIDR_PEP_9                  (_U_(0x1) << HSTIDR_PEP_9_Pos)           /**< (HSTIDR) Pipe 9 Interrupt Disable Mask */
#define HSTIDR_DMA_0_Pos              25                                             /**< (HSTIDR) DMA Channel 0 Interrupt Disable Position */
#define HSTIDR_DMA_0                  (_U_(0x1) << HSTIDR_DMA_0_Pos)           /**< (HSTIDR) DMA Channel 0 Interrupt Disable Mask */
#define HSTIDR_DMA_1_Pos              26                                             /**< (HSTIDR) DMA Channel 1 Interrupt Disable Position */
#define HSTIDR_DMA_1                  (_U_(0x1) << HSTIDR_DMA_1_Pos)           /**< (HSTIDR) DMA Channel 1 Interrupt Disable Mask */
#define HSTIDR_DMA_2_Pos              27                                             /**< (HSTIDR) DMA Channel 2 Interrupt Disable Position */
#define HSTIDR_DMA_2                  (_U_(0x1) << HSTIDR_DMA_2_Pos)           /**< (HSTIDR) DMA Channel 2 Interrupt Disable Mask */
#define HSTIDR_DMA_3_Pos              28                                             /**< (HSTIDR) DMA Channel 3 Interrupt Disable Position */
#define HSTIDR_DMA_3                  (_U_(0x1) << HSTIDR_DMA_3_Pos)           /**< (HSTIDR) DMA Channel 3 Interrupt Disable Mask */
#define HSTIDR_DMA_4_Pos              29                                             /**< (HSTIDR) DMA Channel 4 Interrupt Disable Position */
#define HSTIDR_DMA_4                  (_U_(0x1) << HSTIDR_DMA_4_Pos)           /**< (HSTIDR) DMA Channel 4 Interrupt Disable Mask */
#define HSTIDR_DMA_5_Pos              30                                             /**< (HSTIDR) DMA Channel 5 Interrupt Disable Position */
#define HSTIDR_DMA_5                  (_U_(0x1) << HSTIDR_DMA_5_Pos)           /**< (HSTIDR) DMA Channel 5 Interrupt Disable Mask */
#define HSTIDR_DMA_6_Pos              31                                             /**< (HSTIDR) DMA Channel 6 Interrupt Disable Position */
#define HSTIDR_DMA_6                  (_U_(0x1) << HSTIDR_DMA_6_Pos)           /**< (HSTIDR) DMA Channel 6 Interrupt Disable Mask */
#define HSTIDR_Msk                    _U_(0xFE03FF7F)                                /**< (HSTIDR) Register Mask  */

#define HSTIDR_PEP__Pos               8                                              /**< (HSTIDR Position) Pipe x Interrupt Disable */
#define HSTIDR_PEP_                   (_U_(0x3FF) << HSTIDR_PEP__Pos)          /**< (HSTIDR Mask) PEP_ */
#define HSTIDR_DMA__Pos               25                                             /**< (HSTIDR Position) DMA Channel 6 Interrupt Disable */
#define HSTIDR_DMA_                   (_U_(0x7F) << HSTIDR_DMA__Pos)           /**< (HSTIDR Mask) DMA_ */

/* -------- HSTIER : (USBHS Offset: 0x418) (/W 32) Host Global Interrupt Enable Register -------- */

#define HSTIER_OFFSET                 (0x418)                                       /**<  (HSTIER) Host Global Interrupt Enable Register  Offset */

#define HSTIER_DCONNIES_Pos           0                                              /**< (HSTIER) Device Connection Interrupt Enable Position */
#define HSTIER_DCONNIES               (_U_(0x1) << HSTIER_DCONNIES_Pos)        /**< (HSTIER) Device Connection Interrupt Enable Mask */
#define HSTIER_DDISCIES_Pos           1                                              /**< (HSTIER) Device Disconnection Interrupt Enable Position */
#define HSTIER_DDISCIES               (_U_(0x1) << HSTIER_DDISCIES_Pos)        /**< (HSTIER) Device Disconnection Interrupt Enable Mask */
#define HSTIER_RSTIES_Pos             2                                              /**< (HSTIER) USB Reset Sent Interrupt Enable Position */
#define HSTIER_RSTIES                 (_U_(0x1) << HSTIER_RSTIES_Pos)          /**< (HSTIER) USB Reset Sent Interrupt Enable Mask */
#define HSTIER_RSMEDIES_Pos           3                                              /**< (HSTIER) Downstream Resume Sent Interrupt Enable Position */
#define HSTIER_RSMEDIES               (_U_(0x1) << HSTIER_RSMEDIES_Pos)        /**< (HSTIER) Downstream Resume Sent Interrupt Enable Mask */
#define HSTIER_RXRSMIES_Pos           4                                              /**< (HSTIER) Upstream Resume Received Interrupt Enable Position */
#define HSTIER_RXRSMIES               (_U_(0x1) << HSTIER_RXRSMIES_Pos)        /**< (HSTIER) Upstream Resume Received Interrupt Enable Mask */
#define HSTIER_HSOFIES_Pos            5                                              /**< (HSTIER) Host Start of Frame Interrupt Enable Position */
#define HSTIER_HSOFIES                (_U_(0x1) << HSTIER_HSOFIES_Pos)         /**< (HSTIER) Host Start of Frame Interrupt Enable Mask */
#define HSTIER_HWUPIES_Pos            6                                              /**< (HSTIER) Host Wake-Up Interrupt Enable Position */
#define HSTIER_HWUPIES                (_U_(0x1) << HSTIER_HWUPIES_Pos)         /**< (HSTIER) Host Wake-Up Interrupt Enable Mask */
#define HSTIER_PEP_0_Pos              8                                              /**< (HSTIER) Pipe 0 Interrupt Enable Position */
#define HSTIER_PEP_0                  (_U_(0x1) << HSTIER_PEP_0_Pos)           /**< (HSTIER) Pipe 0 Interrupt Enable Mask */
#define HSTIER_PEP_1_Pos              9                                              /**< (HSTIER) Pipe 1 Interrupt Enable Position */
#define HSTIER_PEP_1                  (_U_(0x1) << HSTIER_PEP_1_Pos)           /**< (HSTIER) Pipe 1 Interrupt Enable Mask */
#define HSTIER_PEP_2_Pos              10                                             /**< (HSTIER) Pipe 2 Interrupt Enable Position */
#define HSTIER_PEP_2                  (_U_(0x1) << HSTIER_PEP_2_Pos)           /**< (HSTIER) Pipe 2 Interrupt Enable Mask */
#define HSTIER_PEP_3_Pos              11                                             /**< (HSTIER) Pipe 3 Interrupt Enable Position */
#define HSTIER_PEP_3                  (_U_(0x1) << HSTIER_PEP_3_Pos)           /**< (HSTIER) Pipe 3 Interrupt Enable Mask */
#define HSTIER_PEP_4_Pos              12                                             /**< (HSTIER) Pipe 4 Interrupt Enable Position */
#define HSTIER_PEP_4                  (_U_(0x1) << HSTIER_PEP_4_Pos)           /**< (HSTIER) Pipe 4 Interrupt Enable Mask */
#define HSTIER_PEP_5_Pos              13                                             /**< (HSTIER) Pipe 5 Interrupt Enable Position */
#define HSTIER_PEP_5                  (_U_(0x1) << HSTIER_PEP_5_Pos)           /**< (HSTIER) Pipe 5 Interrupt Enable Mask */
#define HSTIER_PEP_6_Pos              14                                             /**< (HSTIER) Pipe 6 Interrupt Enable Position */
#define HSTIER_PEP_6                  (_U_(0x1) << HSTIER_PEP_6_Pos)           /**< (HSTIER) Pipe 6 Interrupt Enable Mask */
#define HSTIER_PEP_7_Pos              15                                             /**< (HSTIER) Pipe 7 Interrupt Enable Position */
#define HSTIER_PEP_7                  (_U_(0x1) << HSTIER_PEP_7_Pos)           /**< (HSTIER) Pipe 7 Interrupt Enable Mask */
#define HSTIER_PEP_8_Pos              16                                             /**< (HSTIER) Pipe 8 Interrupt Enable Position */
#define HSTIER_PEP_8                  (_U_(0x1) << HSTIER_PEP_8_Pos)           /**< (HSTIER) Pipe 8 Interrupt Enable Mask */
#define HSTIER_PEP_9_Pos              17                                             /**< (HSTIER) Pipe 9 Interrupt Enable Position */
#define HSTIER_PEP_9                  (_U_(0x1) << HSTIER_PEP_9_Pos)           /**< (HSTIER) Pipe 9 Interrupt Enable Mask */
#define HSTIER_DMA_0_Pos              25                                             /**< (HSTIER) DMA Channel 0 Interrupt Enable Position */
#define HSTIER_DMA_0                  (_U_(0x1) << HSTIER_DMA_0_Pos)           /**< (HSTIER) DMA Channel 0 Interrupt Enable Mask */
#define HSTIER_DMA_1_Pos              26                                             /**< (HSTIER) DMA Channel 1 Interrupt Enable Position */
#define HSTIER_DMA_1                  (_U_(0x1) << HSTIER_DMA_1_Pos)           /**< (HSTIER) DMA Channel 1 Interrupt Enable Mask */
#define HSTIER_DMA_2_Pos              27                                             /**< (HSTIER) DMA Channel 2 Interrupt Enable Position */
#define HSTIER_DMA_2                  (_U_(0x1) << HSTIER_DMA_2_Pos)           /**< (HSTIER) DMA Channel 2 Interrupt Enable Mask */
#define HSTIER_DMA_3_Pos              28                                             /**< (HSTIER) DMA Channel 3 Interrupt Enable Position */
#define HSTIER_DMA_3                  (_U_(0x1) << HSTIER_DMA_3_Pos)           /**< (HSTIER) DMA Channel 3 Interrupt Enable Mask */
#define HSTIER_DMA_4_Pos              29                                             /**< (HSTIER) DMA Channel 4 Interrupt Enable Position */
#define HSTIER_DMA_4                  (_U_(0x1) << HSTIER_DMA_4_Pos)           /**< (HSTIER) DMA Channel 4 Interrupt Enable Mask */
#define HSTIER_DMA_5_Pos              30                                             /**< (HSTIER) DMA Channel 5 Interrupt Enable Position */
#define HSTIER_DMA_5                  (_U_(0x1) << HSTIER_DMA_5_Pos)           /**< (HSTIER) DMA Channel 5 Interrupt Enable Mask */
#define HSTIER_DMA_6_Pos              31                                             /**< (HSTIER) DMA Channel 6 Interrupt Enable Position */
#define HSTIER_DMA_6                  (_U_(0x1) << HSTIER_DMA_6_Pos)           /**< (HSTIER) DMA Channel 6 Interrupt Enable Mask */
#define HSTIER_Msk                    _U_(0xFE03FF7F)                                /**< (HSTIER) Register Mask  */

#define HSTIER_PEP__Pos               8                                              /**< (HSTIER Position) Pipe x Interrupt Enable */
#define HSTIER_PEP_                   (_U_(0x3FF) << HSTIER_PEP__Pos)          /**< (HSTIER Mask) PEP_ */
#define HSTIER_DMA__Pos               25                                             /**< (HSTIER Position) DMA Channel 6 Interrupt Enable */
#define HSTIER_DMA_                   (_U_(0x7F) << HSTIER_DMA__Pos)           /**< (HSTIER Mask) DMA_ */

/* -------- HSTPIP : (USBHS Offset: 0x41c) (R/W 32) Host Pipe Register -------- */

#define HSTPIP_OFFSET                 (0x41C)                                       /**<  (HSTPIP) Host Pipe Register  Offset */

#define HSTPIP_PEN0_Pos               0                                              /**< (HSTPIP) Pipe 0 Enable Position */
#define HSTPIP_PEN0                   (_U_(0x1) << HSTPIP_PEN0_Pos)            /**< (HSTPIP) Pipe 0 Enable Mask */
#define HSTPIP_PEN1_Pos               1                                              /**< (HSTPIP) Pipe 1 Enable Position */
#define HSTPIP_PEN1                   (_U_(0x1) << HSTPIP_PEN1_Pos)            /**< (HSTPIP) Pipe 1 Enable Mask */
#define HSTPIP_PEN2_Pos               2                                              /**< (HSTPIP) Pipe 2 Enable Position */
#define HSTPIP_PEN2                   (_U_(0x1) << HSTPIP_PEN2_Pos)            /**< (HSTPIP) Pipe 2 Enable Mask */
#define HSTPIP_PEN3_Pos               3                                              /**< (HSTPIP) Pipe 3 Enable Position */
#define HSTPIP_PEN3                   (_U_(0x1) << HSTPIP_PEN3_Pos)            /**< (HSTPIP) Pipe 3 Enable Mask */
#define HSTPIP_PEN4_Pos               4                                              /**< (HSTPIP) Pipe 4 Enable Position */
#define HSTPIP_PEN4                   (_U_(0x1) << HSTPIP_PEN4_Pos)            /**< (HSTPIP) Pipe 4 Enable Mask */
#define HSTPIP_PEN5_Pos               5                                              /**< (HSTPIP) Pipe 5 Enable Position */
#define HSTPIP_PEN5                   (_U_(0x1) << HSTPIP_PEN5_Pos)            /**< (HSTPIP) Pipe 5 Enable Mask */
#define HSTPIP_PEN6_Pos               6                                              /**< (HSTPIP) Pipe 6 Enable Position */
#define HSTPIP_PEN6                   (_U_(0x1) << HSTPIP_PEN6_Pos)            /**< (HSTPIP) Pipe 6 Enable Mask */
#define HSTPIP_PEN7_Pos               7                                              /**< (HSTPIP) Pipe 7 Enable Position */
#define HSTPIP_PEN7                   (_U_(0x1) << HSTPIP_PEN7_Pos)            /**< (HSTPIP) Pipe 7 Enable Mask */
#define HSTPIP_PEN8_Pos               8                                              /**< (HSTPIP) Pipe 8 Enable Position */
#define HSTPIP_PEN8                   (_U_(0x1) << HSTPIP_PEN8_Pos)            /**< (HSTPIP) Pipe 8 Enable Mask */
#define HSTPIP_PRST0_Pos              16                                             /**< (HSTPIP) Pipe 0 Reset Position */
#define HSTPIP_PRST0                  (_U_(0x1) << HSTPIP_PRST0_Pos)           /**< (HSTPIP) Pipe 0 Reset Mask */
#define HSTPIP_PRST1_Pos              17                                             /**< (HSTPIP) Pipe 1 Reset Position */
#define HSTPIP_PRST1                  (_U_(0x1) << HSTPIP_PRST1_Pos)           /**< (HSTPIP) Pipe 1 Reset Mask */
#define HSTPIP_PRST2_Pos              18                                             /**< (HSTPIP) Pipe 2 Reset Position */
#define HSTPIP_PRST2                  (_U_(0x1) << HSTPIP_PRST2_Pos)           /**< (HSTPIP) Pipe 2 Reset Mask */
#define HSTPIP_PRST3_Pos              19                                             /**< (HSTPIP) Pipe 3 Reset Position */
#define HSTPIP_PRST3                  (_U_(0x1) << HSTPIP_PRST3_Pos)           /**< (HSTPIP) Pipe 3 Reset Mask */
#define HSTPIP_PRST4_Pos              20                                             /**< (HSTPIP) Pipe 4 Reset Position */
#define HSTPIP_PRST4                  (_U_(0x1) << HSTPIP_PRST4_Pos)           /**< (HSTPIP) Pipe 4 Reset Mask */
#define HSTPIP_PRST5_Pos              21                                             /**< (HSTPIP) Pipe 5 Reset Position */
#define HSTPIP_PRST5                  (_U_(0x1) << HSTPIP_PRST5_Pos)           /**< (HSTPIP) Pipe 5 Reset Mask */
#define HSTPIP_PRST6_Pos              22                                             /**< (HSTPIP) Pipe 6 Reset Position */
#define HSTPIP_PRST6                  (_U_(0x1) << HSTPIP_PRST6_Pos)           /**< (HSTPIP) Pipe 6 Reset Mask */
#define HSTPIP_PRST7_Pos              23                                             /**< (HSTPIP) Pipe 7 Reset Position */
#define HSTPIP_PRST7                  (_U_(0x1) << HSTPIP_PRST7_Pos)           /**< (HSTPIP) Pipe 7 Reset Mask */
#define HSTPIP_PRST8_Pos              24                                             /**< (HSTPIP) Pipe 8 Reset Position */
#define HSTPIP_PRST8                  (_U_(0x1) << HSTPIP_PRST8_Pos)           /**< (HSTPIP) Pipe 8 Reset Mask */
#define HSTPIP_Msk                    _U_(0x1FF01FF)                                 /**< (HSTPIP) Register Mask  */

#define HSTPIP_PEN_Pos                0                                              /**< (HSTPIP Position) Pipe x Enable */
#define HSTPIP_PEN                    (_U_(0x1FF) << HSTPIP_PEN_Pos)           /**< (HSTPIP Mask) PEN */
#define HSTPIP_PRST_Pos               16                                             /**< (HSTPIP Position) Pipe 8 Reset */
#define HSTPIP_PRST                   (_U_(0x1FF) << HSTPIP_PRST_Pos)          /**< (HSTPIP Mask) PRST */

/* -------- HSTFNUM : (USBHS Offset: 0x420) (R/W 32) Host Frame Number Register -------- */

#define HSTFNUM_OFFSET                (0x420)                                       /**<  (HSTFNUM) Host Frame Number Register  Offset */

#define HSTFNUM_MFNUM_Pos             0                                              /**< (HSTFNUM) Micro Frame Number Position */
#define HSTFNUM_MFNUM                 (_U_(0x7) << HSTFNUM_MFNUM_Pos)          /**< (HSTFNUM) Micro Frame Number Mask */
#define HSTFNUM_FNUM_Pos              3                                              /**< (HSTFNUM) Frame Number Position */
#define HSTFNUM_FNUM                  (_U_(0x7FF) << HSTFNUM_FNUM_Pos)         /**< (HSTFNUM) Frame Number Mask */
#define HSTFNUM_FLENHIGH_Pos          16                                             /**< (HSTFNUM) Frame Length Position */
#define HSTFNUM_FLENHIGH              (_U_(0xFF) << HSTFNUM_FLENHIGH_Pos)      /**< (HSTFNUM) Frame Length Mask */
#define HSTFNUM_Msk                   _U_(0xFF3FFF)                                  /**< (HSTFNUM) Register Mask  */


/* -------- HSTADDR1 : (USBHS Offset: 0x424) (R/W 32) Host Address 1 Register -------- */

#define HSTADDR1_OFFSET               (0x424)                                       /**<  (HSTADDR1) Host Address 1 Register  Offset */

#define HSTADDR1_HSTADDRP0_Pos        0                                              /**< (HSTADDR1) USB Host Address Position */
#define HSTADDR1_HSTADDRP0            (_U_(0x7F) << HSTADDR1_HSTADDRP0_Pos)    /**< (HSTADDR1) USB Host Address Mask */
#define HSTADDR1_HSTADDRP1_Pos        8                                              /**< (HSTADDR1) USB Host Address Position */
#define HSTADDR1_HSTADDRP1            (_U_(0x7F) << HSTADDR1_HSTADDRP1_Pos)    /**< (HSTADDR1) USB Host Address Mask */
#define HSTADDR1_HSTADDRP2_Pos        16                                             /**< (HSTADDR1) USB Host Address Position */
#define HSTADDR1_HSTADDRP2            (_U_(0x7F) << HSTADDR1_HSTADDRP2_Pos)    /**< (HSTADDR1) USB Host Address Mask */
#define HSTADDR1_HSTADDRP3_Pos        24                                             /**< (HSTADDR1) USB Host Address Position */
#define HSTADDR1_HSTADDRP3            (_U_(0x7F) << HSTADDR1_HSTADDRP3_Pos)    /**< (HSTADDR1) USB Host Address Mask */
#define HSTADDR1_Msk                  _U_(0x7F7F7F7F)                                /**< (HSTADDR1) Register Mask  */


/* -------- HSTADDR2 : (USBHS Offset: 0x428) (R/W 32) Host Address 2 Register -------- */

#define HSTADDR2_OFFSET               (0x428)                                       /**<  (HSTADDR2) Host Address 2 Register  Offset */

#define HSTADDR2_HSTADDRP4_Pos        0                                              /**< (HSTADDR2) USB Host Address Position */
#define HSTADDR2_HSTADDRP4            (_U_(0x7F) << HSTADDR2_HSTADDRP4_Pos)    /**< (HSTADDR2) USB Host Address Mask */
#define HSTADDR2_HSTADDRP5_Pos        8                                              /**< (HSTADDR2) USB Host Address Position */
#define HSTADDR2_HSTADDRP5            (_U_(0x7F) << HSTADDR2_HSTADDRP5_Pos)    /**< (HSTADDR2) USB Host Address Mask */
#define HSTADDR2_HSTADDRP6_Pos        16                                             /**< (HSTADDR2) USB Host Address Position */
#define HSTADDR2_HSTADDRP6            (_U_(0x7F) << HSTADDR2_HSTADDRP6_Pos)    /**< (HSTADDR2) USB Host Address Mask */
#define HSTADDR2_HSTADDRP7_Pos        24                                             /**< (HSTADDR2) USB Host Address Position */
#define HSTADDR2_HSTADDRP7            (_U_(0x7F) << HSTADDR2_HSTADDRP7_Pos)    /**< (HSTADDR2) USB Host Address Mask */
#define HSTADDR2_Msk                  _U_(0x7F7F7F7F)                                /**< (HSTADDR2) Register Mask  */


/* -------- HSTADDR3 : (USBHS Offset: 0x42c) (R/W 32) Host Address 3 Register -------- */

#define HSTADDR3_OFFSET               (0x42C)                                       /**<  (HSTADDR3) Host Address 3 Register  Offset */

#define HSTADDR3_HSTADDRP8_Pos        0                                              /**< (HSTADDR3) USB Host Address Position */
#define HSTADDR3_HSTADDRP8            (_U_(0x7F) << HSTADDR3_HSTADDRP8_Pos)    /**< (HSTADDR3) USB Host Address Mask */
#define HSTADDR3_HSTADDRP9_Pos        8                                              /**< (HSTADDR3) USB Host Address Position */
#define HSTADDR3_HSTADDRP9            (_U_(0x7F) << HSTADDR3_HSTADDRP9_Pos)    /**< (HSTADDR3) USB Host Address Mask */
#define HSTADDR3_Msk                  _U_(0x7F7F)                                    /**< (HSTADDR3) Register Mask  */


/* -------- HSTPIPCFG : (USBHS Offset: 0x500) (R/W 32) Host Pipe Configuration Register -------- */

#define HSTPIPCFG_OFFSET              (0x500)                                       /**<  (HSTPIPCFG) Host Pipe Configuration Register  Offset */

#define HSTPIPCFG_ALLOC_Pos           1                                              /**< (HSTPIPCFG) Pipe Memory Allocate Position */
#define HSTPIPCFG_ALLOC               (_U_(0x1) << HSTPIPCFG_ALLOC_Pos)        /**< (HSTPIPCFG) Pipe Memory Allocate Mask */
#define HSTPIPCFG_PBK_Pos             2                                              /**< (HSTPIPCFG) Pipe Banks Position */
#define HSTPIPCFG_PBK                 (_U_(0x3) << HSTPIPCFG_PBK_Pos)          /**< (HSTPIPCFG) Pipe Banks Mask */
#define   HSTPIPCFG_PBK_1_BANK_Val    _U_(0x0)                                       /**< (HSTPIPCFG) Single-bank pipe  */
#define   HSTPIPCFG_PBK_2_BANK_Val    _U_(0x1)                                       /**< (HSTPIPCFG) Double-bank pipe  */
#define   HSTPIPCFG_PBK_3_BANK_Val    _U_(0x2)                                       /**< (HSTPIPCFG) Triple-bank pipe  */
#define HSTPIPCFG_PBK_1_BANK          (HSTPIPCFG_PBK_1_BANK_Val << HSTPIPCFG_PBK_Pos)  /**< (HSTPIPCFG) Single-bank pipe Position  */
#define HSTPIPCFG_PBK_2_BANK          (HSTPIPCFG_PBK_2_BANK_Val << HSTPIPCFG_PBK_Pos)  /**< (HSTPIPCFG) Double-bank pipe Position  */
#define HSTPIPCFG_PBK_3_BANK          (HSTPIPCFG_PBK_3_BANK_Val << HSTPIPCFG_PBK_Pos)  /**< (HSTPIPCFG) Triple-bank pipe Position  */
#define HSTPIPCFG_PSIZE_Pos           4                                              /**< (HSTPIPCFG) Pipe Size Position */
#define HSTPIPCFG_PSIZE               (_U_(0x7) << HSTPIPCFG_PSIZE_Pos)        /**< (HSTPIPCFG) Pipe Size Mask */
#define   HSTPIPCFG_PSIZE_8_BYTE_Val  _U_(0x0)                                       /**< (HSTPIPCFG) 8 bytes  */
#define   HSTPIPCFG_PSIZE_16_BYTE_Val _U_(0x1)                                       /**< (HSTPIPCFG) 16 bytes  */
#define   HSTPIPCFG_PSIZE_32_BYTE_Val _U_(0x2)                                       /**< (HSTPIPCFG) 32 bytes  */
#define   HSTPIPCFG_PSIZE_64_BYTE_Val _U_(0x3)                                       /**< (HSTPIPCFG) 64 bytes  */
#define   HSTPIPCFG_PSIZE_128_BYTE_Val _U_(0x4)                                       /**< (HSTPIPCFG) 128 bytes  */
#define   HSTPIPCFG_PSIZE_256_BYTE_Val _U_(0x5)                                       /**< (HSTPIPCFG) 256 bytes  */
#define   HSTPIPCFG_PSIZE_512_BYTE_Val _U_(0x6)                                       /**< (HSTPIPCFG) 512 bytes  */
#define   HSTPIPCFG_PSIZE_1024_BYTE_Val _U_(0x7)                                       /**< (HSTPIPCFG) 1024 bytes  */
#define HSTPIPCFG_PSIZE_8_BYTE        (HSTPIPCFG_PSIZE_8_BYTE_Val << HSTPIPCFG_PSIZE_Pos)  /**< (HSTPIPCFG) 8 bytes Position  */
#define HSTPIPCFG_PSIZE_16_BYTE       (HSTPIPCFG_PSIZE_16_BYTE_Val << HSTPIPCFG_PSIZE_Pos)  /**< (HSTPIPCFG) 16 bytes Position  */
#define HSTPIPCFG_PSIZE_32_BYTE       (HSTPIPCFG_PSIZE_32_BYTE_Val << HSTPIPCFG_PSIZE_Pos)  /**< (HSTPIPCFG) 32 bytes Position  */
#define HSTPIPCFG_PSIZE_64_BYTE       (HSTPIPCFG_PSIZE_64_BYTE_Val << HSTPIPCFG_PSIZE_Pos)  /**< (HSTPIPCFG) 64 bytes Position  */
#define HSTPIPCFG_PSIZE_128_BYTE      (HSTPIPCFG_PSIZE_128_BYTE_Val << HSTPIPCFG_PSIZE_Pos)  /**< (HSTPIPCFG) 128 bytes Position  */
#define HSTPIPCFG_PSIZE_256_BYTE      (HSTPIPCFG_PSIZE_256_BYTE_Val << HSTPIPCFG_PSIZE_Pos)  /**< (HSTPIPCFG) 256 bytes Position  */
#define HSTPIPCFG_PSIZE_512_BYTE      (HSTPIPCFG_PSIZE_512_BYTE_Val << HSTPIPCFG_PSIZE_Pos)  /**< (HSTPIPCFG) 512 bytes Position  */
#define HSTPIPCFG_PSIZE_1024_BYTE     (HSTPIPCFG_PSIZE_1024_BYTE_Val << HSTPIPCFG_PSIZE_Pos)  /**< (HSTPIPCFG) 1024 bytes Position  */
#define HSTPIPCFG_PTOKEN_Pos          8                                              /**< (HSTPIPCFG) Pipe Token Position */
#define HSTPIPCFG_PTOKEN              (_U_(0x3) << HSTPIPCFG_PTOKEN_Pos)       /**< (HSTPIPCFG) Pipe Token Mask */
#define   HSTPIPCFG_PTOKEN_SETUP_Val  _U_(0x0)                                       /**< (HSTPIPCFG) SETUP  */
#define   HSTPIPCFG_PTOKEN_IN_Val     _U_(0x1)                                       /**< (HSTPIPCFG) IN  */
#define   HSTPIPCFG_PTOKEN_OUT_Val    _U_(0x2)                                       /**< (HSTPIPCFG) OUT  */
#define HSTPIPCFG_PTOKEN_SETUP        (HSTPIPCFG_PTOKEN_SETUP_Val << HSTPIPCFG_PTOKEN_Pos)  /**< (HSTPIPCFG) SETUP Position  */
#define HSTPIPCFG_PTOKEN_IN           (HSTPIPCFG_PTOKEN_IN_Val << HSTPIPCFG_PTOKEN_Pos)  /**< (HSTPIPCFG) IN Position  */
#define HSTPIPCFG_PTOKEN_OUT          (HSTPIPCFG_PTOKEN_OUT_Val << HSTPIPCFG_PTOKEN_Pos)  /**< (HSTPIPCFG) OUT Position  */
#define HSTPIPCFG_AUTOSW_Pos          10                                             /**< (HSTPIPCFG) Automatic Switch Position */
#define HSTPIPCFG_AUTOSW              (_U_(0x1) << HSTPIPCFG_AUTOSW_Pos)       /**< (HSTPIPCFG) Automatic Switch Mask */
#define HSTPIPCFG_PTYPE_Pos           12                                             /**< (HSTPIPCFG) Pipe Type Position */
#define HSTPIPCFG_PTYPE               (_U_(0x3) << HSTPIPCFG_PTYPE_Pos)        /**< (HSTPIPCFG) Pipe Type Mask */
#define   HSTPIPCFG_PTYPE_CTRL_Val    _U_(0x0)                                       /**< (HSTPIPCFG) Control  */
#define   HSTPIPCFG_PTYPE_ISO_Val     _U_(0x1)                                       /**< (HSTPIPCFG) Isochronous  */
#define   HSTPIPCFG_PTYPE_BLK_Val     _U_(0x2)                                       /**< (HSTPIPCFG) Bulk  */
#define   HSTPIPCFG_PTYPE_INTRPT_Val  _U_(0x3)                                       /**< (HSTPIPCFG) Interrupt  */
#define HSTPIPCFG_PTYPE_CTRL          (HSTPIPCFG_PTYPE_CTRL_Val << HSTPIPCFG_PTYPE_Pos)  /**< (HSTPIPCFG) Control Position  */
#define HSTPIPCFG_PTYPE_ISO           (HSTPIPCFG_PTYPE_ISO_Val << HSTPIPCFG_PTYPE_Pos)  /**< (HSTPIPCFG) Isochronous Position  */
#define HSTPIPCFG_PTYPE_BLK           (HSTPIPCFG_PTYPE_BLK_Val << HSTPIPCFG_PTYPE_Pos)  /**< (HSTPIPCFG) Bulk Position  */
#define HSTPIPCFG_PTYPE_INTRPT        (HSTPIPCFG_PTYPE_INTRPT_Val << HSTPIPCFG_PTYPE_Pos)  /**< (HSTPIPCFG) Interrupt Position  */
#define HSTPIPCFG_PEPNUM_Pos          16                                             /**< (HSTPIPCFG) Pipe Endpoint Number Position */
#define HSTPIPCFG_PEPNUM              (_U_(0xF) << HSTPIPCFG_PEPNUM_Pos)       /**< (HSTPIPCFG) Pipe Endpoint Number Mask */
#define HSTPIPCFG_INTFRQ_Pos          24                                             /**< (HSTPIPCFG) Pipe Interrupt Request Frequency Position */
#define HSTPIPCFG_INTFRQ              (_U_(0xFF) << HSTPIPCFG_INTFRQ_Pos)      /**< (HSTPIPCFG) Pipe Interrupt Request Frequency Mask */
#define HSTPIPCFG_Msk                 _U_(0xFF0F377E)                                /**< (HSTPIPCFG) Register Mask  */

/* CTRL_BULK mode */
#define HSTPIPCFG_CTRL_BULK_PINGEN_Pos 20                                             /**< (HSTPIPCFG) Ping Enable Position */
#define HSTPIPCFG_CTRL_BULK_PINGEN     (_U_(0x1) << HSTPIPCFG_CTRL_BULK_PINGEN_Pos)  /**< (HSTPIPCFG) Ping Enable Mask */
#define HSTPIPCFG_CTRL_BULK_BINTERVAL_Pos 24                                             /**< (HSTPIPCFG) bInterval Parameter for the Bulk-Out/Ping Transaction Position */
#define HSTPIPCFG_CTRL_BULK_BINTERVAL     (_U_(0xFF) << HSTPIPCFG_CTRL_BULK_BINTERVAL_Pos)  /**< (HSTPIPCFG) bInterval Parameter for the Bulk-Out/Ping Transaction Mask */
#define HSTPIPCFG_CTRL_BULK_Msk       _U_(0xFF100000)                                /**< (HSTPIPCFG_CTRL_BULK) Register Mask  */


/* -------- HSTPIPISR : (USBHS Offset: 0x530) (R/ 32) Host Pipe Status Register -------- */

#define HSTPIPISR_OFFSET              (0x530)                                       /**<  (HSTPIPISR) Host Pipe Status Register  Offset */

#define HSTPIPISR_RXINI_Pos           0                                              /**< (HSTPIPISR) Received IN Data Interrupt Position */
#define HSTPIPISR_RXINI               (_U_(0x1) << HSTPIPISR_RXINI_Pos)        /**< (HSTPIPISR) Received IN Data Interrupt Mask */
#define HSTPIPISR_TXOUTI_Pos          1                                              /**< (HSTPIPISR) Transmitted OUT Data Interrupt Position */
#define HSTPIPISR_TXOUTI              (_U_(0x1) << HSTPIPISR_TXOUTI_Pos)       /**< (HSTPIPISR) Transmitted OUT Data Interrupt Mask */
#define HSTPIPISR_PERRI_Pos           3                                              /**< (HSTPIPISR) Pipe Error Interrupt Position */
#define HSTPIPISR_PERRI               (_U_(0x1) << HSTPIPISR_PERRI_Pos)        /**< (HSTPIPISR) Pipe Error Interrupt Mask */
#define HSTPIPISR_NAKEDI_Pos          4                                              /**< (HSTPIPISR) NAKed Interrupt Position */
#define HSTPIPISR_NAKEDI              (_U_(0x1) << HSTPIPISR_NAKEDI_Pos)       /**< (HSTPIPISR) NAKed Interrupt Mask */
#define HSTPIPISR_OVERFI_Pos          5                                              /**< (HSTPIPISR) Overflow Interrupt Position */
#define HSTPIPISR_OVERFI              (_U_(0x1) << HSTPIPISR_OVERFI_Pos)       /**< (HSTPIPISR) Overflow Interrupt Mask */
#define HSTPIPISR_SHORTPACKETI_Pos    7                                              /**< (HSTPIPISR) Short Packet Interrupt Position */
#define HSTPIPISR_SHORTPACKETI        (_U_(0x1) << HSTPIPISR_SHORTPACKETI_Pos)  /**< (HSTPIPISR) Short Packet Interrupt Mask */
#define HSTPIPISR_DTSEQ_Pos           8                                              /**< (HSTPIPISR) Data Toggle Sequence Position */
#define HSTPIPISR_DTSEQ               (_U_(0x3) << HSTPIPISR_DTSEQ_Pos)        /**< (HSTPIPISR) Data Toggle Sequence Mask */
#define   HSTPIPISR_DTSEQ_DATA0_Val   _U_(0x0)                                       /**< (HSTPIPISR) Data0 toggle sequence  */
#define   HSTPIPISR_DTSEQ_DATA1_Val   _U_(0x1)                                       /**< (HSTPIPISR) Data1 toggle sequence  */
#define HSTPIPISR_DTSEQ_DATA0         (HSTPIPISR_DTSEQ_DATA0_Val << HSTPIPISR_DTSEQ_Pos)  /**< (HSTPIPISR) Data0 toggle sequence Position  */
#define HSTPIPISR_DTSEQ_DATA1         (HSTPIPISR_DTSEQ_DATA1_Val << HSTPIPISR_DTSEQ_Pos)  /**< (HSTPIPISR) Data1 toggle sequence Position  */
#define HSTPIPISR_NBUSYBK_Pos         12                                             /**< (HSTPIPISR) Number of Busy Banks Position */
#define HSTPIPISR_NBUSYBK             (_U_(0x3) << HSTPIPISR_NBUSYBK_Pos)      /**< (HSTPIPISR) Number of Busy Banks Mask */
#define   HSTPIPISR_NBUSYBK_0_BUSY_Val _U_(0x0)                                       /**< (HSTPIPISR) 0 busy bank (all banks free)  */
#define   HSTPIPISR_NBUSYBK_1_BUSY_Val _U_(0x1)                                       /**< (HSTPIPISR) 1 busy bank  */
#define   HSTPIPISR_NBUSYBK_2_BUSY_Val _U_(0x2)                                       /**< (HSTPIPISR) 2 busy banks  */
#define   HSTPIPISR_NBUSYBK_3_BUSY_Val _U_(0x3)                                       /**< (HSTPIPISR) 3 busy banks  */
#define HSTPIPISR_NBUSYBK_0_BUSY      (HSTPIPISR_NBUSYBK_0_BUSY_Val << HSTPIPISR_NBUSYBK_Pos)  /**< (HSTPIPISR) 0 busy bank (all banks free) Position  */
#define HSTPIPISR_NBUSYBK_1_BUSY      (HSTPIPISR_NBUSYBK_1_BUSY_Val << HSTPIPISR_NBUSYBK_Pos)  /**< (HSTPIPISR) 1 busy bank Position  */
#define HSTPIPISR_NBUSYBK_2_BUSY      (HSTPIPISR_NBUSYBK_2_BUSY_Val << HSTPIPISR_NBUSYBK_Pos)  /**< (HSTPIPISR) 2 busy banks Position  */
#define HSTPIPISR_NBUSYBK_3_BUSY      (HSTPIPISR_NBUSYBK_3_BUSY_Val << HSTPIPISR_NBUSYBK_Pos)  /**< (HSTPIPISR) 3 busy banks Position  */
#define HSTPIPISR_CURRBK_Pos          14                                             /**< (HSTPIPISR) Current Bank Position */
#define HSTPIPISR_CURRBK              (_U_(0x3) << HSTPIPISR_CURRBK_Pos)       /**< (HSTPIPISR) Current Bank Mask */
#define   HSTPIPISR_CURRBK_BANK0_Val  _U_(0x0)                                       /**< (HSTPIPISR) Current bank is bank0  */
#define   HSTPIPISR_CURRBK_BANK1_Val  _U_(0x1)                                       /**< (HSTPIPISR) Current bank is bank1  */
#define   HSTPIPISR_CURRBK_BANK2_Val  _U_(0x2)                                       /**< (HSTPIPISR) Current bank is bank2  */
#define HSTPIPISR_CURRBK_BANK0        (HSTPIPISR_CURRBK_BANK0_Val << HSTPIPISR_CURRBK_Pos)  /**< (HSTPIPISR) Current bank is bank0 Position  */
#define HSTPIPISR_CURRBK_BANK1        (HSTPIPISR_CURRBK_BANK1_Val << HSTPIPISR_CURRBK_Pos)  /**< (HSTPIPISR) Current bank is bank1 Position  */
#define HSTPIPISR_CURRBK_BANK2        (HSTPIPISR_CURRBK_BANK2_Val << HSTPIPISR_CURRBK_Pos)  /**< (HSTPIPISR) Current bank is bank2 Position  */
#define HSTPIPISR_RWALL_Pos           16                                             /**< (HSTPIPISR) Read/Write Allowed Position */
#define HSTPIPISR_RWALL               (_U_(0x1) << HSTPIPISR_RWALL_Pos)        /**< (HSTPIPISR) Read/Write Allowed Mask */
#define HSTPIPISR_CFGOK_Pos           18                                             /**< (HSTPIPISR) Configuration OK Status Position */
#define HSTPIPISR_CFGOK               (_U_(0x1) << HSTPIPISR_CFGOK_Pos)        /**< (HSTPIPISR) Configuration OK Status Mask */
#define HSTPIPISR_PBYCT_Pos           20                                             /**< (HSTPIPISR) Pipe Byte Count Position */
#define HSTPIPISR_PBYCT               (_U_(0x7FF) << HSTPIPISR_PBYCT_Pos)      /**< (HSTPIPISR) Pipe Byte Count Mask */
#define HSTPIPISR_Msk                 _U_(0x7FF5F3BB)                                /**< (HSTPIPISR) Register Mask  */

/* CTRL mode */
#define HSTPIPISR_CTRL_TXSTPI_Pos     2                                              /**< (HSTPIPISR) Transmitted SETUP Interrupt Position */
#define HSTPIPISR_CTRL_TXSTPI         (_U_(0x1) << HSTPIPISR_CTRL_TXSTPI_Pos)  /**< (HSTPIPISR) Transmitted SETUP Interrupt Mask */
#define HSTPIPISR_CTRL_RXSTALLDI_Pos  6                                              /**< (HSTPIPISR) Received STALLed Interrupt Position */
#define HSTPIPISR_CTRL_RXSTALLDI      (_U_(0x1) << HSTPIPISR_CTRL_RXSTALLDI_Pos)  /**< (HSTPIPISR) Received STALLed Interrupt Mask */
#define HSTPIPISR_CTRL_Msk            _U_(0x44)                                      /**< (HSTPIPISR_CTRL) Register Mask  */

/* ISO mode */
#define HSTPIPISR_ISO_UNDERFI_Pos     2                                              /**< (HSTPIPISR) Underflow Interrupt Position */
#define HSTPIPISR_ISO_UNDERFI         (_U_(0x1) << HSTPIPISR_ISO_UNDERFI_Pos)  /**< (HSTPIPISR) Underflow Interrupt Mask */
#define HSTPIPISR_ISO_CRCERRI_Pos     6                                              /**< (HSTPIPISR) CRC Error Interrupt Position */
#define HSTPIPISR_ISO_CRCERRI         (_U_(0x1) << HSTPIPISR_ISO_CRCERRI_Pos)  /**< (HSTPIPISR) CRC Error Interrupt Mask */
#define HSTPIPISR_ISO_Msk             _U_(0x44)                                      /**< (HSTPIPISR_ISO) Register Mask  */

/* BLK mode */
#define HSTPIPISR_BLK_TXSTPI_Pos      2                                              /**< (HSTPIPISR) Transmitted SETUP Interrupt Position */
#define HSTPIPISR_BLK_TXSTPI          (_U_(0x1) << HSTPIPISR_BLK_TXSTPI_Pos)   /**< (HSTPIPISR) Transmitted SETUP Interrupt Mask */
#define HSTPIPISR_BLK_RXSTALLDI_Pos   6                                              /**< (HSTPIPISR) Received STALLed Interrupt Position */
#define HSTPIPISR_BLK_RXSTALLDI       (_U_(0x1) << HSTPIPISR_BLK_RXSTALLDI_Pos)  /**< (HSTPIPISR) Received STALLed Interrupt Mask */
#define HSTPIPISR_BLK_Msk             _U_(0x44)                                      /**< (HSTPIPISR_BLK) Register Mask  */

/* INTRPT mode */
#define HSTPIPISR_INTRPT_UNDERFI_Pos  2                                              /**< (HSTPIPISR) Underflow Interrupt Position */
#define HSTPIPISR_INTRPT_UNDERFI      (_U_(0x1) << HSTPIPISR_INTRPT_UNDERFI_Pos)  /**< (HSTPIPISR) Underflow Interrupt Mask */
#define HSTPIPISR_INTRPT_RXSTALLDI_Pos 6                                              /**< (HSTPIPISR) Received STALLed Interrupt Position */
#define HSTPIPISR_INTRPT_RXSTALLDI     (_U_(0x1) << HSTPIPISR_INTRPT_RXSTALLDI_Pos)  /**< (HSTPIPISR) Received STALLed Interrupt Mask */
#define HSTPIPISR_INTRPT_Msk          _U_(0x44)                                      /**< (HSTPIPISR_INTRPT) Register Mask  */


/* -------- HSTPIPICR : (USBHS Offset: 0x560) (/W 32) Host Pipe Clear Register -------- */

#define HSTPIPICR_OFFSET              (0x560)                                       /**<  (HSTPIPICR) Host Pipe Clear Register  Offset */

#define HSTPIPICR_RXINIC_Pos          0                                              /**< (HSTPIPICR) Received IN Data Interrupt Clear Position */
#define HSTPIPICR_RXINIC              (_U_(0x1) << HSTPIPICR_RXINIC_Pos)       /**< (HSTPIPICR) Received IN Data Interrupt Clear Mask */
#define HSTPIPICR_TXOUTIC_Pos         1                                              /**< (HSTPIPICR) Transmitted OUT Data Interrupt Clear Position */
#define HSTPIPICR_TXOUTIC             (_U_(0x1) << HSTPIPICR_TXOUTIC_Pos)      /**< (HSTPIPICR) Transmitted OUT Data Interrupt Clear Mask */
#define HSTPIPICR_NAKEDIC_Pos         4                                              /**< (HSTPIPICR) NAKed Interrupt Clear Position */
#define HSTPIPICR_NAKEDIC             (_U_(0x1) << HSTPIPICR_NAKEDIC_Pos)      /**< (HSTPIPICR) NAKed Interrupt Clear Mask */
#define HSTPIPICR_OVERFIC_Pos         5                                              /**< (HSTPIPICR) Overflow Interrupt Clear Position */
#define HSTPIPICR_OVERFIC             (_U_(0x1) << HSTPIPICR_OVERFIC_Pos)      /**< (HSTPIPICR) Overflow Interrupt Clear Mask */
#define HSTPIPICR_SHORTPACKETIC_Pos   7                                              /**< (HSTPIPICR) Short Packet Interrupt Clear Position */
#define HSTPIPICR_SHORTPACKETIC       (_U_(0x1) << HSTPIPICR_SHORTPACKETIC_Pos)  /**< (HSTPIPICR) Short Packet Interrupt Clear Mask */
#define HSTPIPICR_Msk                 _U_(0xB3)                                      /**< (HSTPIPICR) Register Mask  */

/* CTRL mode */
#define HSTPIPICR_CTRL_TXSTPIC_Pos    2                                              /**< (HSTPIPICR) Transmitted SETUP Interrupt Clear Position */
#define HSTPIPICR_CTRL_TXSTPIC        (_U_(0x1) << HSTPIPICR_CTRL_TXSTPIC_Pos)  /**< (HSTPIPICR) Transmitted SETUP Interrupt Clear Mask */
#define HSTPIPICR_CTRL_RXSTALLDIC_Pos 6                                              /**< (HSTPIPICR) Received STALLed Interrupt Clear Position */
#define HSTPIPICR_CTRL_RXSTALLDIC     (_U_(0x1) << HSTPIPICR_CTRL_RXSTALLDIC_Pos)  /**< (HSTPIPICR) Received STALLed Interrupt Clear Mask */
#define HSTPIPICR_CTRL_Msk            _U_(0x44)                                      /**< (HSTPIPICR_CTRL) Register Mask  */

/* ISO mode */
#define HSTPIPICR_ISO_UNDERFIC_Pos    2                                              /**< (HSTPIPICR) Underflow Interrupt Clear Position */
#define HSTPIPICR_ISO_UNDERFIC        (_U_(0x1) << HSTPIPICR_ISO_UNDERFIC_Pos)  /**< (HSTPIPICR) Underflow Interrupt Clear Mask */
#define HSTPIPICR_ISO_CRCERRIC_Pos    6                                              /**< (HSTPIPICR) CRC Error Interrupt Clear Position */
#define HSTPIPICR_ISO_CRCERRIC        (_U_(0x1) << HSTPIPICR_ISO_CRCERRIC_Pos)  /**< (HSTPIPICR) CRC Error Interrupt Clear Mask */
#define HSTPIPICR_ISO_Msk             _U_(0x44)                                      /**< (HSTPIPICR_ISO) Register Mask  */

/* BLK mode */
#define HSTPIPICR_BLK_TXSTPIC_Pos     2                                              /**< (HSTPIPICR) Transmitted SETUP Interrupt Clear Position */
#define HSTPIPICR_BLK_TXSTPIC         (_U_(0x1) << HSTPIPICR_BLK_TXSTPIC_Pos)  /**< (HSTPIPICR) Transmitted SETUP Interrupt Clear Mask */
#define HSTPIPICR_BLK_RXSTALLDIC_Pos  6                                              /**< (HSTPIPICR) Received STALLed Interrupt Clear Position */
#define HSTPIPICR_BLK_RXSTALLDIC      (_U_(0x1) << HSTPIPICR_BLK_RXSTALLDIC_Pos)  /**< (HSTPIPICR) Received STALLed Interrupt Clear Mask */
#define HSTPIPICR_BLK_Msk             _U_(0x44)                                      /**< (HSTPIPICR_BLK) Register Mask  */

/* INTRPT mode */
#define HSTPIPICR_INTRPT_UNDERFIC_Pos 2                                              /**< (HSTPIPICR) Underflow Interrupt Clear Position */
#define HSTPIPICR_INTRPT_UNDERFIC     (_U_(0x1) << HSTPIPICR_INTRPT_UNDERFIC_Pos)  /**< (HSTPIPICR) Underflow Interrupt Clear Mask */
#define HSTPIPICR_INTRPT_RXSTALLDIC_Pos 6                                              /**< (HSTPIPICR) Received STALLed Interrupt Clear Position */
#define HSTPIPICR_INTRPT_RXSTALLDIC     (_U_(0x1) << HSTPIPICR_INTRPT_RXSTALLDIC_Pos)  /**< (HSTPIPICR) Received STALLed Interrupt Clear Mask */
#define HSTPIPICR_INTRPT_Msk          _U_(0x44)                                      /**< (HSTPIPICR_INTRPT) Register Mask  */


/* -------- HSTPIPIFR : (USBHS Offset: 0x590) (/W 32) Host Pipe Set Register -------- */

#define HSTPIPIFR_OFFSET              (0x590)                                       /**<  (HSTPIPIFR) Host Pipe Set Register  Offset */

#define HSTPIPIFR_RXINIS_Pos          0                                              /**< (HSTPIPIFR) Received IN Data Interrupt Set Position */
#define HSTPIPIFR_RXINIS              (_U_(0x1) << HSTPIPIFR_RXINIS_Pos)       /**< (HSTPIPIFR) Received IN Data Interrupt Set Mask */
#define HSTPIPIFR_TXOUTIS_Pos         1                                              /**< (HSTPIPIFR) Transmitted OUT Data Interrupt Set Position */
#define HSTPIPIFR_TXOUTIS             (_U_(0x1) << HSTPIPIFR_TXOUTIS_Pos)      /**< (HSTPIPIFR) Transmitted OUT Data Interrupt Set Mask */
#define HSTPIPIFR_PERRIS_Pos          3                                              /**< (HSTPIPIFR) Pipe Error Interrupt Set Position */
#define HSTPIPIFR_PERRIS              (_U_(0x1) << HSTPIPIFR_PERRIS_Pos)       /**< (HSTPIPIFR) Pipe Error Interrupt Set Mask */
#define HSTPIPIFR_NAKEDIS_Pos         4                                              /**< (HSTPIPIFR) NAKed Interrupt Set Position */
#define HSTPIPIFR_NAKEDIS             (_U_(0x1) << HSTPIPIFR_NAKEDIS_Pos)      /**< (HSTPIPIFR) NAKed Interrupt Set Mask */
#define HSTPIPIFR_OVERFIS_Pos         5                                              /**< (HSTPIPIFR) Overflow Interrupt Set Position */
#define HSTPIPIFR_OVERFIS             (_U_(0x1) << HSTPIPIFR_OVERFIS_Pos)      /**< (HSTPIPIFR) Overflow Interrupt Set Mask */
#define HSTPIPIFR_SHORTPACKETIS_Pos   7                                              /**< (HSTPIPIFR) Short Packet Interrupt Set Position */
#define HSTPIPIFR_SHORTPACKETIS       (_U_(0x1) << HSTPIPIFR_SHORTPACKETIS_Pos)  /**< (HSTPIPIFR) Short Packet Interrupt Set Mask */
#define HSTPIPIFR_NBUSYBKS_Pos        12                                             /**< (HSTPIPIFR) Number of Busy Banks Set Position */
#define HSTPIPIFR_NBUSYBKS            (_U_(0x1) << HSTPIPIFR_NBUSYBKS_Pos)     /**< (HSTPIPIFR) Number of Busy Banks Set Mask */
#define HSTPIPIFR_Msk                 _U_(0x10BB)                                    /**< (HSTPIPIFR) Register Mask  */

/* CTRL mode */
#define HSTPIPIFR_CTRL_TXSTPIS_Pos    2                                              /**< (HSTPIPIFR) Transmitted SETUP Interrupt Set Position */
#define HSTPIPIFR_CTRL_TXSTPIS        (_U_(0x1) << HSTPIPIFR_CTRL_TXSTPIS_Pos)  /**< (HSTPIPIFR) Transmitted SETUP Interrupt Set Mask */
#define HSTPIPIFR_CTRL_RXSTALLDIS_Pos 6                                              /**< (HSTPIPIFR) Received STALLed Interrupt Set Position */
#define HSTPIPIFR_CTRL_RXSTALLDIS     (_U_(0x1) << HSTPIPIFR_CTRL_RXSTALLDIS_Pos)  /**< (HSTPIPIFR) Received STALLed Interrupt Set Mask */
#define HSTPIPIFR_CTRL_Msk            _U_(0x44)                                      /**< (HSTPIPIFR_CTRL) Register Mask  */

/* ISO mode */
#define HSTPIPIFR_ISO_UNDERFIS_Pos    2                                              /**< (HSTPIPIFR) Underflow Interrupt Set Position */
#define HSTPIPIFR_ISO_UNDERFIS        (_U_(0x1) << HSTPIPIFR_ISO_UNDERFIS_Pos)  /**< (HSTPIPIFR) Underflow Interrupt Set Mask */
#define HSTPIPIFR_ISO_CRCERRIS_Pos    6                                              /**< (HSTPIPIFR) CRC Error Interrupt Set Position */
#define HSTPIPIFR_ISO_CRCERRIS        (_U_(0x1) << HSTPIPIFR_ISO_CRCERRIS_Pos)  /**< (HSTPIPIFR) CRC Error Interrupt Set Mask */
#define HSTPIPIFR_ISO_Msk             _U_(0x44)                                      /**< (HSTPIPIFR_ISO) Register Mask  */

/* BLK mode */
#define HSTPIPIFR_BLK_TXSTPIS_Pos     2                                              /**< (HSTPIPIFR) Transmitted SETUP Interrupt Set Position */
#define HSTPIPIFR_BLK_TXSTPIS         (_U_(0x1) << HSTPIPIFR_BLK_TXSTPIS_Pos)  /**< (HSTPIPIFR) Transmitted SETUP Interrupt Set Mask */
#define HSTPIPIFR_BLK_RXSTALLDIS_Pos  6                                              /**< (HSTPIPIFR) Received STALLed Interrupt Set Position */
#define HSTPIPIFR_BLK_RXSTALLDIS      (_U_(0x1) << HSTPIPIFR_BLK_RXSTALLDIS_Pos)  /**< (HSTPIPIFR) Received STALLed Interrupt Set Mask */
#define HSTPIPIFR_BLK_Msk             _U_(0x44)                                      /**< (HSTPIPIFR_BLK) Register Mask  */

/* INTRPT mode */
#define HSTPIPIFR_INTRPT_UNDERFIS_Pos 2                                              /**< (HSTPIPIFR) Underflow Interrupt Set Position */
#define HSTPIPIFR_INTRPT_UNDERFIS     (_U_(0x1) << HSTPIPIFR_INTRPT_UNDERFIS_Pos)  /**< (HSTPIPIFR) Underflow Interrupt Set Mask */
#define HSTPIPIFR_INTRPT_RXSTALLDIS_Pos 6                                              /**< (HSTPIPIFR) Received STALLed Interrupt Set Position */
#define HSTPIPIFR_INTRPT_RXSTALLDIS     (_U_(0x1) << HSTPIPIFR_INTRPT_RXSTALLDIS_Pos)  /**< (HSTPIPIFR) Received STALLed Interrupt Set Mask */
#define HSTPIPIFR_INTRPT_Msk          _U_(0x44)                                      /**< (HSTPIPIFR_INTRPT) Register Mask  */


/* -------- HSTPIPIMR : (USBHS Offset: 0x5c0) (R/ 32) Host Pipe Mask Register -------- */

#define HSTPIPIMR_OFFSET              (0x5C0)                                       /**<  (HSTPIPIMR) Host Pipe Mask Register  Offset */

#define HSTPIPIMR_RXINE_Pos           0                                              /**< (HSTPIPIMR) Received IN Data Interrupt Enable Position */
#define HSTPIPIMR_RXINE               (_U_(0x1) << HSTPIPIMR_RXINE_Pos)        /**< (HSTPIPIMR) Received IN Data Interrupt Enable Mask */
#define HSTPIPIMR_TXOUTE_Pos          1                                              /**< (HSTPIPIMR) Transmitted OUT Data Interrupt Enable Position */
#define HSTPIPIMR_TXOUTE              (_U_(0x1) << HSTPIPIMR_TXOUTE_Pos)       /**< (HSTPIPIMR) Transmitted OUT Data Interrupt Enable Mask */
#define HSTPIPIMR_PERRE_Pos           3                                              /**< (HSTPIPIMR) Pipe Error Interrupt Enable Position */
#define HSTPIPIMR_PERRE               (_U_(0x1) << HSTPIPIMR_PERRE_Pos)        /**< (HSTPIPIMR) Pipe Error Interrupt Enable Mask */
#define HSTPIPIMR_NAKEDE_Pos          4                                              /**< (HSTPIPIMR) NAKed Interrupt Enable Position */
#define HSTPIPIMR_NAKEDE              (_U_(0x1) << HSTPIPIMR_NAKEDE_Pos)       /**< (HSTPIPIMR) NAKed Interrupt Enable Mask */
#define HSTPIPIMR_OVERFIE_Pos         5                                              /**< (HSTPIPIMR) Overflow Interrupt Enable Position */
#define HSTPIPIMR_OVERFIE             (_U_(0x1) << HSTPIPIMR_OVERFIE_Pos)      /**< (HSTPIPIMR) Overflow Interrupt Enable Mask */
#define HSTPIPIMR_SHORTPACKETIE_Pos   7                                              /**< (HSTPIPIMR) Short Packet Interrupt Enable Position */
#define HSTPIPIMR_SHORTPACKETIE       (_U_(0x1) << HSTPIPIMR_SHORTPACKETIE_Pos)  /**< (HSTPIPIMR) Short Packet Interrupt Enable Mask */
#define HSTPIPIMR_NBUSYBKE_Pos        12                                             /**< (HSTPIPIMR) Number of Busy Banks Interrupt Enable Position */
#define HSTPIPIMR_NBUSYBKE            (_U_(0x1) << HSTPIPIMR_NBUSYBKE_Pos)     /**< (HSTPIPIMR) Number of Busy Banks Interrupt Enable Mask */
#define HSTPIPIMR_FIFOCON_Pos         14                                             /**< (HSTPIPIMR) FIFO Control Position */
#define HSTPIPIMR_FIFOCON             (_U_(0x1) << HSTPIPIMR_FIFOCON_Pos)      /**< (HSTPIPIMR) FIFO Control Mask */
#define HSTPIPIMR_PDISHDMA_Pos        16                                             /**< (HSTPIPIMR) Pipe Interrupts Disable HDMA Request Enable Position */
#define HSTPIPIMR_PDISHDMA            (_U_(0x1) << HSTPIPIMR_PDISHDMA_Pos)     /**< (HSTPIPIMR) Pipe Interrupts Disable HDMA Request Enable Mask */
#define HSTPIPIMR_PFREEZE_Pos         17                                             /**< (HSTPIPIMR) Pipe Freeze Position */
#define HSTPIPIMR_PFREEZE             (_U_(0x1) << HSTPIPIMR_PFREEZE_Pos)      /**< (HSTPIPIMR) Pipe Freeze Mask */
#define HSTPIPIMR_RSTDT_Pos           18                                             /**< (HSTPIPIMR) Reset Data Toggle Position */
#define HSTPIPIMR_RSTDT               (_U_(0x1) << HSTPIPIMR_RSTDT_Pos)        /**< (HSTPIPIMR) Reset Data Toggle Mask */
#define HSTPIPIMR_Msk                 _U_(0x750BB)                                   /**< (HSTPIPIMR) Register Mask  */

/* CTRL mode */
#define HSTPIPIMR_CTRL_TXSTPE_Pos     2                                              /**< (HSTPIPIMR) Transmitted SETUP Interrupt Enable Position */
#define HSTPIPIMR_CTRL_TXSTPE         (_U_(0x1) << HSTPIPIMR_CTRL_TXSTPE_Pos)  /**< (HSTPIPIMR) Transmitted SETUP Interrupt Enable Mask */
#define HSTPIPIMR_CTRL_RXSTALLDE_Pos  6                                              /**< (HSTPIPIMR) Received STALLed Interrupt Enable Position */
#define HSTPIPIMR_CTRL_RXSTALLDE      (_U_(0x1) << HSTPIPIMR_CTRL_RXSTALLDE_Pos)  /**< (HSTPIPIMR) Received STALLed Interrupt Enable Mask */
#define HSTPIPIMR_CTRL_Msk            _U_(0x44)                                      /**< (HSTPIPIMR_CTRL) Register Mask  */

/* ISO mode */
#define HSTPIPIMR_ISO_UNDERFIE_Pos    2                                              /**< (HSTPIPIMR) Underflow Interrupt Enable Position */
#define HSTPIPIMR_ISO_UNDERFIE        (_U_(0x1) << HSTPIPIMR_ISO_UNDERFIE_Pos)  /**< (HSTPIPIMR) Underflow Interrupt Enable Mask */
#define HSTPIPIMR_ISO_CRCERRE_Pos     6                                              /**< (HSTPIPIMR) CRC Error Interrupt Enable Position */
#define HSTPIPIMR_ISO_CRCERRE         (_U_(0x1) << HSTPIPIMR_ISO_CRCERRE_Pos)  /**< (HSTPIPIMR) CRC Error Interrupt Enable Mask */
#define HSTPIPIMR_ISO_Msk             _U_(0x44)                                      /**< (HSTPIPIMR_ISO) Register Mask  */

/* BLK mode */
#define HSTPIPIMR_BLK_TXSTPE_Pos      2                                              /**< (HSTPIPIMR) Transmitted SETUP Interrupt Enable Position */
#define HSTPIPIMR_BLK_TXSTPE          (_U_(0x1) << HSTPIPIMR_BLK_TXSTPE_Pos)   /**< (HSTPIPIMR) Transmitted SETUP Interrupt Enable Mask */
#define HSTPIPIMR_BLK_RXSTALLDE_Pos   6                                              /**< (HSTPIPIMR) Received STALLed Interrupt Enable Position */
#define HSTPIPIMR_BLK_RXSTALLDE       (_U_(0x1) << HSTPIPIMR_BLK_RXSTALLDE_Pos)  /**< (HSTPIPIMR) Received STALLed Interrupt Enable Mask */
#define HSTPIPIMR_BLK_Msk             _U_(0x44)                                      /**< (HSTPIPIMR_BLK) Register Mask  */

/* INTRPT mode */
#define HSTPIPIMR_INTRPT_UNDERFIE_Pos 2                                              /**< (HSTPIPIMR) Underflow Interrupt Enable Position */
#define HSTPIPIMR_INTRPT_UNDERFIE     (_U_(0x1) << HSTPIPIMR_INTRPT_UNDERFIE_Pos)  /**< (HSTPIPIMR) Underflow Interrupt Enable Mask */
#define HSTPIPIMR_INTRPT_RXSTALLDE_Pos 6                                              /**< (HSTPIPIMR) Received STALLed Interrupt Enable Position */
#define HSTPIPIMR_INTRPT_RXSTALLDE     (_U_(0x1) << HSTPIPIMR_INTRPT_RXSTALLDE_Pos)  /**< (HSTPIPIMR) Received STALLed Interrupt Enable Mask */
#define HSTPIPIMR_INTRPT_Msk          _U_(0x44)                                      /**< (HSTPIPIMR_INTRPT) Register Mask  */


/* -------- HSTPIPIER : (USBHS Offset: 0x5f0) (/W 32) Host Pipe Enable Register -------- */

#define HSTPIPIER_OFFSET              (0x5F0)                                       /**<  (HSTPIPIER) Host Pipe Enable Register  Offset */

#define HSTPIPIER_RXINES_Pos          0                                              /**< (HSTPIPIER) Received IN Data Interrupt Enable Position */
#define HSTPIPIER_RXINES              (_U_(0x1) << HSTPIPIER_RXINES_Pos)       /**< (HSTPIPIER) Received IN Data Interrupt Enable Mask */
#define HSTPIPIER_TXOUTES_Pos         1                                              /**< (HSTPIPIER) Transmitted OUT Data Interrupt Enable Position */
#define HSTPIPIER_TXOUTES             (_U_(0x1) << HSTPIPIER_TXOUTES_Pos)      /**< (HSTPIPIER) Transmitted OUT Data Interrupt Enable Mask */
#define HSTPIPIER_PERRES_Pos          3                                              /**< (HSTPIPIER) Pipe Error Interrupt Enable Position */
#define HSTPIPIER_PERRES              (_U_(0x1) << HSTPIPIER_PERRES_Pos)       /**< (HSTPIPIER) Pipe Error Interrupt Enable Mask */
#define HSTPIPIER_NAKEDES_Pos         4                                              /**< (HSTPIPIER) NAKed Interrupt Enable Position */
#define HSTPIPIER_NAKEDES             (_U_(0x1) << HSTPIPIER_NAKEDES_Pos)      /**< (HSTPIPIER) NAKed Interrupt Enable Mask */
#define HSTPIPIER_OVERFIES_Pos        5                                              /**< (HSTPIPIER) Overflow Interrupt Enable Position */
#define HSTPIPIER_OVERFIES            (_U_(0x1) << HSTPIPIER_OVERFIES_Pos)     /**< (HSTPIPIER) Overflow Interrupt Enable Mask */
#define HSTPIPIER_SHORTPACKETIES_Pos  7                                              /**< (HSTPIPIER) Short Packet Interrupt Enable Position */
#define HSTPIPIER_SHORTPACKETIES      (_U_(0x1) << HSTPIPIER_SHORTPACKETIES_Pos)  /**< (HSTPIPIER) Short Packet Interrupt Enable Mask */
#define HSTPIPIER_NBUSYBKES_Pos       12                                             /**< (HSTPIPIER) Number of Busy Banks Enable Position */
#define HSTPIPIER_NBUSYBKES           (_U_(0x1) << HSTPIPIER_NBUSYBKES_Pos)    /**< (HSTPIPIER) Number of Busy Banks Enable Mask */
#define HSTPIPIER_PDISHDMAS_Pos       16                                             /**< (HSTPIPIER) Pipe Interrupts Disable HDMA Request Enable Position */
#define HSTPIPIER_PDISHDMAS           (_U_(0x1) << HSTPIPIER_PDISHDMAS_Pos)    /**< (HSTPIPIER) Pipe Interrupts Disable HDMA Request Enable Mask */
#define HSTPIPIER_PFREEZES_Pos        17                                             /**< (HSTPIPIER) Pipe Freeze Enable Position */
#define HSTPIPIER_PFREEZES            (_U_(0x1) << HSTPIPIER_PFREEZES_Pos)     /**< (HSTPIPIER) Pipe Freeze Enable Mask */
#define HSTPIPIER_RSTDTS_Pos          18                                             /**< (HSTPIPIER) Reset Data Toggle Enable Position */
#define HSTPIPIER_RSTDTS              (_U_(0x1) << HSTPIPIER_RSTDTS_Pos)       /**< (HSTPIPIER) Reset Data Toggle Enable Mask */
#define HSTPIPIER_Msk                 _U_(0x710BB)                                   /**< (HSTPIPIER) Register Mask  */

/* CTRL mode */
#define HSTPIPIER_CTRL_TXSTPES_Pos    2                                              /**< (HSTPIPIER) Transmitted SETUP Interrupt Enable Position */
#define HSTPIPIER_CTRL_TXSTPES        (_U_(0x1) << HSTPIPIER_CTRL_TXSTPES_Pos)  /**< (HSTPIPIER) Transmitted SETUP Interrupt Enable Mask */
#define HSTPIPIER_CTRL_RXSTALLDES_Pos 6                                              /**< (HSTPIPIER) Received STALLed Interrupt Enable Position */
#define HSTPIPIER_CTRL_RXSTALLDES     (_U_(0x1) << HSTPIPIER_CTRL_RXSTALLDES_Pos)  /**< (HSTPIPIER) Received STALLed Interrupt Enable Mask */
#define HSTPIPIER_CTRL_Msk            _U_(0x44)                                      /**< (HSTPIPIER_CTRL) Register Mask  */

/* ISO mode */
#define HSTPIPIER_ISO_UNDERFIES_Pos   2                                              /**< (HSTPIPIER) Underflow Interrupt Enable Position */
#define HSTPIPIER_ISO_UNDERFIES       (_U_(0x1) << HSTPIPIER_ISO_UNDERFIES_Pos)  /**< (HSTPIPIER) Underflow Interrupt Enable Mask */
#define HSTPIPIER_ISO_CRCERRES_Pos    6                                              /**< (HSTPIPIER) CRC Error Interrupt Enable Position */
#define HSTPIPIER_ISO_CRCERRES        (_U_(0x1) << HSTPIPIER_ISO_CRCERRES_Pos)  /**< (HSTPIPIER) CRC Error Interrupt Enable Mask */
#define HSTPIPIER_ISO_Msk             _U_(0x44)                                      /**< (HSTPIPIER_ISO) Register Mask  */

/* BLK mode */
#define HSTPIPIER_BLK_TXSTPES_Pos     2                                              /**< (HSTPIPIER) Transmitted SETUP Interrupt Enable Position */
#define HSTPIPIER_BLK_TXSTPES         (_U_(0x1) << HSTPIPIER_BLK_TXSTPES_Pos)  /**< (HSTPIPIER) Transmitted SETUP Interrupt Enable Mask */
#define HSTPIPIER_BLK_RXSTALLDES_Pos  6                                              /**< (HSTPIPIER) Received STALLed Interrupt Enable Position */
#define HSTPIPIER_BLK_RXSTALLDES      (_U_(0x1) << HSTPIPIER_BLK_RXSTALLDES_Pos)  /**< (HSTPIPIER) Received STALLed Interrupt Enable Mask */
#define HSTPIPIER_BLK_Msk             _U_(0x44)                                      /**< (HSTPIPIER_BLK) Register Mask  */

/* INTRPT mode */
#define HSTPIPIER_INTRPT_UNDERFIES_Pos 2                                              /**< (HSTPIPIER) Underflow Interrupt Enable Position */
#define HSTPIPIER_INTRPT_UNDERFIES     (_U_(0x1) << HSTPIPIER_INTRPT_UNDERFIES_Pos)  /**< (HSTPIPIER) Underflow Interrupt Enable Mask */
#define HSTPIPIER_INTRPT_RXSTALLDES_Pos 6                                              /**< (HSTPIPIER) Received STALLed Interrupt Enable Position */
#define HSTPIPIER_INTRPT_RXSTALLDES     (_U_(0x1) << HSTPIPIER_INTRPT_RXSTALLDES_Pos)  /**< (HSTPIPIER) Received STALLed Interrupt Enable Mask */
#define HSTPIPIER_INTRPT_Msk          _U_(0x44)                                      /**< (HSTPIPIER_INTRPT) Register Mask  */


/* -------- HSTPIPIDR : (USBHS Offset: 0x620) (/W 32) Host Pipe Disable Register -------- */

#define HSTPIPIDR_OFFSET              (0x620)                                       /**<  (HSTPIPIDR) Host Pipe Disable Register  Offset */

#define HSTPIPIDR_RXINEC_Pos          0                                              /**< (HSTPIPIDR) Received IN Data Interrupt Disable Position */
#define HSTPIPIDR_RXINEC              (_U_(0x1) << HSTPIPIDR_RXINEC_Pos)       /**< (HSTPIPIDR) Received IN Data Interrupt Disable Mask */
#define HSTPIPIDR_TXOUTEC_Pos         1                                              /**< (HSTPIPIDR) Transmitted OUT Data Interrupt Disable Position */
#define HSTPIPIDR_TXOUTEC             (_U_(0x1) << HSTPIPIDR_TXOUTEC_Pos)      /**< (HSTPIPIDR) Transmitted OUT Data Interrupt Disable Mask */
#define HSTPIPIDR_PERREC_Pos          3                                              /**< (HSTPIPIDR) Pipe Error Interrupt Disable Position */
#define HSTPIPIDR_PERREC              (_U_(0x1) << HSTPIPIDR_PERREC_Pos)       /**< (HSTPIPIDR) Pipe Error Interrupt Disable Mask */
#define HSTPIPIDR_NAKEDEC_Pos         4                                              /**< (HSTPIPIDR) NAKed Interrupt Disable Position */
#define HSTPIPIDR_NAKEDEC             (_U_(0x1) << HSTPIPIDR_NAKEDEC_Pos)      /**< (HSTPIPIDR) NAKed Interrupt Disable Mask */
#define HSTPIPIDR_OVERFIEC_Pos        5                                              /**< (HSTPIPIDR) Overflow Interrupt Disable Position */
#define HSTPIPIDR_OVERFIEC            (_U_(0x1) << HSTPIPIDR_OVERFIEC_Pos)     /**< (HSTPIPIDR) Overflow Interrupt Disable Mask */
#define HSTPIPIDR_SHORTPACKETIEC_Pos  7                                              /**< (HSTPIPIDR) Short Packet Interrupt Disable Position */
#define HSTPIPIDR_SHORTPACKETIEC      (_U_(0x1) << HSTPIPIDR_SHORTPACKETIEC_Pos)  /**< (HSTPIPIDR) Short Packet Interrupt Disable Mask */
#define HSTPIPIDR_NBUSYBKEC_Pos       12                                             /**< (HSTPIPIDR) Number of Busy Banks Disable Position */
#define HSTPIPIDR_NBUSYBKEC           (_U_(0x1) << HSTPIPIDR_NBUSYBKEC_Pos)    /**< (HSTPIPIDR) Number of Busy Banks Disable Mask */
#define HSTPIPIDR_FIFOCONC_Pos        14                                             /**< (HSTPIPIDR) FIFO Control Disable Position */
#define HSTPIPIDR_FIFOCONC            (_U_(0x1) << HSTPIPIDR_FIFOCONC_Pos)     /**< (HSTPIPIDR) FIFO Control Disable Mask */
#define HSTPIPIDR_PDISHDMAC_Pos       16                                             /**< (HSTPIPIDR) Pipe Interrupts Disable HDMA Request Disable Position */
#define HSTPIPIDR_PDISHDMAC           (_U_(0x1) << HSTPIPIDR_PDISHDMAC_Pos)    /**< (HSTPIPIDR) Pipe Interrupts Disable HDMA Request Disable Mask */
#define HSTPIPIDR_PFREEZEC_Pos        17                                             /**< (HSTPIPIDR) Pipe Freeze Disable Position */
#define HSTPIPIDR_PFREEZEC            (_U_(0x1) << HSTPIPIDR_PFREEZEC_Pos)     /**< (HSTPIPIDR) Pipe Freeze Disable Mask */
#define HSTPIPIDR_Msk                 _U_(0x350BB)                                   /**< (HSTPIPIDR) Register Mask  */

/* CTRL mode */
#define HSTPIPIDR_CTRL_TXSTPEC_Pos    2                                              /**< (HSTPIPIDR) Transmitted SETUP Interrupt Disable Position */
#define HSTPIPIDR_CTRL_TXSTPEC        (_U_(0x1) << HSTPIPIDR_CTRL_TXSTPEC_Pos)  /**< (HSTPIPIDR) Transmitted SETUP Interrupt Disable Mask */
#define HSTPIPIDR_CTRL_RXSTALLDEC_Pos 6                                              /**< (HSTPIPIDR) Received STALLed Interrupt Disable Position */
#define HSTPIPIDR_CTRL_RXSTALLDEC     (_U_(0x1) << HSTPIPIDR_CTRL_RXSTALLDEC_Pos)  /**< (HSTPIPIDR) Received STALLed Interrupt Disable Mask */
#define HSTPIPIDR_CTRL_Msk            _U_(0x44)                                      /**< (HSTPIPIDR_CTRL) Register Mask  */

/* ISO mode */
#define HSTPIPIDR_ISO_UNDERFIEC_Pos   2                                              /**< (HSTPIPIDR) Underflow Interrupt Disable Position */
#define HSTPIPIDR_ISO_UNDERFIEC       (_U_(0x1) << HSTPIPIDR_ISO_UNDERFIEC_Pos)  /**< (HSTPIPIDR) Underflow Interrupt Disable Mask */
#define HSTPIPIDR_ISO_CRCERREC_Pos    6                                              /**< (HSTPIPIDR) CRC Error Interrupt Disable Position */
#define HSTPIPIDR_ISO_CRCERREC        (_U_(0x1) << HSTPIPIDR_ISO_CRCERREC_Pos)  /**< (HSTPIPIDR) CRC Error Interrupt Disable Mask */
#define HSTPIPIDR_ISO_Msk             _U_(0x44)                                      /**< (HSTPIPIDR_ISO) Register Mask  */

/* BLK mode */
#define HSTPIPIDR_BLK_TXSTPEC_Pos     2                                              /**< (HSTPIPIDR) Transmitted SETUP Interrupt Disable Position */
#define HSTPIPIDR_BLK_TXSTPEC         (_U_(0x1) << HSTPIPIDR_BLK_TXSTPEC_Pos)  /**< (HSTPIPIDR) Transmitted SETUP Interrupt Disable Mask */
#define HSTPIPIDR_BLK_RXSTALLDEC_Pos  6                                              /**< (HSTPIPIDR) Received STALLed Interrupt Disable Position */
#define HSTPIPIDR_BLK_RXSTALLDEC      (_U_(0x1) << HSTPIPIDR_BLK_RXSTALLDEC_Pos)  /**< (HSTPIPIDR) Received STALLed Interrupt Disable Mask */
#define HSTPIPIDR_BLK_Msk             _U_(0x44)                                      /**< (HSTPIPIDR_BLK) Register Mask  */

/* INTRPT mode */
#define HSTPIPIDR_INTRPT_UNDERFIEC_Pos 2                                              /**< (HSTPIPIDR) Underflow Interrupt Disable Position */
#define HSTPIPIDR_INTRPT_UNDERFIEC     (_U_(0x1) << HSTPIPIDR_INTRPT_UNDERFIEC_Pos)  /**< (HSTPIPIDR) Underflow Interrupt Disable Mask */
#define HSTPIPIDR_INTRPT_RXSTALLDEC_Pos 6                                              /**< (HSTPIPIDR) Received STALLed Interrupt Disable Position */
#define HSTPIPIDR_INTRPT_RXSTALLDEC     (_U_(0x1) << HSTPIPIDR_INTRPT_RXSTALLDEC_Pos)  /**< (HSTPIPIDR) Received STALLed Interrupt Disable Mask */
#define HSTPIPIDR_INTRPT_Msk          _U_(0x44)                                      /**< (HSTPIPIDR_INTRPT) Register Mask  */


/* -------- HSTPIPINRQ : (USBHS Offset: 0x650) (R/W 32) Host Pipe IN Request Register -------- */

#define HSTPIPINRQ_OFFSET             (0x650)                                       /**<  (HSTPIPINRQ) Host Pipe IN Request Register  Offset */

#define HSTPIPINRQ_INRQ_Pos           0                                              /**< (HSTPIPINRQ) IN Request Number before Freeze Position */
#define HSTPIPINRQ_INRQ               (_U_(0xFF) << HSTPIPINRQ_INRQ_Pos)       /**< (HSTPIPINRQ) IN Request Number before Freeze Mask */
#define HSTPIPINRQ_INMODE_Pos         8                                              /**< (HSTPIPINRQ) IN Request Mode Position */
#define HSTPIPINRQ_INMODE             (_U_(0x1) << HSTPIPINRQ_INMODE_Pos)      /**< (HSTPIPINRQ) IN Request Mode Mask */
#define HSTPIPINRQ_Msk                _U_(0x1FF)                                     /**< (HSTPIPINRQ) Register Mask  */


/* -------- HSTPIPERR : (USBHS Offset: 0x680) (R/W 32) Host Pipe Error Register -------- */

#define HSTPIPERR_OFFSET              (0x680)                                       /**<  (HSTPIPERR) Host Pipe Error Register  Offset */

#define HSTPIPERR_DATATGL_Pos         0                                              /**< (HSTPIPERR) Data Toggle Error Position */
#define HSTPIPERR_DATATGL             (_U_(0x1) << HSTPIPERR_DATATGL_Pos)      /**< (HSTPIPERR) Data Toggle Error Mask */
#define HSTPIPERR_DATAPID_Pos         1                                              /**< (HSTPIPERR) Data PID Error Position */
#define HSTPIPERR_DATAPID             (_U_(0x1) << HSTPIPERR_DATAPID_Pos)      /**< (HSTPIPERR) Data PID Error Mask */
#define HSTPIPERR_PID_Pos             2                                              /**< (HSTPIPERR) Data PID Error Position */
#define HSTPIPERR_PID                 (_U_(0x1) << HSTPIPERR_PID_Pos)          /**< (HSTPIPERR) Data PID Error Mask */
#define HSTPIPERR_TIMEOUT_Pos         3                                              /**< (HSTPIPERR) Time-Out Error Position */
#define HSTPIPERR_TIMEOUT             (_U_(0x1) << HSTPIPERR_TIMEOUT_Pos)      /**< (HSTPIPERR) Time-Out Error Mask */
#define HSTPIPERR_CRC16_Pos           4                                              /**< (HSTPIPERR) CRC16 Error Position */
#define HSTPIPERR_CRC16               (_U_(0x1) << HSTPIPERR_CRC16_Pos)        /**< (HSTPIPERR) CRC16 Error Mask */
#define HSTPIPERR_COUNTER_Pos         5                                              /**< (HSTPIPERR) Error Counter Position */
#define HSTPIPERR_COUNTER             (_U_(0x3) << HSTPIPERR_COUNTER_Pos)      /**< (HSTPIPERR) Error Counter Mask */
#define HSTPIPERR_Msk                 _U_(0x7F)                                      /**< (HSTPIPERR) Register Mask  */

#define HSTPIPERR_CRC_Pos             4                                              /**< (HSTPIPERR Position) CRCx6 Error */
#define HSTPIPERR_CRC                 (_U_(0x1) << HSTPIPERR_CRC_Pos)          /**< (HSTPIPERR Mask) CRC */

/* -------- CTRL : (USBHS Offset: 0x800) (R/W 32) General Control Register -------- */

#define CTRL_OFFSET                   (0x800)                                       /**<  (CTRL) General Control Register  Offset */

#define CTRL_RDERRE_Pos               4                                              /**< (CTRL) Remote Device Connection Error Interrupt Enable Position */
#define CTRL_RDERRE                   (_U_(0x1) << CTRL_RDERRE_Pos)            /**< (CTRL) Remote Device Connection Error Interrupt Enable Mask */
#define CTRL_VBUSHWC_Pos              8                                              /**< (CTRL) VBUS Hardware Control Position */
#define CTRL_VBUSHWC                  (_U_(0x1) << CTRL_VBUSHWC_Pos)           /**< (CTRL) VBUS Hardware Control Mask */
#define CTRL_FRZCLK_Pos               14                                             /**< (CTRL) Freeze USB Clock Position */
#define CTRL_FRZCLK                   (_U_(0x1) << CTRL_FRZCLK_Pos)            /**< (CTRL) Freeze USB Clock Mask */
#define CTRL_USBE_Pos                 15                                             /**< (CTRL) USBHS Enable Position */
#define CTRL_USBE                     (_U_(0x1) << CTRL_USBE_Pos)              /**< (CTRL) USBHS Enable Mask */
#define CTRL_UID_Pos                  24                                             /**< (CTRL) UID Pin Enable Position */
#define CTRL_UID                      (_U_(0x1) << CTRL_UID_Pos)               /**< (CTRL) UID Pin Enable Mask */
#define CTRL_UIMOD_Pos                25                                             /**< (CTRL) USBHS Mode Position */
#define CTRL_UIMOD                    (_U_(0x1) << CTRL_UIMOD_Pos)             /**< (CTRL) USBHS Mode Mask */
#define   CTRL_UIMOD_HOST_Val         _U_(0x0)                                       /**< (CTRL) The module is in USB Host mode.  */
#define   CTRL_UIMOD_DEVICE_Val       _U_(0x1)                                       /**< (CTRL) The module is in USB Device mode.  */
#define CTRL_UIMOD_HOST               (CTRL_UIMOD_HOST_Val << CTRL_UIMOD_Pos)  /**< (CTRL) The module is in USB Host mode. Position  */
#define CTRL_UIMOD_DEVICE             (CTRL_UIMOD_DEVICE_Val << CTRL_UIMOD_Pos)  /**< (CTRL) The module is in USB Device mode. Position  */
#define CTRL_Msk                      _U_(0x300C110)                                 /**< (CTRL) Register Mask  */


/* -------- SR : (USBHS Offset: 0x804) (R/ 32) General Status Register -------- */

#define SR_OFFSET                     (0x804)                                       /**<  (SR) General Status Register  Offset */

#define SR_RDERRI_Pos                 4                                              /**< (SR) Remote Device Connection Error Interrupt (Host mode only) Position */
#define SR_RDERRI                     (_U_(0x1) << SR_RDERRI_Pos)              /**< (SR) Remote Device Connection Error Interrupt (Host mode only) Mask */
#define SR_SPEED_Pos                  12                                             /**< (SR) Speed Status (Device mode only) Position */
#define SR_SPEED                      (_U_(0x3) << SR_SPEED_Pos)               /**< (SR) Speed Status (Device mode only) Mask */
#define   SR_SPEED_FULL_SPEED_Val     _U_(0x0)                                       /**< (SR) Full-Speed mode  */
#define   SR_SPEED_HIGH_SPEED_Val     _U_(0x1)                                       /**< (SR) High-Speed mode  */
#define   SR_SPEED_LOW_SPEED_Val      _U_(0x2)                                       /**< (SR) Low-Speed mode  */
#define SR_SPEED_FULL_SPEED           (SR_SPEED_FULL_SPEED_Val << SR_SPEED_Pos)  /**< (SR) Full-Speed mode Position  */
#define SR_SPEED_HIGH_SPEED           (SR_SPEED_HIGH_SPEED_Val << SR_SPEED_Pos)  /**< (SR) High-Speed mode Position  */
#define SR_SPEED_LOW_SPEED            (SR_SPEED_LOW_SPEED_Val << SR_SPEED_Pos)  /**< (SR) Low-Speed mode Position  */
#define SR_CLKUSABLE_Pos              14                                             /**< (SR) UTMI Clock Usable Position */
#define SR_CLKUSABLE                  (_U_(0x1) << SR_CLKUSABLE_Pos)           /**< (SR) UTMI Clock Usable Mask */
#define SR_Msk                        _U_(0x7010)                                    /**< (SR) Register Mask  */


/* -------- SCR : (USBHS Offset: 0x808) (/W 32) General Status Clear Register -------- */

#define SCR_OFFSET                    (0x808)                                       /**<  (SCR) General Status Clear Register  Offset */

#define SCR_RDERRIC_Pos               4                                              /**< (SCR) Remote Device Connection Error Interrupt Clear Position */
#define SCR_RDERRIC                   (_U_(0x1) << SCR_RDERRIC_Pos)            /**< (SCR) Remote Device Connection Error Interrupt Clear Mask */
#define SCR_Msk                       _U_(0x10)                                      /**< (SCR) Register Mask  */


/* -------- SFR : (USBHS Offset: 0x80c) (/W 32) General Status Set Register -------- */

#define SFR_OFFSET                    (0x80C)                                       /**<  (SFR) General Status Set Register  Offset */

#define SFR_RDERRIS_Pos               4                                              /**< (SFR) Remote Device Connection Error Interrupt Set Position */
#define SFR_RDERRIS                   (_U_(0x1) << SFR_RDERRIS_Pos)            /**< (SFR) Remote Device Connection Error Interrupt Set Mask */
#define SFR_VBUSRQS_Pos               9                                              /**< (SFR) VBUS Request Set Position */
#define SFR_VBUSRQS                   (_U_(0x1) << SFR_VBUSRQS_Pos)            /**< (SFR) VBUS Request Set Mask */
#define SFR_Msk                       _U_(0x210)                                     /**< (SFR) Register Mask  */


/** \brief DEVDMA hardware registers */
typedef struct
{
  __IO uint32_t DEVDMANXTDSC; /**< (DEVDMA Offset: 0x00) Device DMA Channel Next Descriptor Address Register */
  __IO uint32_t DEVDMAADDRESS; /**< (DEVDMA Offset: 0x04) Device DMA Channel Address Register */
  __IO uint32_t DEVDMACONTROL; /**< (DEVDMA Offset: 0x08) Device DMA Channel Control Register */
  __IO uint32_t DEVDMASTATUS; /**< (DEVDMA Offset: 0x0C) Device DMA Channel Status Register */
} devdma_t;

/** \brief HSTDMA hardware registers */
typedef struct
{
  __IO uint32_t HSTDMANXTDSC; /**< (HSTDMA Offset: 0x00) Host DMA Channel Next Descriptor Address Register */
  __IO uint32_t HSTDMAADDRESS; /**< (HSTDMA Offset: 0x04) Host DMA Channel Address Register */
  __IO uint32_t HSTDMACONTROL; /**< (HSTDMA Offset: 0x08) Host DMA Channel Control Register */
  __IO uint32_t HSTDMASTATUS; /**< (HSTDMA Offset: 0x0C) Host DMA Channel Status Register */
} hstdma_t;

/** \brief USBHS hardware registers */
typedef struct
{
  __IO uint32_t DEVCTRL;  /**< (USBHS Offset: 0x00) Device General Control Register */
  __I  uint32_t DEVISR;   /**< (USBHS Offset: 0x04) Device Global Interrupt Status Register */
  __O  uint32_t DEVICR;   /**< (USBHS Offset: 0x08) Device Global Interrupt Clear Register */
  __O  uint32_t DEVIFR;   /**< (USBHS Offset: 0x0C) Device Global Interrupt Set Register */
  __I  uint32_t DEVIMR;   /**< (USBHS Offset: 0x10) Device Global Interrupt Mask Register */
  __O  uint32_t DEVIDR;   /**< (USBHS Offset: 0x14) Device Global Interrupt Disable Register */
  __O  uint32_t DEVIER;   /**< (USBHS Offset: 0x18) Device Global Interrupt Enable Register */
  __IO uint32_t DEVEPT;   /**< (USBHS Offset: 0x1C) Device Endpoint Register */
  __I  uint32_t DEVFNUM;  /**< (USBHS Offset: 0x20) Device Frame Number Register */
  __I  uint8_t                        Reserved1[220];
  __IO uint32_t DEVEPTCFG[10]; /**< (USBHS Offset: 0x100) Device Endpoint Configuration Register */
  __I  uint8_t                        Reserved2[8];
  __I  uint32_t DEVEPTISR[10]; /**< (USBHS Offset: 0x130) Device Endpoint Interrupt Status Register */
  __I  uint8_t                        Reserved3[8];
  __O  uint32_t DEVEPTICR[10]; /**< (USBHS Offset: 0x160) Device Endpoint Interrupt Clear Register */
  __I  uint8_t                        Reserved4[8];
  __O  uint32_t DEVEPTIFR[10]; /**< (USBHS Offset: 0x190) Device Endpoint Interrupt Set Register */
  __I  uint8_t                        Reserved5[8];
  __I  uint32_t DEVEPTIMR[10]; /**< (USBHS Offset: 0x1C0) Device Endpoint Interrupt Mask Register */
  __I  uint8_t                        Reserved6[8];
  __O  uint32_t DEVEPTIER[10]; /**< (USBHS Offset: 0x1F0) Device Endpoint Interrupt Enable Register */
  __I  uint8_t                        Reserved7[8];
  __O  uint32_t DEVEPTIDR[10]; /**< (USBHS Offset: 0x220) Device Endpoint Interrupt Disable Register */
  __I  uint8_t                        Reserved8[200];
       devdma_t DEVDMA[7]; /**< Offset: 0x310 Device DMA Channel Next Descriptor Address Register */
  __I  uint8_t                        Reserved9[128];
  __IO uint32_t HSTCTRL;  /**< (USBHS Offset: 0x400) Host General Control Register */
  __I  uint32_t HSTISR;   /**< (USBHS Offset: 0x404) Host Global Interrupt Status Register */
  __O  uint32_t HSTICR;   /**< (USBHS Offset: 0x408) Host Global Interrupt Clear Register */
  __O  uint32_t HSTIFR;   /**< (USBHS Offset: 0x40C) Host Global Interrupt Set Register */
  __I  uint32_t HSTIMR;   /**< (USBHS Offset: 0x410) Host Global Interrupt Mask Register */
  __O  uint32_t HSTIDR;   /**< (USBHS Offset: 0x414) Host Global Interrupt Disable Register */
  __O  uint32_t HSTIER;   /**< (USBHS Offset: 0x418) Host Global Interrupt Enable Register */
  __IO uint32_t HSTPIP;   /**< (USBHS Offset: 0x41C) Host Pipe Register */
  __IO uint32_t HSTFNUM;  /**< (USBHS Offset: 0x420) Host Frame Number Register */
  __IO uint32_t HSTADDR1; /**< (USBHS Offset: 0x424) Host Address 1 Register */
  __IO uint32_t HSTADDR2; /**< (USBHS Offset: 0x428) Host Address 2 Register */
  __IO uint32_t HSTADDR3; /**< (USBHS Offset: 0x42C) Host Address 3 Register */
  __I  uint8_t                        Reserved10[208];
  __IO uint32_t HSTPIPCFG[10]; /**< (USBHS Offset: 0x500) Host Pipe Configuration Register */
  __I  uint8_t                        Reserved11[8];
  __I  uint32_t HSTPIPISR[10]; /**< (USBHS Offset: 0x530) Host Pipe Status Register */
  __I  uint8_t                        Reserved12[8];
  __O  uint32_t HSTPIPICR[10]; /**< (USBHS Offset: 0x560) Host Pipe Clear Register */
  __I  uint8_t                        Reserved13[8];
  __O  uint32_t HSTPIPIFR[10]; /**< (USBHS Offset: 0x590) Host Pipe Set Register */
  __I  uint8_t                        Reserved14[8];
  __I  uint32_t HSTPIPIMR[10]; /**< (USBHS Offset: 0x5C0) Host Pipe Mask Register */
  __I  uint8_t                        Reserved15[8];
  __O  uint32_t HSTPIPIER[10]; /**< (USBHS Offset: 0x5F0) Host Pipe Enable Register */
  __I  uint8_t                        Reserved16[8];
  __O  uint32_t HSTPIPIDR[10]; /**< (USBHS Offset: 0x620) Host Pipe Disable Register */
  __I  uint8_t                        Reserved17[8];
  __IO uint32_t HSTPIPINRQ[10]; /**< (USBHS Offset: 0x650) Host Pipe IN Request Register */
  __I  uint8_t                        Reserved18[8];
  __IO uint32_t HSTPIPERR[10]; /**< (USBHS Offset: 0x680) Host Pipe Error Register */
  __I  uint8_t                        Reserved19[104];
       hstdma_t HSTDMA[7]; /**< Offset: 0x710 Host DMA Channel Next Descriptor Address Register */
  __I  uint8_t                        Reserved20[128];
  __IO uint32_t CTRL;     /**< (USBHS Offset: 0x800) General Control Register */
  __I  uint32_t SR;       /**< (USBHS Offset: 0x804) General Status Register */
  __O  uint32_t SCR;      /**< (USBHS Offset: 0x808) General Status Clear Register */
  __O  uint32_t SFR;      /**< (USBHS Offset: 0x80C) General Status Set Register */
} dcd_registers_t;

#define USB_REG           ((dcd_registers_t *)0x40038000U)         /**< \brief (USBHS) Base Address */

#define EP_MAX            10

#define FIFO_RAM_ADDR     0xA0100000u

// Errata: The DMA feature is not available for Pipe/Endpoint 7
#define EP_DMA_SUPPORT(epnum) (epnum >= 1 && epnum <= 6)

#else // TODO : SAM3U


#endif

#endif /* _COMMON_USB_REGS_H_ */
