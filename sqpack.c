#include <stdio.h>
#include <errno.h>
#include <time.h>

#ifdef UNIX
#include <unistd.h>
#else
#include <io.h>
#endif

#include <fcntl.h>
#ifdef __EMX__
#include <share.h>
#include <sys/types.h>
#endif
#include <sys/stat.h>

#include <msgapi.h>
#include <fidoconfig.h>
#include <common.h>

#if defined ( __WATCOMC__ )
#include <string.h>
#include <stdlib.h>
#include <prog.h>
#include <share.h>
#endif

unsigned long msgCopied, msgProcessed; // per Area

void readLastreadFile(char *fileName, UINT32 **lastreadp, ULONG *lcountp,
		      HAREA area)
{
   int fd;
   struct stat st;
   unsigned long i, temp;
   unsigned char buffer[4];
   char *name;

   name = (char *) malloc(strlen(fileName)+4+1);
   strcpy(name, fileName);
   strcat(name, ".sql");
   
   fd = sopen(name, O_BINARY | O_RDWR, SH_DENYNO, S_IWRITE | S_IREAD);
   if (fd != -1) {
      
      fstat(fd, &st);
      *lcountp = st.st_size / sizeof(UINT32);
      *lastreadp = (UINT32 *) malloc(*lcountp * sizeof(UINT32));
      
      for (i = 0; i < *lcountp; i++) {
         read(fd, &buffer, 4);
         temp = buffer[0] + (((unsigned long)(buffer[1])) << 8) +
                (((unsigned long)(buffer[2])) << 16) +
                (((unsigned long)(buffer[3])) << 24);
         (*lastreadp)[i] = MsgUidToMsgn(area, temp, UID_NEXT);
      }

      close(fd);
      
   } else *lastreadp = NULL;

   free(name);
}

void writeLastreadFile(char *fileName, UINT32 *lastread, ULONG lcount,
			HAREA area)
{
   char *name;
   unsigned char buffer[4];
   int fd;
   unsigned long i, temp;
   
   
   if (lastread) {

      name = (char *) malloc(strlen(fileName)+4+1);
      strcpy(name, fileName);
      strcat(name, ".sql");

      fd = sopen(name, O_BINARY | O_RDWR, SH_DENYNO, S_IWRITE | S_IREAD);

      if (fd != -1) {

         lseek(fd, 0l, SEEK_SET);

         for (i = 0; i < lcount; i++) {

            temp = MsgMsgnToUid(area, lastread[i]);
            
            buffer[0] = temp & 0xFF;
            buffer[1] = (temp >> 8) & 0xFF;
            buffer[2] = (temp >> 16) & 0xFF;
            buffer[3] = (temp >> 24) & 0xFF;

            write(fd, &buffer, 4);
         }

         close(fd);
              
      } else printf("Could not write lastread file %s, error %u!\n", name, errno);
      
      free(name);
   }
}

unsigned long getOffsetInLastread(UINT32 *lastread, ULONG lcount, dword msgnum)
{

   unsigned long i;

   for (i = 0; i < lcount; i++) {
      if (lastread[i] == msgnum) return i;
   }

   return (-1);
   
}

void processMsg(dword msgNum, dword numMsg, HAREA oldArea, HAREA newArea,
		s_area area, UINT32 *oldLastread, ULONG lcount,
		UINT32 *newLastread)
{
   HMSG msg, newMsg;
   XMSG xmsg;
   struct tm tmTime;
   time_t ttime, actualTime = time(NULL);
   char *text, *ctrlText;
   dword  textLen, ctrlLen, oldMsgnum, newMsgnum;
   
   unsigned long offset;

   msg = MsgOpenMsg(oldArea, MOPEN_RW, msgNum);
   if (msg == NULL) return;

   if ((area.max == 0) || ((numMsg - msgProcessed) <= area.max)) { //only max msgs should be in new area
//      printf("%u: %u - %u = %u\n", area.max, numMsg, msgCopied, numMsg - msgCopied);
      MsgReadMsg(msg, &xmsg, 0, 0, NULL, 0, NULL);
      DosDate_to_TmDate(&(xmsg.date_arrived), &tmTime);
      ttime = mktime(&tmTime);

      if ((area.purge == 0) || (abs(actualTime - ttime) <= ( area.purge * 24 *60 * 60))) {
         // copy msg
         textLen = MsgGetTextLen(msg);
         ctrlLen = MsgGetCtrlLen(msg);

         text = (char *) malloc(textLen+1);
         ctrlText = (char *) malloc(ctrlLen+1);

         MsgReadMsg(msg, NULL, 0, textLen, (byte*)text, ctrlLen, (byte*)ctrlText);
         newMsg = MsgOpenMsg(newArea, MOPEN_CREATE, 0);
         MsgWriteMsg(newMsg, 0, &xmsg, (byte*)text, textLen, textLen, ctrlLen, (byte*)ctrlText);
         MsgCloseMsg(newMsg);

         msgCopied++;

         if (oldLastread) {

            oldMsgnum = msgNum;
            newMsgnum = msgCopied;
            
            if ((offset = getOffsetInLastread(oldLastread, lcount, oldMsgnum)) != (-1)) {
               // printf("%u\n", newMsgnum);
               newLastread[offset] = newMsgnum;
            }
         }

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
   dword highMsg, i, numMsg;

   UINT32 *oldLastread, *newLastread = NULL;

   if (!area.max && !area.purge) {
      printf("   No purging needed!\n");
      return;
   }

   //generated tmp-FileName
   newName = (char *) malloc(strlen(oldName)+4+1);
   strcpy(newName, oldName);
   strcat(newName, "_tmp");

   oldArea = MsgOpenArea((byte *) oldName, MSGAREA_NORMAL, MSGTYPE_SQUISH);
   newArea = MsgOpenArea((byte *) newName, MSGAREA_CREATE, MSGTYPE_SQUISH);

   if ((oldArea != NULL) && (newArea != NULL)) {
      ULONG lcount;

      highMsg = MsgGetHighMsg(oldArea);
      numMsg = MsgGetNumMsg(oldArea);
      readLastreadFile(oldName, &oldLastread, &lcount, oldArea);
      if (oldLastread) {
         newLastread = (UINT32 *) malloc(lcount * sizeof(UINT32));
         memcpy(newLastread, oldLastread, lcount * sizeof(UINT32));
      }

      for (i = 1; i <= highMsg; i++) {
         
         processMsg(i, numMsg, oldArea, newArea, area,
		    oldLastread, lcount, newLastread);
      }

      writeLastreadFile(oldName, newLastread, lcount, newArea);

      MsgCloseArea(oldArea);
      MsgCloseArea(newArea);

      printf("   oldMsg: %u   newMsg: %u\n", numMsg, msgCopied);

      free(oldLastread);
      free(newLastread);

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
   
   printf("sqpack v1.0.1\n");

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
            msgCopied = 0;
            msgProcessed = 0;
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
