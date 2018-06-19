/* TODO:
 *  -Save&Load custom UDP port 
 *  -Auto save timer counter <?>
 *  -Ofline/online concurrent fail (long MQTT timeout) :(   
 *  -Custom text for ON / OFF
 *  -Custom text for TIMER / STATE <?>
 *  -Migrate to IBM MQTT servers! (secure connection)
 *  -Get time on wifi connection
 *  -Active sensor by daytime option 
 *  -Flash space: universal header/style/script/body/end html
 *	-Figure out web firmware updater option <ESP01 can't do it>
 */

/* Misc */
#define tbi(port, bit)   		(port) & (1 << (bit)) 
#define sbi(port, bit)  		(port) |= (1 << (bit))
#define cbi(port, bit)   		(port) &= ~(1 << (bit))
#define FPSTR(pstr_pointer) 	(reinterpret_cast<const __FlashStringHelper *>(pstr_pointer))
#define F(string_literal) 		(FPSTR(PSTR(string_literal)))

/* Basic configuration ESP01 */
#define INPUT_PIN         3   //RX
#define LED_PIN           2
#define GPIO01_TX  		  1   //TX
#define OUTPUT_PIN		  0
#define MAX_TIME_SET      11700   //seconds
#define CONFIG_TIME_OUT   120     //seconds
#define RESET_TIME_OUT    120     //seconds
#define SSID_AP           "ESP01_ID"
#define PWD_AP            "adminesp01"  
#define BROADCAST_IP      "255.255.255.255"  
#define MAX_DEVICES       25
#define DEVICES_REFRESH   1000
#define DEVICES_TIME_OUT  1500
#define MQTT_RECONNECT    10000

#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <ESP8266mDNS.h>
#include <WiFiUdp.h>
#include <PubSubClient.h>
#include <EEPROM.h>
#include <memory>

/* DATA FROM FLASH */
char ssid[33];
char password[33];
char deviceName[33];
char masterIP[16];
char inputEdgeConf;
char outputSetConf;
unsigned char timeSetByte;

unsigned int mqttPort; //2bytes
char mqttServer[51];
char mqttUsername[33];      
char mqttPwd[33];           // 34195cfd
char mqttOutputSet[101];
char mqttOutputState[101];
char mqttInputState[101];
char mqttTimerSet[101];
char mqttTimerState[101];
char mqttAllSet[101];
char mqttAlarmTrigger[101];
char mqttRefresh[101];

/*GLOBA VARIABLES*/
String deviceMAC="000000";
IPAddress deviceIP;
String deviceState="S1,IX,OX,XX";
const String deviceType = "ESP01";
String deviceConfig="C1,IR,OL,41"; //On input RISING edge output is setted to LOW until 5 minutes

unsigned int timeSet= 30; //Seconds,  MIN 0, MAX=11640
unsigned int timeCount=0;
unsigned int timeLastCount=0;
unsigned int timeWifiResetCount=0;
unsigned long timeLastReset = 0; //Seconds
unsigned long timeLoop=0;
unsigned long timeLoopConnection=0;
unsigned long timeLoopUDP=0;
unsigned long timeLoopMQTT=0;
unsigned long timeNow = 0; //Seconds
unsigned long timeBoot= 0;

bool firstConnection =    true; //First connection flag
bool serverBegan =        false;
bool udpBegan=            false;
bool mqttBegan =          false;
bool configHasRequests=   false;
bool configMode=          false;

//Config Byte 1
bool alarmTrigger=        true;	 //[0] Set device input as alarm trigger
bool configButtonEnable = true;	 //[1] Enable config button on WiFi STA
bool httpServerEnable =   true;	 //[2] Enable http server
bool mqttServerEnable =   true;	 //[3] Enable mqtt server
bool udpPortEnable =      true;	 //[4] Enable udp port communication
bool inputEdge =          true;	 //[5] Set output ACTIVE when input: True=Rising, False=Falling
bool outputSet=           LOW; 	 //[6] Logical value to set when ACTIVE
bool outputInit=          true;	 //[7] Output init state: True=ACTIVE
//Config Byte 2 reserved

bool inputStateNow = 	  false;
bool inputState =         false; //Runtime input state  
bool outputState =        LOW;	 //Runtime output state

/*DEVICES INFORMATION ARRAYS*/
/* Feature disabled, not scalable!
String devicesMAC[MAX_DEVICES]={""};
String devicesNAME[MAX_DEVICES]={""};
String devicesIP[MAX_DEVICES]={""};
String devicesSTATE[MAX_DEVICES]={""};
unsigned long devicesLASTSEEN[MAX_DEVICES]={0};

void clearDevicesList()
{
  for(int i; i< MAX_DEVICES;i++)
  { 
      devicesMAC[i]="";
      devicesNAME[i]="";
      devicesIP[i]="";   
      devicesSTATE[i]="";
      devicesLASTSEEN[i]=0;   
  }
}

void printDevicesList()
{
  for(int i; i< MAX_DEVICES;i++)
  { 
    if(devicesMAC[i]!="")
      Serial.printf("[%s]Device %d: %s,%s,%s [%s] \n", timeToString().c_str(), i, devicesMAC[i].c_str(), devicesNAME[i].c_str(),devicesIP[i].c_str(),devicesSTATE[i].c_str());    
  }
}

int indexOfMAC(String MAC, int from=0)
{
    if(from>=MAX_DEVICES)
        return -1;

    for(int i=from; i<MAX_DEVICES;i++)
    {
      if(MAC.equals(devicesMAC[i]))
      {    
            return i;
      }
    }
    
    return -1;  
}


int addDevice(String MAC, String IP, String NAME, String STATE, unsigned long lastSeen, int index=-1)
{
  int indexMac= indexOfMAC(MAC);
  if(indexMac != -1)
  {
      devicesNAME[indexMac]=NAME;
      devicesIP[indexMac]=IP;
      devicesSTATE[indexMac]=STATE;
      devicesLASTSEEN[indexMac]=lastSeen;
      return 0; // Already exist, updated 
  }
  
  for(int i; i< MAX_DEVICES;i++)
  { 
      if(devicesMAC[i]=="") //first empty place
      {
        devicesMAC[i]=MAC;
        devicesNAME[i]=NAME;
        devicesIP[i]=IP;
        devicesSTATE[i]=STATE;
        devicesLASTSEEN[i]=lastSeen; 
        printf("[%s]New Device in Net: \n \t\t\t MAC: %s \n\t\t\t IP: %s \n \t\t\t NAME: '%s' \n",
              timeToString().c_str(),MAC.c_str(), IP.c_str(), NAME.c_str());
        return 1;
      }
  }
  return -1; //No empty places
}

void removeDevice(int index)
{  
  if(index > 0 && index < MAX_DEVICES)
  {
      printf("[%s]Removing Device: \n \t\t\t MAC: %s \n\t\t\t NAME: '%s' \n",
              timeToString().c_str(),devicesMAC[index].c_str(),  devicesNAME[index].c_str());
      devicesMAC[index]="";
      devicesNAME[index]="";
      devicesIP[index]="";
      devicesSTATE[index]="";
  }
}


void purgeDevices()
{
    for(int i=1; i<MAX_DEVICES;i++)
    {
      if(devicesMAC[i]!="")
      {
        if(timeNow-devicesLASTSEEN[i]>DEVICES_TIME_OUT*30)
        {    
             removeDevice(i);
        }
        else if(timeNow-devicesLASTSEEN[i]>DEVICES_TIME_OUT)
        {    
             devicesSTATE[i]="OFFLINE"; 
        }
      }
    }

}
*/
/*------------ EPPROM ROUTINES -----------------*/

void clearEPROM()
{
	Serial.printf("[%s]Erasing EEPROM...\n",timeToString().c_str());
	for(int i=0; i<1024;i++)  
		EEPROM.write(i,0xFF);
	EEPROM.commit();  
}

void saveConfig()
{   
  for(int i=0; i<33;i++)
  {
      if(i<33)
      {
        EEPROM.write(i, ssid[i]);
        EEPROM.write(i+33, password[i]);
        EEPROM.write(i+66, deviceName[i]); 
      }
           
      if(i<16)
        EEPROM.write(i+99, masterIP[i]);  
  }
  
	unsigned char configByte1 = 0;  	
	unsigned char configByte2 = 0;

  	alarmTrigger? 		sbi(configByte1,0): cbi(configByte1,0);
  	configButtonEnable? sbi(configByte1,1): cbi(configByte1,1);
  	httpServerEnable? 	sbi(configByte1,2): cbi(configByte1,2);
  	mqttServerEnable? 	sbi(configByte1,3): cbi(configByte1,3);
  	udpPortEnable? 		sbi(configByte1,4): cbi(configByte1,4);
  	inputEdge? 			sbi(configByte1,5): cbi(configByte1,5);
  	outputSet? 			sbi(configByte1,6): cbi(configByte1,6);
  	outputInit? 		sbi(configByte1,7): cbi(configByte1,7);

  	EEPROM.write(115,configByte1);
  	EEPROM.write(116,configByte2);

  	Serial.printf("[%s]Writed config bytes: %d, %d \n",timeToString().c_str(), configByte1, configByte2);

  if(timeSet<=60)
  {
    timeSetByte=timeSet;    
  }
  else if(timeSet==MAX_TIME_SET)
  {
    timeSetByte=0xFF;
  }
  else 
  {
    timeSetByte=60+timeSet/60;
  }
  EEPROM.write(117,timeSetByte);

  EEPROM.write(118,mqttPort>>8);
  EEPROM.write(119,mqttPort&0x00ff);
  
  for(int i=0; i<101;i++)
  {
      if(i<51)
      {
        EEPROM.write(i+120,mqttServer[i]); 
      }
      if(i<33)
      {
        EEPROM.write(i+171, mqttUsername[i]);
        EEPROM.write(i+204, mqttPwd[i]);       
      }           
      EEPROM.write(i+237, mqttOutputSet[i]);
      EEPROM.write(i+338, mqttOutputState[i]);
      EEPROM.write(i+439, mqttInputState[i]);
      EEPROM.write(i+540, mqttTimerSet[i]);
      EEPROM.write(i+641, mqttTimerState[i]);
      EEPROM.write(i+742, mqttAllSet[i]);       
      EEPROM.write(i+843, mqttAlarmTrigger[i]);       
      EEPROM.write(i+944, mqttRefresh[i]);       
  }
  
  EEPROM.commit();  
}

void loadConfig()
{
  for(int i=0; i<33;i++)
  {
      ssid[i]=EEPROM.read(i);
      password[i]=EEPROM.read(i+33);
      deviceName[i]=EEPROM.read(i+66);      
      if(i<16)
        masterIP[i]=EEPROM.read(i+99);  
  }
  
  unsigned char configByte1 = EEPROM.read(115);
  unsigned char configByte2 = EEPROM.read(116);
  Serial.printf("[%s]Readed config bytes: %d, %d \n", timeToString().c_str(),configByte1, configByte2);

	alarmTrigger = 			tbi(configByte1,0);
	configButtonEnable = 	tbi(configByte1,1);
	httpServerEnable = 		tbi(configByte1,2);
	mqttServerEnable = 		tbi(configByte1,3);
	udpPortEnable = 		tbi(configByte1,4);
	inputEdge = 			tbi(configByte1,5);
	outputSet = 			tbi(configByte1,6);
	outputInit = 			tbi(configByte1,7);

  timeSetByte=EEPROM.read(117);  
  if(timeSetByte <=60)
  {
    timeSet=timeSetByte;
  }
  else if(timeSetByte < 255)
  {
    timeSet=(timeSetByte-60)*60;
  }
  else if(timeSetByte == 255)
  {
    timeSet=MAX_TIME_SET;  
  }  

  mqttPort=EEPROM.read(118)<<8;
  mqttPort+=EEPROM.read(119);

  for(int i=0; i<101;i++)
  {
      if(i<51)
      {
        mqttServer[i]=EEPROM.read(i+120); 
      }
      if(i<33)
      {
        mqttUsername[i]=EEPROM.read(i+171);
        mqttPwd[i]=EEPROM.read(i+204);       
      }           
      mqttOutputSet[i]=EEPROM.read(i+237);
      mqttOutputState[i]=EEPROM.read(i+338);
      mqttInputState[i]=EEPROM.read(i+439);
      mqttTimerSet[i]=EEPROM.read(i+540);
      mqttTimerState[i]=EEPROM.read(i+641); 
      mqttAllSet[i]=EEPROM.read(i+742);       
      mqttAlarmTrigger[i]=EEPROM.read(i+843);       
      mqttRefresh[i]=EEPROM.read(i+944);      
  }  
}


/* ---------------  WEB SERVER and UDP ------------------- */
std::unique_ptr<WiFiUDP> Udp;
unsigned int localUdpPort = 45454;  // local port to listen on
char incomingPacket[90];  // buffer for incoming packets

std::unique_ptr<ESP8266WebServer> server; 

const char htmlPage[]    	PROGMEM = "<!DOCTYPE html><html><head><title>SmartSwitch en DEVICE_NAME</title><meta http-equiv='refresh' content='2'; charset='UTF-8'><style>body{height:2000px;background:linear-gradient(141deg,#2cb5e8 0%,#1fc8db 51%, #0fb8ad  75%);}button{color:white;background-color:blue;border:none;border-radius: 10px;text-decoration:none;outline: none;transition-duration:0.1s;box-shadow: 0 8px 16px 0 rgba(0,0,0,0.2), 0 6px 12px 0 rgba(0,0,0,0.19);}button:active {background-color:darkred;transform:translateY(4px);}.buttonHome{position:sticky;top:0;background-color:darkblue;font-size:4vw;width:90%;padding: 1%;margin:0% 0% 0% 5%;}.buttonON {font-size:6vw;width:40%;padding: 5% 5%;margin:5%;}.buttonOff {background-color:red;font-size:6vw;width:40%;padding: 5% 5%;margin:5%;}.buttonTime {background-color:green;font-size:4vw;width: 10%;margin: 0%;padding: 2% 0%;}.textState {color:#696969;font-size:5vw;text-align:center;margin: 0 0;transition-duration:0.4s;}.textTime {color:#F8F8FF;font-size:3vw;text-align:center;margin: 0 0;transition-duration:0.4s;}.tittles {color:#2F4F4F;font-size:8vw;text-align:center;}</style></head><body><h1 class='tittles'> SmartSwitch </h1><a href='MASTER_IP'><button class='buttonHome'>Ir a lista de dispositivos</button></a><br><h1 class='tittles'> DEVICE_NAME </h1><a href='forceOutputOn'> <button class='buttonON'>&#x2714;Encender</button></a><a href='forceOutputOff'><button class='buttonOff'>&#x2718;Apagar</button></a><p class='textState'> SmartSwitch <b> OUTPUT_STATE </b></p><p class='textTime'> TIME_OFF </p><h1 class='tittles'> Configurar  </h1><p class='textTime'> Seleccione cuantos minutos se mantendr&aacute; encendido: </p><br><a href='timeSet0'><button class='buttonTime'>&empty;</button></a><a href='timeSet1'><button class='buttonTime'>1 </button></a><a href='timeSet5'><button class='buttonTime'> 5 </button></a><a href='timeSet10'><button class='buttonTime'>10 </button></a><a href='timeSet30'><button class='buttonTime'>30</button></a><a href='timeSet60'><button class='buttonTime'>60</button></a><a href='timeSet90'><button class='buttonTime'>90</button></a><a href='timeSet120'><button class='buttonTime'>120</button></a><a href='timeSet180'><button class='buttonTime'>180</button></a><a href='timeSetInf'><button class='buttonTime'>&infin;</button></a><br><p class='textTime'>TIME_SET</p><br><br><a href='config'><button class='buttonHome' style='display:CONFIG_MODE;'>&#x2699; Configuraci&oacute;n avanzada ...</button></a></body></html>";
const char htmlConfig[]    	PROGMEM = "<!DOCTYPE html><html><head><title>SmartSwitch en DEVICE_NAME</title><meta charset='UTF-8'><style>body{height:2000px;background:linear-gradient(141deg,#2cb5e8 0%,#1fc8db 51%, #0fb8ad  75%);}button,.button{color:white;background-color:blue;border:none;border-radius:10px;text-decoration:none;outline: none;font-size:6vw;width:100%;padding: 5%;transition-duration:0.1s;box-shadow: 0 8px 16px 0 rgba(0,0,0,0.2), 0 6px 12px 0 rgba(0,0,0,0.19);}button:active,.button:active {background-color:darkred;transform:translateY(4px);}.buttonSave{position:fixed;bottom:0;right: 0;}.buttonReset{background-color:red;padding: 2%;}.buttonHome{position:sticky;top:0;background-color:darkblue;font-size:4vw;width:90%;padding: 1%;margin:0% 0% 0% 5%;}.textState {color:#696969;font-size:5vw;text-align:center;margin: 0 0;transition-duration:0.4s;}.textTime {color:#F8F8FF;font-size:3vw;margin:0;text-align:center;}.tittles {color:#2F4F4F;font-size:8vw;text-align:center;}input[type=number],input[type=text],input[type=password],input[type=checkbox],select {color:#F0F8FF;background-color:black;font-size:4vw;border:none;padding: 2.5%;border-radius:15px;width:95%;transition-duration:0.4s;}input:focus,select:focus{background-color:#4F4F5F;}</style></head><body><h1 class='tittles'> SmartSwitch </h1><a href='http://DEVICE_IP/'><button class='button buttonHome'>Return to device menu</button></a><br><br><p class='textTime'>&#9940; You can reconfigure the device here &#9940;</p><p class='textTime'>DEVICE_MAC</p><form action='saveNewConfig' method='POST'><p class='tittles'><b>WiFi Configuration:</b></p><p class='textState'> Enter valid SSID: </p><input type='text' name='ssid' value='DEVICE_SSID' maxlength='32'><p class='textState'> Enter password: </p><input type='password' name='pwd' value='DEVICE_PWD' maxlength='32'><p class='tittles'><b>Device Configuration:</b></p><p class='textState'> Enter device name (Alphanumeric): </p><input type='text' name='name' value='DEVICE_NAME' maxlength='32'  pattern='[A-Za-z0-9.-]{4,}'><p class='textState'> Enter broadcast IP: </p><input type='text' name='master' value='MASTER_IP' maxlength='15'><br><br><p class='textState'> Select input/output control: </p><select name='edge'><option value='falling' INPUT_FALLING>When input edge falling</option><option value='rising' INPUT_RISING>When input edge rising</option></select><br><br><select name='output'><option value='low' OUTPUT_LOW>Set output logical to low</option><option value='high' OUTPUT_HIGH>Set output logical to high</option></select><br><br><p class='textState'> Auto-Off timer </p><input type='number' name='time' value='TIME_SET' min='0' max='11700'><p class='textTime'>0 : Input control disabled <br> 11700 : Auto-off disabled (max. 194 minutes)</p><br><p class='textState'> <b>OTHER CONFIGS: </b></p><select name='trigger'><option value='true' TRIGGER_EN>Input edge is a global alarm trigger</option><option value='false' TRIGGER_DIS>Input edge only affects local outputs</option></select><br><br><select name='httpserver'><option value='true' HTTP_EN>HTTP server enabled (local access)</option><option value='false' HTTP_DIS>HTTP server disabled</option></select><br><br><select name='configbutton'><option value='true' CONFIG_EN>Show config. button on WiFi STA</option><option value='false' CONFIG_DIS>Config. button only visible on WiFi AP</option></select><br><br><select name='mqttserver'><option value='true' MQTT_EN>MQTT server enabled (online access)</option><option value='false' MQTT_DIS>MQTT server disabled</option></select><br><br><select name='udpserver'><option value='true' UDP_EN>UDP port enabled (local communication)</option><option value='false' UDP_DIS>UDP port disabled</option></select><br><br><select name='outputinit'><option value='true' OUTPUT_EN>Device power-on set output on (ACTIVE)</option><option value='false' OUTPUT_DIS>Device power-on set output off </option></select><br><br><p class='tittles'><b>MQTT Configuration:</b></p><p class='textState'> Enter MQTT server (url or ip): </p><input type='text' name='server' value='MQTT_SERVER' maxlength='50'><p class='textState'> Enter MQTT port (numeric): </p><input type='text' name='port' value='MQTT_PORT' maxlength='6' pattern='[0-9]{4,6}'><p class='textState'> Enter MQTT username: </p><input type='text' name='username' value='MQTT_USERNAME' maxlength='32'><p class='textState'> Enter MQTT password: </p><input type='text' name='mqttpwd' value='MQTT_PWD' maxlength='32'><p class='textState'> Output set topic (to suscribe): </p><input type='text' name='settopic' value='OUTPUTSET_TOPIC' maxlength='100'><p class='textState'> Output status topic (to publish): </p><input type='text' name='outtopic' value='OUTPUTSTATUS_TOPIC' maxlength='100'><p class='textState'> Input status topic (to publish): </p><input type='text' name='intopic' value='INPUTSTATUS_TOPIC' maxlength='100'><p class='textState'> Timer set topic (to suscribe): </p><input type='text' name='timersettopic' value='TIMERSET_TOPIC' maxlength='100'><p class='textState'> Timer status topic (to publish): </p><input type='text' name='timertopic' value='TIMER_TOPIC' maxlength='100'><p class='textState'> Set all ligths <B>global</B> topic (to suscribe): </p><input type='text' name='allsettopic' value='ALL_SET' maxlength='100'><p class='textState'> Alarm trigger <B>global</B> topic (to publish): </p><input type='text' name='alarmtrigger' value='ALARM_TRIGGER' maxlength='100'><p class='textState'> Refresh <B>global</B> topic (to suscribe): </p><input type='text' name='refreshtopic' value='ALL_REFRESH' maxlength='100'><br><br> <input type='submit' value='&#x2714; Save and connect' class='button buttonSave'>  </form><br><p class='textTime'>Designed in Colombia by <b>Juan64Bits</b></p><a href='resetDevice'><button class='buttonReset'> &#8635; Ignore and Reset </button></a><br><br><br><br><br><br><br><br></body></html>";
const char htmlMsg[]    	PROGMEM = "<!DOCTYPE html><html><head><title>SmartSwitch en DEVICE_NAME</title><meta charset='UTF-8'><style>body{height:2000px;background:linear-gradient(141deg,#2cb5e8 0%,#1fc8db 51%, #0fb8ad  75%);}.textTime {color:#F8F8FF;font-size:5vw;margin:0;text-align:center;}.tittles {color:#2F4F4F;font-size:8vw;text-align:center;}</style></head><body><h1 class='tittles'> &#9888;Warning Message&#9888;</h1><p class='textTime'> MESSAGE </p></body></html>";
//const char htmlDevList[]    PROGMEM = "<!DOCTYPE html><html><head><title>SmartSwitch en DEVICE_NAME</title><meta http-equiv='refresh' content='2'; charset='UTF-8'><style>body{height:2000px;background:linear-gradient(141deg,#2cb5e8 0%,#1fc8db 51%, #0fb8ad  75%);}button{color:white;background-color:blue;border:none;border-radius: 10px;text-decoration:none;outline: none;transition-duration:0.1s;box-shadow: 0 8px 16px 0 rgba(0,0,0,0.2), 0 6px 12px 0 rgba(0,0,0,0.19);}button:active {background-color:darkred;transform:translateY(4px);}.buttonHome{position:sticky;top:0;background-color:darkblue;font-size:4vw;width:90%;padding: 1%;margin:0% 0% 0% 5%;} .ON {background-color:blue;font-size:4vw;width:80%;padding: 1%;margin:0.5% 10%;}.OFF {background-color:red;font-size:4vw;width:80%;padding: 1%;margin:0.5% 10%;}.tittles {color:#2F4F4F;font-size:6vw;text-align:center;}.textState {color:#696969;font-size:3vw;text-align:center;margin: 0 0;transition-duration:0.4s;}</style></head><body><h1 class='tittles'> SmartSwitch </h1><a href='http://MASTER_IP/devicesList'><button class='buttonHome'>&#8635; Actualizar lista de dispositivos</button></a><br><h1 class='tittles'> Dispositivos Conectados: </h1>DEVICES_LIST<br><br><p class='textState'> Este listado se actualiza automáticamente, si algún dispositivo no aparece por favor revice que esté encendido.</p></body></html>";
//const char htmlDevices[]    PROGMEM = "<a href='http://DEVICE_IP/'> <button class=DEVICE_STATE>DEVICE_NAME</button></a>";

void redirectToRoot() 
{
  if(serverBegan) 
  {
    server->sendHeader("Location", String("/"), true);
    server->send ( 302, "text/plain", "");
  } 
}

void htmlRoot() 
{
  if(serverBegan) 
  {
      String rootPage=FPSTR(htmlPage);
      rootPage.replace("DEVICE_NAME",deviceName);
      rootPage.replace("MASTER_IP",masterIP);  
      rootPage.replace("CONFIG_MODE",configMode||configButtonEnable?"inline-block":"none");
    
      //Show output state
      if(outputState==outputSet)
      {
        rootPage.replace("OUTPUT_STATE","Encendido");
      }
      else
      {
        rootPage.replace("OUTPUT_STATE","Apagado");
      }
    
      //Show time to power off
      long int timeStatus= timeSet - timeCount;
    
        if(timeSet==0)
        {
            rootPage.replace("TIME_OFF","¡Auto-encendido desactivado!");
        }
        else if(timeSet >= MAX_TIME_SET)   
        { 
            rootPage.replace("TIME_OFF","¡Configurado para no apagarse!");
        } else if(outputState!=outputSet) //OFF
        {
            rootPage.replace("TIME_OFF","...");
        }
        else if(timeStatus>0 && timeStatus<=60)
        {    
            rootPage.replace("TIME_OFF","Auto-apagado en " + String(timeStatus) + " seg.");
        }        
        else if(timeStatus>60)
        {
            rootPage.replace("TIME_OFF","Auto-apagado en " + String(timeStatus/60) + " min " + String(timeStatus%60) + " seg");        
        }
      
      //Show time setted to power off
        if(timeSet==0)
        {
            rootPage.replace("TIME_SET","¡Auto-encendido desactivado!");
        }
        else if(timeSet >= MAX_TIME_SET)
        {    
            rootPage.replace("TIME_SET","¡Configurado para no apagarse!"); 
        }
        else if(timeSet<=60) 
        {
            rootPage.replace("TIME_SET","Auto-apagado configurado en: " + String(timeSet) + "seg");
        }
        else
        {
            rootPage.replace("TIME_SET","Auto-apagado configurado en: " + String(timeSet/60) + "min");
        }  
        
      server->send(200, "text/html", rootPage); 
  }
}

void htmlPageConfig() 
{
  if(serverBegan) 
  {
		String rootPage=FPSTR(htmlConfig);
		rootPage.replace("DEVICE_SSID",ssid);
		rootPage.replace("DEVICE_PWD",password);  
		rootPage.replace("DEVICE_MAC",String(WiFi.macAddress()));
		rootPage.replace("DEVICE_NAME",deviceName);
		rootPage.replace("MASTER_IP",masterIP);
		rootPage.replace("DEVICE_IP",deviceIP.toString().c_str());

		if(inputEdge)
			rootPage.replace("INPUT_RISING","selected");
		else
			rootPage.replace("INPUT_FALLING","selected");

		if(outputSet)
			rootPage.replace("OUTPUT_HIGH","selected");
		else
			rootPage.replace("OUTPUT_LOW","selected");

		if(alarmTrigger)
			rootPage.replace("TRIGGER_EN","selected");
		else
			rootPage.replace("TRIGGER_DIS","selected");

		if(httpServerEnable)
			rootPage.replace("HTTP_EN","selected");
		else
			rootPage.replace("HTTP_DIS","selected");

		if(mqttServerEnable)
			rootPage.replace("MQTT_EN","selected");
		else
			rootPage.replace("MQTT_DIS","selected");

		if(udpPortEnable)
			rootPage.replace("UDP_EN","selected");
		else
			rootPage.replace("UDP_DIS","selected");

		if(outputInit)
			rootPage.replace("OUTPUT_EN","selected");
		else
			rootPage.replace("OUTPUT_DIS","selected");

		if(configButtonEnable)
			rootPage.replace("CONFIG_EN","selected");
		else
			rootPage.replace("CONFIG_DIS","selected");        

		rootPage.replace("TIME_SET",String(timeSet,DEC));  

		rootPage.replace("MQTT_SERVER",mqttServer);
		rootPage.replace("MQTT_PORT",String(mqttPort,DEC));

		rootPage.replace("MQTT_USERNAME",mqttUsername);
		rootPage.replace("MQTT_PWD",mqttPwd);

		String tempString=String(mqttOutputSet);
		tempString.replace(deviceName,"DEVICE_NAME");
		tempString.replace(mqttUsername,"MQTT_USER");
		rootPage.replace("OUTPUTSET_TOPIC",tempString);
		
		tempString=String(mqttOutputState);
		tempString.replace(deviceName,"DEVICE_NAME");
		tempString.replace(mqttUsername,"MQTT_USER");
		rootPage.replace("OUTPUTSTATUS_TOPIC",tempString);

		tempString=String(mqttInputState);
		tempString.replace(deviceName,"DEVICE_NAME");
		tempString.replace(mqttUsername,"MQTT_USER");
		rootPage.replace("INPUTSTATUS_TOPIC",tempString);  

		tempString=String(mqttTimerSet);
		tempString.replace(deviceName,"DEVICE_NAME");
		tempString.replace(mqttUsername,"MQTT_USER");
		rootPage.replace("TIMERSET_TOPIC",tempString);  

		tempString=String(mqttTimerState);
		tempString.replace(deviceName,"DEVICE_NAME");
		tempString.replace(mqttUsername,"MQTT_USER");
		rootPage.replace("TIMER_TOPIC",tempString);  

		tempString=String(mqttAllSet);
		tempString.replace(deviceName,"DEVICE_NAME");
		tempString.replace(mqttUsername,"MQTT_USER");
		rootPage.replace("ALL_SET",tempString);  
		
		tempString=String(mqttAlarmTrigger);
		tempString.replace(deviceName,"DEVICE_NAME");
		tempString.replace(mqttUsername,"MQTT_USER");
		rootPage.replace("ALARM_TRIGGER",tempString);  
		
		tempString=String(mqttRefresh);
		tempString.replace(deviceName,"DEVICE_NAME");
		tempString.replace(mqttUsername,"MQTT_USER");
		rootPage.replace("ALL_REFRESH",tempString);  

		server->send(200, "text/html", rootPage);      
		if(!configHasRequests && configMode)
		{
		  Serial.printf("[%s]Reconnect timer disabled. Please reset from configuration web page.\n",timeToString().c_str());
		  configHasRequests=true;
		}
  }
}

void htmlPageMsg(String msg) 
{
  if(serverBegan) 
  {
      String rootPage=FPSTR(htmlMsg); 
      rootPage.replace("MESSAGE",msg);
      server->send(200, "text/html", rootPage);
  }
}  

/* Feature disabled
void htmlDevicesList()
{
  if(serverBegan) 
  {
      String rootPage=FPSTR(htmlDevList);
      rootPage.replace("DEVICE_NAME",deviceName);      
      rootPage.replace("MASTER_IP",deviceIP.toString()); 
    
      String rootDevices = "", device="";
      
      for(int i; i< MAX_DEVICES;i++)
      { 
        if(devicesMAC[i]!="")
        {
          device=htmlDevices;
          device.replace("DEVICE_NAME",devicesNAME[i]);
          //device.replace("DEVICE_IP",devicesIP[i]);
          device.replace("DEVICE_IP",devicesNAME[i] + ".local");
          if(devicesSTATE[i]=="OFFLINE")
          {
            device.replace("DEVICE_STATE","'ON' disabled");
          }
          else
          {  
            device.replace("DEVICE_STATE","'" + devicesSTATE[i] + "'");
          }
          rootDevices += device;
        }
      }  
    
      rootPage.replace("DEVICES_LIST",rootDevices);  
      server->send(200, "text/html", rootPage);  
  }
}
*/

void saveNewConfig()
{
	if(serverBegan) 
	{
		server->arg("ssid").toCharArray(ssid,33);
		server->arg("pwd").toCharArray(password,33);
		server->arg("name").toCharArray(deviceName,33);
		server->arg("master").toCharArray(masterIP,16); 
      
      if(server->arg("edge")=="falling")
      {
        inputEdge=false;  
      }
      else if(server->arg("edge")=="rising")
      {
        inputEdge=true; 
      }
    
      if(server->arg("output")=="low")
      {
        outputSet=LOW;  
      }
      else if(server->arg("output")=="high")
      {
        outputSet=HIGH; 
      }


      if(server->arg("trigger")=="true")
      {
        alarmTrigger=true;  
      }
      else if(server->arg("trigger")=="false")
      {
        alarmTrigger=false; 
      }      
    

      if(server->arg("httpserver")=="true")
      {
        httpServerEnable=true;  
      }
      else if(server->arg("httpserver")=="false")
      {
        httpServerEnable=false; 
      } 

	  if(server->arg("configbutton")=="true")
      {
        configButtonEnable=true;  
      }
      else if(server->arg("configbutton")=="false")
      {
        configButtonEnable=false; 
      }  

	  if(server->arg("mqttserver")=="true")
      {
        mqttServerEnable=true;  
      }
      else if(server->arg("mqttserver")=="false")
      {
        mqttServerEnable=false; 
      }  

	  if(server->arg("udpserver")=="true")
      {
        udpPortEnable=true;  
      }
      else if(server->arg("udpserver")=="false")
      {
        udpPortEnable=false; 
      }             

	  if(server->arg("outputinit")=="true")
      {
        outputInit=true;  
      }
      else if(server->arg("outputinit")=="false")
      {
        outputInit=false; 
      }         

		timeSet=server->arg("time").toInt();

		mqttPort=server->arg("port").toInt();
		server->arg("server").toCharArray(mqttServer,51);
		server->arg("username").toCharArray(mqttUsername,33);
		server->arg("mqttpwd").toCharArray(mqttPwd,33);

		String tempString=server->arg("settopic");
		tempString.replace("DEVICE_NAME",deviceName);
		tempString.replace("MQTT_USER",mqttUsername);			
		tempString.toCharArray(mqttOutputSet,101);


		tempString=server->arg("outtopic");
		tempString.replace("DEVICE_NAME",deviceName);
		tempString.replace("MQTT_USER",mqttUsername);			
		tempString.toCharArray(mqttOutputState,101);

		tempString=server->arg("intopic");
		tempString.replace("DEVICE_NAME",deviceName);
		tempString.replace("MQTT_USER",mqttUsername);			
		tempString.toCharArray(mqttInputState,101);

		tempString=server->arg("timersettopic");
		tempString.replace("DEVICE_NAME",deviceName);
		tempString.replace("MQTT_USER",mqttUsername);			
		tempString.toCharArray(mqttTimerSet,101);

		tempString=server->arg("timertopic");
		tempString.replace("DEVICE_NAME",deviceName);
		tempString.replace("MQTT_USER",mqttUsername);			
		tempString.toCharArray(mqttTimerState,101);

		tempString=server->arg("allsettopic");
		tempString.replace("DEVICE_NAME",deviceName);
		tempString.replace("MQTT_USER",mqttUsername);			
		tempString.toCharArray(mqttAllSet,101);

		tempString=server->arg("alarmtrigger");
		tempString.replace("DEVICE_NAME",deviceName);
		tempString.replace("MQTT_USER",mqttUsername);			
		tempString.toCharArray(mqttAlarmTrigger,101);
		
		tempString=server->arg("refreshtopic");
		tempString.replace("DEVICE_NAME",deviceName);
		tempString.replace("MQTT_USER",mqttUsername);			
		tempString.toCharArray(mqttRefresh,101);  
		         
		htmlPageMsg("Data will be saved and device restarted. If the SSID and PASSWORD are correct you will have access on your WiFi network.");
	}

	saveConfig();
	Serial.printf("[%s]Config. data from web server saved, attempting to reconnect WiFi.\n",timeToString().c_str());
	tryWifi(); 
}

void setTimerTo(int value)
{
	timeSet=value*60;
	redirectToRoot();
	saveConfig();
	mqttReportTimeSet();
}

void timeSet0(){ setTimerTo(0);}
void timeSet1(){ setTimerTo(1);}
void timeSet5(){ setTimerTo(5);}
void timeSet10(){ setTimerTo(10);}
void timeSet30(){ setTimerTo(30);}
void timeSet60(){ setTimerTo(60);}
void timeSet90(){ setTimerTo(90);}
void timeSet120(){ setTimerTo(120);}
void timeSet180(){ setTimerTo(180);}
void timeSetInf(){ setTimerTo(MAX_TIME_SET/60);}
void forceOutputOn()  {forceOutput(true); redirectToRoot();}
void forceOutputOff() {forceOutput(false); redirectToRoot();}

void resetDevice()
{
  htmlPageMsg("The device is now restarting. If the saved SSID and PASSWORD are correct you will have access on your WiFi network.");
  reconfigDevice();
  ESP.reset();  
}


void sendUdpMessage(String msg, String ip=BROADCAST_IP)
{
    Udp->beginPacket(ip.c_str(), localUdpPort);
    Udp->write(msg.c_str());
    Udp->endPacket();   
}

void udpWriteStatus()
{
    //       0123456789A
    //STATE="S1,IX,OX,XX";

    deviceState = "S1,IX,OX,";

    if(inputState)
    {
        deviceState.setCharAt(4,'H');
    }
    else
    {
        deviceState.setCharAt(4,'L');
    }

    if(outputState)
    {
        deviceState.setCharAt(7,'H');
    }
    else     
    {
        deviceState.setCharAt(7,'L');
    }

    long int timeStatus= timeSet - timeCount;     
    
    if(timeSet==0)
    {
        deviceState += "00";
    }
    else if(timeSet == MAX_TIME_SET)   
    { 
        deviceState += "FF";
    } else if(outputState!=outputSet) //OFF
    {
        deviceState += "XX";
    }
    else if(timeStatus>0 && timeStatus<=60)
    {    
        deviceState += String(timeStatus, HEX);
    }        
    else if(timeStatus>60)
    {
        deviceState += String(60+timeStatus/60, HEX);
    }

    //        0123456789A
    //CONFIG="C1,IX,OX,XX";   
    
    deviceConfig="C1,IX,OX,";

    if(inputEdge)
    {
        deviceConfig.setCharAt(4,'R'); 
    }            
    else
    {
        deviceConfig.setCharAt(4,'F');
    }

    if(outputSet)
    {
        deviceConfig.setCharAt(7,'H'); 
    }
    else
    {
        deviceConfig.setCharAt(7,'L');
    }

    if(timeSet==0)
    {
        deviceConfig += "00";
    }
    else if(timeSet == MAX_TIME_SET)
    {    
        deviceConfig += "FF"; 
    }
    else if(timeSet<=60) 
    {
        deviceConfig += String(timeSet,HEX);
    }
    else
    {
        deviceConfig += String(60+timeSet/60,HEX); 
    }              
        
        
    String replyString= deviceMAC + "#ACK#" + deviceState + "#" + deviceType + ":" + deviceConfig + ":" + deviceName;
    sendUdpMessage(replyString,masterIP);
    timeLoopUDP=timeNow;
}

/*----------------- MQTT Server ------------*/

char incomingPayload[128];
std::unique_ptr<WiFiClient> espClient;
std::unique_ptr<PubSubClient> client;

void mqttCallback(char* topic, byte* payload, unsigned int length) 
{
  Serial.printf("[%s]Message arrived [",timeToString().c_str());
  Serial.print(topic);
  Serial.print("] ");
  for (int i = 0; i < length; i++) 
  {
    incomingPayload[i]=(char)payload[i];
  }
  incomingPayload[length]=0;
  Serial.printf(": [%s] \n",incomingPayload);

  // PARSE THE MQTT MESSAGE
  if(String(topic)==mqttOutputSet || String(topic)==mqttAllSet)
  {
    if ((char)payload[0] == '1') {
      forceOutputOn();
    } else if ((char)payload[0] == '0')  {
      forceOutputOff();
    }    
  }
  else if(String(topic)==mqttTimerSet)
  {
    timeSet=String(incomingPayload).toInt()*60;    
    mqttReportTimeSet();
    saveConfig();
  }
  else if(String(topic)==mqttRefresh)
  { 
    mqttReportAll();
  }
}

void mqttReportAll()
{
  mqttReportInput();
  mqttReportOutput();
  mqttReportTimeSet();
}

void mqttReportInput()
{
  if(mqttBegan)
  {
    Serial.printf("[%s]MQTT Publishing state IN[%s] \n",timeToString().c_str(),inputState==inputEdge?"ACTIVE":"OFF");    
    client->publish(mqttInputState, inputStateNow==inputEdge?"1":"0",true);
  }
}

void mqttReportTrigger()
{
    if(mqttBegan)
    {
        client->publish(mqttAlarmTrigger, inputState==inputEdge?"1":"0",true);
    }
}

void mqttReportOutput()
{
  if(mqttBegan)
  {
    Serial.printf("[%s]MQTT Publishing state OUT[%s]\n",timeToString().c_str(),outputState==outputSet?"ON":"OFF");
    client->publish(mqttOutputState, outputState==outputSet?"1":"0",true);
  }
}

void mqttReportTimeSet()
{
  if(mqttBegan)
  {
    Serial.printf("[%s]MQTT Publishing state TIMER[%d] \n",timeToString().c_str(),timeSet/60);    
    client->publish(mqttTimerState, String(timeSet/60,DEC).c_str(),true);
  }
}

/* ---------------  INIT ROUTINES ------------------- */
void hardwareInit()
{
	//Enter in config mode if TX is pulled down
	pinMode(GPIO01_TX,INPUT);
	if(digitalRead(GPIO01_TX)==LOW)
	{
		configMode=true;
		while(digitalRead(GPIO01_TX)==LOW){}
	}

	Serial.begin(115200,SERIAL_8N1,SERIAL_TX_ONLY);
	Serial.println();			

	pinMode(INPUT_PIN,INPUT);
	digitalWrite(INPUT_PIN,HIGH); //PULL UP
	inputState = digitalRead(INPUT_PIN);
	pinMode(OUTPUT_PIN,OUTPUT);	
	handleOutputActiveLow(outputSet);
	pinMode(LED_PIN,OUTPUT);
	digitalWrite(LED_PIN,LOW);        
}

void tryWifi()
{	
	reconfigDevice();
	Serial.printf("[%s]Reseting global vaiables and reconnecting WiFi\n",timeToString().c_str());
	serverBegan=false;
	udpBegan=false;
	mqttBegan=false;
	timeLoopConnection=timeNow;
	timeWifiResetCount=0;
	timeBoot=timeNow;
	firstConnection=true;
	configMode=false; 
	configHasRequests=false; 
	wifiInit();    
}

void reconfigDevice()
{    
	WiFi.setOutputPower(20);
	WiFi.setAutoConnect(false);
	WiFi.setAutoReconnect(true);
	WiFi.persistent(false);
	WiFi.disconnect();
	WiFi.softAPdisconnect();   
	Serial.printf("[%s]WiFi reconfigured.\n",timeToString().c_str());
}

void wifiInit()
{	
	Serial.printf("[%s]Connecting to [%s][%s] \n",timeToString().c_str(), ssid, password);
	WiFi.mode(WIFI_AP_STA);
	WiFi.begin(ssid, password);
	//WiFi.waitForConnectResult();	
}

void serverInit()
{
  server.reset(new ESP8266WebServer(80));

  server->on("/", htmlRoot);
  //server->on("/devicesList", htmlDevicesList);
  server->on("/forceOutputOff", forceOutputOff);
  server->on("/forceOutputOn", forceOutputOn);
  server->on("/timeSet0", timeSet0);  
  server->on("/timeSet1", timeSet1);
  server->on("/timeSet5", timeSet5);
  server->on("/timeSet10", timeSet10);
  server->on("/timeSet30", timeSet30);
  server->on("/timeSet60", timeSet60);
  server->on("/timeSet90", timeSet90);
  server->on("/timeSet120", timeSet120);
  server->on("/timeSet180", timeSet180);
  server->on("/timeSetInf", timeSetInf);
  server->on("/config", htmlPageConfig);
  server->on("/saveNewConfig", saveNewConfig);
  server->on("/resetDevice", resetDevice);  
  server->begin(); serverBegan=true; 
  Serial.printf("[%s]WiFi Web server began at http://%s/\n",timeToString().c_str(),deviceIP.toString().c_str());

  
  if ( MDNS.begin ( deviceName ) ) 
  {
    MDNS.addService("http", "tcp", 80);
    Serial.printf( "[%s]MDNS responder started at http://%s.local/ \n",timeToString().c_str(), deviceName);
  }
  
}

void udpInit()
{  
  Udp.reset(new WiFiUDP);
  Udp->begin(localUdpPort);
  udpBegan=true;
  Serial.printf("[%s]Now listening at IP %s, UDP port %d\n",timeToString().c_str(), deviceIP.toString().c_str(), localUdpPort);
  udpWriteStatus();
}

void configInit()
{
  reconfigDevice();
  WiFi.mode(WIFI_AP); 
  String ssidAP = String(SSID_AP) + deviceMAC;  
  WiFi.softAP(ssidAP.c_str(), PWD_AP);
  deviceIP = WiFi.softAPIP(); 
  Serial.printf("[%s]Access Point IP: %s\n",timeToString().c_str(),deviceIP.toString().c_str());
   
  serverInit();   

  Serial.printf("[%s]Now you can configure your device from AP \n",timeToString().c_str());
}

void mqttInit() 
{
    Serial.printf("[%s]Attempting MQTT connection...[%s][%s]\n",timeToString().c_str(),mqttUsername,mqttPwd);

	espClient.reset(new WiFiClient);
	client.reset(new PubSubClient(*espClient));
	client->setServer(mqttServer, mqttPort);
	client->setCallback(mqttCallback);

    if (client->connect("",mqttUsername,mqttPwd)) 
    {
		Serial.printf("[%s]MQTT connected, ready to publish status and suscribe to set outputs.\n",timeToString().c_str());
		client->subscribe(mqttOutputSet);
		client->subscribe(mqttTimerSet);
		client->subscribe(mqttAllSet);
		client->subscribe(mqttRefresh);
		mqttBegan=true;
		mqttReportAll();
    } 
    else 
    {
		Serial.printf("[%s]MQTT failed, rc=",timeToString().c_str());
		Serial.print(client->state());
		Serial.println(", try again.");
    }
 
    timeLoopMQTT=timeNow; 
   
}

/*************************** MISC *******************************/

String timeToString()
{
	String timeString="";	
	unsigned long timeTemp=millis();
	unsigned char days = 	timeTemp/86400000UL;
	timeTemp%=86400000UL;
	unsigned char hours = 	timeTemp/3600000UL;
	timeTemp%=3600000UL;
	unsigned char minutes = timeTemp/60000UL;
	timeTemp%=60000UL;
	unsigned char seconds =	timeTemp/1000UL;
	timeTemp%=1000UL;

	if(days>0)
		timeString += String(days) + "d ";

	if(hours<10)
		timeString += "0";
			
	timeString += String(hours) + ":";	

	if(minutes<10)
		timeString += "0";

	timeString += String(minutes) + ":";

	if(seconds<10)
		timeString += "0";

	timeString += String(seconds) + ":";

	if(timeTemp<10)
		timeString += "00";
	else if(timeTemp<100)
		timeString += "0";

	timeString += String(timeTemp);

	return timeString;
}

void handleOutputActiveLow(bool setTo)
{
    if(outputSet) //Active High
    {
      pinMode(OUTPUT_PIN, OUTPUT);
      digitalWrite(OUTPUT_PIN, setTo);      
    }
    else //Active low
    {
      if(setTo)
      {
        pinMode(OUTPUT_PIN, INPUT);  //High impedance?
      }
      else
      {
        pinMode(OUTPUT_PIN, OUTPUT); //Set LOW
        digitalWrite(OUTPUT_PIN, LOW);
      }      
    }
    outputState = setTo;
}

void forceOutput(bool state)
{
    handleOutputActiveLow(state?outputSet:!outputSet);     
    timeLastReset=timeNow;      
    Serial.printf("[%s]Output Forced %s \n",timeToString().c_str(), state? "ON":"OFF");      
    mqttReportOutput();    
}

void parseCommand(String msg)
{
    if(msg.indexOf("#ACK#")>4)
    {
      /* Feature disabled !!! Not scalable :(
        String newDeviceMAC= msg.substring(0,msg.indexOf("#ACK#"));
        String newDeviceIP = Udp->remoteIP().toString();
        String newDeviceName = msg.substring(msg.lastIndexOf(":")+1);
        String newDeviceSTATE = "";
        if(msg.indexOf(",IL,OL,")>0 || msg.indexOf(",IH,OL,")>0)
        {
          if(msg.indexOf(",IR,OL,")>0 || msg.indexOf(",IF,OL,")>0)
          {
            newDeviceSTATE="ON"; 
          }
          else if(msg.indexOf(",IR,OH,")>0 || msg.indexOf(",IF,OH,")>0)
          {
            newDeviceSTATE="OFF";
          }
        }
        else if(msg.indexOf(",IL,OH,")>0 || msg.indexOf(",IH,OH,")>0)
        {
          if(msg.indexOf(",IR,OL,")>0 || msg.indexOf(",IF,OL,")>0)
          {
            newDeviceSTATE="OFF"; 
          }
          else if(msg.indexOf(",IR,OH,")>0 || msg.indexOf(",IF,OH,")>0)
          {
            newDeviceSTATE="ON";
          }
        }                
        addDevice(newDeviceMAC,newDeviceIP,newDeviceName,newDeviceSTATE, timeNow);     
        */        
    }
    else if(msg.indexOf(":UPD:NOW")!=-1)
    {
          //TODO
    }
    else if(msg.indexOf(":SET:C1,")!=-1)
    {
      if(msg.indexOf("IR")>=0)
      {
          inputEdge=true;
      }
      else if(msg.indexOf("IF")>=0)
      {
          inputEdge=false;
      }
                  
      if(msg.indexOf("OH")>=0)
      {
          outputSet=HIGH;
      }
      else if(msg.indexOf("OL")>=0)
      {
          outputSet=LOW;
      }
  
      int timeToSet=-1;
      
      timeToSet = msg.substring(msg.lastIndexOf(",")+1).toInt();
      if(timeToSet>=0 && timeToSet<= 255)
      {
        if(timeToSet <=60)
        {
          timeSet=timeToSet;
        }
        else if(timeToSet < 255)
        {
          timeSet=(timeToSet-60)*60;
        }
        else if(timeToSet == 255)
        {
          timeSet=MAX_TIME_SET;  
        }
      } 
      Serial.printf("[%s]Reconfigure, when input      : %s \n",timeToString().c_str(), inputEdge?"Rising":"Falling");
      Serial.printf("             set output      : %s \n", outputSet?"High":"Low");
      Serial.printf("             time setted     : %d seconds\n",timeSet);
      saveConfig();                  
    }
    else if(msg.indexOf(":SET:ON")!=-1)
    {
       forceOutputOn();
    }
    else if(msg.indexOf(":SET:OFF")!=-1)
    {
       forceOutputOff();
    } 
    else if(msg.indexOf(":SET:RESET")!=-1)
    {
       ESP.reset();
    }
    else if(msg.indexOf(":SET:CLEAREPROM")!=-1)
    {
       clearEPROM();
    }
}

/*-------------------LOOPS--------------------------*/
void hardwareLoop()
{
    //Hardware LOOP
    timeNow=millis();
    if(timeNow-timeLoop>200) //Update states every 200ms
    {            
        inputStateNow = digitalRead(INPUT_PIN);
        if(!configMode) digitalWrite(LED_PIN,outputState);
        if(inputStateNow!=inputState)
        {
            if(inputStateNow==inputEdge && timeSet>0) //timeSet=0 -> Never change
            {
                forceOutput(true);                              
            }  
            //Report changes
            Serial.printf("[%s]Input edge detected: %s \n",timeToString().c_str(), inputStateNow? "R":"F"); 
            mqttReportInput(); 
            if(alarmTrigger) mqttReportTrigger();                 
            inputState = inputStateNow;
        }

        timeLoop=timeNow;

        //Auto-off timer
        timeCount=(timeNow-timeLastReset)/1000;
        if(timeCount>=timeSet && timeSet<MAX_TIME_SET) //timeSet==MAX -> Always set. 
        {
			if(outputState==outputSet  && timeSet>0 ) //timeSet=0 -> Never change
			{
				forceOutput(false);
			}
        }
    }   
}

void udpLoop()
{
      //UDP loop
      if(udpBegan)
      {                 
          if(timeNow-timeLoopUDP>DEVICES_REFRESH)
          { 
              udpWriteStatus();     
          }
          // Receive and process data
          int packetSize = Udp->parsePacket();
          if (packetSize)
          {
            // receive incoming UDP packets
            int len = Udp->read(incomingPacket, 90);
            if (len > 0)
            {
              incomingPacket[len] = 0;
            }
            // Only from MASTER
            if(Udp->remoteIP().toString()==String(masterIP))
            {     
              parseCommand(String(incomingPacket));                     
            }
            Udp->flush();
          }
      }
}

void mqttLoop()
{
    //MQTT loop
    if(mqttBegan)
    {
        if(client->connected())
        {
          client->loop();
        }
        else
        {
            //Reconnect, TODO: mqttInit makes down server & udp
            if(timeNow-timeLoopMQTT>MQTT_RECONNECT)
            {  
				Serial.printf("[%s]MQTT connection losed, attempting to reconnect.\n",timeToString().c_str());             
				tryWifi();    
				//mqttInit();
            }  
        }      
    } 
    else
    {
		tryWifi();    
		//mqttInit();
    }
}

void serverLoop()
{
	//SERVER LOOP
	if(serverBegan)
	{
		server->handleClient();
	}
}

void wifiLoop()
{
    // WiFi loop
    if(WiFi.status() == WL_CONNECTED)
    {
		if(firstConnection)
		{
			firstConnection = false;
			deviceIP= WiFi.localIP();			
			Serial.printf("[%s]WIFI CONNECTED, configuring device...\n",timeToString().c_str());  

			if(mqttServerEnable) mqttInit();
			if(httpServerEnable) serverInit();
			if(udpPortEnable) udpInit();        
		}       

		if(udpPortEnable) udpLoop();
		if(mqttServerEnable) mqttLoop();    
		timeLoopConnection=timeNow; //last connection          
    }
    else if(firstConnection && ((timeNow-timeBoot)/1000) > CONFIG_TIME_OUT && !configMode)
    {
		configMode=true;
		timeWifiResetCount=0;
		Serial.printf("[%s]WIFI connection FAILED -> Init configuration server on AP mode.\n",timeToString().c_str());
		configInit();      
    }  
    else if(firstConnection && !configMode)
    {
		if(timeNow-timeLoopConnection>1000)
		{
			timeWifiResetCount++;
			Serial.printf("[%s]Wifi connection timer: %d s\n",timeToString().c_str(),CONFIG_TIME_OUT-timeWifiResetCount);
			timeLoopConnection=timeNow;
			//wifiInit();
		}
    }  
    else if(configMode)
    {   
       
      if(timeNow-timeLoopConnection>1000 && !configHasRequests)
      {                    
		timeLoopConnection=timeNow; 
		timeWifiResetCount++;
		Serial.printf("[%s]Reset connection timer: %d s\n",timeToString().c_str(),RESET_TIME_OUT-timeWifiResetCount);
		digitalWrite(LED_PIN,!digitalRead(LED_PIN));
		if(timeWifiResetCount>=RESET_TIME_OUT )
		{
			Serial.printf("[%s]No reconfigured, attempting to reconnect WiFi.\n",timeToString().c_str());
			tryWifi();             
		}
      }
      
    }
    else
    {
		if(timeNow-timeLoopConnection>5000)
		{                    
			Serial.printf("[%s]Wifi connection losed, attempting to reconnect WiFi.\n",timeToString().c_str());
			tryWifi();
		}
    }         
}

/* -----------------------------ARDUINO STYLE-------------------------------- */
void setup()
{
	ESP.eraseConfig();
	reconfigDevice(); 

	hardwareInit(); 

	EEPROM.begin(1024);
	//clearEPROM();
	loadConfig();

	forceOutput(outputInit);

	Serial.printf("[%s]My MAC: ",timeToString().c_str());
	deviceMAC=String(WiFi.macAddress());
	Serial.println(deviceMAC); 

	deviceMAC.replace(":","");
	deviceMAC = deviceMAC.substring(6);	

	timeLoop=millis();
	timeLoopConnection=timeLoop;
	timeWifiResetCount=0;
	timeLastReset=timeLoop;
	timeBoot=timeLoop;
	timeLoopUDP=timeLoop;  
	timeLoopMQTT=timeLoop;  
  
	if(configMode) 
		configInit();
	else
		wifiInit();
}

void loop()
{		
    hardwareLoop();
	wifiLoop();
    if(httpServerEnable) serverLoop();         
}
