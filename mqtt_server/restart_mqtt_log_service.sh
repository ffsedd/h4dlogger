sh check_mqtt.sh
echo "/etc/init.d/mqtt_log_service stop"
/etc/init.d/mqtt_log_service stop
echo kill_mqtt.sh
sh kill_mqtt.sh
echo "/etc/init.d/mqtt_log_service start"
/etc/init.d/mqtt_log_service start
echo sleep 2
sleep 2
sh check_mqtt.sh
