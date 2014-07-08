// Copyright (c) 2010 Martin Knafve / hMailServer.com.  
// http://www.hmailserver.com

#pragma once

#include "TCPConnection.h"

using boost::asio::ip::tcp;

namespace HM
{
   class AnsiStringConnection :
      public TCPConnection
   {
   public:
      AnsiStringConnection(bool useSSL,
                    boost::asio::io_service& io_service,    
                    boost::asio::ssl::context& context);
      ~AnsiStringConnection(void);

   public:

     void PostReceive();
     void PostBufferReceive();
     bool SendData(const AnsiString &sAnsiString);
     bool SendData(shared_ptr<ByteBuffer> pByteBuffer);
     virtual AnsiString GetCommandSeparator() const = 0;
   private:
      
      

   };

}