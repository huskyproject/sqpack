/* $Id$ */
/*****************************************************************************
* SqPack --- FTN messagebase packer (purger)
*****************************************************************************
* Copyright (C) 1997-1999 Matthias Tichy (mtt@tichy.de).
* Copyright (C) 1999-2002 Husky developers team
*
* This file is part of HUSKY Fidonet Software project.
*
* SQPACK is free software; you can redistribute it and/or modify it
* under the terms of the GNU General Public License as published by the
* Free Software Foundation; either version 2, or (at your option) any
* later version.
*
* SQPACK is distributed in the hope that it will be useful, but
* WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
* General Public License for more details.
*
* You should have received a copy of the GNU General Public License
* along with HPT; see the file COPYING.  If not, write to the Free
* Software Foundation, 675 Mass Ave, Cambridge, MA 02139, USA.
*****************************************************************************
*/

#include <stdio.h>
#include <errno.h>
#include <time.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#ifdef UNIX
#include <unistd.h>
#else
#include <io.h>
#endif

#ifdef __TURBOC__
#include <share.h>
#endif

#ifdef __EMX__
#include <share.h>
#include <sys/types.h>
#endif
#include <fcntl.h>
#include <sys/stat.h>

#include <smapi/msgapi.h>
#include <smapi/prog.h>
#include <fidoconf/fidoconf.h>
#include <fidoconf/common.h>
#include <fidoconf/log.h>
#include <fidoconf/xstr.h>

#if defined ( __WATCOMC__ )
#include <string.h>
#include <stdlib.h>
#include <smapi/prog.h>
#include <share.h>
#endif

#if defined(_MSC_VER) && (_MSC_VER >= 1200)
#include <share.h>
#endif
#ifdef _MAKE_DLL_MVC_
#define SH_DENYNO _SH_DENYNO
#endif

#define PROGRAM_NAME "sqpack v1.3.0-current"
#define LOGFILE "sqpack.log"

unsigned long msgCopied, msgProcessed; // per Area
unsigned long totaloldMsg, totalmsgCopied;

void SqReadLastreadFile(char *fileName, UINT32 **lastreadp, ULONG *lcountp,
                        HAREA area)
{
    int fd;
    struct stat st;
    unsigned long i, temp;
    unsigned char buffer[4];
    char *name=NULL;

    w_log(LL_FUNC, "SqReadLastreadFile() begin");

    xstrscat( &name, fileName,  ".sql" , NULL);

    fd = sopen(name, O_BINARY | O_RDWR, SH_DENYNO, S_IWRITE | S_IREAD);
    if (fd != -1) {

        fstat(fd, &st);
        *lcountp = st.st_size / 4;
        *lastreadp = (UINT32 *) malloc(*lcountp * sizeof(UINT32));

        for (i = 0; i < *lcountp; i++) {
            read(fd, &buffer, 4);
            temp = buffer[0] + (((unsigned long)(buffer[1])) << 8) +
                (((unsigned long)(buffer[2])) << 16) +
                (((unsigned long)(buffer[3])) << 24);
            (*lastreadp)[i] = MsgUidToMsgn(area, temp, UID_PREV);
        }

        close(fd);

    } else {
        *lastreadp = NULL;
        *lcountp = 0;
    };

    free(name);
    w_log(LL_FUNC, "SqReadLastreadFile() end");
}


void SqWriteLastreadFile(char *fileName, UINT32 *lastread, ULONG lcount,
                         HAREA area)
{
    char *name=NULL;
    unsigned char buffer[4];
    int fd;
    unsigned long i, temp;

    w_log(LL_FUNC, "SqWriteLastreadFile() begin");
    if (lastread) {

        xstrscat( &name, fileName,  ".sql" , NULL );

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

        } else
            w_log(LL_ERR, "Could not write lastread file '%s': %s", name, strerror(errno));

        free(name);
    }
    w_log(LL_FUNC, "SqWriteLastreadFile() end");
}

/*
*  get_dword
*
*  Reads in a 4 byte word that is stored in little endian (Intel) notation
*  and converts it to the local representation n an architecture-
*  independent manner
*/

#define get_dword(ptr)            \
    ((dword)((unsigned char)(ptr)[0]) |           \
    (((dword)((unsigned char)(ptr)[1])) << 8)  | \
    (((dword)((unsigned char)(ptr)[2])) << 16) | \
    (((dword)((unsigned char)(ptr)[3])) << 24))  \

/*
*  get_word
*
*  Reads in a 2 byte word that is stored in little endian (Intel) notation
*  and converts it to the local representation in an architecture-
*  independent manner
*/

#define get_word(ptr)         \
    ((word)((unsigned char)(ptr)[0]) |         \
(((word)((unsigned char)(ptr)[1])) << 8 ))

typedef struct
{
    unsigned long UserCRC;         /* CRC-32 of user name (lowercase) */
    unsigned long UserID;          /* Unique UserID */
    unsigned long LastReadMsg;     /* Last read message number */
    unsigned long HighReadMsg;     /* Highest read message number */
}
JAMLREAD;
#define JAMLREAD_SIZE 16

int read_jamlread(int fd, JAMLREAD *plread)
{
    unsigned char buf[JAMLREAD_SIZE];

    w_log(LL_FUNC, "read_jamlread() begin");
    if (read(fd, buf, JAMLREAD_SIZE) != JAMLREAD_SIZE) {
        w_log(LL_ERR, "read_jamlread() error: %s", strerror(errno));
        w_log(LL_FUNC, "read_jamlread() failed");
        return 0;
    }

    plread->UserCRC     = get_dword(buf);
    plread->UserID      = get_dword(buf+4);
    plread->LastReadMsg = get_dword(buf+8);
    plread->HighReadMsg = get_dword(buf+12);

    w_log(LL_FUNC, "read_jamlread() OK");
    return 1;
}

int write_jamlread(int fd, JAMLREAD *plread)
{
    unsigned char buf[JAMLREAD_SIZE];

    w_log(LL_FUNC, "write_jamlread() begin");
    put_dword(buf, plread->UserCRC);
    put_dword(buf + 4, plread->UserID);
    put_dword(buf + 8, plread->LastReadMsg);
    put_dword(buf + 12, plread->HighReadMsg);

    if (write(fd, buf, JAMLREAD_SIZE) != JAMLREAD_SIZE) {
        w_log(LL_ERR, "write_jamlread() error: %s", strerror(errno));
        w_log(LL_FUNC, "write_jamlread() failed");
        return 0;
    }

    w_log(LL_FUNC, "write_jamlread() OK");
    return 1;
}

int write_partial_jamlread(int fd, JAMLREAD *plread)
{
    unsigned char buf[JAMLREAD_SIZE/2];

    w_log(LL_FUNC, "write_partial_jamlread() begin");
    put_dword(buf + 0, plread->LastReadMsg);
    put_dword(buf + 4, plread->HighReadMsg);

    if (write(fd, buf, JAMLREAD_SIZE/2) != JAMLREAD_SIZE/2) {
        w_log(LL_ERR, "write_partial_jamlread() error: %s", strerror(errno));
        w_log(LL_FUNC, "write_partial_jamlread() failed");
        return 0;
    }

    w_log(LL_FUNC, "write_partial_jamlread() OK");
    return 1;
}

void JamReadLastreadFile(char *fileName, UINT32 **lastreadp, ULONG *lcountp,
                         HAREA area)
{
    int fd;
    struct stat st;
    unsigned long i;
    char *name = NULL;
    JAMLREAD lread;

    w_log(LL_FUNC, "JamReadLastreadFile() begin");

    xstrscat( &name, fileName,  ".jlr" , NULL);

    fd = sopen(name, O_BINARY | O_RDWR, SH_DENYNO, S_IWRITE | S_IREAD);
    if (fd != -1) {

        fstat(fd, &st);
        *lcountp = st.st_size / JAMLREAD_SIZE;
        *lastreadp = (UINT32 *) malloc(*lcountp * sizeof(UINT32) * 2);

        for (i = 0; i < *lcountp; i++) {
            read_jamlread(fd, &lread);
            (*lastreadp)[i*2] = MsgUidToMsgn(area, lread.LastReadMsg, UID_PREV);
            (*lastreadp)[i*2+1] = MsgUidToMsgn(area, lread.HighReadMsg, UID_PREV);
        }

        close(fd);

    } else {
        w_log(LL_ERR, "JamReadLastreadFile(): can't open %s: %s", name, strerror(errno));
        *lastreadp = NULL;
        *lcountp = 0;
    };

    *lcountp = (*lcountp) << 1; /* rest of sqpack does not now of 2 lastread ptrs */

    free(name);
    w_log(LL_FUNC, "JamReadLastreadFile() end");
}

void JamWriteLastreadFile(char *fileName, UINT32 *lastread, ULONG lcount,
                          HAREA area)
{
    char *name = NULL;
    int fd;
    unsigned long i;
    JAMLREAD lread;

    w_log(LL_FUNC, "JamWriteLastreadFile() begin");
    if (lastread) {

        xstrscat( &name, fileName,  ".jlr" , NULL);

        fd = sopen(name, O_BINARY | O_RDWR, SH_DENYNO, S_IWRITE | S_IREAD);

        if (fd != -1) {

            for (i = 0; i < (lcount >> 1); i++) {

                lread.LastReadMsg = MsgMsgnToUid(area, lastread[i*2]);
                lread.HighReadMsg = MsgMsgnToUid(area, lastread[i*2+1]);

                lseek(fd, i*JAMLREAD_SIZE + JAMLREAD_SIZE/2, SEEK_SET);
                write_partial_jamlread(fd, &lread);
            }

            close(fd);

        } else
            w_log(LL_ERR, "JamWriteLastreadFile(): can't open %s: %s", name, strerror(errno));

        free(name);
    }
    w_log(LL_FUNC, "JamWriteLastreadFile() end");
}

void SdmReadLastreadFile(char *fileName, UINT32 **lastreadp, ULONG *lcountp,
                         HAREA area)
{
    int fd;
    struct stat st;
    unsigned long i;
    char *name;
    UINT16 temp;

    w_log(LL_FUNC, "SdmReadLastreadFile() begin");
    name = (char *) malloc(strlen(fileName)+9+1);
    strcpy(name, fileName);
    Add_Trailing(name, PATH_DELIM);
    strcat(name, "lastread");

    fd = sopen(name, O_BINARY | O_RDWR, SH_DENYNO, S_IWRITE | S_IREAD);
    if (fd != -1) {

        fstat(fd, &st);
        *lcountp = st.st_size / 2; /*sizeof(UINT16)*/
        *lastreadp = (UINT32 *) malloc(*lcountp * sizeof(UINT32));

        for (i = 0; i < *lcountp; i++) {
            read(fd, &temp, 2);
            (*lastreadp)[i] = MsgUidToMsgn(area, temp, UID_PREV);
        }

        close(fd);

    } else {
        w_log(LL_ERR, "SdmReadLastreadFile(): can't open %s: %s", name, strerror(errno));
        *lastreadp = NULL;
        *lcountp = 0;
    };

    free(name);
    w_log(LL_FUNC, "SdmReadLastreadFile() end");
}

void SdmWriteLastreadFile(char *fileName, UINT32 *lastread, ULONG lcount,
                          HAREA area)
{
    char *name;
    int fd;
    unsigned long i;
    UINT16 temp;
    unsigned char buf[2];

    w_log(LL_FUNC, "SdmWriteLastreadFile() begin");
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
                put_word(buf, temp);
                write(fd, buf, 2);
            }

            close(fd);

        } else
            w_log(LL_ERR, "SdmWriteLastreadFile(): can't open %s: %s", name, strerror(errno));

        free(name);
    }
    w_log(LL_FUNC, "SdmWriteLastreadFile() end");
}

void readLastreadFile(char *fileName, UINT32 **lastreadp, ULONG *lcountp,
                      HAREA area, int areaType)
{
    w_log(LL_FUNC, "readLastreadFile() begin");
    if (areaType == MSGTYPE_SQUISH)
        SqReadLastreadFile(fileName, lastreadp, lcountp, area);
    else if (areaType == MSGTYPE_JAM)
        JamReadLastreadFile(fileName, lastreadp, lcountp, area);
    else if (areaType == MSGTYPE_SDM)
        SdmReadLastreadFile(fileName, lastreadp, lcountp, area);
    w_log(LL_FUNC, "readLastreadFile() end");
}

void writeLastreadFile(char *fileName, UINT32 *lastreadp, ULONG lcount,
                       HAREA area, int areaType)
{
    w_log(LL_FUNC, "writeLastreadFile() begin");
    if (areaType == MSGTYPE_SQUISH)
        SqWriteLastreadFile(fileName, lastreadp, lcount, area);
    else if (areaType == MSGTYPE_JAM)
        JamWriteLastreadFile(fileName, lastreadp, lcount, area);
    else if (areaType == MSGTYPE_SDM)
        SdmWriteLastreadFile(fileName, lastreadp, lcount, area);
    w_log(LL_FUNC, "writeLastreadFile() end");
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

    w_log(LL_FUNC, "processMsg() begin");
    msg = MsgOpenMsg(oldArea, MOPEN_RW, msgNum);
    if (msg == NULL) return rc;

    if (MsgReadMsg(msg, &xmsg, 0, 0, NULL, 0, NULL)<0) {
        MsgCloseMsg(msg);
        msgProcessed++;
        return rc;
    }

    unsent = ((xmsg.attr & MSGLOCAL) && !(xmsg.attr & MSGSENT)) || (xmsg.attr & MSGLOCKED);

    if ( unsent || (((area -> max == 0) || ((numMsg - msgProcessed + msgCopied) <= area -> max) ||
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
            xmsg.replyto = MsgUidToMsgn(oldArea, xmsg.replyto, UID_EXACT) > shift ? MsgUidToMsgn(oldArea, xmsg.replyto, UID_EXACT) - shift : 0;
            if ((area->msgbType & MSGTYPE_SQUISH) == MSGTYPE_SQUISH){

                for (i = 0; i < MAX_REPLY; i++)
                    xmsg.replies[i] = MsgUidToMsgn(oldArea, xmsg.replies[i], UID_EXACT) > shift ? MsgUidToMsgn(oldArea, xmsg.replies[i], UID_EXACT) - shift : 0;
            }else {
                xmsg.replies[0] = MsgUidToMsgn(oldArea, xmsg.replies[0], UID_EXACT) > shift ? MsgUidToMsgn(oldArea, xmsg.replies[0], UID_EXACT) - shift : 0;
                xmsg.xmreplynext = MsgUidToMsgn(oldArea, xmsg.xmreplynext, UID_EXACT) > shift ? MsgUidToMsgn(oldArea, xmsg.xmreplynext, UID_EXACT) - shift : 0;
            }
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
    w_log(LL_FUNC, "processMsg() end");
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

void updateMsgLinks(UINT32 msgNum, HAREA area, UINT32 rmCount, UINT32 *rmMap, int areaType)
{
    HMSG msg;
    XMSG xmsg;
    int i;

    w_log(LL_FUNC, "updateMsgLinks() begin");
    msg = MsgOpenMsg(area, MOPEN_RW, getShiftedNum(msgNum, rmCount, rmMap));
    if (msg == NULL) return;

    MsgReadMsg(msg, &xmsg, 0, 0, NULL, 0, NULL);

    xmsg.replyto = getShiftedNum(xmsg.replyto, rmCount, rmMap);
    if ((areaType & MSGTYPE_SQUISH) == MSGTYPE_SQUISH)
        for (i = 0; i < MAX_REPLY; i++)
            xmsg.replies[i] = getShiftedNum(xmsg.replies[i], rmCount, rmMap);
        else {
            xmsg.replies[0] = getShiftedNum(xmsg.replies[0], rmCount, rmMap);
            xmsg.xmreplynext = getShiftedNum(xmsg.xmreplynext, rmCount, rmMap);
        }

        MsgWriteMsg(msg, 0, &xmsg, NULL, 0, 0, 0, NULL);
        MsgCloseMsg(msg);
        w_log(LL_FUNC, "updateMsgLinks() end");
}


void renameArea(int areaType, char *oldName, char *newName)
{
    char *oldTmp=NULL, *newTmp=NULL;
    unsigned long oldsize=0, newsize=0;
    struct stat sb;

    w_log(LL_FUNC, "renameArea() begin");

    xstrcat(&oldTmp, oldName);
    xstrcat(&newTmp, newName);

    if (areaType==MSGTYPE_SQUISH) {
        xstrcat(&oldTmp, ".sqd");
        xstrcat(&newTmp, ".sqd");
        /* sizes of files: for statistics */
        stat(oldTmp,&sb);
        oldsize += sb.st_size;
        stat(newTmp,&sb);
        newsize += sb.st_size;
        remove(oldTmp);
        rename(newTmp, oldTmp);

        oldTmp[strlen(oldTmp)-1] = 'i';
        newTmp[strlen(newTmp)-1] = 'i';
        /* sizes of files: for statistics */
        stat(oldTmp,&sb);
        oldsize += sb.st_size;
        stat(newTmp,&sb);
        newsize += sb.st_size;
        remove(oldTmp);
        rename(newTmp, oldTmp);
    }

    if (areaType==MSGTYPE_JAM) {
        xstrcat(&oldTmp, ".jdt");
        xstrcat(&newTmp, ".jdt");
        remove(oldTmp);
        rename(newTmp, oldTmp);

        oldTmp[strlen(oldTmp)-1] = 'x';
        newTmp[strlen(newTmp)-1] = 'x';
        /* sizes of files: for statistics */
        stat(oldTmp,&sb);
        oldsize += sb.st_size;
        stat(newTmp,&sb);
        newsize += sb.st_size;
        remove(oldTmp);
        rename(newTmp, oldTmp);

        oldTmp[strlen(oldTmp)-2] = 'h';
        newTmp[strlen(newTmp)-2] = 'h';
        oldTmp[strlen(oldTmp)-1] = 'r';
        newTmp[strlen(newTmp)-1] = 'r';
        /* sizes of files: for statistics */
        stat(oldTmp,&sb);
        oldsize += sb.st_size;
        stat(newTmp,&sb);
        newsize += sb.st_size;
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

    w_log( LL_STAT, "Old size: %lu, new size: %lu", oldsize, newsize );
    free(oldTmp);
    free(newTmp);
    w_log(LL_FUNC, "renameArea() end");
}

void purgeArea(s_area *area)
{
    char *oldName = area -> fileName;
    char *newName=NULL;
    HAREA oldArea=NULL, newArea = NULL;
    dword highMsg, i, j, numMsg, hw=0;
    int areaType = area -> msgbType & (MSGTYPE_JAM | MSGTYPE_SQUISH | MSGTYPE_SDM);

    UINT32 *oldLastread, *newLastread = 0;
    UINT32 *removeMap;
    UINT32 rmIndex = 0;

    w_log(LL_FUNC, "purgeArea() begin");
    if (area->nopack) {
        printf("   No purging needed!\n");
        return;
    }

    //generated tmp-FileName
    xstrscat(&newName, oldName, "_tmp", NULL);

    /*oldArea = MsgOpenArea((byte *) oldName, MSGAREA_NORMAL, -1, -1, -1, MSGTYPE_SQUISH);*/
    oldArea = MsgOpenArea((byte *) oldName, MSGAREA_NORMAL, (word) areaType);

    /*if (oldArea) newArea = MsgOpenArea((byte *) newName, MSGAREA_CREATE, area.fperm, area.uid, area.gid,MSGTYPE_SQUISH);*/
    if (oldArea) {
        if (areaType == MSGTYPE_SDM)
            newArea = oldArea;
        else
            newArea = MsgOpenArea((byte *) newName, MSGAREA_CREATE, (word) areaType);
    }

    if ((oldArea != NULL) && (newArea != NULL)) {
        ULONG lcount;

        highMsg = MsgGetHighMsg(oldArea);
        numMsg = MsgGetNumMsg(oldArea);
        if (areaType != MSGTYPE_SDM) hw = MsgGetHighWater(oldArea);
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
                strncpy(oldmsgname, oldName, PATHLEN);
                Add_Trailing(oldmsgname, PATH_DELIM);
                strncpy(newmsgname, oldmsgname, PATHLEN);
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
                updateMsgLinks(i, newArea, rmIndex + 1, removeMap, areaType);
        }

        if (rmIndex) { /* someting was removed, maybe need to update lastreadfile */
            for (j = 0; j < lcount; j++) {
                for (i=0; i<rmIndex; i+=2) {
                    if (oldLastread[j] >= removeMap[i]) {
                        if (oldLastread[j] > removeMap[i] + removeMap[i+1]) {
                            newLastread[j] -= removeMap[i+1];
                        } else {
                            newLastread[j] -= oldLastread[j] - removeMap[i] + 1;
                        }
                    }
                }
            }
        }

        writeLastreadFile(oldName, newLastread, lcount, newArea, areaType);

        MsgCloseArea(oldArea);
        if (areaType != MSGTYPE_SDM) {
            if ((numMsg - msgCopied) > hw) hw=0;
            else hw -= (numMsg - msgCopied);
            MsgSetHighWater(newArea, hw);
            MsgCloseArea(newArea);
        }

        w_log(LL_STAT, "OldMsg: %lu; NewMsg: %lu", (unsigned long)numMsg, msgCopied);
        totaloldMsg+=numMsg; totalmsgCopied+=msgCopied; // total

        free(oldLastread);
        free(newLastread);

        //rename oldArea to newArea
        renameArea(areaType, oldName, newName);
    }
    else {
        if (oldArea) {
            MsgCloseArea(oldArea);
            if (areaType & MSGTYPE_SDM )
              w_log(LL_ERR, "Could not create '%s%c*.msg'!", newName, PATH_DELIM );
            else
              w_log(LL_ERR, "Could not create '%s.*'!", newName );
        }else{
            if (areaType & MSGTYPE_SDM )
              w_log(LL_ERR, "Could not open '%s%c*.msg'!", oldName, PATH_DELIM );
            else
              w_log(LL_ERR, "Could not open '%s.*'!", oldName );
        }
    }
    free(newName);
    w_log(LL_FUNC, "purgeArea() end");
}

void handleArea(s_area *area)
{
    w_log(LL_FUNC, "handleArea() begin");
    if ((area -> msgbType & MSGTYPE_SQUISH) == MSGTYPE_SQUISH ||
        (area -> msgbType & MSGTYPE_JAM) == MSGTYPE_JAM ||
        (area -> msgbType & MSGTYPE_SDM) == MSGTYPE_SDM) {
        w_log( LL_INFO, "Purge area %s (%s)", area -> areaName,
               area -> msgbType & MSGTYPE_SQUISH ? "squish" :
                              area -> msgbType & MSGTYPE_JAM ? "jam" :
                              area -> msgbType & MSGTYPE_SDM ? "msg/OPUS" : "unknown type"
             );
        msgCopied = 0;
        msgProcessed = 0;
        purgeArea(area);
    };
    w_log(LL_FUNC, "handleArea() end");
}

void doArea(s_area *area, char *cmp)
{
    if (patimat(area->areaName,cmp)) handleArea(area);
}

int main(int argc, char **argv) {

    s_fidoconfig *cfg;
    unsigned int i;
    struct _minf m;

    printf( PROGRAM_NAME "\n");

    if (argc!=2) {
        if (argc>2) printf("too many arguments!\n");
        printf ("Usage: sqpack <areamask>\n");
    } else {

        setvar("module", "sqpack");
        cfg = readConfig(NULL);

        if (cfg != NULL ) {
/*            char *buff = NULL;
            xstrscat(&buff, cfg->logFileDir, LOGFILE, NULL);
            openLog(buff, PROGRAM_NAME, cfg);
            nfree(buff);
*/          openLog(LOGFILE, PROGRAM_NAME, cfg);
            m.req_version = 0;
            m.def_zone = cfg->addr[0].zone;
            if (MsgOpenApi(&m)!= 0) {
                w_log(LL_CRIT,"MsgOpenApi Error. Exit.");
                closeLog();
                disposeConfig(cfg);
                exit(1);
            }

            // purge dupe area
            doArea(&(cfg->dupeArea), argv[1]);
            // purge bad area
            doArea(&(cfg->badArea), argv[1]);

            for (i=0; i < cfg->netMailAreaCount; i++)
                // purge netmail areas
                doArea(&(cfg->netMailAreas[i]), argv[1]);

            for (i=0; i < cfg->echoAreaCount; i++)
                // purge echomail areas
                doArea(&(cfg->echoAreas[i]), argv[1]);

            for (i=0; i < cfg->localAreaCount; i++)
                // purge local areas
                doArea(&(cfg->localAreas[i]), argv[1]);

            w_log(LL_SUMMARY,"Total oldMsg: %lu; total newMsg: %lu",
                (unsigned long)totaloldMsg, (unsigned long)totalmsgCopied);
            disposeConfig(cfg);
            w_log(LL_STOP,"End");
            closeLog();
            return 0;

        } else {
            printf("Could not read fido config\n");
            return 1;
        }
    }
    return 0;
}
