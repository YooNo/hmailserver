// Copyright (c) 2005 Martin Knafve / hMailServer.com.  
// http://www.hmailserver.com

#pragma once

#include "../Common/Threading/Task.h"

namespace HM
{
   class Message;
   class ClientInfo;
   
   class SMTPDeliveryManager : public Task
   {
   public:
	   SMTPDeliveryManager();
	   virtual ~SMTPDeliveryManager();

      void SetDeliverMessage();
      void DoWork();
   
      const String &GetQueueName() const;

      void UncachePendingMessages();

      void OnPropertyChanged(std::shared_ptr<Property> pProperty);

   private:
      
      void SendStatistics_(bool bIgnoreMessageCount = false);

      void LoadPendingMessageList_();
      std::shared_ptr<Message> GetNextMessage_();

      long cur_number_of_sent_;

      std::shared_ptr<DALRecordset> pending_messages_;
      
      const String queue_name_;

      bool uncache_pending_messages_;

      int queue_id_;

      Event deliver_messages_;
   };

}
