#include <Wire.h> // optional 
#include <LiquidCrystal_I2C.h> // optional
#include <Dhcp.h>
#include <Dns.h>
#include <Ethernet.h>
#include <EthernetClient.h>
#include <EthernetServer.h>
#include <EthernetUdp.h>
#include <SPI.h>
#include <Ethernet.h>
#include <SD.h>
#include <ArduinoJson.h>

// json models
// http://objgen.com/json/models/9rCO
// https://github.com/bblanchon/ArduinoJson

// size of buffer used to capture HTTP requests
#define REQ_BUF_SZ 90
#define JSN_BUF_SZ 600
char HTTP_req[REQ_BUF_SZ] = {0}; // buffered HTTP request stored as null terminated string
char req_index = 0;              // index into HTTP_req buffer

// MAC address
byte mac[] = { 0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED };
// IPAddress ip(192, 168, 0, 20); // IP address, may need to change depending on network
EthernetServer server(80);  // create a server at port 80
File webFile;

bool debugMode = true;
struct confStruct {
  double vsn;
  String usr;
  String pwd;
  String ipconf;
  IPAddress ipstatic;
  IPAddress ipdynamic;
  String wutype;
  bool stndbyaftstrtup;
  int wuint;
  String devname0;
  IPAddress devip0;
  String devname1;
  IPAddress devip1;
  String devname2;
  IPAddress devip2;
  String devname3;
  IPAddress devip3;
  String devname4;
  IPAddress devip4;
};
confStruct config;

// LCD
// @DEVELOPMENT DEVICE
LiquidCrystal_I2C lcd(0x27, 20, 4);     // set the LCD address to 0x27 for a 20 chars and 4 line display

void setup() {
    // initialize LCD
    lcd.init();                           
    // Print welcome message to the LCD
    printDisplay("MKR-WoL", "Version 1.0", "powered by", "MKR-Solutions");
    lcd.backlight();
    delay(500);
    // initialize SD
    if(debugMode) {
      Serial.begin(250000);       // for debugging
      // initialize SD card
      Serial.println("Initializing SD card...");
      printDisplay("Initializing", "SD card...", "", ""); 
    }
    if(!SD.begin(4)) {
        if(debugMode) {
          Serial.println("ERROR - SD card initialization failed!");
          printDisplay("ERROR", "SD card", "initialization", "failed!");
        } 
        return;    // init failed
    }
    if(debugMode) {
      Serial.println("SUCCESS - SD card initialized.");
      printDisplay("SUCCESS", "SD card", "initialized!", ""); 
    }
    // check for index.htm file
    if (!SD.exists("index.htm")) {
      if(debugMode) 
        Serial.println("ERROR - Can't find index.htm file!");
      return;  // can't find index file
    }
    Serial.println("SUCCESS - Found index.htm file.");

    if(debugMode) {
      Serial.println("Initializing ethernet...");
      printDisplay("Initializing", "ethernet...", "", ""); 
    }
    Ethernet.begin(mac);  // initialize Ethernet device
    if(debugMode) {
      Serial.print("server is at ");
      config.ipdynamic = Ethernet.localIP();
      Serial.println(config.ipdynamic);
    }
    printDisplay("MKR-WoL", "IP of Webserver is:", ipToString(config.ipdynamic), "Have fun!"); 
    
    loadConfig(false);
    // change config to save the dynamic ip
    changeConfig();
    
    server.begin();           // start to listen for clients
}

void loop() {
    EthernetClient client = server.available();  // try to get client
    if(client) {  // got client?
        boolean currentLineIsBlank = true;
        int cntSpace = 0;
        while (client.connected()) {
            if (client.available()) {   // client data available to read
                char c = client.read(); // read 1 byte (character) from client
                // buffer first part of HTTP request in HTTP_req array (string)
                // leave last element in array as 0 to null terminate string (REQ_BUF_SZ - 1)
                if(req_index < (REQ_BUF_SZ - 1)) {
                  if(c == ' ')
                     cntSpace++;
                  if(cntSpace == 1) {
                    HTTP_req[req_index] = c; // save HTTP request character
                    req_index++;
                  }
                }
                // print HTTP request character to serial monitor
                //Serial.print(c);
                // last line of client request is blank and ends with \n
                // respond to client only after last line received
                if (c == '\n' && currentLineIsBlank) {
                    char *leader = HTTP_req;
                    char *follower = leader;
                    // While we're not at the end of the string (current character not NULL)
                    while (*leader) {
                        // Check to see if the current character is a %
                        if (*leader == '%') {
                            // Grab the next two characters and move leader forwards
                            leader++;
                            char high = *leader;
                            leader++;
                            char low = *leader;
                            // Convert ASCII 0-9A-F to a value 0-15
                            if (high > 0x39) high -= 7;
                            high &= 0x0f;
                            // Same again for the low byte:
                            if (low > 0x39) low -= 7;
                            low &= 0x0f;
                            // Combine the two into a single byte and store in follower:
                            *follower = (high << 4) | low;
                        } else {
                            // All other characters copy verbatim
                            *follower = *leader;
                        }
                        // Move both pointers to the next character:
                        leader++;
                        follower++;
                    }
                    // Terminate the new string with a NULL character to trim it off
                    *follower = 0;
                    if(debugMode) {
                      Serial.println("-----------------");
                      Serial.println(HTTP_req);
                      Serial.println("-----------------");
                    }
                    // open requested web page file
                    String fileurl = HTTP_req;
                    String params = "";
                    String key = "";
                    String data = "";
                    String u = "";
                    String p = "";
                    bool loggedIn = false;
                    // get clean url
                    fileurl.replace(" /", "");
                    //fileurl = fileurl.substring(0, fileurl.indexOf(' '));
                    if(fileurl == "")
                      fileurl = "index.htm";
                    // get url params
                    if(sizeOfSeparatedString(fileurl, '?') > 1) {
                      params = getValue(fileurl, '?', 1);
                      fileurl = getValue(fileurl, '?', 0);
                      for(int i = 0; i < sizeOfSeparatedString(params, '&'); i++){
                        String paramName = getValue(getValue(params, '&', i), '=', 0);
                        String paramValue = getValue(getValue(params, '&', i), '=', 1);
                        if(paramName == "u")
                          u = paramValue;
                        else if(paramName == "p")
                          p = paramValue;
                        else if(paramName == "k")
                          key = paramValue;
                        else if(paramName == "d")
                          data = paramValue;
                        if(debugMode)
                          Serial.println(paramName + ": " + paramValue);
                        paramName = "";
                        paramValue = "";
                      }
                    }
                    // check if logged in
                    if(u == config.usr && p == config.pwd)
                      loggedIn = true;
                    if(fileurl.indexOf("content.cnt") != -1 || fileurl.indexOf("cnt.cnt") != -1 || fileurl.indexOf("login.cnt") != -1) {
                      if(loggedIn)
                        fileurl = "cnt.cnt";
                      else
                        fileurl = "login.cnt";
                    } else if(fileurl.indexOf("navi.nav") != -1 || fileurl.indexOf("log.nav") != -1 || fileurl.indexOf("nolog.nav") != -1) {
                      if(loggedIn)
                        fileurl = "log.nav";
                      else
                        fileurl = "nolog.nav";
                    } else if(fileurl.indexOf("conf.jsn") != -1 || fileurl.indexOf("cnfori.jsn") != -1) {
                      if(loggedIn)
                        fileurl = "conf.jsn";
                      else
                        fileurl = "404.htm";
                    } else if(fileurl.indexOf("save.jsn") != -1 || fileurl.indexOf("buf.tmp") != -1) {
                      if(loggedIn) {
                        // validate changed settings
                        // filter and validate user input (urlencoding,...)
                        
                        if(key == "usr0") {
                          
                          config.usr = data;
                        } else if(key == "pwd0") {
                          config.pwd = data;
                        } else if(key == "ipstatic") {
                          config.ipstatic[0] = getValue(data, '.', 0).toInt();
                          config.ipstatic[1] = getValue(data, '.', 1).toInt();
                          config.ipstatic[2] = getValue(data, '.', 2).toInt();
                          config.ipstatic[3] = getValue(data, '.', 3).toInt();
                        }
//                        config.ipconf = root["ipconf"].as<String>();
//                        // config.ipdynamic = ipAddress;
//                        config.wutype = root["wutype"].as<String>();
//                        config.stndbyaftstrtup = root["stndbyaftstrtup"].as<bool>();
//                        config.wuint = root["wuint"].as<int>();
                        else if(key == "devname0") {
                          config.devname0 = data;
                        } else if(key == "devip0") {
                          config.devip0[0] = getValue(data, '.', 0).toInt();
                          config.devip0[1] = getValue(data, '.', 1).toInt();
                          config.devip0[2] = getValue(data, '.', 2).toInt();
                          config.devip0[3] = getValue(data, '.', 3).toInt();
                        }


//                        config.devip1[0] = getValue(root["devip1"].as<String>(), '.', 0).toInt();
//                        config.devip1[1] = getValue(root["devip1"].as<String>(), '.', 1).toInt();
//                        config.devip1[2] = getValue(root["devip1"].as<String>(), '.', 2).toInt();
//                        config.devip1[3] = getValue(root["devip1"].as<String>(), '.', 3).toInt();
//                        config.devname2 = root["devname2"].as<String>();
//                        config.devip2[0] = getValue(root["devip2"].as<String>(), '.', 0).toInt();
//                        config.devip2[1] = getValue(root["devip2"].as<String>(), '.', 1).toInt();
//                        config.devip2[2] = getValue(root["devip2"].as<String>(), '.', 2).toInt();
//                        config.devip2[3] = getValue(root["devip2"].as<String>(), '.', 3).toInt();
//                        config.devname3 = root["devname3"].as<String>();
//                        config.devip3[0] = getValue(root["devip3"].as<String>(), '.', 0).toInt();
//                        config.devip3[1] = getValue(root["devip3"].as<String>(), '.', 1).toInt();
//                        config.devip3[2] = getValue(root["devip3"].as<String>(), '.', 2).toInt();
//                        config.devip3[3] = getValue(root["devip3"].as<String>(), '.', 3).toInt();
//                        config.devname4 = root["devname4"].as<String>();
//                        config.devip4[0] = getValue(root["devip4"].as<String>(), '.', 0).toInt();
//                        config.devip4[1] = getValue(root["devip4"].as<String>(), '.', 1).toInt();
//                        config.devip4[2] = getValue(root["devip4"].as<String>(), '.', 2).toInt();
//                        config.devip4[3] = getValue(root["devip4"].as<String>(), '.', 3).toInt();

                        
                        changeConfig();
                        // save json
                        fileurl = "buf.tmp";
                      } else
                        fileurl = "404.htm";
                    } else if(fileurl.indexOf("reset") != -1) {
                        loadConfig(true);
                        changeConfig();
                    }
                    webFile = SD.open(fileurl);
                    if(!webFile) {
                      fileurl = "404.htm";
                      webFile = SD.open(fileurl);
                    } 
                    if(debugMode) {
                      Serial.println("Loading file: " + fileurl);
                      printDisplay("Loading file:", fileurl, "USER: " + u, "");
                    }
                    // send response to client
                    client.println("HTTP/1.1 200 OK");
                    if(fileurl.indexOf(".txt") != -1 || fileurl.indexOf(".htm") != -1) {
                        client.println("Content-Type: text/html");  
                    } else if(fileurl.indexOf(".css") != -1) {
                        client.println("Content-Type: text/css");  
                    } else if(fileurl.indexOf(".jsn") != -1) {
                        client.println("Content-Type: application/json");  
                    }
                    client.println("Connnection: close");
                    client.println();
                    while(webFile.available()) {
                      client.write(webFile.read()); // send web page to client
                    }
                    webFile.close();
                    // reset buffer index and all buffer elements to 0
                    req_index = 0;
                    strClear(HTTP_req, REQ_BUF_SZ);
                    // Serial.println("Sent to client: " + fileurl);
                    break;
                }
                // every line of text received from the client ends with \r\n
                if (c == '\n') {
                    // last character on line of received text
                    // starting new line with next character read
                    currentLineIsBlank = true;
                } 
                else if (c != '\r') {
                    // a text character was received from client
                    currentLineIsBlank = false;
                }
            } // end if (client.available())
        } // end while (client.connected())
        //delay(1);      // give the web browser time to receive the data
        client.stop(); // close the connection
    } // end if (client)
}

void loadConfig(bool reset) {
  StaticJsonBuffer<JSN_BUF_SZ> jsonBuffer;
  char jsonBuf[JSN_BUF_SZ];
  // read config
  int reqIndex = 0;
  if(!reset)
    webFile = SD.open("conf.jsn");
  else if(reset)
    webFile = SD.open("cnfori.jsn");
  while(webFile.available()) {
    jsonBuf[reqIndex] = webFile.read();
    reqIndex++;
  }
  webFile.close();
    
  JsonObject& root = jsonBuffer.parseObject(jsonBuf);
  if(!root.success()) {
    if(debugMode) {
      Serial.println("parseObject() failed");
      printDisplay("ERROR", "parseObject()", "failed", "");
    }
    return;
  }

  if(debugMode) {
    root.prettyPrintTo(Serial);
    Serial.println("--");
  }
  config.vsn = root["vsn"].as<double>();
  config.usr = root["usr0"].as<String>();
  config.pwd = root["pwd0"].as<String>();
  config.ipconf = root["ipconf"].as<String>();
  config.ipstatic[0] = getValue(root["ipstatic"].as<String>(), '.', 0).toInt();
  config.ipstatic[1] = getValue(root["ipstatic"].as<String>(), '.', 1).toInt();
  config.ipstatic[2] = getValue(root["ipstatic"].as<String>(), '.', 2).toInt();
  config.ipstatic[3] = getValue(root["ipstatic"].as<String>(), '.', 3).toInt();
  // config.ipdynamic = ipAddress;
  config.wutype = root["wutype"].as<String>();
  config.stndbyaftstrtup = root["stndbyaftstrtup"].as<bool>();
  config.wuint = root["wuint"].as<int>();
  config.devname0 = root["devname0"].as<String>();
  config.devip0[0] = getValue(root["devip0"].as<String>(), '.', 0).toInt();
  config.devip0[1] = getValue(root["devip0"].as<String>(), '.', 1).toInt();
  config.devip0[2] = getValue(root["devip0"].as<String>(), '.', 2).toInt();
  config.devip0[3] = getValue(root["devip0"].as<String>(), '.', 3).toInt();
  config.devname1 = root["devname1"].as<String>();
  config.devip1[0] = getValue(root["devip1"].as<String>(), '.', 0).toInt();
  config.devip1[1] = getValue(root["devip1"].as<String>(), '.', 1).toInt();
  config.devip1[2] = getValue(root["devip1"].as<String>(), '.', 2).toInt();
  config.devip1[3] = getValue(root["devip1"].as<String>(), '.', 3).toInt();
  config.devname2 = root["devname2"].as<String>();
  config.devip2[0] = getValue(root["devip2"].as<String>(), '.', 0).toInt();
  config.devip2[1] = getValue(root["devip2"].as<String>(), '.', 1).toInt();
  config.devip2[2] = getValue(root["devip2"].as<String>(), '.', 2).toInt();
  config.devip2[3] = getValue(root["devip2"].as<String>(), '.', 3).toInt();
  config.devname3 = root["devname3"].as<String>();
  config.devip3[0] = getValue(root["devip3"].as<String>(), '.', 0).toInt();
  config.devip3[1] = getValue(root["devip3"].as<String>(), '.', 1).toInt();
  config.devip3[2] = getValue(root["devip3"].as<String>(), '.', 2).toInt();
  config.devip3[3] = getValue(root["devip3"].as<String>(), '.', 3).toInt();
  config.devname4 = root["devname4"].as<String>();
  config.devip4[0] = getValue(root["devip4"].as<String>(), '.', 0).toInt();
  config.devip4[1] = getValue(root["devip4"].as<String>(), '.', 1).toInt();
  config.devip4[2] = getValue(root["devip4"].as<String>(), '.', 2).toInt();
  config.devip4[3] = getValue(root["devip4"].as<String>(), '.', 3).toInt();
}

void changeConfig() {
  StaticJsonBuffer<JSN_BUF_SZ> jsonBuffer;
  char jsonBuf[JSN_BUF_SZ];

  JsonObject& root = jsonBuffer.createObject();
  root["vsn"] = config.vsn;
  root["usr0"] = config.usr;
  root["pwd0"] = config.pwd;
  root["ipconf"] = config.ipconf;
  root["ipstatic"] = ipToString(config.ipstatic);
  root["ipdynamic"] =  ipToString(config.ipdynamic);
  root["wutype"] = config.wutype;
  root["stndbyaftstrtup"] = config.stndbyaftstrtup;
  root["wuint"] = config.wuint;
  root["devname0"] = config.devname0;
  root["devip0"] = ipToString(config.devip0);
  root["devname1"] = config.devname1;
  root["devip1"] = ipToString(config.devip1);
  root["devname2"] = config.devname2;
  root["devip2"] = ipToString(config.devip2);
  root["devname3"] = config.devname3;
  root["devip3"] = ipToString(config.devip3);
  root["devname4"] = config.devname4;
  root["devip4"] = ipToString(config.devip4);

  if(debugMode) {
    root.prettyPrintTo(Serial);
    Serial.println("--");
  }
  
  // save config
  SD.remove("conf.jsn");
  webFile = SD.open("conf.jsn", FILE_WRITE);
  // if the file is available, write to it:
  if(webFile) {
    root.printTo(webFile);
    webFile.close();
    writeTmp("ok", "Konfiguration wurde gespeichert"); 
  } else {
     writeTmp("error", "Fehler beim Speichern der Konfiguration"); 
  }
}

void writeTmp(const char *type, const char *msg) {
  char tmp[60] = "{\"r\": \"";
  strcat(tmp, type);
  strcat(tmp, "\", \"m\": \"");
  strcat(tmp, msg);
  strcat(tmp, "\"}");
  
  // save tmp buffer
  SD.remove("buf.tmp");
  webFile = SD.open("buf.tmp", FILE_WRITE);
  // if the file is available, write to it:
  if(webFile) {
    webFile.println(tmp);
    webFile.close();
  }
}

// sets every element of str to 0 (clears array)
void strClear(char *str, char length) {
    for (int i = 0; i < length; i++) {
        str[i] = 0;
    }
}

// get value of separated string
String getValue(String data, char separator, int index) {
  int found = 0;
  int strIndex[] = {0, -1};
  int maxIndex = data.length()-1;
  for(int i=0; i<=maxIndex && found<=index; i++){
    if(data.charAt(i)==separator || i==maxIndex){
        found++;
        strIndex[0] = strIndex[1]+1;
        strIndex[1] = (i == maxIndex) ? i+1 : i;
    }
  }
  return found>index ? data.substring(strIndex[0], strIndex[1]) : "";
}

// get size of separated string
int sizeOfSeparatedString(String data, char separator) {
  int found = 0;
  int maxIndex = data.length()-1;
  for(int i=0; i<=maxIndex; i++){
    if(data.charAt(i)==separator || i==maxIndex){
        found++;
    }
  }
  return found;
}

String ipToString(IPAddress ipAddress) {
  return String(ipAddress[0]) + "." + String(ipAddress[1]) + "." + String(ipAddress[2]) + "." + String(ipAddress[3]);
}

void printDisplay(String line1, String line2, String line3, String line4) {
  lcd.clear();
  lcd.setCursor(0,0);
  lcd.print(line1);
  lcd.setCursor(0,1);
  lcd.print(line2);
  lcd.setCursor(0,2);
  lcd.print(line3);
  lcd.setCursor(0,3);
  lcd.print(line4);
}
