/* ScummVM - Scumm Interpreter
 * Dreamcast port
 * Copyright (C) 2002  Marcus Comstedt
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.

 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.

 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 *
 * $Header$
 *
 */

#include "stdafx.h"
#include "scumm.h"
#include "dc.h"
#include "icon.h"


enum vmsaveResult {
  VMSAVE_OK,
  VMSAVE_NOVM,
  VMSAVE_NOSPACE,
  VMSAVE_WRITEERROR,  
};


static int lastvm=-1;

static vmsaveResult trySave(Scumm *s, const char *data, int size,
			    const char *filename, class Icon &icon, int vm)
{
  struct vmsinfo info;
  struct superblock super;
  struct vms_file file;
  struct vms_file_header header;
  struct timestamp tstamp;
  struct tm tm;
  time_t t;
  unsigned char iconbuffer[512+32];

  if(!vmsfs_check_unit(vm, 0, &info))
    return VMSAVE_NOVM;
  if(!vmsfs_get_superblock(&info, &super))
    return VMSAVE_NOVM;
  int free_cnt = vmsfs_count_free(&super);
  if(vmsfs_open_file(&super, filename, &file))
    free_cnt += file.blks;
  if(((128+512+size+511)>>9) > free_cnt)
    return VMSAVE_NOSPACE;

  memset(&header, 0, sizeof(header));
  strncpy(header.shortdesc, "ScummVM savegame", 16);
  char *game_name = s->getGameName();
  strncpy(header.longdesc, game_name, 32);
  free(game_name);
  strncpy(header.id, "ScummVM", 16);
  icon.create_vmicon(iconbuffer);
  header.numicons = 1;
  memcpy(header.palette, iconbuffer, sizeof(header.palette));
  time(&t);
  tm = *localtime(&t);
  tstamp.year = tm.tm_year+1900;
  tstamp.month = tm.tm_mon+1;
  tstamp.day = tm.tm_mday;
  tstamp.hour = tm.tm_hour;
  tstamp.minute = tm.tm_min;
  tstamp.second = tm.tm_sec;
  tstamp.wkday = (tm.tm_wday+6)%7;

  vmsfs_beep(&info, 1);

  vmsfs_errno = 0;
  if(!vmsfs_create_file(&super, filename, &header,
			iconbuffer+sizeof(header.palette), NULL,
			data, size, &tstamp)) {
    fprintf(stderr, "%s\n", vmsfs_describe_error());
    vmsfs_beep(&info, 0);
    return VMSAVE_WRITEERROR;
  }

  vmsfs_beep(&info, 0);
  return VMSAVE_OK;
}

static bool tryLoad(char *&buffer, int &size, const char *filename, int vm)
{
  struct vmsinfo info;
  struct superblock super;
  struct vms_file file;
  struct vms_file_header header;
  struct timestamp tstamp;
  struct tm tm;
  time_t t;
  unsigned char iconbuffer[512+32];

  if(!vmsfs_check_unit(vm, 0, &info))
    return false;
  if(!vmsfs_get_superblock(&info, &super))
    return false;
  if(!vmsfs_open_file(&super, filename, &file))
    return false;
  
  buffer = new char[size = file.size];

  if(vmsfs_read_file(&file, (unsigned char *)buffer, size))
    return true;

  delete buffer;
  return false;
}

vmsaveResult writeSaveGame(Scumm *s, const char *data, int size,
			   const char *filename, class Icon &icon)
{
  vmsaveResult r, res = VMSAVE_NOVM;

  if(lastvm >= 0 &&
     (res = trySave(s, data, size, filename, icon, lastvm)) == VMSAVE_OK)
    return res;

  for(int i=0; i<24; i++)
    if((r = trySave(s, data, size, filename, icon, i)) == VMSAVE_OK) {
      lastvm = i;
      return r;
    } else if(r > res)
      res = r;

  return res;
}

bool readSaveGame(char *&buffer, int &size, const char *filename)
{
  if(lastvm >= 0 &&
     tryLoad(buffer, size, filename, lastvm))
    return true;

  for(int i=0; i<24; i++)
    if(tryLoad(buffer, size, filename, i)) {
      lastvm = i;
      return true;
    }

  return false;
}


struct vmStreamContext {
  bool issave;
  char *buffer;
  int pos, size;
  char filename[16];
};

bool SerializerStream::fopen(const char *filename, const char *mode)
{
  vmStreamContext *c = new vmStreamContext;
  context = c;
  if(strchr(mode, 'w')) {
    c->issave = true;
    strncpy(c->filename, filename, 16);
    c->pos = 0;
    c->buffer = new char[c->size = 128*1024];
    return true;
  } else if(readSaveGame(c->buffer, c->size, filename)) {
    c->issave = false;
    c->pos = 0;
    return true;
  } else {
    delete c;
    context = NULL;
    return false;
  }
}

void SerializerStream::fclose()
{
  extern Scumm scumm;
  extern Icon icon;

  if(context) {
    vmStreamContext *c = (vmStreamContext *)context;
    if(c->issave)
      writeSaveGame(&scumm, c->buffer, c->pos,
		    c->filename, icon);
    delete c->buffer;
    delete c;
    context = NULL;
  }
}

int SerializerStream::fread(void *buf, int size, int cnt)
{
  vmStreamContext *c = (vmStreamContext *)context;

  if (!c || c->issave)
    return -1; 

  int nbyt = size*cnt;
  if (c->pos + nbyt > c->size) {
    cnt = (c->size - c->pos)/size;
    nbyt = size*cnt;
  }
  if (nbyt)
    memcpy(buf, c->buffer + c->pos, nbyt);
  c->pos += nbyt;
  return cnt;
}

int SerializerStream::fwrite(void *buf, int size, int cnt)
{
  vmStreamContext *c = (vmStreamContext *)context;

  if (!c || !c->issave)
    return -1;

  int nbyt = size*cnt;
  if (c->pos + nbyt > c->size) {
    cnt = (c->size - c->pos)/size;
    nbyt = size*cnt;
  }
  if (nbyt)
    memcpy(c->buffer + c->pos, buf, nbyt);
  c->pos += nbyt;
  return cnt;
}

