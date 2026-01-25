#ifndef sdmanager_h
#define sdmanager_h

class SDManager : public SDFS {
  public:
    bool ready;
  public:
    SDManager(FSImplPtr impl) : SDFS(impl) {}
    bool start();
    void stop();
    bool cardPresent();
    void listSD(File &plSDfile, File &plSDindex, const char * dirname, uint8_t levels);
    void indexSDPlaylist();
    uint32_t countSDFiles(const char * dirname, uint8_t levels);
  private:
    uint32_t _sdFCount;
    uint32_t _sdTotalFiles;
    uint32_t _sdLastKnownCount;
    const char* _sdCountPath = "/data/sdcount.dat";
  private:
    bool _checkNoMedia(const char* path);
    bool _endsWith (const char* base, const char* str);
};

extern SDManager sdman;
#if defined(SD_SPIPINS) || SD_HSPI
extern SPIClass  SDSPI;
#endif
#endif
