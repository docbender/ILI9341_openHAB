//---------------------------------------------------------------------------
//
// Name:        iliWeb.cpp
// Author:      Vita Tucek
// Created:     4.5.2017
// Modified:    
// License:     MIT
// Description: Web server for ILI panel
//
//---------------------------------------------------------------------------

#include "iliWeb.h"
#include <FS.h>

//---------------------------------------------------------------------------------------------------------
void iliWeb::getFileList(void) {
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

String iliWeb::getContentType(String filename){
  if(server.hasArg("download")) return "application/octet-stream";
  else if(filename.endsWith(".htm")) return "text/html";
  else if(filename.endsWith(".html")) return "text/html";
  else if(filename.endsWith(".css")) return "text/css";
  else if(filename.endsWith(".js")) return "application/javascript";
  else if(filename.endsWith(".png")) return "image/png";
  else if(filename.endsWith(".bmp")) return "application/octet-stream";
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
void iliWeb::getFile(String path) {
  
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
void iliWeb::saveFile(String path, char *data, int length) {
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
iliWeb::iliWeb(){
   simpleBinaryWebEsp8266();
}

//web server initialization
void iliWeb::begin(int currentAddress, UpdateEventHandler onAddressUpdated, EventHandler onPageLoading){
   //return printscreen
   server.on("/printscreen.bmp", HTTP_GET,[&]() {       
      getFile("/printscreen.bmp");
   });   
   //create a printscreen
   server.on("/printscreen", HTTP_GET,[&]() {
      if(onCreatePrintscreen==NULL){
         server.send(500, "text/plain", "PRINTSCREEN NOT IMPLEMENTED");
         return;
      }
      onCreatePrintscreen();
      getFile("/printscreen.bmp");
   });   
   
   simpleBinaryWebEsp8266::begin(currentAddress,onAddressUpdated,onPageLoading);
}


