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
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include <huskylib/compiler.h>
#include <huskylib/huskylib.h>
#include <huskylib/locking.h>
#include <huskylib/strext.h>

#ifdef HAS_UNISTD_H
#include <unistd.h>
#endif

#ifdef HAS_IO_H
#include <io.h>
#endif

#ifdef HAS_SHARE_H
#include <share.h>
#endif

#include <smapi/msgapi.h>
#include <fidoconf/fidoconf.h>
#include <fidoconf/common.h>
#include <huskylib/log.h>
#include <huskylib/xstr.h>

#include "version.h"

#define LOGFILE "sqpack.log"

unsigned long msgCopied, msgProcessed; /*  per Area */
unsigned long totaloldMsg, totalmsgCopied;
unsigned long totalOldBaseSize, totalNewBaseSize;
int lock_fd;
char * versionStr;
int area_found;
s_fidoconfig * config;
void SqReadLastreadFile(char * fileName, UINT32 ** lastreadp, ULONG * lcountp, HAREA area)
{
    int fd;
    struct stat st;
    unsigned long i, temp;
    unsigned char buffer[4];
    char * name = NULL;

    w_log(LL_FUNC, "SqReadLastreadFile() begin");
    xstrscat(&name, fileName, ".sql", NULL);
    fd = sopen(name, O_BINARY | O_RDWR, SH_DENYNO, S_IWRITE | S_IREAD);

    if(fd != -1)
    {
        fstat(fd, &st);
        *lcountp   = st.st_size / 4;
        *lastreadp = (UINT32 *)malloc(*lcountp * sizeof(UINT32));

        for(i = 0; i < *lcountp; i++)
        {
            read(fd, &buffer, 4);
            temp = buffer[0] + (((unsigned long)(buffer[1])) << 8) +
                   (((unsigned long)(buffer[2])) << 16) + (((unsigned long)(buffer[3])) << 24);
            (*lastreadp)[i] = MsgUidToMsgn(area, temp, UID_PREV);
        }
        close(fd);
    }
    else
    {
        *lastreadp = NULL;
        *lcountp   = 0;
    }

    nfree(name);
    w_log(LL_FUNC, "SqReadLastreadFile() end");
} /* SqReadLastreadFile */

void SqWriteLastreadFile(char * fileName, UINT32 * lastread, ULONG lcount, HAREA area)
{
    char * name = NULL;
    unsigned char buffer[4];
    int fd;
    unsigned long i, temp;

    w_log(LL_FUNC, "SqWriteLastreadFile() begin");

    if(lastread)
    {
        xstrscat(&name, fileName, ".sql", NULL);
        fd = sopen(name, O_BINARY | O_RDWR, SH_DENYNO, S_IWRITE | S_IREAD);

        if(fd != -1)
        {
            lseek(fd, 0l, SEEK_SET);

            for(i = 0; i < lcount; i++)
            {
                temp      = MsgMsgnToUid(area, lastread[i]);
                buffer[0] = (UCHAR)(temp & 0xFF);
                buffer[1] = (UCHAR)((temp >> 8) & 0xFF);
                buffer[2] = (UCHAR)((temp >> 16) & 0xFF);
                buffer[3] = (UCHAR)((temp >> 24) & 0xFF);
                write(fd, &buffer, 4);
            }
            close(fd);
        }
        else
        {
            w_log(LL_ERR, "Could not write lastread file '%s': %s", name, strerror(errno));
        }

        nfree(name);
    }

    w_log(LL_FUNC, "SqWriteLastreadFile() end");
} /* SqWriteLastreadFile */

typedef struct
{
    unsigned long UserCRC;         /* CRC-32 of user name (lowercase) */
    unsigned long UserID;          /* Unique UserID */
    unsigned long LastReadMsg;     /* Last read message number */
    unsigned long HighReadMsg;     /* Highest read message number */
} JAMLREAD;
#define JAMLREAD_SIZE 16

int read_jamlread(int fd, JAMLREAD * plread)
{
    unsigned char buf[JAMLREAD_SIZE];

    w_log(LL_FUNC, "read_jamlread() begin");

    if(read(fd, buf, JAMLREAD_SIZE) != JAMLREAD_SIZE)
    {
        w_log(LL_ERR, "read_jamlread() error: %s", strerror(errno));
        w_log(LL_FUNC, "read_jamlread() failed");
        return 0;
    }

    plread->UserCRC     = get_dword(buf);
    plread->UserID      = get_dword(buf + 4);
    plread->LastReadMsg = get_dword(buf + 8);
    plread->HighReadMsg = get_dword(buf + 12);
    w_log(LL_FUNC, "read_jamlread() OK");
    return 1;
}

int write_jamlread(int fd, JAMLREAD * plread)
{
    unsigned char buf[JAMLREAD_SIZE];

    w_log(LL_FUNC, "write_jamlread() begin");
    put_dword(buf, plread->UserCRC);
    put_dword(buf + 4, plread->UserID);
    put_dword(buf + 8, plread->LastReadMsg);
    put_dword(buf + 12, plread->HighReadMsg);

    if(write(fd, buf, JAMLREAD_SIZE) != JAMLREAD_SIZE)
    {
        w_log(LL_ERR, "write_jamlread() error: %s", strerror(errno));
        w_log(LL_FUNC, "write_jamlread() failed");
        return 0;
    }

    w_log(LL_FUNC, "write_jamlread() OK");
    return 1;
}

int write_partial_jamlread(int fd, JAMLREAD * plread)
{
    unsigned char buf[JAMLREAD_SIZE / 2];

    w_log(LL_FUNC, "write_partial_jamlread() begin");
    put_dword(buf + 0, plread->LastReadMsg);
    put_dword(buf + 4, plread->HighReadMsg);

    if(write(fd, buf, JAMLREAD_SIZE / 2) != JAMLREAD_SIZE / 2)
    {
        w_log(LL_ERR, "write_partial_jamlread() error: %s", strerror(errno));
        w_log(LL_FUNC, "write_partial_jamlread() failed");
        return 0;
    }

    w_log(LL_FUNC, "write_partial_jamlread() OK");
    return 1;
}

void JamReadLastreadFile(char * fileName, UINT32 ** lastreadp, ULONG * lcountp, HAREA area)
{
    int fd;
    struct stat st;
    unsigned long i;
    char * name = NULL;
    JAMLREAD lread;

    w_log(LL_FUNC, "JamReadLastreadFile() begin");
    xstrscat(&name, fileName, ".jlr", NULL);
    fd = sopen(name, O_BINARY | O_RDWR, SH_DENYNO, S_IWRITE | S_IREAD);

    if(fd != -1)
    {
        fstat(fd, &st);
        *lcountp   = st.st_size / JAMLREAD_SIZE;
        *lastreadp = (UINT32 *)malloc(*lcountp * sizeof(UINT32) * 2);

        for(i = 0; i < *lcountp; i++)
        {
            read_jamlread(fd, &lread);
            (*lastreadp)[i * 2]     = MsgUidToMsgn(area, lread.LastReadMsg, UID_PREV);
            (*lastreadp)[i * 2 + 1] = MsgUidToMsgn(area, lread.HighReadMsg, UID_PREV);
        }
        close(fd);
    }
    else
    {
        w_log(LL_ERR, "JamReadLastreadFile(): can't open %s: %s", name, strerror(errno));
        *lastreadp = NULL;
        *lcountp   = 0;
    }

    *lcountp = (*lcountp) << 1; /* rest of sqpack does not now of 2 lastread ptrs */

    nfree(name);
    w_log(LL_FUNC, "JamReadLastreadFile() end");
} /* JamReadLastreadFile */

void JamWriteLastreadFile(char * fileName, UINT32 * lastread, ULONG lcount, HAREA area)
{
    char * name = NULL;
    int fd;
    unsigned long i;
    JAMLREAD lread;

    w_log(LL_FUNC, "JamWriteLastreadFile() begin");

    if(lastread)
    {
        xstrscat(&name, fileName, ".jlr", NULL);
        fd = sopen(name, O_BINARY | O_RDWR, SH_DENYNO, S_IWRITE | S_IREAD);

        if(fd != -1)
        {
            for(i = 0; i < (lcount >> 1); i++)
            {
                lread.LastReadMsg = MsgMsgnToUid(area, lastread[i * 2]);
                lread.HighReadMsg = MsgMsgnToUid(area, lastread[i * 2 + 1]);
                lseek(fd, i * JAMLREAD_SIZE + JAMLREAD_SIZE / 2, SEEK_SET);
                write_partial_jamlread(fd, &lread);
            }
            close(fd);
        }
        else
        {
            w_log(LL_ERR, "JamWriteLastreadFile(): can't open %s: %s", name, strerror(errno));
        }

        nfree(name);
    }

    w_log(LL_FUNC, "JamWriteLastreadFile() end");
} /* JamWriteLastreadFile */

void SdmReadLastreadFile(char * fileName, UINT32 ** lastreadp, ULONG * lcountp, HAREA area)
{
    int fd;
    struct stat st;
    unsigned long i;
    char * name;
    UINT16 temp;

    w_log(LL_FUNC, "SdmReadLastreadFile() begin");
    name = (char *)malloc(strlen(fileName) + 9 + 1);
    strcpy(name, fileName);
    Add_Trailing(name, PATH_DELIM);
    strcat(name, "lastread");
    fd = sopen(name, O_BINARY | O_RDWR, SH_DENYNO, S_IWRITE | S_IREAD);

    if(fd != -1)
    {
        fstat(fd, &st);
        *lcountp   = st.st_size / 2; /*sizeof(UINT16)*/
        *lastreadp = (UINT32 *)malloc(*lcountp * sizeof(UINT32));

        for(i = 0; i < *lcountp; i++)
        {
            read(fd, &temp, 2);
            (*lastreadp)[i] = MsgUidToMsgn(area, temp, UID_PREV);
        }
        close(fd);
    }
    else
    {
        w_log(LL_ERR, "SdmReadLastreadFile(): can't open %s: %s", name, strerror(errno));
        *lastreadp = NULL;
        *lcountp   = 0;
    }

    nfree(name);
    w_log(LL_FUNC, "SdmReadLastreadFile() end");
} /* SdmReadLastreadFile */

void SdmWriteLastreadFile(char * fileName, UINT32 * lastread, ULONG lcount, HAREA area)
{
    char * name;
    int fd;
    unsigned long i;
    unsigned char buf[2];

    unused(area);
    w_log(LL_FUNC, "SdmWriteLastreadFile() begin");

    if(lastread)
    {
        name = (char *)malloc(strlen(fileName) + 9 + 1);
        strcpy(name, fileName);
        Add_Trailing(name, PATH_DELIM);
        strcat(name, "lastread");
        fd = sopen(name, O_BINARY | O_RDWR, SH_DENYNO, S_IWRITE | S_IREAD);

        if(fd != -1)
        {
            lseek(fd, 0, SEEK_SET);

            for(i = 0; i < lcount; i++)
            {
#if 0 /* Messages renumbered, MsgMsgnToUid() returns old value */
      /* We should reopen area before write lastreads */
                UINT16 temp = (UINT16)MsgMsgnToUid(area, lastread[i]);
                put_word(buf, temp);
#else /* msgns are equial to uids after renumber */
                put_word(buf, (UINT16)lastread[i]);
#endif
                write(fd, buf, 2);
            }
            close(fd);
        }
        else
        {
            w_log(LL_ERR, "SdmWriteLastreadFile(): can't open %s: %s", name, strerror(errno));
        }

        nfree(name);
    }

    w_log(LL_FUNC, "SdmWriteLastreadFile() end");
#ifdef __WATCOMC__
    area = area; /* prevent warning */
#endif
} /* SdmWriteLastreadFile */

void readLastreadFile(char * fileName,
                      UINT32 ** lastreadp,
                      ULONG * lcountp,
                      HAREA area,
                      int areaType)
{
    w_log(LL_FUNC, "readLastreadFile() begin");

    if(areaType == MSGTYPE_SQUISH)
    {
        SqReadLastreadFile(fileName, lastreadp, lcountp, area);
    }
    else if(areaType == MSGTYPE_JAM)
    {
        JamReadLastreadFile(fileName, lastreadp, lcountp, area);
    }
    else if(areaType == MSGTYPE_SDM)
    {
        SdmReadLastreadFile(fileName, lastreadp, lcountp, area);
    }

    w_log(LL_FUNC, "readLastreadFile() end");
}

void writeLastreadFile(char * fileName, UINT32 * lastreadp, ULONG lcount, HAREA area, int areaType)
{
    w_log(LL_FUNC, "writeLastreadFile() begin");

    if(areaType == MSGTYPE_SQUISH)
    {
        SqWriteLastreadFile(fileName, lastreadp, lcount, area);
    }
    else if(areaType == MSGTYPE_JAM)
    {
        JamWriteLastreadFile(fileName, lastreadp, lcount, area);
    }
    else if(areaType == MSGTYPE_SDM)
    {
        SdmWriteLastreadFile(fileName, lastreadp, lcount, area);
    }

    w_log(LL_FUNC, "writeLastreadFile() end");
}

unsigned long getOffsetInLastread(UINT32 * lastread, ULONG lcount, dword msgnum)
{
    unsigned long i;

    for(i = 0; i < lcount; i++)
    {
        if(lastread[i] == msgnum)
        {
            return i;
        }
    }
    return (unsigned long)(-1);
}

/* returns zero if msg was killed, nonzero if it was copied */
int processMsg(dword msgNum,
               dword numMsg,
               HAREA oldArea,
               HAREA newArea,
               s_area * area,
               UINT32 shift)
{
    HMSG msg, newMsg;
    XMSG xmsg;
    struct tm tmTime;
    time_t ttime, actualTime = time(NULL);
    char * text, * ctrlText;
    dword textLen, ctrlLen;
    int unsent, i, rc = 0;
    unsigned long uid2msgn;

    /*    unsigned long offset; */
    w_log(LL_FUNC, "processMsg() begin");
    msg = MsgOpenMsg(oldArea, MOPEN_RW, msgNum);

    if(msg == NULL)
    {
        return rc;
    }

    if(MsgReadMsg(msg, &xmsg, 0, 0, NULL, 0, NULL) == (dword) - 1l)
    {
        MsgCloseMsg(msg);
        msgProcessed++;
        return rc;
    }

    unsent = ((xmsg.attr & MSGLOCAL) && !(xmsg.attr & MSGSENT)) || (xmsg.attr & MSGLOCKED);

    if(unsent ||
       (((area->max == 0) || ((numMsg - msgProcessed + msgCopied) <= area->max) ||
         (area->keepUnread && !(xmsg.attr & MSGREAD))) &&
        !((xmsg.attr & MSGREAD) && area->killRead)))
    {
        /*only max msgs should be in new area*/
        if(xmsg.attr & MSGLOCAL)
        {
            DosDate_to_TmDate((SCOMBO *)&(xmsg.date_written), &tmTime);
        }
        else
        {
            DosDate_to_TmDate((SCOMBO *)&(xmsg.date_arrived), &tmTime);
        }

        /*     DosDate_to_TmDate(&(xmsg.attr & MSGLOCAL ? xmsg.date_written :
           xmsg.date_arrived), &tmTime);*/
        ttime = mktime(&tmTime);

        if(ttime == 0xfffffffflu)
        {
            ttime = 0;                        /* emx */
        }

        if(unsent || (area->purge == 0) || ttime == 0 ||
           (labs(actualTime - ttime) <= (long)(area->purge * 24 * 60 * 60)))
        {
            uid2msgn     = MsgUidToMsgn(oldArea, xmsg.replyto, UID_EXACT);
            xmsg.replyto = uid2msgn > shift ? uid2msgn - shift : 0;

            if((area->msgbType & MSGTYPE_SQUISH) == MSGTYPE_SQUISH)
            {
                for(i = 0; i < MAX_REPLY; i++)
                {
                    uid2msgn        = MsgUidToMsgn(oldArea, xmsg.replies[i], UID_EXACT);
                    xmsg.replies[i] = uid2msgn > shift ? uid2msgn - shift : 0;
                }
            }
            else
            {
                uid2msgn         = MsgUidToMsgn(oldArea, xmsg.replies[0], UID_EXACT);
                xmsg.replies[0]  = uid2msgn > shift ? uid2msgn - shift : 0;
                uid2msgn         = MsgUidToMsgn(oldArea, xmsg.xmreplynext, UID_EXACT);
                xmsg.xmreplynext = uid2msgn > shift ? uid2msgn - shift : 0;
            }

            /*  copy msg */
            textLen           = MsgGetTextLen(msg);
            ctrlLen           = MsgGetCtrlLen(msg);
            text              = (char *)malloc(textLen + 1);
            text[textLen]     = '\0';
            ctrlText          = (char *)malloc(ctrlLen + 1);
            ctrlText[ctrlLen] = '\0';
            MsgReadMsg(msg, NULL, 0, textLen, (byte *)text, ctrlLen, (byte *)ctrlText);

            if(area->msgbType & MSGTYPE_SDM)
            {
                MsgWriteMsg(msg,
                            0,
                            &xmsg,
                            (byte *)text,
                            textLen,
                            textLen,
                            ctrlLen,
                            (byte *)ctrlText);
            }
            else
            {
                newMsg = MsgOpenMsg(newArea, MOPEN_CREATE, 0);
                MsgWriteMsg(newMsg,
                            0,
                            &xmsg,
                            (byte *)text,
                            textLen,
                            textLen,
                            ctrlLen,
                            (byte *)ctrlText);
                MsgCloseMsg(newMsg);
            }

            msgCopied++;
            nfree(text);
            nfree(ctrlText);
            rc = 1;
        }
    }

    MsgCloseMsg(msg);
    msgProcessed++;
    w_log(LL_FUNC, "processMsg() end");
    return rc;
} /* processMsg */

UINT32 getShiftedNum(UINT32 msgNum, UINT32 rmCount, UINT32 * rmMap)
{
    UINT32 i, nMsgNum = msgNum;

    if(*rmMap == 1)
    {
        rmMap   += 2;
        rmCount -= 2;
    }

    for(i = 0; i < rmCount; i += 2)
    {
        if(msgNum < rmMap[i])
        {
            break;
        }

        if(msgNum >= rmMap[i] + rmMap[i + 1])
        {
            nMsgNum -= rmMap[i + 1];
        }
        else
        {
            return 0L;
        }
    }
    return nMsgNum;
} /* getShiftedNum */

void updateMsgLinks(UINT32 msgNum, HAREA area, UINT32 rmCount, UINT32 * rmMap, int areaType)
{
    HMSG msg;
    XMSG xmsg;
    int i;

    w_log(LL_FUNC, "updateMsgLinks() begin");
    msg = MsgOpenMsg(area, MOPEN_RW, getShiftedNum(msgNum, rmCount, rmMap));

    if(msg == NULL)
    {
        return;
    }

    MsgReadMsg(msg, &xmsg, 0, 0, NULL, 0, NULL);
    xmsg.replyto = getShiftedNum(xmsg.replyto, rmCount, rmMap);

    if((areaType & MSGTYPE_SQUISH) == MSGTYPE_SQUISH)
    {
        for(i = 0; i < MAX_REPLY; i++)
        {
            xmsg.replies[i] = getShiftedNum(xmsg.replies[i], rmCount, rmMap);
        }
    }
    else
    {
        xmsg.replies[0]  = getShiftedNum(xmsg.replies[0], rmCount, rmMap);
        xmsg.xmreplynext = getShiftedNum(xmsg.xmreplynext, rmCount, rmMap);
    }

    MsgWriteMsg(msg, 0, &xmsg, NULL, 0, 0, 0, NULL);
    MsgCloseMsg(msg);
    w_log(LL_FUNC, "updateMsgLinks() end");
} /* updateMsgLinks */

int renameArea(int areaType, char * oldName, char * newName)
{
    char * oldTmp = NULL, * newTmp = NULL;
    unsigned long oldsize = 0, newsize = 0;
    struct stat sb;

    w_log(LL_FUNC, "renameArea() begin");
    xstrcat(&oldTmp, oldName);
    xstrcat(&newTmp, newName);

    if(areaType == MSGTYPE_SQUISH)
    {
        xstrcat(&oldTmp, ".sqd");
        xstrcat(&newTmp, ".sqd");
        /* sizes of files: for statistics */
        stat(oldTmp, &sb);
        oldsize += sb.st_size;
        stat(newTmp, &sb);
        newsize += sb.st_size;
        remove(oldTmp);
        rename(newTmp, oldTmp);
        oldTmp[strlen(oldTmp) - 1] = 'i';
        newTmp[strlen(newTmp) - 1] = 'i';
        /* sizes of files: for statistics */
        stat(oldTmp, &sb);
        oldsize += sb.st_size;
        stat(newTmp, &sb);
        newsize += sb.st_size;

        if(remove(oldTmp))
        {
            return errno;
        }

        if(rename(newTmp, oldTmp))
        {
            return errno;
        }
    }

    if(areaType == MSGTYPE_JAM)
    {
        xstrcat(&oldTmp, ".jdt");
        xstrcat(&newTmp, ".jdt");
        /* sizes of files: for statistics */
        stat(oldTmp, &sb);
        oldsize += sb.st_size;
        stat(newTmp, &sb);
        newsize += sb.st_size;

        if(remove(oldTmp))
        {
            return errno;
        }

        if(rename(newTmp, oldTmp))
        {
            return errno;
        }

        oldTmp[strlen(oldTmp) - 1] = 'x';
        newTmp[strlen(newTmp) - 1] = 'x';
        /* sizes of files: for statistics */
        stat(oldTmp, &sb);
        oldsize += sb.st_size;
        stat(newTmp, &sb);
        newsize += sb.st_size;

        if(remove(oldTmp))
        {
            return errno;
        }

        if(rename(newTmp, oldTmp))
        {
            return errno;
        }

        oldTmp[strlen(oldTmp) - 2] = 'h';
        newTmp[strlen(newTmp) - 2] = 'h';
        oldTmp[strlen(oldTmp) - 1] = 'r';
        newTmp[strlen(newTmp) - 1] = 'r';
        /* sizes of files: for statistics */
        stat(oldTmp, &sb);
        oldsize += sb.st_size;
        stat(newTmp, &sb);
        newsize += sb.st_size;

        if(remove(oldTmp))
        {
            return errno;
        }

        if(rename(newTmp, oldTmp))
        {
            return errno;
        }

/*
        newTmp[strlen(newTmp)-2] = 'l';
        oldTmp[strlen(oldTmp)-2] = 'l';

        if (remove(oldTmp))
            return errno;
        if (rename(newTmp, oldTmp))
            return errno;
        if (remove(newTmp))
            return errno;
 */
    }

    w_log(LL_STAT, "      old size:%10lu; new size:%10lu", oldsize, newsize);
    totalOldBaseSize += oldsize, totalNewBaseSize += newsize;
    nfree(oldTmp);
    nfree(newTmp);
    w_log(LL_FUNC, "renameArea() end");
    return 0;
} /* renameArea */

void purgeArea(s_area * area)
{
    char * oldName = area->fileName;
    char * newName = NULL;
    HAREA oldArea = NULL, newArea = NULL;
    dword i, j, numMsg, hw = 0;
    int areaType = area->msgbType & (MSGTYPE_JAM | MSGTYPE_SQUISH | MSGTYPE_SDM);
    UINT32 * oldLastread, * newLastread = 0;
    UINT32 * removeMap;
    UINT32 rmIndex = 0;

    w_log(LL_FUNC, "purgeArea() begin");

    if(area->nopack)
    {
        printf("   No purging needed!\n");
        w_log(LL_FUNC, "purgeArea() end");
        return;
    }

    /* generated tmp-FileName */
#ifdef __DOS__
    xstrscat(&newName, "_sqpktmp", NULL);
#else
    xstrscat(&newName, oldName, "_tmp", NULL);
#endif

    oldArea = MsgOpenArea((byte *)oldName, MSGAREA_NORMAL, (word)areaType);

    if(oldArea)
    {
        if(areaType == MSGTYPE_SDM)
        {
            newArea = oldArea;
        }
        else
        {
            newArea = MsgOpenArea((byte *)newName, MSGAREA_CREATE, (word)areaType);
        }
    }

    if((oldArea != NULL) && (newArea != NULL))
    {
        ULONG lcount;
        MsgLock(oldArea);
        numMsg  = MsgGetNumMsg(oldArea);

        if(areaType != MSGTYPE_SDM)
        {
            hw = MsgGetHighWater(oldArea);
        }

        readLastreadFile(oldName, &oldLastread, &lcount, oldArea, areaType);

        if(oldLastread)
        {
            newLastread = (UINT32 *)malloc(lcount * sizeof(UINT32));
            memcpy(newLastread, oldLastread, lcount * sizeof(UINT32));
        }

        removeMap = (UINT32 *)calloc(2, sizeof(UINT32));

        for(i = j = 1; i <= numMsg; i++, j++)
        {
            if(!processMsg(j, numMsg, oldArea, newArea, area,
                           removeMap[0] == 1 ? removeMap[1] : 0))
            {
                if(!(rmIndex & 1))
                {
                    /* We started to delete new portion of */
                    removeMap =
                        (UINT32 *)realloc(removeMap, (rmIndex + 2) * sizeof(UINT32));
                    removeMap[rmIndex++] = i;
                    removeMap[rmIndex]   = 0;
                }

                removeMap[rmIndex]++; /* Anyway, update counter */

                if(areaType == MSGTYPE_SDM)
                {
                    MsgKillMsg(oldArea, j--);
                }
            }
            else
            {
                /* We are copying msgs */
                if(rmIndex & 1)
                {
                    rmIndex++;
                }
            }
        }

        if(rmIndex > 2 || removeMap[0] > 1)
        {
            for(i = 1; i <= numMsg; i++)
            {
                updateMsgLinks(i, newArea, rmIndex, removeMap, areaType);
            }
        }

        if(areaType == MSGTYPE_SDM)
        {
            /* renumber the area */
            /* TODO: update replylinks */
            char oldmsgname[PATHLEN], newmsgname[PATHLEN];
            size_t pathlen, oldlen;
            oldlen = strlen(oldName);
            if(oldlen > PATHLEN - 2)
            {
                oldName[PATHLEN - 2] = '\0';
                oldlen = PATHLEN - 2;
            }
            numMsg = MsgGetNumMsg(oldArea);
            strncpy(oldmsgname, oldName, oldlen);
            Add_Trailing(oldmsgname, PATH_DELIM);
            pathlen = strlen(oldmsgname);
            strncpy(newmsgname, oldmsgname, pathlen);

            for(i = 1; i <= numMsg; i++)
            {
                j = MsgMsgnToUid(oldArea, i);

                if(i == j)
                {
                    continue;
                }

                sprintf(oldmsgname + pathlen, "%u.msg", (unsigned int)j);
                sprintf(newmsgname + pathlen, "%u.msg", (unsigned int)i);
                rename(oldmsgname, newmsgname);
            }
        }

        if(rmIndex)    /* someting was removed, maybe need to update lastreadfile */
        {
            for(j = 0; j < lcount; j++)
            {
                for(i = 0; i < rmIndex; i += 2)
                {
                    if(oldLastread[j] >= removeMap[i])
                    {
                        if(oldLastread[j] >= removeMap[i] + removeMap[i + 1])
                        {
                            newLastread[j] -= removeMap[i + 1];
                        }
                        else
                        {
                            newLastread[j] -= oldLastread[j] - removeMap[i] + 1;
                        }
                    }
                }
            }
        }

        writeLastreadFile(oldName, newLastread, lcount, newArea, areaType);
        MsgUnlock(oldArea);
        MsgCloseArea(oldArea);

        if(areaType != MSGTYPE_SDM)
        {
            if((numMsg - msgCopied) > hw)
            {
                hw = 0;
            }
            else
            {
                hw -= (numMsg - msgCopied);
            }

            MsgSetHighWater(newArea, hw);
            MsgCloseArea(newArea);
        }

        w_log(LL_STAT, "      old  msg:%10lu; new  msg:%10lu", (unsigned long)numMsg, msgCopied);
        totaloldMsg    += numMsg;
        totalmsgCopied += msgCopied;                    /*  total */
        nfree(oldLastread);
        nfree(newLastread);

        /* rename oldArea to newArea */
        if(renameArea(areaType, oldName, newName))
        {
            w_log(LL_ERR, "Couldn't rename message base %s to %s: %s!", oldName, newName,
                  strerror(errno));
        }
    }
    else
    {
        if(oldArea)
        {
            MsgCloseArea(oldArea);

            if(areaType & MSGTYPE_SDM)
            {
                w_log(LL_ERR, "Could not create '%s%c*.msg'!", newName, PATH_DELIM);
            }
            else
            {
                w_log(LL_ERR, "Could not create '%s.*'!", newName);
            }
        }
        else
        {
            if(areaType & MSGTYPE_SDM)
            {
                w_log(LL_ERR, "Could not open '%s%c*.msg'!", oldName, PATH_DELIM);
            }
            else
            {
                w_log(LL_ERR, "Could not open '%s.*'!", oldName);
            }
        }
    }

    nfree(newName);
    w_log(LL_FUNC, "purgeArea() end");
} /* purgeArea */

void handleArea(s_area * area)
{
    ULONG freeSpace = 0;
    int process     = 1;

    area_found = 1;
    w_log(LL_FUNC, "handleArea() begin");

    if((area->msgbType & MSGTYPE_SQUISH) == MSGTYPE_SQUISH ||
       (area->msgbType & MSGTYPE_JAM) == MSGTYPE_JAM ||
       (area->msgbType & MSGTYPE_SDM) == MSGTYPE_SDM)
    {
        struct stat sb;
        ULONG baseSize    = 0;
        char * msgBaseDir = sstrdup(area->fileName);
        char * p          = strrchr(msgBaseDir, PATH_DELIM);

        if(p)
        {
            *p = '\0';
        }

        freeSpace = husky_GetDiskFreeSpace(msgBaseDir);

        if(p)
        {
            *p = PATH_DELIM;
        }

        if((area->msgbType & MSGTYPE_SQUISH) == MSGTYPE_SQUISH)
        {
            xstrcat(&msgBaseDir, ".sqd");
            memset(&sb, 0, sizeof(sb));
            stat(msgBaseDir, &sb);
            baseSize += sb.st_size;
            msgBaseDir[strlen(msgBaseDir) - 1] = 'i';
            memset(&sb, 0, sizeof(sb));
            stat(msgBaseDir, &sb);
            baseSize += sb.st_size;
        }

        if((area->msgbType & MSGTYPE_JAM) == MSGTYPE_JAM)
        {
            xstrcat(&msgBaseDir, ".jdt");
            memset(&sb, 0, sizeof(sb));
            stat(msgBaseDir, &sb);
            baseSize += sb.st_size;
            msgBaseDir[strlen(msgBaseDir) - 1] = 'x';
            memset(&sb, 0, sizeof(sb));
            stat(msgBaseDir, &sb);
            baseSize += sb.st_size;
            msgBaseDir[strlen(msgBaseDir) - 2] = 'h';
            msgBaseDir[strlen(msgBaseDir) - 1] = 'r';
            memset(&sb, 0, sizeof(sb));
            stat(msgBaseDir, &sb);
            baseSize += sb.st_size;
        }

        baseSize /= 1024; /* convert to Kbytes */

        if(baseSize >= freeSpace && config->minDiskFreeSpace != 0)
        {
            process = 0;
        }

        if(process)
        {
            w_log(LL_INFO,
                  "Purge area %s (%s)",
                  area->areaName,
                  area->msgbType &
                  MSGTYPE_SQUISH ? "squish" : area->msgbType & MSGTYPE_JAM ? "jam" : area->msgbType & MSGTYPE_SDM ? "msg/OPUS" : "unknown type");
            msgCopied    = 0;
            msgProcessed = 0;
            purgeArea(area);
        }
        else
        {
            w_log(LL_CRIT,
                  "Not enough free space for purge area %s (avaiable %ulK, need %ulK)",
                  area->areaName,
                  freeSpace,
                  baseSize);
        }
    }

    w_log(LL_FUNC, "handleArea() end");
} /* handleArea */

void doArea(s_area * area, char ** areaMasks, int areaMaskCount)
{
    int i;
    int wasInclusion = 0; /* states that there are one or more inclusion masks */

    /* and areas that didn't matched the inclusion mask */
    /* should not be processed */
    if(area)
    {
        /* check for inclusion */
        for(i = 0; i < areaMaskCount; i++)
        {
            if(*areaMasks[i] == '!')
            {
                continue;
            }

            wasInclusion++;

            if(patimat(area->areaName, areaMasks[i]))
            {
                break;
            }
        }

        if(wasInclusion && (i == areaMaskCount))
        {
            return;                                        /* not in inclusion mask */
        }

        /* check for exclusion */
        for(i = 0; i < areaMaskCount; i++)
        {
            if(*areaMasks[i] != '!')
            {
                continue;
            }

            if(patimat(area->areaName, areaMasks[i] + 1))
            {
                break;
            }
        }

        if(i != areaMaskCount)
        {
            return;                      /* is in exclusion mask */
        }

        handleArea(area);
    }
} /* doArea */

int main(int argc, char ** argv)
{
    unsigned int i;
    struct _minf m;
    char * configFile = NULL;
    int areaMaskCount = 0;
    char ** areaMasks = NULL;

#if defined (__NT__)
    SetUnhandledExceptionFilter(&UExceptionFilter);
#endif

    area_found = 0;
    versionStr = GenVersionStr("sqpack", VER_MAJOR, VER_MINOR, VER_PATCH, VER_BRANCH, cvs_date);
    printf("%s\n", versionStr);

    if(argc <= 1)
    {
        printf("sqpack purges messages from squish or jam message bases\n");
        printf("according to -p and -m parameters in EchoArea lines\n");
        printf("Usage: sqpack [-c config] <[!]areamask> [ [!]areamask ... ]\n");
        return 0;
    }

    areaMasks = scalloc(sizeof(char *), argc);
    i         = 0;

    while(i < (unsigned)argc - 1)
    {
        i++;

        if(stricmp(argv[i], "-c") == 0)
        {
            if(i < (unsigned)argc - 1)
            {
                i++;
                configFile = argv[i];
            }
            else
            {
                printf("Error: config filename missing on command line\n");
                return 1;
            }
        }
        else
        {
            areaMasks[areaMaskCount] = argv[i];
            areaMaskCount++;
        }
    }

    if(!areaMaskCount)
    {
        printf("Error: at least one area mask should be specified\n");
        return 1;
    }

    setvar("module", "sqpack");
    config = readConfig(configFile);  /* if config file not specified on command */

    /* line, NULL would be passed. That's ok. */
    if(!config)
    {
        printf("Error: can't read fido config\n");
        return 1;
    }

    if(config->lockfile)
    {
        lock_fd = lockFile(config->lockfile, config->advisoryLock);

        if(lock_fd < 0)
        {
            disposeConfig(config);
            exit(EX_CANTCREAT);
        }
    }

    initLog(config->logFileDir, config->logEchoToScreen, config->loglevels,
            config->screenloglevels);
    openLog(LOGFILE, versionStr);
    w_log(LL_START, "Start");
    m.req_version = 0;
    m.def_zone    = (word)config->addr[0].zone;

    if(MsgOpenApi(&m) != 0)
    {
        w_log(LL_CRIT, "MsgOpenApi Error. Exit.");
        closeLog();

        if(config->lockfile)
        {
            FreelockFile(config->lockfile, lock_fd);
        }

        disposeConfig(config);
        exit(1);
    }

    /* purge dupe area */
    if(config->dupeArea.areaName && config->dupeArea.fileName)
    {
        doArea(&(config->dupeArea), areaMasks, areaMaskCount);
    }

    /* purge bad area  */
    if(config->badArea.areaName && config->badArea.fileName)
    {
        doArea(&(config->badArea), areaMasks, areaMaskCount);
    }

    for(i = 0; i < config->netMailAreaCount; i++)
    {
        /*  purge netmail areas */
        doArea(&(config->netMailAreas[i]), areaMasks, areaMaskCount);
    }

    for(i = 0; i < config->echoAreaCount; i++)
    {
        /*  purge echomail areas */
        doArea(&(config->echoAreas[i]), areaMasks, areaMaskCount);
    }

    for(i = 0; i < config->localAreaCount; i++)
    {
        /*  purge local areas */
        doArea(&(config->localAreas[i]), areaMasks, areaMaskCount);
    }

    if(area_found)
    {
        w_log(LL_SUMMARY, "Total old  msg:%10lu; new  msg:%10lu", (unsigned long)totaloldMsg,
              (unsigned long)totalmsgCopied);
        w_log(LL_SUMMARY,
              "Total old size:%10lu; new size:%10lu",
              (unsigned long)totalOldBaseSize,
              (unsigned long)totalNewBaseSize);
    }
    else
    {
        w_log(LL_WARN, "No areas found");
    }

    w_log(LL_STOP, "End");
    closeLog();

    if(config->lockfile)
    {
        FreelockFile(config->lockfile, lock_fd);
    }

    disposeConfig(config);
    return 0;
} /* main */
