# habPrivate

hidroMonSys was made for Hydroponic Monitoring System based on ESP32. There are some parameters that measured through sensors :

1. pH Sensor DF Robot (pH)
2. EC Sensor DF Robot (EC)
3. SHT21 (Temperature and Humidity for Green House)
4. BH1750 (Light Intensity)
5. DS18B20 (Water Temperature)
6. JSN (Height Sensor)

## Hidro.ino

__This script runs with MQTT Protocol__. You need Raspberry Pi to subscribe the topic from ESP32, and then store it to local db and server db.
Here is what you have to install in your Raspberry Pi :

1. Apache
2. MariaDB
3. PhpMyAdmin
4. Mosquitto
5. Python3

> See instructions.txt and db_details for guidance

## HidroHabRestAPI.ino

This script is same as Hidro.ino but it used HTTP protocol, so raspi doesn't need the mqtt module in it. This protocol would be suit for small datasize with less client.

## uploadToDb.py

Run this script __at background__ of raspi. make sure the local and server db has been made before.