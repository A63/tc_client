#ifdef HAVE_AVFORMAT
extern void* playmedia(void* data);
extern void media_play(const char* args);
extern void media_seek(long int ms);
extern void media_close(void);
#endif
