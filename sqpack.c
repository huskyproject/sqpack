#include <time.h>
#include <msgapi.h>
#include <fidoconfig.h>

void processMsg(dword msgNum, HAREA oldArea, HAREA newArea, s_area area )
{
   HMSG msg, newMsg;
   XMSG xmsg;
   static int msgProcessed = 0;
   struct tm tmTime;
   time_t ttime, actualTime = time(NULL);
   char *text, *ctrlText;
   dword  textLen, ctrlLen;

   msg = MsgOpenMsg(oldArea, MOPEN_RW, msgNum);
   if (msg == NULL) return;

   if ((area.max == 0) || (MsgGetNumMsg(oldArea)-msgProcessed) <= area.max) { //only max msgs should be in new area
      MsgReadMsg(msg, &xmsg, 0, 0, NULL, 0, NULL);
      DosDate_to_TmDate(&(xmsg.date_arrived), &tmTime);
      ttime = mktime(&tmTime);

      if ((area.purge == 0) || (abs(actualTime - ttime) <= ( area.purge * 24 *60 * 60))) {
         // copy msg
         textLen = MsgGetTextLen(msg);
         ctrlLen = MsgGetCtrlLen(msg);

         text = (char *) malloc(textLen+1);
         ctrlText = (char *) malloc(ctrlLen+1);

         MsgReadMsg(msg, NULL, 0, textLen, text, ctrlLen, ctrlText);
         newMsg = MsgOpenMsg(newArea, MOPEN_CREATE, 0);
         MsgWriteMsg(newMsg, 0, &xmsg, text, textLen, textLen, ctrlLen, ctrlText);
         MsgCloseMsg(newMsg);

         free(text);
         free(ctrlText);
      }
      
      MsgCloseMsg(msg);
   }

   msgProcessed++;
}

void renameArea(char *oldName, char *newName)
{
   char *oldTmp, *newTmp;
   
   oldTmp = (char *) malloc(strlen(oldName)+4+1);
   newTmp = (char *) malloc(strlen(newName)+4+1);

   strcpy(oldTmp, oldName);
   strcpy(newTmp, newName);
   strcat(oldTmp, ".sqd");
   strcat(newTmp, ".sqd");
   remove(oldTmp);
   rename(newTmp, oldTmp);

   oldTmp[strlen(oldTmp)-1] = 'l';
   remove(oldTmp);

   oldTmp[strlen(oldTmp)-1] = 'i';
   newTmp[strlen(newTmp)-1] = 'i';
   remove(oldTmp);
   rename(newTmp, oldTmp);

   
   free(oldTmp);
   free(newTmp);
}

void purgeArea(s_area area)
{
   char *oldName = area.fileName;
   char *newName;
   HAREA oldArea, newArea;
   dword highMsg, i;

   //generated tmp-FileName
   newName = (char *) malloc(strlen(oldName)+4+1);
   strcpy(newName, oldName);
   strcat(newName, "_tmp");

   oldArea = MsgOpenArea((byte *) oldName, MSGAREA_NORMAL, MSGTYPE_SQUISH);
   newArea = MsgOpenArea((byte *) newName, MSGAREA_CREATE, MSGTYPE_SQUISH);

   if ((oldArea != NULL) && (newArea != NULL)) {

      highMsg = MsgGetHighMsg(oldArea);

      for (i = 1; i <= highMsg; i++) {
         
         processMsg(i, oldArea, newArea, area);
      }

      MsgCloseArea(oldArea);
      MsgCloseArea(newArea);

      //rename oldArea to newArea
      renameArea(oldName, newName);
   }
   else {
      printf("Could not open %s or create %s.\n", oldName, newName);
   }
}

int main() {

   s_fidoconfig *cfg;
   int i;
   struct _minf m;
   
   printf("sqpack v0.9.9\n");

   cfg = readConfig();

   if (cfg != NULL ) {
      m.req_version = 0;
      m.def_zone = cfg->addr[0].zone;
      if (MsgOpenApi(&m)!= 0) {
         printf("MsgOpenApi Error.\n");
         exit(1);
      }
      
      for (i=0; i < cfg->echoAreaCount; i++) {
         // purge areas
         if ((cfg->echoAreas[i].msgbType & MSGTYPE_SQUISH) == MSGTYPE_SQUISH) {
            printf("%s\n", cfg->echoAreas[i].areaName);
            purgeArea(cfg->echoAreas[i]);
         }
      }
      disposeConfig(cfg);
      return 0;
   } else {
      printf("Could not read fido config\n");
      return 1;
   }

   
}
