/****************************************************************************
**
** Copyright (c) 2022 SoftAtHome
**
** Redistribution and use in source and binary forms, with or
** without modification, are permitted provided that the following
** conditions are met:
**
** 1. Redistributions of source code must retain the above copyright
** notice, this list of conditions and the following disclaimer.
**
** 2. Redistributions in binary form must reproduce the above
** copyright notice, this list of conditions and the following
** disclaimer in the documentation and/or other materials provided
** with the distribution.
**
** Subject to the terms and conditions of this license, each
** copyright holder and contributor hereby grants to those receiving
** rights under this license a perpetual, worldwide, non-exclusive,
** no-charge, royalty-free, irrevocable (except for failure to
** satisfy the conditions of this license) patent license to make,
** have made, use, offer to sell, sell, import, and otherwise
** transfer this software, where such license applies only to those
** patent claims, already acquired or hereafter acquired, licensable
** by such copyright holder or contributor that are necessarily
** infringed by:
**
** (a) their Contribution(s) (the licensed copyrights of copyright
** holders and non-copyrightable additions of contributors, in
** source or binary form) alone; or
**
** (b) combination of their Contribution(s) with the work of
** authorship to which such Contribution(s) was added by such
** copyright holder or contributor, if, at the time the Contribution
** is added, such addition causes such combination to be necessarily
** infringed. The patent license shall not apply to any other
** combinations which include the Contribution.
**
** Except as expressly stated above, no rights or licenses from any
** copyright holder or contributor is granted under this license,
** whether expressly, by implication, estoppel or otherwise.
**
** DISCLAIMER
**
** THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND
** CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES,
** INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
** MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
** DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDERS OR
** CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
** SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
** LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF
** USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
** AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
** LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
** ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
** POSSIBILITY OF SUCH DAMAGE.
**
****************************************************************************/

%define {
    /**
        Access Node Interface (ANI) table. An ANI models the xPON MAC/PHY as
        defined in the ITU-T PON standards.

        This object is not an interface object as described in [Section
        4.2/TR-181i2], but it has many of the same core parameters as an
        interface object, and they follow largely the same conventions. The most
        important deviations are:

        - This object does not have a LowerLayers parameter.
        - The value LowerLayerDown is not a valid value for its Status parameter.

        Because it's not an interface object, it does not occur in the
        InterfaceStack table.

        At most one entry in this table can exist with a given value for Alias,
        or with a given value for Name.
    */
    select XPON.ONU.ANI {
        on action destroy call interface_object_destroyed;

        /**
            Enables or disables the interface.

            If disabled, the device should force the ONU state of this ANI to O1
            (Initial state).

            This parameter is based on ifAdminStatus from [RFC2863].

            Note: forcing the state to O1 implies the device disables the TX
            laser of the associated transceiver(s). It's not required to disable
            the RX part of the transceivers as well.

            This parameter is read-write. But because the plugin does not
            handle its persistency via the odl mechanism, this parameter
            does not have the attribute '%persistent'.
        */
        bool Enable = false;

        /**
            The current operational state of the interface. Although this object
            is not an interface object, it follows largely the conventions of
            [Section 4.2.2/TR-181i2]. The most important deviation is that
            LowerLayerDown is not a valid value.

            When Enable is false then Status SHOULD normally be Down (or
            NotPresent or Error if there is a fault condition on the interface).

            When Enable becomes true then Status SHOULD change to Up if and only
            if the interface is able to transmit and receive network traffic;
            more specifically, Status should change to Up if
            TC.ONUActivation.ONUState becomes O5; it SHOULD change to Dormant if
            and only if the interface is operable but is waiting for external
            actions before it can transmit and receive network traffic (and
            subsequently change to Up if still operable when the expected
            actions have completed); it SHOULD remain in the Error state if
            there is an error or other fault condition detected on the
            interface; it SHOULD remain in the NotPresent state if the interface
            has missing (typically hardware) components; it SHOULD change to
            Unknown if the state of the interface can not be determined for some
            reason.

            This parameter is based on ifOperStatus from [RFC2863].
        */
        %read-only string Status {
            default "Down";
            on action validate call check_enum
                ["NotPresent", "Down", "Dormant", "Up", "Error", "Unknown"];
        }

        /**
            A non-volatile handle used to reference this instance. Alias
            provides a mechanism for an ACS to label this instance for future
            reference.

            If the CPE supports the Alias-based Addressing feature as defined in
            [Section 3.6.1/TR-069] and described in [Appendix II/TR-069], the
            following mandatory constraints MUST be enforced:

            - Its value MUST NOT be empty.
            - Its value MUST start with a letter.
            - If its value is not assigned by the ACS, it MUST start with a "cpe-" prefix.
            - The CPE MUST NOT change the parameter value.
        */
        %unique %key string Alias {
            on action validate call check_maximum_length 64;
        }

        /**
            The textual name of the ANI entry as assigned by the CPE.
        */
        %unique %key string Name {
            on action validate call check_maximum_length 64;
        }

        /**
            The accumulated time in seconds since the interface entered its
            current operational state.
        */
        %read-only uint32 LastChange {
            on action read call lastchange_on_read;
        }

        /**
            PON mode.

            The object Transceiver has a parameter of the same name. For proper
            operation, the PON mode of the transceiver(s) corresponding to this
            ANI must be equal to the PON mode of this ANI. But a user might e.g.
            accidentally insert a G-PON SFP while the PON mode of the ANI is
            XGS-PON.
        */
        %read-only string PONMode {
            default "Unknown";
            on action validate call check_enum
                ["Unknown", "G-PON", "XG-PON", "NG-PON2", "XGS-PON" ];
        }

        /**
            This object represents an ITU-T PON TC layer device.
        */
        object TC {

            /**
                This object shows info related to the activation of this ANI by
                an OLT.
            */
            object ONUActivation {

                /**
                    ONU activation state.

                    See:
                    - [Section C.12/G.9807.1]
                    - [Section 10/G.984.3]
                    - [Section 12/G.987.3]
                    - [Section 12/G.989.3]
                */
                %read-only string ONUState {
                    default "O1";
                    on action validate call check_enum
                        ["O1", "O2", "O3", "O2-3", "O4", "O5", "O6", "O7", "O8", "O9" ];
                }

                /**
                    Identifies the vendor of the ONU. See [Section 9.1.1/G.988].
                */
                %read-only string VendorID {
                    on action validate call check_maximum_length 4;
                }

                /**
                    Represents the combination of the Vendor-ID and the
                    Vendor-specific serial number (VSSN). The parameter shows
                    the serial number in a human readable format. Example: if
                    the vendor ID is ABCD and the VSSN encodes the number
                    1234568, the value of this parameter is ABCD12345678. See
                    [Section 9.1.1/G.988].
                */
                %read-only string SerialNumber {
                    on action validate call check_maximum_length 12;
                }

                /**
                    Identifier that the OLT assigns to this ANI during the
                    activation. See:

                    - [Section C.6.1.5.6/G.9807.1]
                    - [Section 5.5.2/G.984.3]
                    - [Section 6.4.2/G.987.3]
                    - [Section 6.1.5.6/G.989.3]
                */
                %read-only uint32 ONUID {
                    on action validate call check_maximum 1022;
                }
            }

            object Authentication {

                /**
                    All ITU based PON standards specify authentication by PLOAM
                    password or registration ID.

                    See:
                    * G.9807.1 Section C.15.2.1
                    * G.984.3 Section VI.2
                    * G.987.3 Section 15.2.1
                    * G.989.3 Section 15.2.1

                    All those standards mention that a method to enter the
                    password is beyond their scope. This parameter and the
                    parameter 'HexadecimalPassword' standardize a method to
                    enter the password.

                    In case of G-PON as PON mode, the password can be up to 10
                    bytes long. See G.984.3 Section 9.2.4.2. For the other PON
                    modes, the password can be up to 36 bytes long. See:

                    * G.9807.1 Section C.11.3.4.2
                    * G.987.3 Section 11.3.4.2
                    * G.989.3 Section 11.3.4.2

                    If 'HexadecimalPassword' is false, the password is in ASCII
                    format. Then all 95 printable characters with decimal codes
                    in the range 32 to 126 inclusive are allowed. Each character
                    corresponds with 1 byte in the password.

                    If 'HexadecimalPassword' is true, the password is in
                    hexadecimal format. Then only the characters 0 to 9, a to f,
                    and A to F are allowed. Each character corresponds with 1
                    nibble in the password.

                    Depending on the value of 'HexadecimalPassword' and the PON
                    mode, a different number of characters are applicable. If
                    'HexadecimalPassword' is false:

                    * In case of G-PON as PON mode, only the 1st 10 characters are
                      applicable.

                    * In case of another PON mode, only the 1st 36 characters are
                      applicable.

                    If 'HexadecimalPassword' is true:

                    * In case of G-PON as PON mode, the 1st 20 characters are all
                      relevant. The other characters are not applicable.

                    * In case of another PON mode, all 72 characters are relevant.

                    This parameter is set to empty if no authentication via a
                    password is required.

                    When read, this parameter returns an empty string,
                    regardless of the actual value, unless the Controller has a
                    "secured" role.

                    NOTE - For now this parameter always returns an empty string
                           when read.
                */
                string Password {
                    on action validate call check_password;
                    on action read call hide_value;
                }
                /**
                    If false, 'Password' is in ASCII format. If true, 'Password'
                    is in hexadecimal format.

                    See the parameter 'Password' for more info.
                */
                bool HexadecimalPassword;
            }

            /**
                Performance monitoring (PM) counters.
            */
            object PM {

                /**
                    PHY PM.
                */
                object PHY {
                    %read-only uint64 CorrectedFECBytes;
                    %read-only uint64 CorrectedFECCodewords;
                    %read-only uint64 UncorrectableFECCodewords;
                    %read-only uint64 TotalFECCodewords;
                    %read-only uint32 PSBdHECErrorCount;
                    %read-only uint32 HeaderHECErrorCount;
                    %read-only uint32 UnknownProfile;
                }

                /**
                    (X)GEM PM.
                */
                object GEM {
                    %read-only uint64 FramesSent;
                    %read-only uint64 FramesReceived;
                    %read-only uint32 FrameHeaderHECErrors;
                    %read-only uint32 KeyErrors;
                }

                /**
                    PLOAM PM.
                */
                object PLOAM {
                    %read-only uint32 MICErrors;
                    %read-only uint64 DownstreamMessageCount;
                    %read-only uint64 RangingTime;
                    %read-only uint64 UpstreamMessageCount;
                }

                /**
                    OMCI PM.
                */
                object OMCI {
                    %read-only uint64 BaselineMessagesReceived;
                    %read-only uint64 ExtendedMessagesReceived;
                    %read-only uint32 MICErrors;
                }
            }

            /**
                Info about the GEM ports of this ANI.
            */
            object GEM {

                /**
                    GEM port table. Each entry gives info about a(n) (X)GEM port.

                    At most one entry in this table can exist with a given value
                    for PortID.
                */
                %read-only object Port[] {
                    counted with PortNumberOfEntries;

                    /**
                        Identifies a GEM port.

                        See:
                        - [Section C.6.1.5.8/G.9807.1]
                        - [Section 5.5.5/G.984.3]
                        - [Section 6.4.4/G.987.3]
                        - [Section 6.1.5.8/G.989.3]
                    */
                    %unique %key uint32 PortID {
                        on action validate call check_maximum 65534;
                    }

                    /**
                        Type of connection this GEM port is used for.

                        See: [Section 9.2.3/G.988].
                    */
                    %read-only string Direction {
                        default "UNI-to-ANI";
                        on action validate call check_enum
                            ["UNI-to-ANI", "ANI-to-UNI", "bidirectional" ];
                    }

                    /**
                        GEM port type.
                    */
                    %read-only string PortType {
                        default "unicast";
                        on action validate call check_enum
                            ["unicast", "multicast", "broadcast" ];
                    }

                    /**
                        Performance monitoring (PM) counters for this (X)GEM port.
                    */
                    object PM {
                        %read-only uint64 FramesSent;
                        %read-only uint64 FramesReceived;
                    }
                }
            }
            object PerformanceThresholds {
                %read-only uint32 SignalFail {
                    default 5;
                    on action validate call check_range [3, 8];
                }
                %read-only uint32 SignalDegrade {
                    default 9;
                    on action validate call check_range [4, 10];
                }
            }

            /**
                Alarms at TC level for this ANI.
            */
            object Alarms {
                // Loss of signal. See [Section 11.1.2/G.984.3].
                %read-only bool LOS;
                // Loss of frame. See [Section 11.1.2/G.984.3].
                %read-only bool LOF;
                // Signal failed. See [Section 11.1.2/G.984.3].
                %read-only bool SF;
                // Signal degraded. See [Section 11.1.2/G.984.3].
                %read-only bool SD;
                // Loss of GEM channel delineation. See [Section 11.1.2/G.984.3].
                %read-only bool LCDG;
                // Transmitter failure. See [Section 11.1.2/G.984.3].
                %read-only bool TF;
                // Start-up failure. See [Section 11.1.2/G.984.3].
                %read-only bool SUF;
                // Message error message. See [Section 11.1.2/G.984.3].
                %read-only bool MEM;
                // Deactivate ONU-ID. See [Section 11.1.2/G.984.3].
                %read-only bool DACT;
                // Disabled ONU. See [Section 11.1.2/G.984.3].
                %read-only bool DIS;
                // Link mismatching. See [Section 11.1.2/G.984.3].
                %read-only bool MIS;
                // When the ONU receives a PEE message. See [Section 11.1.2/G.984.3].
                %read-only bool PEE;
                // Remote defect indication. See [Section 11.1.2/G.984.3].
                %read-only bool RDI;
                // Loss of downstream synchronization. See [G.9807.1].
                %read-only bool LODS;

                /**
                    The ANI behaves rogue: it is not transmitting in a manner
                    consistent with parameters specified in the ITU-T PON
                    standards. Hence it can threaten all upstream transmissions
                    on the PON, causing interference and disrupting
                    communications of other ONUs on the PON. An example of rogue
                    behavior is transmitting in the wrong timeslot.

                    See:
                    - [Section C.19/G.9807.1]
                    - [Section 19/G.989.3]
                */
                %read-only bool ROGUE;
            }
        }

        /**
            Transceiver table. Each entry models a PON transceiver.

            This table MUST contain at least 0 and at most 2 entries.

            At most one entry in this table can exist with a given value for ID.
        */
        %read-only object Transceiver[2] {
            counted with TransceiverNumberOfEntries;

            /**
                The ID as assigned by the CPE to this Transceiver entry.
            */
            %unique %key uint32 ID {
                on action validate call check_maximum 1;
            }

            /**
                Transceiver type. Allowed values are given by [Table 4-1/SFF-8024].
            */
            %read-only uint32 Identifier {
                on action validate call check_maximum 255;
            }

            /**
              Vendor name. See [Table 4-1/SFF-8472].
            */
            %read-only string VendorName {
                on action validate call check_maximum_length 256;
            }
            /**
              Vendor part number. See "Vendor PN" in [Table 4-1/SFF-8472].
            */
            %read-only string VendorPartNumber {
                on action validate call check_maximum_length 256;
            }

            /**
              Vendor revision. See "Vendor rev" in [Table 4-1/SFF-8472].
            */
            %read-only string VendorRevision {
                on action validate call check_maximum_length 256;
            }

            %read-only string PONMode {
                default "Unknown";
                on action validate call check_enum
                    ["Unknown", "G-PON", "XG-PON", "NG-PON2", "XGS-PON" ];
            }

            // Connector type.
            %read-only string Connector {
                default "Unknown";
                on action validate call check_enum
                    ["Unknown", "LC", "SC", "ST", "FC", "MT-RJ"];
            }

            // Nominal data rate downstream in kbps.
            %read-only uint32 NominalBitRateDownstream;

            // Nominal data rate upstream in kbps.
            %read-only uint32 NominalBitRateUpstream;

            // Measured RX power in 0.1 dBm.
            %read-only %volatile int32 RxPower {
                on action read call read_trx_param;
            }

            // Measured TX power in 0.1 dBm.
            %read-only %volatile int32 TxPower {
                on action read call read_trx_param;
            }

            // Measured supply voltage in mV.
            %read-only %volatile uint32 Voltage {
                on action read call read_trx_param;
            }

            // Measured bias current in µA.
            %read-only %volatile uint32 Bias {
                on action read call read_trx_param;
            }

            /**
                Measured temperature in degrees celsius.

                A value of -274 (which is below absolute zero) indicates a good
                reading has not been obtained.
            */
            %read-only %volatile int32 Temperature {
                on action read call read_trx_param;
                on action validate call check_minimum -274;
            }
        }
        
        ifdef(`CONFIG_SAH_AMX_TR181_XPON_USE_NETDEV_COUNTERS', /**
        * Throughput statistics for this interface.
        *
        * The CPE MUST reset the interface's Stats parameters
        * (unless otherwise stated in individual object or parameter descriptions) either
        * when the interface becomes operationally down due to a previous administrative
        * down (i.e. the interface's Status parameter transitions to a down state after
        * the interface is disabled) or when the interface becomes administratively up
        * (i.e. the interface's Enable parameter transitions from false to true).
        * Administrative and operational interface status is discussed in [Section 4.2.2/TR-181i2].
        *
        * @version 1.0
        */
        object Stats {
            on action read call stats_object_read;
            on action list call stats_object_list;
            on action describe call stats_object_describe;
        })
    }
}

