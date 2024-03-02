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
        Ethernet UNI table (a stackable interface object as described in
        [Section 4.2/TR-181i2]). This object models User Network Interfaces
        carrying Ethernet frames.

        An EthernetUNI can be a virtual or a physical UNI. If the ONU is managed
        via OMCI, an EthernetUNI has an associated service, which is either a
        VEIP (see [Section 9.5.5/G.988]) or a PPTP Ethernet UNI (see [Section
        9.5.1/G.988]).

        If the associated service is a VEIP, the ONU shows a VEIP ME in the OMCI
        MIB. If it's a PPTP Ethernet UNI, the ONU shows a PPTP Ethernet UNI ME
        in the OMCI MIB. It is expected the associated service is a VEIP for a
        virtual UNI, and that it is a PPTP Ethernet UNI for a physical UNI.
        However, some network operators require that the ONU shows a PPTP
        Ethernet UNI ME instead of a VEIP ME in its OMCI MIB even if the
        EthernetUNI models a virtual UNI.

        At most one entry in this table can exist with a given value for Alias,
        or with a given value for Name.
    */
    select XPON.ONU.EthernetUNI {
        on action destroy call interface_object_destroyed;

        /**
            Indicates whether this interface is enabled (true) or not (false).
            If the ONU is managed via OMCI, the interface is enabled if the OLT
            set the parameter Administrative state of the ME managing the
            associated service to 0. (The value 0 unlocks the functions
            performed by the ME.) The ME is either the VEIP ME (see [Section
            9.5.5/G.988]) or the PPTP Ethernet UNI ME (see [Section
            9.5.1/G.988].

            This parameter is based on ifAdminStatus from [RFC2863].
        */
        %read-only bool Enable = false;

        /**
          The current operational state of the interface (see [Section
          4.2.2/TR-181i2]).

          When Enable is false then Status SHOULD normally be Down (or
          NotPresent or Error if there is a fault condition on the interface).

          When Enable becomes true then Status SHOULD change to Up if and only
          if the interface is able to transmit and receive network traffic; it
          SHOULD change to Dormant if and only if the interface is operable but
          is waiting for external actions before it can transmit and receive
          network traffic (and subsequently change to Up if still operable when
          the expected actions have completed); it SHOULD change to
          LowerLayerDown if and only if the interface is prevented from entering
          the Up state because one or more of the interfaces beneath it is down;
          it SHOULD remain in the Error state if there is an error or other
          fault condition detected on the interface; it SHOULD remain in the
          NotPresent state if the interface has missing (typically hardware)
          components; it SHOULD change to Unknown if the state of the interface
          can not be determined for some reason.

          If the ONU is managed via OMCI, then Status can only be Up if the ONU
          has been provisioned at OMCI level in such a way that this interface
          is able to pass traffic.

          This parameter is based on ifOperStatus from [RFC2863].
        */
        %read-only string Status {
            default "Down";
            on action validate call check_enum
                ["NotPresent", "Down", "LowerLayerDown", "Dormant", "Up", "Error", "Unknown"];
        }

        /**
          A non-volatile handle used to reference this instance. Alias provides
          a mechanism for an ACS to label this instance for future reference.

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
            The textual name of the interface entry as assigned by the CPE.
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
            Comma-separated list (maximum number of characters 1024) of strings.
            Each list item MUST be the Path Name of an interface object that is
            stacked immediately below this interface object. If the referenced
            object is deleted, the corresponding item MUST be removed from the
            list. See [Section 4.2.1/TR-181i2].

            Note: it is expected that LowerLayers will not be used: if
            EthernetUNI models a physical UNI, it represents a layer 1
            interface; if EthernetUNI models a virtual UNI, it virtualizes a
            layer 1 interface.
        */
        %read-only csv_string LowerLayers {
            on action validate call check_maximum_length 1024;
        }

        /**
            Comma-separated list (maximum number of characters 1024) of strings.
            Each list item MUST be the Path Name of a row in the ANI table. If
            the referenced object is deleted, the corresponding item MUST be
            removed from the list. References all associated ANI interfaces.

            An ONU is potentially capable of forwarding frames between a UNI and
            its associated ANI interfaces. This parameter defines such a
            relationship. However, the list does not mean that the forwarding is
            actually happening, or the forwarding policies have been
            provisioned, between the UNI and the associated ANI interfaces. For
            an ONU with a single TC layer device, the parameter value can be
            either empty for simplicity, or can be the path name of the
            associated ANI. For an ONU with multiple TC layer devices, this
            parameter value may have one or more path names.
        */
        %read-only csv_string ANIs {
            on action validate call check_maximum_length 1024;
        }

        /**
            String to identify the associated service in the ONU management
            domain to the TR-181 management domain. If the ONU is managed via
            OMCI, it is recommended to format this string as
            "(service_type,MEID)", with service_type being an enumeration of
            VEIP or PPTPEthernetUNI, and MEID the value of the attribute
            "Managed entity ID" of the ME instance managing the service
            associated with this EthernetUNI.

            Examples:

            - (VEIP,1025)
            - (PPTPEthernetUNI,257)

            An OSS (Operations Support System) having access to both the TR-181
            and the OMCI domain can then find out which EthernetUNI instance
            represents a certain VEIP or PPTPEthernetUNI ME instance in the OMCI
            domain.
        */
        %read-only string InterdomainID {
            on action validate call check_maximum_length 256;
        }

        /**
            String that provides an additional way to identify the associated
            service to the TR-181 management domain. If the ONU is managed via
            OMCI and if the service associated with this EthernetUNI is a VEIP,
            then the value of this parameter is the value of the attribute
            "Interdomain name" of the corresponding VEIP ME instance.
        */
        %read-only string InterdomainName {
            on action validate call check_maximum_length 25;
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
