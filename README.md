# tr181-xpon

[[_TOC_]]

## Introduction

### Overview

`tr181-xpon` is the generic part of the TR-181 PON manager. A PON manager is responsible for managing a PON/OMCI stack. It hides the rest of the US management software from any vendor specific PON/OMCI implementation details.

### Abbreviations

| Abbreviation | Meaning                               |
| :----------- | :------------------------------------ |
| DM           | data model                            |
| IF           | interface                             |
| IPC          | interprocess communication            |
| OMCI         | ONU management and control interface  |
| ONU          | Optical Network Unit                  |
| PON          | Passive Optical Network               |
| PPTP         | Physical path termination point       |
| UNI          | User Network Interface                |
| US           | user space                            |
| VEIP         | Virtual Ethernet Interface Point      |

## Architecture

### Overall architecture

The diagram below shows the overall architecture:

```
             Bus (ubus, pcb, ...)
                    |
                    |
        TR-181 XPON | Manager (tr181-xpon)
     +----------------------------------------+
     |                                        |
     |                                        |
     |           +-----------------------------+
     |           | PON vendor module           |
     |           |                             |
     +-----------|                             |
                 +-----------------------------+
                        |
                 +-----------------------------+
                 |  PON/OMCI stack             |
                 |  (executables, libraries,   |
                 |   drivers, device nodes,    |
                 |   etc)                      |
                 +-----------------------------+
```

The northbound IF of `tr181-xpon` exposes the `XPON` object of the [TR-181 DM](https://usp-data-models.broadband-forum.org/#sec:current-data-models). `tr181-xpon` implements the vendor independent part. The vendor specific code is in modules.

`tr181-xpon` assumes there is only 1 vendor module present (in `/usr/lib/amx/tr181-xpon/modules/`). It loads this module, and reports the module it loaded in its DM in the field `XPON.ModuleName`.

`tr181-xpon` uses the Ambiorix modularity API ([libamxm](https://gitlab.com/prpl-foundation/components/ambiorix/libraries/libamxm)) to load a vendor module (and to unload the module upon stopping).


### More detailed architecture

The diagram below shows a more detailed architecture:

```
             Bus (ubus, pcb, ...)
                    |
                    |
        TR-181 XPON | Manager (tr181-xpon)
     +--------------------------------------------------------+
     |                                                        |
     |                                                        |
     |                                                        |
     |               +    module 'pon_stat'  module 'pon_cfg' |
     |               |              /|\           /|\         |
     |               |               |             |          |
     +---------------+---------------+-------------+----------+
            function |               |             |
               calls |      function |    function |
                     |         calls |       calls |
     PON vendor      |               |             |
     module          |               |             |
     +---------------+---------------+-------------+------------------+
     |               |               |             |                  |
     |              \ /              +             +                  |
     |   module 'pon_ctrl'                                            |
     |                                                                |
     +----------------------------------------------------------------+
                   /|\
                    |  Interaction with PON/OMCI stack
                    |  of vendor
                   \|/

               PON/OMCI stack
```

`tr181-xpon` and the vendor module use the Ambiorix modularity API (`libamxm`) to register namespaces with functions. This allows them to communicate via function calls:

- The vendor module must register the namespace `pon_ctrl` with functions such as `set_max_nr_of_onus()` and `set_enable()`.
- `tr181-xpon` registers the `pon_stat` namespace with functions such as `dm_instance_added()` and `dm_instance_removed()`.
- `tr181-xpon` registers the `pon_cfg` namespace. The vendor module can use it to query certain settings, such as the PON mode or whether it should use a VEIP or PPTP Ethernet UNI as IF to the non-OMCI domain.

Sections further below give more info about those namespaces.

The PON vendor module can communicate/interact in several ways with the PON/OMCI stack:
- IPC via Unix socket(s)
- Library calls.
- Start/stop executables.


## Namespaces

`tr181-xpon` and the vendor module must register namespaces with functions to communicate with each other via function calls.

They must register those namespaces during startup.

### pon\_stat namespace

`tr181-xpon` registers the `pon_stat` namespace with following functions before it loads the vendor module:

- `dm_instance_added`
- `dm_instance_removed`
- `dm_object_changed`
- `dm_add_or_change_instance`
- `omci_reset_mib`
- `dm_set_xpon_parameter`
- `watch_file_descriptor_start`
- `watch_file_descriptor_stop`


The vendor module can call the 1st 6 functions to notify `tr181-xpon` to update its DM.

`watch_file_descriptor_start()` instructs `tr181-xpon` to add a file descriptor to its event loop. `tr181-xpon`  calls `handle_file_descriptor()` (see section `pon_ctrl` below) if it detects the file descriptor is ready to read.

### pon\_cfg namespace

`tr181-xpon` registers the `pon_cfg` namespace with following functions before it loads the vendor module:

- `pon_cfg_get_param_value`

The vendor module can use this function to request a (configuration) value from the DM.

Example: the vendor module can request whether it should use a VEIP or PPTP Ethernet UNI as IF to the non-OMCI domain.

### pon\_ctrl namespace

#### General

The vendor module must register the `pon_ctrl` namespace with the following functions when it's loaded. The table indicates for each function if it is mandatory or not:


| Function name                | Mandatory  |
| :--------------------------- | :--------- |
| `set_max_nr_of_onus`         | Yes        |
| `set_enable`                 | Yes        |
| `get_list_of_instances`      | Yes        |
| `get_object_content`         | Yes        |
| `get_param_values`           | Yes        |
| `set_password`               | No         |
| `handle_file_descriptor`     | No         |


#### set\_password()

`tr181-xpon` calls the function `set_password((` if the `Password` parameter of the `TC.Authentication` object of an ANI instance is updated. If a project requires support for a PON password, the password should normally be configured via that parameter. Then the vendor module must implement `set_password()`.

If a project does not require support for a PON password, normally no one will update that parameter. If the vendor module is only used on such projects, there is no need for the vendor module to implement `set_password()`.

#### handle\_file\_descriptor()

A vendor module can call `watch_file_descriptor_start()` to instruct `tr181-xpon` to add a file descriptor to its event loop. `tr181-xpon` calls `handle_file_descriptor()` if such a file descriptor becomes ready to read.

A vendor module which calls `watch_file_descriptor_start()` must implement `handle_file_descriptor()`.

A vendor module which does not call `watch_file_descriptor_start()` does not have to implement `handle_file_descriptor()` (as `tr181-xpon` will never call it).

## Startup behavior

### General

The XPON DM is initially empty except for the `XPON` object and its parameters.

`tr181-xpon` asks the vendor module at startup for info to populate the DM.

More specifically it calls the functions `get_list_of_instances()` and `get_object_content()` to collect info to populate the DM. It first calls `get_list_of_instances()` to query the number of ONU instances. The vendor module might reply that there is just 1 instance: `XPON.ONU.1`. `tr181-xpon` does not create that instance in the DM yet upon receiving that reply. It first calls `get_object_content()` to get the values for one or more parameters of `XPON.ONU.1`. Upon receiving the reply, `tr181-xpon` creates the instance `XPON.ONU.1` and it assigns values to the parameters of that instance for which it got a value from the vendor module. `tr181-xpon` uses a transaction to create the object and to assign values to those parameters in 1 atomic operation.

Assume the vendor module indicated `XPON.ONU.1` exists. Besides querying the content of that instance, `tr181-xpon` also calls `get_list_of_instances()` 3 times to ask the vendor module how many instances there are of `XPON.ONU.1.SoftwareImage`, `XPON.ONU.1.EthernetUNI` and `XPON.ONU.1.ANI` respectively.

`tr181-xpon` populates the XPON DM by descending deeper into the hierarchy until it has queried all sub-objects.

### Maximum number of ONUs

`tr181-xpon` has following compile time setting to configure the max number of ONUs on the board:

```
CONFIG_SAH_AMX_TR181_XPON_MAX_ONUS
```

Its default value is 1. `tr181-xpon` regularly asks the vendor module at startup which instances there are for `XPON.ONU`. `tr181-xpon` assumes that the vendor module might not immediately know the correct answer. It might take a few minutes. A board might e.g. have a G-PON IF and an XGS-PON IF. After startup it might take a while before the vendor module sees those 2 PON IFs. If e.g. the G-PON IF appears, the vendor module might reply to `get_list_of_instances()` call (querying the ONU instances) that `XPON.ONU.1` exists, with `XPON.ONU.1` being the G-PON IF. A bit later the XGS-PON IF might appear, and then the vendor module will reply that there 2 `XPON.ONU` instances: `XPON.ONU.1` for the G-PON IF and `XPON.ONU.2` for the XGS-PON IF.

`tr181-xpon` queries the number of ONUs for about 5 minutes after startup until the number of instances reported by the vendor module is equal to `CONFIG_SAH_AMX_TR181_XPON_MAX_ONUS`. Then `tr181-xpon` assumes all PON IFs are 'found' and it stops polling.


## Specific features

### VEIP versus PPTP Ethernet UNI

For a multi-managed ONU it must be possible to select the IF to the non-OMCI management domain. Cfr. BBF TR-280, R-68:

"Multi-managed ONU MUST implement either a Virtual Ethernet Interface Point (VEIP) interface or a Physical Path Termination Point (PPTP) UNI interface as the interface to the non OMCI management domain."

`tr181-xpon` defines the protected read-write parameter `UsePPTPEthernetUNIasIFtoNonOmciDomain` for the `ONU` object. Its default value is false. The ONU must use a PPTP Ethernet UNI if it is true, else the ONU must use a VEIP.

A vendor module can call `pon_cfg_get_param_value()` to get the value of that parameter. The vendor PON/OMCI stack should offer a way to configure this selection. Then the vendor module can do this selection depending on the value of `UsePPTPEthernetUNIasIFtoNonOmciDomain`.


## Howto test in a docker container

Follow instructions on [Ambiorix getting started](https://gitlab.com/prpl-foundation/components/ambiorix/tutorials/getting-started) to create a container, and build and install the Ambiorix framework.

Also compile and install `mod_sahtrace`. Clone the repo somewhere in the folder shared between the host and the container:

```
git clone git@gitlab.com:prpl-foundation/components/core/modules/mod_sahtrace.git
```

Compile and install it in a non-root container shell:

```
cd mod_sahtrace
make
sudo -E make install
```

For some vendor modules `mod_dmstats` is required. Clone the repo somewhere in the folder shared between the host and the container:

```
git clone git@gitlab.com:prpl-foundation/components/core/modules/mod_dmstats.git
```

Compile and install it in a non-root container shell:

```
cd mod_dmstats
make
sudo -E make install
```

Make sure that the following config variable is defined at the end in `makefile.inc` to enable the netdev interface statistics:
```
CONFIG_SAH_AMX_TR181_XPON_USE_NETDEV_COUNTERS=y
```


Check out `tr181-xpon` and a PON vendor module somewhere in the workspace shared between the container and the host. Choose a vendor module which can be tested in a container.

Because the container does not install netmodel, remove following lines from `odl/tr181-xpon.odl[.m4]`:

```
requires "NetModel.";
import "mod-netmodel.so" as "${mod_name}";
    entry-point mod_nm.mod_netmodel_main;
```

If you want to run `tr181-xpon` with more logging, set the trace level of the zones `xpon` and `module` in `odl/tr181-xpon.odl` to 500:

```
    trace-zones = {
        "xpon" = 500,
        "module" = 500,
        "nm_populate" = 200
    };
```

:warning: Avoid committing `odl/tr181-xpon.odl` with those changes.

All next steps must be done in the container. Compilation can be done in a shell without root permissions. The other steps must be done in a shell with root permissions.

In the container, compile and install `tr181-xpon` and the vendor module: in the top level folder of each, run `make` and `sudo -E make install`.

Ensure the folder with the vendor modules, `/usr/lib/amx/tr181-xpon/modules`, has only 1 vendor module: `tr181-xpon` will load the single vendor module from that folder.

In a shell with root permissions, run following commands:

```
rm -f /var/log/messages; syslog-ng
ubusd &
tail -f /var/log/messages
```

In another shell with root permissions, start `tr181-xpon`:

```
tr181-xpon
```

The `tr181-xpon` process should start and load the single vendor module. You can query the XPON DM in another shell, e.g. with `ubus-cli`:

```
ubus-cli "XPON.?"
```

If you also want to see the protected paramaters, run:

```
ubus-cli "protected;XPON.?"
```

Or start `ubus-cli` and run the following commands in the cli:

```
$ ubus-cli
> protected
> XPON.?
```

The DM should show the vendor module `tr181-xpon` loaded, and whether there was an error or not, e.g.:

```
XPON.ModuleName=mod-xpon-prpl
XPON.ModuleError=0
```

To query the trace levels, run following command in `ubus-cli`:

```
XPON.list_trace_zones()
```

This should give output which looks as follows:

```
 >  XPON.list_trace_zones()
XPON.list_trace_zones() returned
[
    {
        module = 200,
        xpon = 200
    }
]
```

To change the trace zone levels:

```
XPON.set_trace_zone(zone=xpon,level=500)
XPON.set_trace_zone(zone=module,level=500)
```

It depends on the vendor module which next steps you can test. If the vendor module created one instance of `XPON.ONU`, e.g. `XPON.ONU.1`, it normally should be possible to enable the ONU:

```
XPON.ONU.1.Enable=1
```

