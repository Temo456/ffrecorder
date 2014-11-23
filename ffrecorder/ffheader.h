#ifndef FFMPEG_HEADERS_H
#define FFMPEG_HEADERS_H
#define snprintf _snprintf

extern "C"
{
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libswscale/swscale.h>
#include <libavutil/opt.h>
#include <libavutil/avassert.h>
#include <libavutil/mathematics.h>
#include <libavutil/timestamp.h>
#include <libavutil/imgutils.h>
#include <libswresample/swresample.h>
}



#ifdef av_ts2str
#undef av_ts2str
#endif // av_ts2str

#ifdef av_ts2timestr
#undef av_ts2timestr
#endif // av_ts2timestr

#ifdef av_err2str
#undef av_err2str
#endif // av_err2str


#pragma comment(lib, "avcodec.lib")
#pragma comment(lib, "avdevice.lib")
#pragma comment(lib, "avformat.lib")
#pragma comment(lib, "avutil.lib")
#pragma comment(lib, "swscale.lib")
#pragma comment(lib, "swresample.lib")

#include <SDL.h>
#include <SDL_thread.h>
#pragma comment(lib, "SDL2.lib")

#endif