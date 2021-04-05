//Command List
//host: Set the name of mDNS ( 40 or less)
//ssid: Set the SSID ( 40 or less)
//pass: Set the password ( 40 or less)
//mode: 0:STAmode / ( 1:APmode in preparation )
//scan: Get SSID List
//reset Reset
//elase Erase EEPROM data

#include <WiFi.h>
#include <WiFiClient.h>
#include <WebServer.h>
#include <ESPmDNS.h>
#include <Update.h>
#include <EEPROM.h>

const char* host = "";
const char* ssid = "";
const char* pass = "";
const char* cmode = "";

volatile bool WiFiAPmode = false;

WebServer server(80);

/*
   Login page
*/

const char* loginIndex =
  "<form name='loginForm'>"
  "<table width='20%' bgcolor='A09F9F' align='center'>"
  "<tr>"
  "<td colspan=2>"
  "<center><font size=4><b>ESP32 Login Page</b></font></center>"
  "<br>"
  "</td>"
  "<br>"
  "<br>"
  "</tr>"
  "<td>Username:</td>"
  "<td><input type='text' size=25 name='userid'><br></td>"
  "</tr>"
  "<br>"
  "<br>"
  "<tr>"
  "<td>Password:</td>"
  "<td><input type='Password' size=25 name='pwd'><br></td>"
  "<br>"
  "<br>"
  "</tr>"
  "<tr>"
  "<td><input type='submit' onclick='check(this.form)' value='Login'></td>"
  "</tr>"
  "</table>"
  "</form>"
  "<script>"
  "function check(form)"
  "{"
  "if(form.userid.value=='admin' && form.pwd.value=='admin')"
  "{"
  "window.open('/serverIndex')"
  "}"
  "else"
  "{"
  " alert('Error Password or Username')/*displays error message*/"
  "}"
  "}"
  "</script>";

/*
   Server Index Page
*/

const char* serverIndex =
  "<script src='https://ajax.googleapis.com/ajax/libs/jquery/3.2.1/jquery.min.js'></script>"
  "<form method='POST' action='#' enctype='multipart/form-data' id='upload_form'>"
  "<input type='file' name='update'>"
  "<input type='submit' value='Update'>"
  "</form>"
  "<div id='prg'>progress: 0%</div>"
  "<script>"
  "$('form').submit(function(e){"
  "e.preventDefault();"
  "var form = $('#upload_form')[0];"
  "var data = new FormData(form);"
  " $.ajax({"
  "url: '/update',"
  "type: 'POST',"
  "data: data,"
  "contentType: false,"
  "processData:false,"
  "xhr: function() {"
  "var xhr = new window.XMLHttpRequest();"
  "xhr.upload.addEventListener('progress', function(evt) {"
  "if (evt.lengthComputable) {"
  "var per = evt.loaded / evt.total;"
  "$('#prg').html('progress: ' + Math.round(per*100) + '%');"
  "}"
  "}, false);"
  "return xhr;"
  "},"
  "success:function(d, s) {"
  "console.log('success!')"
  "},"
  "error: function (a, b, c) {"
  "}"
  "});"
  "});"
  "</script>";


/*
   setup function
*/
void setup(void) {
  EEPROM.begin(1024);
  Serial.begin(115200);

  String rom_ssid = EEPROM.readString(0);
  String rom_pass = EEPROM.readString(40);
  String rom_host = EEPROM.readString(80);
  WiFiAPmode = EEPROM.read(120);

  rom_ssid.trim();
  rom_pass.trim();
  rom_host.trim();

  String str_host = rom_host;
  String str_ssid = rom_ssid;
  String str_pass = rom_pass;

  //length of str for char
  int data_host_len = str_host.length() + 1;
  int data_ssid_len = str_ssid.length() + 1;
  int data_pass_len = str_pass.length() + 1;

  //for str->char array
  ssid[data_host_len];
  ssid[data_ssid_len];
  pass[data_pass_len];

  //str->char
  host = str_host.c_str();
  ssid = str_ssid.c_str();
  pass = str_pass.c_str();

  Serial.print("HOST:");
  Serial.println(host);
  Serial.print("SSID:");
  Serial.println(ssid);
  Serial.print("PASS:");
  Serial.println(pass);
  Serial.print("APMode:");
  Serial.println(WiFiAPmode);

  // Connect to WiFi network
  if (WiFiAPmode) {
    WiFi.mode(WIFI_AP);
    WiFi.softAP(ssid, pass);
  } else {
    WiFi.mode(WIFI_STA);
    WiFi.begin(ssid, pass);

    Serial.println("");


    // Wait for connection
    while (WiFi.status() != WL_CONNECTED) {
      if (Serial.available() > 0) {
        setNetWifi();
      }
      delay(500);
      Serial.print(".");
    }
  }
  Serial.println("");
  Serial.print("Connected to ");
  Serial.println(ssid);
  Serial.print("IP address: ");
  if (WiFiAPmode) {
    Serial.println(WiFi.softAPIP());
  } else {
    Serial.println(WiFi.localIP());
  }

  /*use mdns for host name resolution*/
  if (!MDNS.begin(host)) { //http://esp32.local
    Serial.println("Error setting up MDNS responder!");
    while (1) {
      if (Serial.available() > 0) {
        setNetWifi();
      }
    }
  }
  Serial.print("HOST: http://");
  Serial.print(host);
  Serial.println(".local/");
  Serial.println("mDNS responder started");
  /*return index page which is stored in serverIndex */
  server.on("/", HTTP_GET, []() {
    server.sendHeader("Connection", "close");
    server.send(200, "text/html", loginIndex);
  });
  server.on("/serverIndex", HTTP_GET, []() {
    server.sendHeader("Connection", "close");
    server.send(200, "text/html", serverIndex);
  });
  /*handling uploading firmware file */
  server.on("/update", HTTP_POST, []() {
    server.sendHeader("Connection", "close");
    server.send(200, "text/plain", (Update.hasError()) ? "FAIL" : "OK");
    ESP.restart();
  }, []() {
    HTTPUpload& upload = server.upload();
    if (upload.status == UPLOAD_FILE_START) {
      Serial.printf("Update: %s\n", upload.filename.c_str());
      if (!Update.begin(UPDATE_SIZE_UNKNOWN)) { //start with max available size
        Update.printError(Serial);
      }
    } else if (upload.status == UPLOAD_FILE_WRITE) {
      /* flashing firmware to ESP*/
      if (Update.write(upload.buf, upload.currentSize) != upload.currentSize) {
        Update.printError(Serial);
      }
    } else if (upload.status == UPLOAD_FILE_END) {
      if (Update.end(true)) { //true to set the size to the current progress
        Serial.printf("Update Success: %u\nRebooting...\n", upload.totalSize);
      } else {
        Update.printError(Serial);
      }
    }
  });
  server.begin();
}

//End Setup

void loop(void) {
  server.handleClient();
  delay(1);
  if (Serial.available() > 0) {
    setNetWifi();
  }
}

void setNetWifi(void) {
  String str = Serial.readStringUntil('\n');
  char c[40];
  if (str.startsWith("host:")) {
    if (str.length() <= 40) {
      str = str.substring(5);
      Serial.print("HOST:");
      Serial.println(str);
      str.toCharArray(c, str.length());
      EEPROM.writeString(80, c);
      EEPROM.commit();
      Serial.println(EEPROM.readString(80));
    } else {
      Serial.println("Over Strings. (40 or less)");
    }
  }
  if (str.startsWith("ssid:")) {
    if (str.length() <= 40) {
      str = str.substring(5);
      Serial.print("SSID:");
      Serial.println(str);
      str.toCharArray(c, str.length());
      EEPROM.writeString(0, c);
      EEPROM.commit();
      Serial.println(EEPROM.readString(0));
    } else {
      Serial.println("Over Strings. (40 or less)");
    }
  }
  if (str.startsWith("pass:")) {
    if (str.length() <= 40) {
      str = str.substring(5);
      Serial.print("PASS:");
      Serial.println(str);
      str.toCharArray(c, str.length());
      EEPROM.writeString(40, c);
      EEPROM.commit();
      Serial.println(EEPROM.readString(40));
    } else {
      Serial.println("Over Strings. (40 or less)");
    }
  }
  if (str.startsWith("read:") > 0) {
    Serial.print("read(HOST):");
    Serial.println(EEPROM.readString(80));
    Serial.print("read(SSID):");
    Serial.println(EEPROM.readString(0));
    Serial.print("read(PASS):");
    Serial.println(EEPROM.readString(40));
    Serial.print("read(WiFiAPmode):");
    Serial.println(EEPROM.read(120));
  }
  if (str.startsWith("mode:") > 0) {
    str = str.charAt(5);
    if (str == "0") {
      WiFiAPmode = false;
    } else {
      WiFiAPmode = true;
    }
    Serial.print("MODE:");
    Serial.println(WiFiAPmode);
    EEPROM.write(120, WiFiAPmode);
    EEPROM.commit();
    Serial.println(EEPROM.read(120));
  }


  if (str.startsWith("scan:") > 0) {
    int n = WiFi.scanNetworks();
    Serial.println("scan done");
    if (n == 0) {
      Serial.println("no networks found");
    } else {
      Serial.print(n);
      Serial.println(" networks found");
      for (int i = 0; i < n; ++i) {
        // Print SSID and RSSI for each network found
        Serial.print(i + 1);
        Serial.print(": ");
        Serial.print(WiFi.SSID(i));
        Serial.print(" (");
        Serial.print(WiFi.RSSI(i));
        Serial.print(")");
        Serial.println((WiFi.encryptionType(i) == WIFI_AUTH_OPEN) ? " " : "*");
        delay(10);
      }
    }
  }

  if (str.startsWith("reset") > 0) {
    ESP.restart();
  }

  if (str.startsWith("elase") > 0) {
    const char* ssid = "";
    const char* pass = "";
    const char* host = "";
    for (int i = 0 ; i < 120 ; i++) {
      EEPROM.write(i, 0);
    }
    EEPROM.commit();
    Serial.println("elase done");
  }

  str.clear();
}
