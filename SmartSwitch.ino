#include <ESP8266WiFi.h>
#include <WiFiClient.h>
#include <ESP8266WebServer.h>
#include <WiFiUdp.h>
#include <EEPROM.h>

/* Misc */
#define tbi(port, bit)   (port) & (1 << (bit)) 
#define sbi(port, bit)   (port) |= (1 << (bit))
#define cbi(port, bit)   (port) &= ~(1 << (bit))

/* Hardware */
#define INPUT_PIN         3   //RX
#define LED_PIN           2
//#define GPIO1           1   //TX
#define OUTPUT_PIN        0
#define MAX_TIME_SET      11700
#define CONFIG_TIME_OUT   120 //seconds
#define RESET_TIME_OUT    480 //seconds
#define SSID_AP           "ESP01_ID"
#define PWD_AP            "adminesp01"    

/* DATA FROM FLASH */
char ssid[33];
char password[33];
char deviceName[33];
char MASTER_IP[16];
unsigned char configByte;
unsigned char timeSetByte;

String deviceMAC="00:00:00:00:00:00";
String deviceState="S1,IX,OX,XX";
const String deviceType = "ESP01";
String deviceConfig="C1,IR,OL,41"; //On input RISING edge output is setted to LOW until 5 minutes
bool deviceNormalMode = true;

bool inputState = false;
bool inputEdge = true;  //When  True=Rising, False=Falling
bool outputSet= LOW;   
bool outputState = LOW;
unsigned int timeSet= 30; //Seconds,  MIN 0, MAX=11640
unsigned int timeCount=0;
unsigned long timeLastReset = 0; //Seconds
unsigned long timeLoop=0;
unsigned long timeLoopConnection=0;
unsigned long timeNow = 0; //Seconds
unsigned long timeBoot= 0;
bool firstConnection = true; //Try connection
bool serverBegan = false;
bool udpBegan=false;
bool configMode=false;

/* ---------------  WEB SERVER and UDP ------------------- */
WiFiUDP Udp;
unsigned int localUdpPort = 45454;  // local port to listen on
char incomingPacket[255];  // buffer for incoming packets

WiFiClient espClient;
ESP8266WebServer server(80);

const String htmlPage = "<!DOCTYPE html><html><head><title>SmartSwitch en DEVICE_NAME</title><meta http-equiv='refresh' content='2'; charset='UTF-8'><style>body{height:2000px;background:linear-gradient(141deg,#2cb5e8 0%,#1fc8db 51%, #0fb8ad  75%);}.buttonHome{color:#F0F8FF;background-color:black;border:none;border-radius:5px;text-decoration:none;text-align: center; ;outline:none;font-size:5vw;width:100%;transition-duration:0.1s;} .buttonHome:active {background-color:darkgray;transform:translateY(2px);}.buttonON {display: inline-block;color:white;background-color:blue;border:none;border-radius:10px;text-decoration:none;outline: none;font-size:6vw;width:40%;padding: 25px 15px;;margin:0% 0% 5% 5%;transition-duration:0.1s;box-shadow: 0 8px 16px 0 rgba(0,0,0,0.2), 0 6px 20px 0 rgba(0,0,0,0.19);}.buttonOff {display: inline-block;color:white;background-color:red;border:none;border-radius:10px;text-decoration:none;outline: none;font-size:6vw;width:40%;padding: 25px 15px;;margin:0% 0% 5% 5%;transition-duration:0.1s;box-shadow: 0 8px 16px 0 rgba(0,0,0,0.2), 0 6px 20px 0 rgba(0,0,0,0.19);}.buttonON:active {background-color:darkblue;transform:translateY(4px);}.buttonOff:active {background-color:darkred;transform:translateY(4px);}.textState {color:#696969;font-size:5vw;text-align:center;margin: 0 0;transition-duration:0.4s;}.textTime {color:#F8F8FF;font-size:3vw;text-align:center;margin: 0 0;transition-duration:0.4s;}.tittles {color:#2F4F4F;font-size:8vw;text-align:center;}.buttonTime {color:#F0F8FF;background-color:green;border:none;border-radius:10px;text-decoration:none;outline:none;font-size:4vw;width:10%; margin: 0px 0px 0px 0px;transition-duration:0.1s;padding: 10px 0px;}.buttonTime:active {background-color:darkgreen;transform:translateY(2px);}</style> </head><body><a href='http://MASTER_IP'><button class='buttonHome'>Regresar al Men&uacute;</button></a><h1 class='tittles'> DEVICE_NAME </h1><a href='forceOutputOn'> <button class='buttonON'>&#x2714;Encender</button></a><a href='forceOutputOff'><button class='buttonOff'>&#x2718;Apagar</button></a><p class='textState'> SmartSwitch <b> OUTPUT_STATE </b></p><p class='textTime'> TIME_OFF </p> <h1 class='tittles'> Configurar  </h1><p class='textTime'> Seleccione cuantos minutos se mantendr&aacute; encendido: </p> <br><a href='timeSet0'><button class='buttonTime'>&empty;</button></a><a href='timeSetHalf'><button class='buttonTime'>&#189;</button></a>  <a href='timeSet1'><button class='buttonTime'>1 </button></a><a href='timeSet5'><button class='buttonTime'> 5 </button></a> <a href='timeSet10'><button class='buttonTime'>10 </button></a><a href='timeSet30'><button class='buttonTime'>30</button></a><a href='timeSet60'><button class='buttonTime'>60</button></a><a href='timeSet90'><button class='buttonTime'>90</button></a> <a href='timeSetInf'><button class='buttonTime'>&infin; </button></a><br><p class='textTime'>TIME_SET</p></body></html>";
const String htmlConfig = "<!DOCTYPE html><html><head><title>SmartSwitch en DEVICE_NAME</title><meta charset='UTF-8'><style>body{height:2000px;background:linear-gradient(141deg,#2cb5e8 0%,#1fc8db 51%, #0fb8ad  75%);}input[type=submit],select{display: inline-block;color:white;background-color:blue;border:none;border-radius:10px;text-decoration:none;outline: none;font-size:6vw;width:90%;padding: 25px 15px;;margin:0% 0% 5% 5%;transition-duration:0.1s;box-shadow: 0 8px 16px 0 rgba(0,0,0,0.2), 0 6px 20px 0 rgba(0,0,0,0.19);}.buttonOff {display: inline-block;color:white;background-color:red;border:none;border-radius:10px;text-decoration:none;outline: none;font-size:6vw;width:90%;padding: 25px 15px;;margin:0% 0% 5% 5%;transition-duration:0.1s;box-shadow: 0 8px 16px 0 rgba(0,0,0,0.2), 0 6px 20px 0 rgba(0,0,0,0.19);}input[type=submit]:active {background-color:darkblue;transform:translateY(4px);}.buttonOff:active {background-color:darkred;transform:translateY(4px);}.textState {color:#696969;font-size:3vw;text-align:center;margin: 0 0;transition-duration:0.4s;}.textTime {color:#F8F8FF;font-size:5vw;text-align:left;margin: 0 0;transition-duration:0.4s;}.tittles {color:#2F4F4F;font-size:8vw;text-align:center;}input[type=password],select {color:#F0F8FF;background-color:black;font-size:5vw;border:none;padding: 10px 10px;border-radius:15px;width:95%;transition-duration:0.4s;}input[type=password]:focus{background-color:#2F4F4F;}input[type=text],select {color:#F0F8FF;background-color:black;font-size:5vw;border:none;padding: 10px 10px;border-radius:15px;width:95%;transition-duration:0.4s;}input[type=text]:focus{background-color:#2F4F4F;}</style> </head><body><h1 class='tittles'> Wellcome to SmartSwitch </h1><p class='textState'>&#9940; WiFi connection failed &#9940; <br> You can change the device information here or reset device to try again. </p><p class='textState'>&#x23F0; DEVICE WILL AUTO RESET IN FEW MINUTES &#x23F0;</p><br><form action='saveNewConfig' method='GET'><p class='textTime'> Enter valid SSID: </p><input type='text' name='ssid' value='DEVICE_SSID' maxlength='32'><p class='textTime'> Enter password: </p><input type='password' name='pwd' value='DEVICE_PWD' maxlength='32'><p class='textTime'> Enter device name: </p><input type='text' name='name' value='DEVICE_NAME' maxlength='32'><p class='textTime'> Enter master IP: </p><input type='text' name='master' value='MASTER_IP' maxlength='15'>    <br><br> <input type='submit' value='&#x2714; Submit'>    </form> <a href='resetDevice'><button class='buttonOff'>&#8635;Reset</button></a><p class='textState'>Designed in Colombia, Juan64Bits</p></body></html>";
const String htmlMsg = "<!DOCTYPE html><html><head><title>SmartSwitch en DEVICE_NAME</title><meta charset='UTF-8'><style>body{height:2000px;background:linear-gradient(141deg,#2cb5e8 0%,#1fc8db 51%, #0fb8ad  75%);}.textState {color:#696969;font-size:3vw;text-align:center;margin: 0 0;transition-duration:0.4s;}.tittles {color:#2F4F4F;font-size:8vw;text-align:center;}</style>  </head><body><h1 class='tittles'> &#9888; Warning Message &#9888;</h1><p class='textState'> MESSAGE </p></body></html>";

void redirectToRoot() 
{
  server.sendHeader("Location", String("/"), true);
  server.send ( 302, "text/plain", "");
}

void htmlRoot() 
{
  String rootPage=htmlPage;
  rootPage.replace("DEVICE_NAME",deviceName);
  rootPage.replace("MASTER_IP",MASTER_IP);  

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
    else if(timeSet == MAX_TIME_SET)   
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
    else if(timeSet == MAX_TIME_SET)
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
    
  server.send(200, "text/html", rootPage); 
}

void htmlPageConfig() 
{
  String rootPage=htmlConfig;
  rootPage.replace("DEVICE_SSID",ssid);
  rootPage.replace("DEVICE_PWD",password);
  rootPage.replace("DEVICE_NAME",deviceName);
  rootPage.replace("MASTER_IP",MASTER_IP); 
  server.send(200, "text/html", rootPage);
}

void htmlPageMsg(String msg) 
{
  String rootPage=htmlMsg; 
  rootPage.replace("MESSAGE",msg);
  server.send(200, "text/html", rootPage);
}  

/* ---------------  WEB SERVER ------------------- */
void hardwareInit()
{  
  Serial.begin(115200,SERIAL_8N1,SERIAL_TX_ONLY);
  Serial.println();

  pinMode(INPUT_PIN,INPUT);
  digitalWrite(INPUT_PIN,HIGH); //PULL UP
  inputState = digitalRead(INPUT_PIN);
  pinMode(OUTPUT_PIN,OUTPUT);
  handleOutputActiveLow(outputSet);
  pinMode(LED_PIN,OUTPUT);
  digitalWrite(LED_PIN,HIGH);     
}

void resetDevice()
{
  htmlPageMsg("The device is now restarting. If the saved SSID and PASSWORD are correct you will have access on your WiFi network.");
  ESP.reset();  
}

void clearEPROM()
{
  for(int i=0; i<117;i++)  
    EEPROM.write(116,0);
  EEPROM.commit();  
}

void saveNewConfig()
{
  server.arg("ssid").toCharArray(ssid,33);
  server.arg("pwd").toCharArray(password,33);
  server.arg("name").toCharArray(deviceName,33);
  server.arg("master").toCharArray(MASTER_IP,16);    
  saveConfig();
  htmlPageMsg("Data saved correctly, the device is now restarting. If the SSID and PASSWORD are correct you will have access on your WiFi network.");
  ESP.reset(); 
}

void saveConfig()
{   
  for(int i=0; i<33;i++)
  {
      EEPROM.write(i, ssid[i]);
      EEPROM.write(i+33, password[i]);
      EEPROM.write(i+66, deviceName[i]);      
      if(i<16)
        EEPROM.write(i+99, MASTER_IP[i]);  
  }
  EEPROM.write(115,configByte);
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
  EEPROM.write(116,timeSetByte);
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
        MASTER_IP[i]=EEPROM.read(i+99);  
  }
  configByte=EEPROM.read(115);
  timeSetByte=EEPROM.read(116);  

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
}

void configInit()
{
  WiFi.mode(WIFI_AP); 
  String ssidAP = String(SSID_AP) + deviceMAC;
  ssidAP.replace(':','_');
  WiFi.softAP(ssidAP.c_str(), PWD_AP);
  IPAddress myIP = WiFi.softAPIP(); 
  Serial.print("Access Point IP: ");
  Serial.println(myIP);
  
  server.close();
  server.on("/", htmlPageConfig);
  server.on("/saveNewConfig", saveNewConfig);
  server.on("/resetDevice", resetDevice);
  server.begin(); serverBegan=true;
  
  Serial.println("Now you can configure your device.");
}

void wifiInit()
{
  Serial.printf("Connecting to %s ", ssid);
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);   
}

void serverInit()
{
  server.on("/", htmlRoot);
  server.on("/forceOutputOff", forceOutputOff);
  server.on("/forceOutputOn", forceOutputOn);
  server.on("/timeSet0", timeSet0);
  server.on("/timeSetHalf", timeSetHalf);
  server.on("/timeSet1", timeSet1);
  server.on("/timeSet5", timeSet5);
  server.on("/timeSet10", timeSet10);
  server.on("/timeSet30", timeSet30);
  server.on("/timeSet60", timeSet60);
  server.on("/timeSet90", timeSet90);
  server.on("/timeSetInf", timeSetInf);
  server.begin(); serverBegan=true; 
}

void udpInit()
{
  Udp.begin(localUdpPort);
  
  Serial.printf("Now listening at IP %s, UDP port %d\n", WiFi.localIP().toString().c_str(), localUdpPort);
}

void setup()
{
  hardwareInit(); 
  
  EEPROM.begin(128);
  loadConfig();
  
  Serial.print("My MAC: ");
  deviceMAC=String(WiFi.macAddress());
  Serial.println(deviceMAC); 

  timeLoop=millis();
  timeLoopConnection=timeLoop;
  timeLastReset=timeLoop;
  timeBoot=timeLoop;
  ESP.eraseConfig();  
  WiFi.disconnect();
  WiFi.softAPdisconnect();
  WiFi.setOutputPower(20.5);
  WiFi.persistent(false);
  
  wifiInit();  
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
    
Serial.printf("Time status:%d seconds\n",timeStatus);
    
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
    
    Udp.beginPacket(MASTER_IP, localUdpPort);
    Udp.write(replyString.c_str());
    Udp.endPacket();  
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
}

void timeSet0(){ timeSet=0;redirectToRoot();saveConfig();}
void timeSetHalf(){ timeSet=30;redirectToRoot();saveConfig();}
void timeSet1(){ timeSet=60;redirectToRoot();saveConfig();}
void timeSet5(){ timeSet=60*5;redirectToRoot();saveConfig();}
void timeSet10(){ timeSet=60*10;redirectToRoot();saveConfig();}
void timeSet30(){ timeSet=60*30;redirectToRoot();saveConfig();}
void timeSet60(){ timeSet=60*60;redirectToRoot();saveConfig();}
void timeSet90(){ timeSet=60*90;redirectToRoot();saveConfig();}
void timeSetInf(){ timeSet=MAX_TIME_SET;redirectToRoot();saveConfig();}

void forceOutputOn()
{
    handleOutputActiveLow( outputSet); 
    outputState = outputSet;
    timeLastReset=millis();      
    Serial.print("Output Forced ON \n");
    //Report    
    redirectToRoot();
    udpWriteStatus();
}

void forceOutputOff()
{
    handleOutputActiveLow( !outputSet);
    outputState=!outputSet;    
    Serial.print("Output Forced OFF \n");
    //Report    
    redirectToRoot();
    udpWriteStatus();
}

void loop()
{
  
    timeNow=millis();
    if(timeNow-timeLoop>150) //Update states every 100ms
    {            
        bool inputStateNow = digitalRead(INPUT_PIN);
        digitalWrite(LED_PIN,!inputStateNow);
        if(inputStateNow!=inputState)
        {
            if(inputStateNow==inputEdge && timeSet>0) //timeSet=0 -> Never change
            {
                handleOutputActiveLow( outputSet); 
                outputState = outputSet;
                timeLastReset=timeNow;                              
            }  
            //Report changes   
            if(udpBegan){udpWriteStatus();}                  
        }
        inputState = inputStateNow;
        timeLoop=timeNow;
    }
    timeCount=(timeNow-timeLastReset)/1000;
    if(timeCount>=timeSet && timeSet<MAX_TIME_SET) //timeSet==MAX -> Always set. 
    {
      if(outputState==outputSet  && timeSet>0 ) //timeSet=0 -> Never change
      {
        handleOutputActiveLow( !outputSet);
        outputState=!outputSet;
      }
    }

    //WEB SERVER HANDLER
    if(serverBegan) {server.handleClient();}
      
    if(WiFi.status() == WL_CONNECTED)
    {
      if(firstConnection)
      {
        firstConnection = false;
        Serial.printf(" CONNECTED -> Init server and udp\n");  
        serverInit();
        udpInit();  
      }   
      // Receive and process data
      int packetSize = Udp.parsePacket();
      if (packetSize)
      {
        // receive incoming UDP packets
          Serial.printf("Received %d bytes from %s, port %d\n", packetSize, MASTER_IP, Udp.remotePort());
          int len = Udp.read(incomingPacket, 255);
          if (len > 0)
          {
            incomingPacket[len] = 0;
          }
          String msg = String(incomingPacket);
          Serial.printf("UDP packet contents: %s\n", msg.c_str());
    
        // Only from MASTER
        if(Udp.remoteIP().toString()==String(MASTER_IP))
        {     
          Serial.printf("Index of command:%d\n",msg.indexOf(":SET:"));
          
          if(msg.indexOf(":UPD:NOW")==8)
          {
              udpWriteStatus();
          }
          else if(msg.indexOf(":SET:C1,")==8)
          {
            Serial.printf("Index of input R:%d\n",msg.indexOf("IR"));
            Serial.printf("Index of input F:%d\n",msg.indexOf("IF"));
          
            if(msg.indexOf("IR")>=0)
            {
                inputEdge=true;
            }
            else if(msg.indexOf("IF")>=0)
            {
                inputEdge=false;
            }
            
            Serial.printf("Index of output H:%d\n",msg.indexOf("OH"));
            Serial.printf("Index of output L:%d\n",msg.indexOf("OL"));
            
            if(msg.indexOf("OH")>=0)
            {
                outputSet=HIGH;
            }
            else if(msg.indexOf("OL")>=0)
            {
                outputSet=LOW;
            }
    
            int timeToSet=-1;
            
    Serial.printf("Index of last , :%d\n",msg.lastIndexOf(","));
    
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
            Serial.printf("Reconfigure, when input      : %s \n", inputEdge?"Rising":"Falling");
            Serial.printf("             set output      : %s \n", outputSet?"High":"Low");
            Serial.printf("             time setted     : %d seconds\n",timeSet);
            saveConfig();                  
          }
          else if(msg.indexOf(":SET:FORCEON")==8)
          {
             forceOutputOn();
          }
          else if(msg.indexOf(":SET:FORCEOFF")==8)
          {
             forceOutputOff();
          } 
          else if(msg.indexOf(":SET:RESETDEVICE")==8)
          {
             ESP.reset();
          }
          else if(msg.indexOf(":SET:CLEAREPROM")==8)
          {
             clearEPROM();
          }            
        }
      }
    }
    else if(firstConnection && ((timeNow-timeBoot)/1000) > CONFIG_TIME_OUT && !configMode)
    {
      configMode=true;
      Serial.printf(" NOT CONNECTED -> Init config mode\n");
      configInit();      
    }  
    else if(firstConnection && !configMode)
    {
      if(timeNow-timeLoopConnection>1000)
      {
          Serial.printf(".");
          timeLoopConnection=timeNow;
      }
    }  
    else if(configMode)
    {
      if((timeNow-timeLoopConnection)/1000>RESET_TIME_OUT) 
      {
        Serial.printf("RESET DEVICE NOW\n");
        ESP.reset();
      } 
    }   
}
