#include <time.h>
#include <msgapi.h>
#include <fidoconfig.h>

void purgeArea(s_area area, HAREA a) {
   int msgCount, i, highest;
   HMSG msg;
   XMSG xmsg;
   struct tm date;
   time_t currentTime, msgTime;
   char rc = 0;

   highest = MsgGetHighMsg(a);
   msgCount = MsgGetNumMsg(a);
   for (i = 0; i < highest; i++) {
      msg = MsgOpenMsg(a, MOPEN_RW, i);
      if (msg==NULL) continue;
      // first test if there are more messages in the area and then kill the msg
      if ((area.max != 0) && (msgCount > area.max)) {
         MsgKillMsg(a, i);
         msgCount--;
      // if there are not too much msgs test if msg is too old
      } else {
         if (area.purge != 0) {
            MsgReadMsg(msg, &xmsg, 0,0,NULL, 0, NULL);
            MsgCloseMsg(msg);
            date.tm_mday = xmsg.date_written.date.da;
            date.tm_mon  = xmsg.date_written.date.mo - 1;
            date.tm_year = xmsg.date_written.date.yr + 80;
            date.tm_hour = xmsg.date_written.time.hh;
            date.tm_min  = xmsg.date_written.time.mm;
            date.tm_sec  = xmsg.date_written.time.ss << 1;
            
            msgTime = mktime(&date);
            currentTime = time(NULL);
            // difference between the to times is greater then the number of seconds of the area.purge days, kill it
            if (abs(currentTime - msgTime) > area.purge * 24 * 60 * 60)  {
//               printf("Trying to delete msg #%d\n", i);
               rc = MsgKillMsg(a, i);
            }
            if (rc) printf("Could not delete msg# %d\n", i);
            rc = 0;
         }
      }
      
   }
}

void openArea(s_area area) {
   int i;
   HAREA a;
   
   printf("Purging Area %s.\n", area.areaName);
   a = MsgOpenArea(area.fileName, MSGAREA_NORMAL, area.msgbType | MSGTYPE_ECHO);
   if (a!=NULL) {
      purgeArea(area, a);
      MsgCloseArea(a);
   } else printf("Error opening %s.\n", area.areaName);
}

int main() {

   s_fidoconfig *cfg;
   int i;
   struct _minf m;
   
   printf("sqpack v0.9\n");

   cfg = readConfig();

   if (cfg != NULL ) {
      m.req_version = 0;
      m.def_zone = cfg->addr[0].zone;
      if (MsgOpenApi(&m)!= 0) {
         printf("MsgOpenApi Error.\n");
         exit(1);
      }
      
      for (i=0; i < cfg->echoAreaCount; i++) {
         if ((cfg->echoAreas[i].msgbType & MSGTYPE_PASSTHROUGH) != MSGTYPE_PASSTHROUGH)
            openArea(cfg->echoAreas[i]);
      }
      disposeConfig(cfg);
   } else printf("Could not read fido config\n");

}
