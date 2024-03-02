#!/usr/bin/amxrt

%config {
    // Application name
    name = "tr181-xpon";
    mod_name = "mod_nm";

    import-dbg = false;

    definition_file = "${name}_definition.odl";
    defaults_dir = "defaults.d/";

    sahtrace = {
        type = "syslog",
        level = 200
    };
    trace-zones = {
        "xpon" = 200,
        "module" = 200,
        "nm_populate" = 200
    };

    NetModel = "nm_EUNI";
    nm_EUNI = {
        InstancePath = "XPON\.ONU\..*\.EthernetUNI.",
        Tags = "xpon netdev upstream",
        Prefix = "xpon-"
    };
}

requires "NetModel.";

import "${name}.so" as "${name}";
ifdef(`CONFIG_SAH_AMX_TR181_XPON_USE_NETDEV_COUNTERS',import "mod-dmstats.so";)
import "mod-netmodel.so" as "${mod_name}";

#include "mod_sahtrace.odl";
include "${definition_file}";
include "${defaults_dir}";

%define {
    entry-point mod_nm.mod_netmodel_main;
    entry-point tr181-xpon.xpon_mngr_main;
}
