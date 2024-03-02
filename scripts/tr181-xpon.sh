#!/bin/sh
ulimit -c unlimited
name="tr181-xpon"

proxypath="Device.XPON."
realpath="XPON."

function tr181_mapping () {
        ubus -t 60 wait_for ProxyManager
        ubus call ProxyManager register "{'proxy' : '$proxypath','real' : '$realpath'}"
}

case $1 in
    start|boot)
        source /etc/environment
        tr181_mapping &
        ${name} -D
        ;;
    stop|shutdown)
        if [ -f /var/run/${name}.pid ]; then
            kill `cat /var/run/${name}.pid`
        else
            killall ${name}
        fi
        ubus call ProxyManager unregister "{'proxy' : '$proxypath','real' : '$realpath'}"
        ;;
    debuginfo|d)
        # ba-cli does not yet handle protected
        if [ -e /var/run/ubus/ubus.sock ] || [ -e /var/run/ubus.sock ]; then
            ubus-cli "protected; XPON.?"
        else
            ba-cli "XPON.?"
        fi
        if [ -x /usr/lib/amx/tr181-xpon/modules/debuginfo_vendor ]; then
            /usr/lib/amx/tr181-xpon/modules/debuginfo_vendor
        fi
        ;;
    restart)
        $0 stop
        sleep 1
        $0 start
        ;;
    *)
        echo "Usage : $0 [start|boot|stop|shutdown|debuginfo|restart]"
        ;;
esac
