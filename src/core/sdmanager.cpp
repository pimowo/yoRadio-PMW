//v0.9.686
#include "options.h"
#if SDC_CS!=255
#include <Arduino.h>
#include <SPI.h>
#include <SD.h>
#include "vfs_api.h"
#include "sd_diskio.h"
//#define USE_SD
#include "config.h"
#include "sdmanager.h"
#include "display.h"
#include "player.h"
#include <ctype.h>

#if defined(SD_SPIPINS) || SD_HSPI
SPIClass  SDSPI(HSPI);
#define SDREALSPI SDSPI
#else
  #define SDREALSPI SPI
#endif

#ifndef SDSPISPEED
  #define SDSPISPEED 20000000
#endif

SDManager sdman(FSImplPtr(new VFSImpl()));

bool SDManager::start(){
  ready = begin(SDC_CS, SDREALSPI, SDSPISPEED);
  vTaskDelay(10);
  if(!ready) ready = begin(SDC_CS, SDREALSPI, SDSPISPEED);
  vTaskDelay(20);
  if(!ready) ready = begin(SDC_CS, SDREALSPI, SDSPISPEED);
  vTaskDelay(50);
  if(!ready) ready = begin(SDC_CS, SDREALSPI, SDSPISPEED);
  return ready;
}

void SDManager::stop(){
  end();
  ready = false;
}
#include "diskio_impl.h"
bool SDManager::cardPresent() {

  if(!ready) return false;
  if(sectorSize()<1) {
    return false;
  }
  uint8_t buff[sectorSize()] = { 0 };
  bool bread = readRAW(buff, 1);
  if(sectorSize()>0 && !bread) return false;
  return bread;
}

bool SDManager::_checkNoMedia(const char* path){
  if (path[strlen(path) - 1] == '/')
    snprintf(config.tmpBuf, sizeof(config.tmpBuf), "%s%s", path, ".nomedia");
  else
    snprintf(config.tmpBuf, sizeof(config.tmpBuf), "%s/%s", path, ".nomedia");
  bool nm = exists(config.tmpBuf);
  return nm;
}

bool SDManager::_endsWith (const char* base, const char* str) {
  int slen = strlen(str) - 1;
  const char *p = base + strlen(base) - 1;
  while(p > base && isspace(*p)) p--;
  p -= slen;
  if (p < base) return false;
  return (strncmp(p, str, slen) == 0);
}

void SDManager::listSD(File &plSDfile, File &plSDindex, const char* dirname, uint8_t levels) {
    File root = sdman.open(dirname);
    if (!root) {
        Serial.println("##[ERROR]#\tFailed to open directory");
        return;
    }
    if (!root.isDirectory()) {
        Serial.println("##[ERROR]#\tNot a directory");
        return;
    }

    uint32_t pos = 0;
    char* filePath;
    while (true) {
        vTaskDelay(2);
        player.loop();
        // Abort if card removed during traversal
        if (!cardPresent()) {
          Serial.println("listSD: SD card removed during listing - aborting");
          break;
        }
        bool isDir;
        String fileName = root.getNextFileName(&isDir);
        if (fileName.isEmpty()) break;
        filePath = (char*)malloc(fileName.length() + 1);
        if (filePath == NULL) {
            Serial.println("Memory allocation failed");
            break;
        }
        strcpy(filePath, fileName.c_str());
        const char* fn = strrchr(filePath, '/') + 1;
        if (isDir) {
            if (levels && !_checkNoMedia(filePath)) {
                listSD(plSDfile, plSDindex, filePath, levels - 1);
            }
        } else {
            if (_endsWith(strlwr((char*)fn), ".mp3") || _endsWith(fn, ".m4a") || _endsWith(fn, ".aac") ||
                _endsWith(fn, ".wav") || _endsWith(fn, ".flac")) {
                pos = plSDfile.position();
                plSDfile.printf("%s\t%s\t0\n", fn, filePath);
                plSDindex.write((uint8_t*)&pos, 4);
                Serial.print(".");
                _sdFCount++;
                if (display.mode() == SDCHANGE && _sdTotalFiles > 0) {
                  int percent = (int)(((_sdFCount * 100ULL) / _sdTotalFiles));
                  if (percent > 100) percent = 100;
                  display.putRequest(SDFILEINDEX, percent);
                }
                if (_sdFCount % 64 == 0) Serial.println();
            }
        }
        free(filePath);
    }
    root.close();
}

void SDManager::indexSDPlaylist() {
  // Single-pass iterative indexing with persisted heuristic count.
  _sdFCount = 0;
  _sdTotalFiles = 0;
  // Abort early if card is not present to avoid SD driver spam
  if (!cardPresent()) {
    Serial.println("indexSDPlaylist: SD card not present - aborting");
    display.putRequest(NEWTITLE);
    display.putRequest(SDFILEINDEX, 0);
    return;
  }

  // Load last known count (heuristic) to estimate progress during this single pass
  _sdLastKnownCount = 0;
  if (exists(_sdCountPath)) {
    File c = open(_sdCountPath, "r");
    if (c) {
      uint32_t v = 0;
      if (c.available() >= 4) {
        c.read((uint8_t*)&v, 4);
        _sdLastKnownCount = v;
      }
      c.close();
    }
  }
  if (_sdLastKnownCount == 0) _sdLastKnownCount = 1; // avoid div by zero

  if (display.mode() == SDCHANGE) {
    display.putRequest(SDFILEINDEX, 0);
  }

  if(exists(PLAYLIST_SD_PATH)) remove(PLAYLIST_SD_PATH);
  if(exists(INDEX_SD_PATH)) remove(INDEX_SD_PATH);
  File playlist = open(PLAYLIST_SD_PATH, "w", true);
  if (!playlist) {
    Serial.println("indexSDPlaylist: failed to open playlist file");
    display.putRequest(NEWTITLE);
    display.putRequest(SDFILEINDEX, 0);
    return;
  }
  File index = open(INDEX_SD_PATH, "w", true);
  if (!index) {
    Serial.println("indexSDPlaylist: failed to open index file");
    playlist.close();
    display.putRequest(NEWTITLE);
    display.putRequest(SDFILEINDEX, 0);
    return;
  }

  // Iterative DFS using a stack of directory paths.
  const int MAX_PATH = 256;
  const int MAX_STACK = 64;
  char stack[MAX_STACK][MAX_PATH];
  int sp = 0;
  strncpy(stack[sp++], "/", MAX_PATH - 1);

  while (sp > 0) {
    vTaskDelay(2);
    player.loop();
    if (!cardPresent()) {
      Serial.println("indexSDPlaylist: SD card removed during indexing - aborting");
      break;
    }
    char dirpath[MAX_PATH];
    sp--;
    strncpy(dirpath, stack[sp], MAX_PATH - 1);
    dirpath[MAX_PATH - 1] = '\0';

    File root = sdman.open(dirpath);
    if (!root) {
      Serial.printf("indexSDPlaylist: failed to open dir %s\n", dirpath);
      continue;
    }
    if (!root.isDirectory()) {
      root.close();
      continue;
    }

    while (true) {
      vTaskDelay(2);
      player.loop();
      if (!cardPresent()) {
        Serial.println("indexSDPlaylist: SD card removed during listing - aborting");
        break;
      }
      bool isDir = false;
      String fname = root.getNextFileName(&isDir);
      if (fname.isEmpty()) break;
      // copy to fixed buffer immediately to minimize String lifetime
      char fnbuf[MAX_PATH];
      strncpy(fnbuf, fname.c_str(), MAX_PATH - 1);
      fnbuf[MAX_PATH - 1] = '\0';

      if (isDir) {
        if (sp < MAX_STACK - 1) {
          // check .nomedia and push
          if (!_checkNoMedia(fnbuf)) {
            strncpy(stack[sp++], fnbuf, MAX_PATH - 1);
            stack[sp - 1][MAX_PATH - 1] = '\0';
          }
        }
      } else {
        const char* fn = strrchr(fnbuf, '/') ? strrchr(fnbuf, '/') + 1 : fnbuf;
        char tmpfn[128];
        strncpy(tmpfn, fn, sizeof(tmpfn) - 1);
        tmpfn[sizeof(tmpfn) - 1] = '\0';
        // lowercase in place
        for (char* p = tmpfn; *p; ++p) *p = tolower(*p);
        if (_endsWith(tmpfn, ".mp3") || _endsWith(tmpfn, ".m4a") || _endsWith(tmpfn, ".aac") ||
            _endsWith(tmpfn, ".wav") || _endsWith(tmpfn, ".flac")) {
          uint32_t pos = playlist.position();
          // write filename and full path
          playlist.printf("%s\t%s\t0\n", fn, fnbuf);
          index.write((uint8_t*)&pos, 4);
          Serial.print(".");
          _sdFCount++;
          if (_sdFCount % 64 == 0) Serial.println();
          if (display.mode() == SDCHANGE && _sdLastKnownCount > 0) {
            int percent = (int)(((_sdFCount * 100ULL) / _sdLastKnownCount));
            if (percent > 100) percent = 100;
            display.putRequest(SDFILEINDEX, percent);
          }
        }
      }
      // String fname freed at end of scope
    }
    root.close();
  }

  // Ensure final progress update
  if (display.mode() == SDCHANGE) {
    display.putRequest(SDFILEINDEX, 100);
  }

  // persist actual count for next run heuristic
  if (_sdFCount > 0) {
    File c = open(_sdCountPath, "w");
    if (c) {
      uint32_t v = _sdFCount;
      c.write((const uint8_t*)&v, 4);
      c.flush();
      c.close();
    }
  }

  index.flush();
  index.close();
  playlist.flush();
  playlist.close();
  Serial.println();
  delay(50);
}
#include <ctype.h>

uint32_t SDManager::countSDFiles(const char * dirname, uint8_t levels) {
  uint32_t total = 0;
  File root = sdman.open(dirname);
  if (!root) {
    return 0;
  }
  if (!root.isDirectory()) {
    root.close();
    return 0;
  }
  while (true) {
    vTaskDelay(2);
    player.loop();
    // Abort if SD removed during counting
    if (!cardPresent()) {
      Serial.println("countSDFiles: SD card removed during counting - aborting");
      break;
    }
    bool isDir;
    String fileName = root.getNextFileName(&isDir);
    if (fileName.isEmpty()) break;
    if (isDir) {
      if (levels) {
        // fileName already contains full path returned by getNextFileName
        if (!_checkNoMedia(fileName.c_str())) {
          total += countSDFiles(fileName.c_str(), levels - 1);
        }
      }
    } else {
      const char* fn_full = fileName.c_str();
      const char* fn = strrchr(fn_full, '/') ? strrchr(fn_full, '/') + 1 : fn_full;
      char tmpfn[64];
      strncpy(tmpfn, fn, sizeof(tmpfn) - 1);
      tmpfn[sizeof(tmpfn) - 1] = '\0';
      strlwr(tmpfn);
      if (_endsWith(tmpfn, ".mp3") || _endsWith(tmpfn, ".m4a") || _endsWith(tmpfn, ".aac") || _endsWith(tmpfn, ".wav") || _endsWith(tmpfn, ".flac")) {
        total++;
      }
    }
  }
  root.close();
  return total;
}
#endif

