//---------------------------------------------------------------------------
//
// Name:        simpleBinaryWebEsp8266Ex.cpp
// Author:      Vita Tucek
// Created:     10.4.2017
// Modified:    
// License:     MIT
// Description: Web server for SimpleBinary
//
//---------------------------------------------------------------------------

#include "simpleBinaryWebEsp8266Ex.h"
#include <FS.h>

//construct html page 
String simpleBinaryWebEsp8266Ex::page()
{   
   String content = F("<html>");
   if(json!=NULL && json.length() > 0){
      content += F("<head><script type = 'text/javascript'>"
                     "function refresh() {"
                       "var req = new XMLHttpRequest();"
                       "req.onreadystatechange = function() { "
                           "if(req.readyState == 4 && req.status == 200){"
                              "console.log(req.responseText);"
                              "var resp = JSON.parse(req.responseText);"
                              "fillTableBody(resp);"
                           "}"
                       "}; "
                       "req.open('GET', 'data.json', true);"
                       "req.send(null);"                       
                     "} "
                     "function fillTableBody(data) {"
                       "if(data.length == 0)"
                           "return;"
                       "var table = document.getElementById('tb');"
                       "var i;"
                       "var tr;"
                       "var tc;"
                       "for (var key in data){"
                           "tr = table.insertRow(-1);"
                           "tc = tr.insertCell(-1);"
                           "tc.innerText = key + ':';"
                           "tc.style.textAlign = 'right';"
                           "tc.style.paddingRight = '20px';"
                           "tc.style.width = '20%';"
                           "tc = tr.insertCell(-1);"
                           "tc.innerText = data[key];"
                       "}"                       
                     "} "          
                     "function init(){"
                        "refresh();"
                     "}"         
		            "</script>"
	            "</head><body onload = 'init()'");
   }else{
      content += F("<body");
   }
   content += F(" bgcolor='#393939' text='white'><div style='text-align: center'><h2>SimpleBinary web control</h2><hr><br>"
                "<div style='display: inline-block; text-align: left'>"
                "<form action='/address'><table id='tb'><tbody>"
                "<tr><td style='text-align: right; width: 20%; padding-right: 20;'>Device address:<td><input type='number' name='ADDRESS' value='");
   content += address;
   content += F("'><td style='width: 10%;'><input type='submit' name='SAVE' value='Save'><td style='width: 30%;'/>");      
      
   if(saveResult==1){
      content += F("Address saved");
   }else if (saveResult==2){
      content += F("Address range is 0-255");
   }
   content += F("</tbody></table></form></div></div></body></html>");
   
   return content;
}

//---------------------------------------------------------------------------------------------------------
void simpleBinaryWebEsp8266Ex::getFileList(void) {
   if(!SPIFFS.begin())
   {
      return;
   }   
   Dir dir = SPIFFS.openDir("/");

   String output = "[";
   while(dir.next()){
      File entry = dir.openFile("r");
      if (output != "[") output += ',';
      bool isDir = false;
      output += "{\"type\":\"";
      output += (isDir)?"dir":"file";
      output += "\",\"name\":\"";
      output += String(entry.name()).substring(1);
      output += "\"}";
      entry.close();
   }
   
   output += "]";
   server.send(200, "application/json", output);
   
   SPIFFS.end();
}

String simpleBinaryWebEsp8266Ex::getContentType(String filename){
  if(server.hasArg("download")) return "application/octet-stream";
  else if(filename.endsWith(".htm")) return "text/html";
  else if(filename.endsWith(".html")) return "text/html";
  else if(filename.endsWith(".css")) return "text/css";
  else if(filename.endsWith(".js")) return "application/javascript";
  else if(filename.endsWith(".png")) return "image/png";
  else if(filename.endsWith(".bmp")) return "image/bmp";
  else if(filename.endsWith(".gif")) return "image/gif";
  else if(filename.endsWith(".jpg")) return "image/jpeg";
  else if(filename.endsWith(".ico")) return "image/x-icon";
  else if(filename.endsWith(".xml")) return "text/xml";
  else if(filename.endsWith(".pdf")) return "application/x-pdf";
  else if(filename.endsWith(".zip")) return "application/x-zip";
  else if(filename.endsWith(".gz")) return "application/x-gzip";
  return "text/plain";
}

//---------------------------------------------------------------------------------------------------------
void simpleBinaryWebEsp8266Ex::getFile(String path) {
  
   if(!SPIFFS.begin())
      return;
     
   Dir dir = SPIFFS.openDir("/");   
   
   if(SPIFFS.exists(path)){
      String contentType = getContentType(path);   
      File file = SPIFFS.open(path, "r");
      size_t sent = server.streamFile(file, contentType);
      file.close();
   }else{
      server.send(404, "text/plain", "FileNotFound");
   }
   
   SPIFFS.end();
}

//---------------------------------------------------------------------------------------------------------
void simpleBinaryWebEsp8266Ex::saveFile(String path, char *data, int length) {
   if(!SPIFFS.begin())
      return;
   
   Dir dir = SPIFFS.openDir("/");

   if(path == "/"){
      server.send(500, "text/plain", "BAD PATH");
      SPIFFS.end();
      return;
   }
   File file = SPIFFS.open(path, "w");
   if(file){
      for(int i=0;i<length;i++)
         file.write(data[i]);
      file.flush();
      file.close();
      
      //redirection
      server.sendHeader("Location", path, true);
      server.send(302, "text/plain", "");      
   }
   else
      server.send(500, "text/plain", "CREATE FAILED");      

   SPIFFS.end();      
}
 
//constructor
simpleBinaryWebEsp8266Ex::simpleBinaryWebEsp8266Ex(){
   saveResult = 0;
   //home page
   server.on("/", HTTP_GET,[&]() {    
      if(pageLoad!=NULL)
         pageLoad();     
            
      server.send(200, F("text/html"), page());      
      saveResult = 0;
   }); 
   //save new device address
   server.on("/address", HTTP_GET,[&]() { 
      int newAddress = server.arg("ADDRESS").toInt();  

      //address out-of-range
      if(newAddress < 0 || newAddress > 255){
         saveResult = 2;
      }else{   
         address = newAddress;
         saveResult = 1;
         //call address update event
         if(addressUpdate!=NULL)
            addressUpdate(address);
      }
      //redirection
      server.sendHeader("Location", String("/"), true);
      server.send(302, "text/plain", "");
   });  
   //return json file
   server.on("/data.json", HTTP_GET,[&]() {       
      server.send(200, F("application/json"), json);
   });   
   //return json file
   server.on("/filelist.json", HTTP_GET, [&]() {   
      getFileList();
   });
   //return printscreen
   server.on("/printscreen.bmp", HTTP_GET,[&]() {       
      getFile("/printscreen.bmp");
   });   
   //create and printscreen
   server.on("/printscreen", HTTP_GET,[&]() {
      if(onCreatePrintscreen==NULL){
         server.send(500, "text/plain", "PRINTSCREEN NOT IMPLEMENTED");
         return;
      }
      onCreatePrintscreen();
      getFile("/printscreen.bmp");
   });   
   //file not found
   server.onNotFound([&](){
      server.send(404, F("text/html"), F("<html><body><center><h2>404 FileNotFound</h2></center></body></html>"));
   });   
}

//web server initialization
void simpleBinaryWebEsp8266Ex::begin(int currentAddress, UpdateEventHandler onAddressUpdated, EventHandler onPageLoading){
   address = currentAddress;
   addressUpdate = onAddressUpdated;
   pageLoad = onPageLoading;
   server.begin();
}

//web server initialization
void simpleBinaryWebEsp8266Ex::begin(int currentAddress, UpdateEventHandler onAddressUpdated){
   begin(currentAddress,onAddressUpdated,NULL);
}

//web server handle procedure (should be called periodically)
void simpleBinaryWebEsp8266Ex::handleClient(void){
   server.handleClient();
}

//set actual device address to be available to display
void simpleBinaryWebEsp8266Ex::setAddress(int currentAddress){
   address = currentAddress;
}

//create json file from key/value pairs
String simpleBinaryWebEsp8266Ex::makeJson(std::pair<const char*, const char*> values[], int length){
   String text = "{";
   if(length==0){
      text += " }";
      return text;
   }
   
   for(int i=0;i<length;i++){
      text += "\"";
      text += values[i].first;
      text += "\":\"";
      text += values[i].second;
      text += "\"";
      if(i<length-1)
         text += ",";
   }   
   text += " }";
   return text;
}

