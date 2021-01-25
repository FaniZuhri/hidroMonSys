import paho.mqtt.client as mqtt  #import mqtt client module
import mysql.connector # import mysql module 
import requests
import datetime  #import date format
#from datetime import datetime
import time  #import time format

mydb = mysql.connector.connect(
    host='localhost', #hostname
    user='smartgh', #username
    passwd='**hydr0p0n1c', #password
    database='smartgh_pemantauan') #database name'
#    print(mydb)


def on_connect(client, userdata, flags, rc):
    #print("Connected: "+str(rc)+" "+str(client)+
    #" "+str(userdata)+" "+str(flags))
    client.subscribe("hidroHAB") #subscribe topic data using mqtt
    #print(client.subscribe("data"))


def on_message(client, userdata, msg):
    print(msg.topic+" "+str(msg.payload))
    if msg.topic == "hidroHAB":
        payload = msg.payload #binary data received from mqtt
        payload = payload.decode("utf-8") #convert payload from binary to str
        #print(payload)
        #print(datetime.datetime.now())
        parse_received_data(msg.topic, payload) #parse data payload
    else:
        saved_data[msg.topic] = float(msg.payload)


def parse_received_data(topic, data):
    # Parsing data (csv)
    splice = data.split(",") #remove comma to parse data
    sn = splice[0]
    date = splice[1]
    time = splice[2]
    temp = splice[3]

    #first data received from esp32 was default RTC so it has to be ignored
    if '20165-165-165' in date:
        #change default date RTC to UTC time from computer
        date = datetime.datetime.now()
        #format date to string format
        date = date.strftime('%Y-%m-%d %H:%M:%S')
        splice = date.split(" ") #parse date and time
        date = splice[0]
        time = splice[1]
#        print(date)
#        print(time)
        #make a dummy data to avoid error. it just for the first data
        data = '2020110001,'+date+','+time+',0,-127.0,0,0,0,0,0'
    splice = data.split(",") #parse data
    temperature = splice[3]
    reservoir_temp = splice[4]
    phValue = splice[5]
    tdsValue = splice[6]
    hum = splice[7]
    cahaya = splice[8]
    vol = splice[9]

    print(splice)
#    print(date)
#    print(time)
#    print(temp)
    # delay_gw_server database local
    concat = date + ' ' + time #give date and time format to dt
    #current time UTC computer
    date_now = datetime.datetime.now().replace(microsecond=0)
    #convert dt to string format
    dt = datetime.datetime.strptime(concat, '%Y-%m-%d %H:%M:%S')
    diff = (date_now - dt) #calculate delay_gw_server
    diff = diff.seconds #just pick seconds part
    print(date_now)
    print(dt)
    print(diff)
    # send to db local moni
    mycursor = mydb.cursor()
    sql ="INSERT INTO moni (sn, dgw, tgw, delay_gw_server) VALUES (%s, %s, %s, %s)"
    val = (sn, date, time, diff)
    mycursor.execute(sql, val)
    # send to db local moni_detail
    sql21 = "INSERT INTO moni_detail (id, sensor, nilai)"
    sql22 = " VALUES (LAST_INSERT_ID(), %s, %s)"
    sql2 = "".join((sql21, sql22))
    val2 = [('Cahaya', cahaya), ('Temp', temp), ('Hum', hum), ('Volume', vol),
            ('TDS_val', tdsValue), ('Water_temp', reservoir_temp),
            ('pH_val', phValue)]
    mycursor.executemany(sql2, val2)
    mydb.commit()
    # send to db server moni & moni_detail on server omahiot.com
    #send to moni on omahiot.com
    data1 = "cahaya"
    data2 = "temperature"
    data3 = "humidity"
    data4 = "distance"
    data5 = "TDS"
    data6 = "reservoir_temp"
    data7 = "pH"
    s = "{}x{}x{}x{}x{}x{}x{}" #make a sensor array
    sensor = (s.format(data1, data2, data3, data4, data5, data6, data7))
#    s = "{}"
#    sensor = (s.format(data2))

    val1 = cahaya
    val2 = temp
    val3 = hum
    val4 = vol
    val5 = tdsValue
    val6 = reservoir_temp
    val7 = phValue
    n = "{}x{}x{}x{}x{}x{}x{}" #make value of sensor array
    nilai = (n.format(val1, val2, val3, val4, val5, val6, val7))
#    n = "{}"
#    nilai = (n.format(val2))
    #preparing send to moni in omahiot.com
    data = {'sn': sn,
            'dgw': date,
            'tgw': time,
            'sensor': sensor,
            'nilai': nilai}
    #post data to db server omahiot.com
    post =requests.get('http://smart-gh.com/input.php?sn=2020060001', params=data)
    if post.status_code == 200:
        print('Data Monitoring has been sent to Database Server')
    elif post.status_code == 404:
        print('Not Found. \n')
    elif post.status_code == 500:
        print('Failed \n')
    # send to dbserver plant_user
    set1 = "ph"
    set2 = "tds"
    sett = "{}x{}"
    sens = (sett.format(set1, set2))
    setval1 = phValue
    setval2 = tdsValue
    setval = "{}x{}"
    val = (sett.format(set1, set2))
    setup = {'tanaman': "Tomat",
             'thd_ph': phValue,
             'thd_tds': tdsValue,
             'thd_wl': vol,
             'tgl_tanam': "2020-02-11",
             'sensor': sens,
             'nilai': val}
    sendsetup =requests.get('http://omahiot.com/input.php?sn=2020060001', params=setup)
    if sendsetup.status_code == 200:
        print('Data Setup has been sent to Database Server \n')
    elif sendsetup.status_code == 404:
        print('Not Found.')
    # publish data
#    for topic in topics:
#        print("Publishing %f to topic %s"%(saved_data[topic], topic))
#        client.publish(topic, saved_data[topic])
#    print(splice)


client = mqtt.Client()
client.on_connect = on_connect
client.on_message = on_message


client.username_pw_set("", "")
client.connect("192.168.43.182", 1883, 60)

client.loop_forever()
