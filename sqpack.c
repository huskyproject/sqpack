#include <stdio.h>
#include <errno.h>
#include <time.h>
#include <stdlib.h>
#include <string.h>

#ifdef UNIX
#include <unistd.h>
#else
#include <io.h>
#endif

#ifdef __TURBOC__
#include <share.h>
#endif

#include <fcntl.h>
#ifdef __EMX__
#include <share.h>
#include <sys/types.h>
#endif
#include <sys/stat.h>

#include <smapi/msgapi.h>
#include <smapi/prog.h>
#include <smapi/api_jam.h>
#include <fidoconf/fidoconf.h>
#include <fidoconf/common.h>

#if defined ( __WATCOMC__ )
#include <string.h>
#include <stdlib.h>
#include <smapi/prog.h>
#include <share.h>
#endif

#if defined(_MSC_VER) && (_MSC_VER >= 1200)
#include <share.h>
#endif

/* Changes
  v1.0.4
        SMS Packing ALL areas
  v1.0.3
	K.N. Sqpack now newer thrashes reply links. Works fine for me. 
	PS	There is something strange about all this code: it worked
		fine for the first time, without any debugging. Be prepared for 
		bugs ;) 
	Also allows additional customization: kill read and keep unread
*/

unsigned long msgCopied, msgProcessed; // per Area
unsigned long totaloldMsg, totalmsgCopied;

void SqReadLastreadFile(char *fileName, UINT32 **lastreadp, ULONG *lcountp,
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
      
   } else {
		*lastreadp = NULL;
		*lcountp = 0;
   };

   free(name);
}

void SqWriteLastreadFile(char *fileName, UINT32 *lastread, ULONG lcount,
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

void JamReadLastreadFile(char *fileName, UINT32 **lastreadp, ULONG *lcountp,
		      HAREA area)
{
   int fd;
   struct stat st;
   unsigned long i;
   char *name;
   JAMLREAD lread;

   name = (char *) malloc(strlen(fileName)+4+1);
   strcpy(name, fileName);
   strcat(name, ".jlr");
   
   fd = sopen(name, O_BINARY | O_RDWR, SH_DENYNO, S_IWRITE | S_IREAD);
   if (fd != -1) {
      
      fstat(fd, &st);
      *lcountp = st.st_size / sizeof(JAMLREAD);
      *lastreadp = (UINT32 *) malloc(*lcountp * sizeof(UINT32) * 2);
      
      for (i = 0; i < *lcountp; i++) {
         read(fd, &lread, sizeof(JAMLREAD));
         (*lastreadp)[i*2] = MsgUidToMsgn(area, lread.LastReadMsg, UID_NEXT);
         (*lastreadp)[i*2+1] = MsgUidToMsgn(area, lread.HighReadMsg, UID_NEXT);
      }

      close(fd);
      
   } else {
		*lastreadp = NULL;
		*lcountp = 0;
   };

   free(name);
}

void JamWriteLastreadFile(char *fileName, UINT32 *lastread, ULONG lcount,
			HAREA area)
{
   char *name;
   int fd;
   unsigned long i;
   JAMLREAD lread;
   
   
   if (lastread) {

      name = (char *) malloc(strlen(fileName)+4+1);
      strcpy(name, fileName);
      strcat(name, ".jlr");

      fd = sopen(name, O_BINARY | O_RDWR, SH_DENYNO, S_IWRITE | S_IREAD);

      if (fd != -1) {

         for (i = 0; i < lcount; i++) {

            lread.LastReadMsg = MsgMsgnToUid(area, lastread[i*2]);
            lread.HighReadMsg = MsgMsgnToUid(area, lastread[i*2+1]);

            lseek(fd, i*sizeof(JAMLREAD)+((UINT32)&lread.LastReadMsg-(UINT32)&lread), SEEK_SET);
            write(fd, &lread.LastReadMsg,
                  sizeof(lread.LastReadMsg)+sizeof(lread.HighReadMsg));

         }

         close(fd);
              
      } else printf("Could not write lastread file %s, error %u!\n", name, errno);
      
      free(name);
   }
}

void SdmReadLastreadFile(char *fileName, UINT32 **lastreadp, ULONG *lcountp,
		      HAREA area)
{
   int fd;
   struct stat st;
   unsigned long i;
   char *name;
   UINT16 temp;

   name = (char *) malloc(strlen(fileName)+9+1);
   strcpy(name, fileName);
   Add_Trailing(name, PATH_DELIM);
   strcat(name, "lastread");
   
   fd = sopen(name, O_BINARY | O_RDWR, SH_DENYNO, S_IWRITE | S_IREAD);
   if (fd != -1) {
      
      fstat(fd, &st);
      *lcountp = st.st_size / sizeof(UINT16);
      *lastreadp = (UINT32 *) malloc(*lcountp * sizeof(UINT32));
      
      for (i = 0; i < *lcountp; i++) {
         read(fd, &temp, 2);
         (*lastreadp)[i] = MsgUidToMsgn(area, temp, UID_NEXT);
      }

      close(fd);
      
   } else {
		*lastreadp = NULL;
		*lcountp = 0;
   };

   free(name);
}

void SdmWriteLastreadFile(char *fileName, UINT32 *lastread, ULONG lcount,
			HAREA area)
{
   char *name;
   int fd;
   unsigned long i;
   UINT16 temp;
   
   if (lastread) {

      name = (char *) malloc(strlen(fileName)+9+1);
      strcpy(name, fileName);
      Add_Trailing(name, PATH_DELIM);
      strcat(name, "lastread");

      fd = sopen(name, O_BINARY | O_RDWR, SH_DENYNO, S_IWRITE | S_IREAD);

      if (fd != -1) {

         lseek(fd, 0, SEEK_SET);
         for (i = 0; i < lcount; i++) {

            temp = (UINT16)MsgMsgnToUid(area, lastread[i]);
            write(fd, &temp, sizeof(UINT16));

         }

         close(fd);
              
      } else printf("Could not write lastread file %s, error %u!\n", name, errno);
      
      free(name);
   }
}

void readLastreadFile(char *fileName, UINT32 **lastreadp, ULONG *lcountp,
		      HAREA area, int areaType)
{
  if (areaType == MSGTYPE_SQUISH)
    SqReadLastreadFile(fileName, lastreadp, lcountp, area);
  else if (areaType == MSGTYPE_JAM)
    JamReadLastreadFile(fileName, lastreadp, lcountp, area);
  else if (areaType == MSGTYPE_SDM)
    SdmReadLastreadFile(fileName, lastreadp, lcountp, area);
}

void writeLastreadFile(char *fileName, UINT32 *lastreadp, ULONG lcount,
		      HAREA area, int areaType)
{
  if (areaType == MSGTYPE_SQUISH)
    SqWriteLastreadFile(fileName, lastreadp, lcount, area);
  else if (areaType == MSGTYPE_JAM)
    JamWriteLastreadFile(fileName, lastreadp, lcount, area);
  else if (areaType == MSGTYPE_SDM)
    SdmWriteLastreadFile(fileName, lastreadp, lcount, area);
}

unsigned long getOffsetInLastread(UINT32 *lastread, ULONG lcount, dword msgnum)
{

   unsigned long i;

   for (i = 0; i < lcount; i++) {
      if (lastread[i] == msgnum) return i;
   }

   return (-1);
   
}

/* returns zero if msg was killed, nonzero if it was copied */

int processMsg(dword msgNum, dword numMsg, HAREA oldArea, HAREA newArea,
		s_area *area, UINT32 shift)
{
   HMSG msg, newMsg;
   XMSG xmsg;
   struct tm tmTime;
   time_t ttime, actualTime = time(NULL);
   char *text, *ctrlText;
   dword  textLen, ctrlLen;
   int unsent, i, rc = 0;
   
//   unsigned long offset;

   msg = MsgOpenMsg(oldArea, MOPEN_RW, msgNum);
   if (msg == NULL) return rc;

   if (MsgReadMsg(msg, &xmsg, 0, 0, NULL, 0, NULL)<0) {
      MsgCloseMsg(msg);
      msgProcessed++;
      return rc;
   }

   unsent = (xmsg.attr & MSGLOCAL) && !(xmsg.attr & MSGSENT);

   if (unsent || (((area -> max == 0) || ((numMsg - msgProcessed + msgCopied) <= area -> max) ||
(area -> keepUnread && !(xmsg.attr & MSGREAD))) && !((xmsg.attr & MSGREAD) && area -> killRead))) {
      //only max msgs should be in new area
     
     if (xmsg.attr & MSGLOCAL) {
        DosDate_to_TmDate((SCOMBO*)&(xmsg.date_written), &tmTime);
     } else {
        DosDate_to_TmDate((SCOMBO*)&(xmsg.date_arrived), &tmTime);
     }
/*     DosDate_to_TmDate(&(xmsg.attr & MSGLOCAL ? xmsg.date_written :
			 xmsg.date_arrived), &tmTime);*/
     ttime = mktime(&tmTime);
     if (ttime == 0xfffffffflu) ttime = 0; /* emx */

     if (unsent || (area -> purge == 0) || ttime == 0 ||
         (abs(actualTime - ttime) <= (area -> purge * 24 *60 * 60))) {
	xmsg.replyto = xmsg.replyto > shift ? xmsg.replyto - shift : 0;
	xmsg.replynext = xmsg.replynext > shift ? xmsg.replynext - shift : 0;
	for (i = 0; i < MAX_REPLY; i++)
		xmsg.replies[i] = xmsg.replies[i] > shift ? xmsg.replies[i] - shift : 0; 
	// copy msg
        textLen = MsgGetTextLen(msg);
        ctrlLen = MsgGetCtrlLen(msg);

        text = (char *) malloc(textLen+1);
        text[textLen] = '\0';

        ctrlText = (char *) malloc(ctrlLen+1);
        ctrlText[ctrlLen] = '\0';

        MsgReadMsg(msg, NULL, 0, textLen, (byte*)text, ctrlLen, (byte*)ctrlText);

        if (area->msgbType & MSGTYPE_SDM)
		MsgWriteMsg(msg, 0, &xmsg, (byte*)text, textLen, textLen, ctrlLen, (byte*)ctrlText);
        else {
		newMsg = MsgOpenMsg(newArea, MOPEN_CREATE, 0);
		MsgWriteMsg(newMsg, 0, &xmsg, (byte*)text, textLen, textLen, ctrlLen, (byte*)ctrlText);
		MsgCloseMsg(newMsg);
        }

        msgCopied++;
        free(text);
        free(ctrlText);
	rc = 1;
      }
      
   }
   MsgCloseMsg(msg);
   msgProcessed++;
   return rc;
}

UINT32 getShiftedNum(UINT32 msgNum, UINT32 rmCount, UINT32 *rmMap)
{
   UINT32 i, nMsgNum = msgNum;
   msgNum += rmMap[1];
   for (i = 0; i < rmCount; i+=2)
	if (msgNum >= rmMap[i])
		nMsgNum -= rmMap[i + 1];
	else 
		break;
   return msgNum > 0L ? msgNum : 0L;
}

void updateMsgLinks(UINT32 msgNum, HAREA area, UINT32 rmCount, UINT32 *rmMap)
{
   HMSG msg;
   XMSG xmsg;
   int i;
   
   msg = MsgOpenMsg(area, MOPEN_RW, getShiftedNum(msgNum, rmCount, rmMap));
   if (msg == NULL) return;

   MsgReadMsg(msg, &xmsg, 0, 0, NULL, 0, NULL);
	
   xmsg.replyto = getShiftedNum(xmsg.replyto, rmCount, rmMap);
   xmsg.replynext = getShiftedNum(xmsg.replynext, rmCount, rmMap);
   for (i = 0; i < MAX_REPLY; i++)
	xmsg.replies[i] = getShiftedNum(xmsg.replies[i], rmCount, rmMap);
   
   MsgWriteMsg(msg, 0, &xmsg, NULL, 0, 0, 0, NULL);
   MsgCloseMsg(msg);
}


void renameArea(int areaType, char *oldName, char *newName)
{
   char *oldTmp, *newTmp;
   
   oldTmp = (char *) malloc(strlen(oldName)+4+1);
   newTmp = (char *) malloc(strlen(newName)+4+1);

   strcpy(oldTmp, oldName);
   strcpy(newTmp, newName);

   if (areaType==MSGTYPE_SQUISH) {
     strcat(oldTmp, ".sqd");
     strcat(newTmp, ".sqd");
     remove(oldTmp);
     rename(newTmp, oldTmp);

     oldTmp[strlen(oldTmp)-1] = 'i';
     newTmp[strlen(newTmp)-1] = 'i';
     remove(oldTmp);
     rename(newTmp, oldTmp);
   }

   if (areaType==MSGTYPE_JAM) {
     strcat(oldTmp, ".jdt");
     strcat(newTmp, ".jdt");
     remove(oldTmp);
     rename(newTmp, oldTmp);

     oldTmp[strlen(oldTmp)-1] = 'x';
     newTmp[strlen(newTmp)-1] = 'x';
     remove(oldTmp);
     rename(newTmp, oldTmp);

     oldTmp[strlen(oldTmp)-2] = 'h';
     newTmp[strlen(newTmp)-2] = 'h';
     oldTmp[strlen(oldTmp)-1] = 'r';
     newTmp[strlen(newTmp)-1] = 'r';
     remove(oldTmp);
     rename(newTmp, oldTmp);

     newTmp[strlen(newTmp)-2] = 'l';
#if 0
     oldTmp[strlen(oldTmp)-2] = 'l';

     remove(oldTmp);
     rename(newTmp, oldTmp);
#endif
     remove(newTmp); // erase new lastread file

   }
   
   free(oldTmp);
   free(newTmp);
}

void purgeArea(s_area *area)
{
	char *oldName = area -> fileName;
	char *newName;
	HAREA oldArea, newArea;
	dword highMsg, i, j, numMsg;
	int areaType = area -> msgbType & (MSGTYPE_JAM | MSGTYPE_SQUISH | MSGTYPE_SDM);

	UINT32 *oldLastread, *newLastread = NULL;
	UINT32 *removeMap;
	UINT32 rmIndex = 0;

	if (area->nopack) {
			printf("   No purging needed!\n");
			return;
	}

	//generated tmp-FileName
	newName = (char *) malloc(strlen(oldName)+4+1);
	strcpy(newName, oldName);
	strcat(newName, "_tmp");

	/*oldArea = MsgOpenArea((byte *) oldName, MSGAREA_NORMAL, -1, -1, -1, MSGTYPE_SQUISH);*/
	oldArea = MsgOpenArea((byte *) oldName, MSGAREA_NORMAL, areaType);
	
	/*if (oldArea) newArea = MsgOpenArea((byte *) newName, MSGAREA_CREATE, area.fperm, area.uid, area.gid,MSGTYPE_SQUISH);*/
	if (oldArea) {
		if (areaType == MSGTYPE_SDM)
			newArea = oldArea;
		else
			newArea = MsgOpenArea((byte *) newName, MSGAREA_CREATE, areaType);
	}

	if ((oldArea != NULL) && (newArea != NULL)) {
		ULONG lcount;

		highMsg = MsgGetHighMsg(oldArea);
		numMsg = MsgGetNumMsg(oldArea);
		readLastreadFile(oldName, &oldLastread, &lcount, oldArea, areaType);
		if (oldLastread) {
			newLastread = (UINT32 *) malloc(lcount * sizeof(UINT32));
			memcpy(newLastread, oldLastread, lcount * sizeof(UINT32));
		}

		removeMap = (UINT32 *) calloc(2, sizeof(UINT32));

		for (i = 1; i <= highMsg; i++) {
			if (!processMsg(i, numMsg, oldArea, newArea, area, 
				removeMap[1])) {
				if (!(rmIndex & 1)) {
				/* We started to delete new portion of */
					removeMap = (UINT32 *) realloc(removeMap, (rmIndex + 2) * sizeof(UINT32));
					removeMap[rmIndex++] = i;
					removeMap[rmIndex] = 0;
				}; 
				removeMap[rmIndex]++; /* Anyway, update counter */
				if (areaType == MSGTYPE_SDM)
					MsgKillMsg(oldArea, i);
			} else {
				/* We are copying msgs */
				if (rmIndex & 1) rmIndex++;
			};
		};

		if (rmIndex && areaType == MSGTYPE_SDM) {
			/* renumber the area */
			char oldmsgname[PATHLEN], newmsgname[PATHLEN];
			for (i = j = 1; i <= highMsg; i++) {
				strcpy(oldmsgname, oldName);
				Add_Trailing(oldmsgname, PATH_DELIM);
				strcpy(newmsgname, oldmsgname);
				sprintf(oldmsgname+strlen(oldmsgname), "%u.msg", (unsigned int)i);
				sprintf(newmsgname+strlen(newmsgname), "%u.msg", (unsigned int)j);
				if (access(oldmsgname, 0))
					continue;
				if (i == j) {
					j++;
					continue;
				}
				if (rename(oldmsgname, newmsgname) == 0)
					j++;
			}
		}

		if (rmIndex > 2) { /* there were several areas with deleted msgs */
			for (j = 1; j <= highMsg; j++) 
				updateMsgLinks(i, newArea, rmIndex + 1, removeMap);
		}

		if (rmIndex) { /* someting was removed, maybe need to update lastreadfile */
		   for (j = 0; j < lcount; j++) {
		      for (i=0; i<rmIndex; i+=2) {
			 if (oldLastread[j] > removeMap[i]) {
			    if (oldLastread[j] > removeMap[i] + removeMap[i+1]) {
			       newLastread[j] -= removeMap[i+1];
			    } else {
			       newLastread[j] -= oldLastread[j] - removeMap[i];
			    }
			 }
		      }
		   }
		}

		writeLastreadFile(oldName, newLastread, lcount, newArea, areaType);

		MsgCloseArea(oldArea);
		if (areaType != MSGTYPE_SDM)
			MsgCloseArea(newArea);

		printf("   oldMsg: %lu   newMsg: %lu\n", (unsigned long)numMsg, msgCopied);
        totaloldMsg+=numMsg; totalmsgCopied+=msgCopied; // total
		
		free(oldLastread);
		free(newLastread);

		//rename oldArea to newArea
		renameArea(areaType, oldName, newName);
	}
	else {
		if (oldArea) MsgCloseArea(oldArea);
		printf("Could not open %s or create %s.\n", oldName, newName);
	}
	free(newName);
}

void handleArea(s_area *area)
{
	if ((area -> msgbType & MSGTYPE_SQUISH) == MSGTYPE_SQUISH ||
	    (area -> msgbType & MSGTYPE_JAM) == MSGTYPE_JAM ||
	    (area -> msgbType & MSGTYPE_SDM) == MSGTYPE_SDM) {
        	printf("%s\n", area -> areaName);
	        msgCopied = 0;
		msgProcessed = 0;
		purgeArea(area);
	};
}

int main() {

   s_fidoconfig *cfg;
   int i;
   struct _minf m;
   
   printf("sqpack v1.1.0\n");

   cfg = readConfig(NULL);

   if (cfg != NULL ) {
      m.req_version = 0;
      m.def_zone = cfg->addr[0].zone;
      if (MsgOpenApi(&m)!= 0) {
         printf("MsgOpenApi Error.\n");
         exit(1);
      }
      // purge dupe area
      handleArea(&(cfg->dupeArea));
      // purge bad area
      handleArea(&(cfg->badArea));
      for (i=0; i < cfg->netMailAreaCount; i++)
      // purge netmail areas
	 handleArea(&(cfg->netMailAreas[i]));
      for (i=0; i < cfg->echoAreaCount; i++)
         // purge echomail areas
	 handleArea(&(cfg->echoAreas[i]));
      for (i=0; i < cfg->localAreaCount; i++) 
         // purge local areas
	 handleArea(&(cfg->localAreas[i]));
      disposeConfig(cfg);
      printf("\ntotal oldMsg: %lu   total newMsg: %lu\n", 
	     (unsigned long)totaloldMsg, (unsigned long)totalmsgCopied);
      return 0;
   } else {
      printf("Could not read fido config\n");
      return 1;
   }
}
