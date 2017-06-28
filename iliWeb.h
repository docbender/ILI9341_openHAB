//---------------------------------------------------------------------------
//
// Name:        iliWeb.h
// Author:      Vita Tucek
// Created:     4.5.2017
// Modified:    
// License:     MIT
// Description: Web server for ILI panel
//
//---------------------------------------------------------------------------

#ifndef _ILI_WEB_H_
#define _ILI_WEB_H_

#include "simpleBinaryWebEsp8266.h"

class iliWeb: public simpleBinaryWebEsp8266
{
   public:  
      //constructor
      iliWeb(); 
      //web server initialization
      void begin(int currentAddress, UpdateEventHandler onAddressUpdated, EventHandler onPageLoading);      
      
      EventHandler onCreatePrintscreen;

   private:     
      void getFileList(void);
      String getContentType(String filename);
      void getFile(String path);
      void saveFile(String path, char *data, int length);      
};

#endif