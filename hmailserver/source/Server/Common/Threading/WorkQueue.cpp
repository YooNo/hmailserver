// Copyright (c) 2005 Martin Knafve / hMailServer.com.  
// http://www.hmailserver.com
// Created 2005-07-21

#include "StdAfx.h"

#include "WorkQueue.h"
#include "Task.h"



#ifdef _DEBUG
#define DEBUG_NEW new(_NORMAL_BLOCK, __FILE__, __LINE__)
#define new DEBUG_NEW

#endif

namespace HM
{


   WorkQueue::WorkQueue(unsigned int iMaxSimultaneous, const String &sQueueName) :
      queue_name_ (sQueueName),
      max_simultaneous_(0),
      work_( io_service_)
   {
      SetMaxSimultaneous(iMaxSimultaneous);

      LOG_DEBUG(Formatter::Format("Creating work queue {0}", queue_name_));
   }

   void 
   WorkQueue::SetMaxSimultaneous(int iMaxSimultaneous)
   {
      max_simultaneous_ = iMaxSimultaneous;

      // Hard code limit to 100. Everything over this won't be good for stability.
      if (max_simultaneous_ > 100)
         max_simultaneous_ = 100;
   }

   WorkQueue::~WorkQueue(void)
   {
     LOG_DEBUG(Formatter::Format("Destructing work queue {0}", queue_name_));
   }

   void 
   WorkQueue::AddTask(std::shared_ptr<Task> pTask)
   {
      LOG_DEBUG(Formatter::Format("Adding task {0} to work queue {1}", pTask->GetName(), queue_name_));

      // Post a wrapped task into the queue.
      std::function<void ()> func = std::bind(&WorkQueue::ExecuteTask, this, pTask);
      io_service_.post( func );
   }

   void 
   WorkQueue::ExecuteTask(std::shared_ptr<Task> pTask)
   {
      {
         boost::lock_guard<boost::recursive_mutex> guard(runningTasksMutex_);
         runningTasks_.insert(pTask);
      }

      LOG_DEBUG(Formatter::Format("Executing task {0} in work queue {1}", pTask->GetName(), queue_name_));

      try
      {
         pTask->DoWork();
      }
      catch (const boost::thread_interrupted &)
      {
          boost::this_thread::disable_interruption disabled;
          RemoveRunningTask_(pTask);
          return;
      }
      catch (boost::system::system_error error)
      {
         String exception_text = Formatter::Format("An error occured while executing task {0}", pTask->GetName());
         ErrorManager::Instance()->ReportError(ErrorManager::Medium, 5501, "WorkQueue::ExecuteTask", exception_text, error);
      }
      catch (std::exception error)
      {
         String exception_text = Formatter::Format("An error occured while executing task {0}", pTask->GetName());
         ErrorManager::Instance()->ReportError(ErrorManager::Medium, 5501, "WorkQueue::ExecuteTask", exception_text, error);
      }
      catch (...)
      {
         String exception_text = Formatter::Format("An error occured while executing task {0}", pTask->GetName());
         ErrorManager::Instance()->ReportError(ErrorManager::High, 5317, "WorkQueue::ExecuteTask", exception_text);
      }

      RemoveRunningTask_(pTask);
   }

   void
   WorkQueue::RemoveRunningTask_(std::shared_ptr<Task> task)
   {
      boost::lock_guard<boost::recursive_mutex> guard(runningTasksMutex_);
      std::set<std::shared_ptr<Task>>::iterator iter = runningTasks_.find(task);
      runningTasks_.erase(task);
   }

   void 
   WorkQueue::Start()
   {
      LOG_DEBUG(Formatter::Format("Starting work queue {0}", queue_name_));

      //io_service_.reset();

      for ( std::size_t i = 0; i < max_simultaneous_; ++i )
      {
         std::shared_ptr<boost::thread> thread = std::shared_ptr<boost::thread>
            (new boost::thread(std::bind( &WorkQueue::IoServiceRunWorker, this )));

         workerThreads_.insert(thread);
      }

      LOG_DEBUG(Formatter::Format("Started work queue {0}", queue_name_));
   }

   void 
   WorkQueue::IoServiceRunWorker()
   {
      LOG_DEBUG(Formatter::Format("Running worker in work queue {0}", queue_name_));
      io_service_.run();
      LOG_DEBUG(Formatter::Format("Worker exited in work queue {0}", queue_name_));
   }

   void 
   WorkQueue::Stop()
   {
      LOG_DEBUG(Formatter::Format("Stopping working queue {0}.", queue_name_));

      // Prevent new tasks from being started.
      io_service_.stop();

      LOG_DEBUG(Formatter::Format("Interupt and join threads in working queue {0}", queue_name_));

      std::set<std::shared_ptr<boost::thread>> completedThreads;

      int attemptCount = 10000 / 250; // 10 seconds, 250 ms between each

      for (int i = 0; i < attemptCount; i++)
      {
         for (std::shared_ptr<boost::thread> thread : workerThreads_)
         {
            thread->interrupt();
         }

         for(std::shared_ptr<boost::thread> thread : workerThreads_)
         {
            if (thread->timed_join(boost::posix_time::milliseconds(1)))
            {
               completedThreads.insert(thread);
            }
         }

         for(std::shared_ptr<boost::thread> thread : completedThreads)
         {
            std::set<std::shared_ptr<boost::thread>>::iterator iter = workerThreads_.find(thread);

            workerThreads_.erase(iter);
         }

         completedThreads.clear();

         if (workerThreads_.size() == 0)
         {
            LOG_DEBUG(Formatter::Format("All threads are joined in queue {0}.", queue_name_));
            return;
         }

         

         boost::lock_guard<boost::recursive_mutex> guard(runningTasksMutex_);
         std::set<std::shared_ptr<Task>>::iterator iter = runningTasks_.begin();
         if (iter != runningTasks_.end())
         {
            LOG_DEBUG(Formatter::Format("Still {0} remaining threads in queue {1}. First task: {2}", workerThreads_.size(), queue_name_, (*iter)->GetName()));
         }
         else
         {
            LOG_DEBUG(Formatter::Format("Still {0} remaining threads in queue {1}. First task: <Unknown>", workerThreads_.size(), queue_name_));

         }

         Sleep(250);
      }

      LOG_DEBUG(Formatter::Format("Given up waiting for threads to join in queue {0}.", queue_name_));
   }

   const String&
   WorkQueue::GetName() const
   //---------------------------------------------------------------------------
   // DESCRIPTION:
   // Returns the name of this queue.
   //---------------------------------------------------------------------------
   {
      return queue_name_;
   }
}
