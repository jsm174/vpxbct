#define PLOG_OMIT_LOG_DEFINES 
#define PLOG_NO_DBG_OUT_INSTANCE_ID 1

#include <plog/Log.h>
#include <plog/Init.h>
#include <plog/Formatters/TxtFormatter.h>
#include <plog/Appenders/RollingFileAppender.h>
#include <plog/Appenders/ColorConsoleAppender.h>

#include "VPXBatoceraCaptureTool.h"
#include <unistd.h>

int main(int argc, char* argv[]) {
   plog::init<PLOG_DEFAULT_INSTANCE_ID>();
   plog::Logger<PLOG_DEFAULT_INSTANCE_ID>::getInstance()->setMaxSeverity(plog::info);

   char path[1024];
   ssize_t count = readlink("/proc/self/exe", path, sizeof(path));
   string szPath;
   if (count != -1) {
       szPath = string(path, count);
       size_t lastSlash = szPath.find_last_of('/');
       if (lastSlash != string::npos)
           szPath = szPath.substr(0, lastSlash + 1);
   }

   string szLogPath = szPath + "vpxbct.log";

   static plog::RollingFileAppender<plog::TxtFormatter> fileAppender(szLogPath.c_str(), 1024 * 1024 * 5, 1);
   plog::Logger<PLOG_DEFAULT_INSTANCE_ID>::getInstance()->addAppender(&fileAppender);

   static plog::ColorConsoleAppender<plog::TxtFormatter> consoleAppender;
   plog::Logger<PLOG_DEFAULT_INSTANCE_ID>::getInstance()->addAppender(&consoleAppender);

   VPXBatoceraCaptureTool* pServer = new VPXBatoceraCaptureTool();
   pServer->SetBasePath(szPath);
 
   int res = pServer->Start();

   delete pServer;

   return res;
}