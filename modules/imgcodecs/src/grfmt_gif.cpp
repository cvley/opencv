// cvley <ley@hackcv.com>
// Gif Encoder and Decoder support
//

#include "precomp.hpp"
#include "grfmt_gif.hpp"

#ifdef HAVE_GIF

#if defined _WIN32 && defined __GNUC__
typedef unsigned char boolean;
#endif

#undef FALSE
#undef TRUE

extern "C" {
#include <gif_lib.h>
#include <stdio.h>
#include <stdlib.h>
}

#if GIFLIB_MAJOR >= 5
#define MakeMapObject GifMakeMapObject
#define FreeMapObject GifFreeMapObject
#define QuantizeBuffer GifQuantizeBuffer
#define EGifOpen(userPtr, writeFunc) EGifOpen(userPtr, writeFunc, NULL)
#define EGifOpenFileName(name, test) EGifOpenFileName(name, test, NULL)
#define DGifOpenFileName(name) DGifOpenFileName(name, NULL)
#define DGifOpen(userPtr, writeFunc) DGifOpen(userPtr, writeFunc, NULL)
#if GIFLIB_MINOR >= 1
#define EGifCloseFile(gif) EGifCloseFile(gif, NULL)
#define DGifCloseFile(gif) DGifCloseFile(gif, NULL)
#endif
#endif


namespace cv
{

struct GifUserData {
	uchar* source;
	int len;
	int pos;
};

static int gif_read_data(GifFileType* f, GifByteType* buffer, int size) {
	struct GifUserData* user_data = (struct GifUserData*)f->UserData;
	if (size > user_data->len) {
		size = user_data->len;
	}
	memcpy(buffer, user_data->source + user_data->pos, size);
	user_data->pos += size;
	return size;
}

static int gif_write_data(GifFileType* f, const GifByteType* buffer, int size) {
	struct GifUserData* user_data = (struct GifUserData*)f->UserData;
	user_data->len += size;
	user_data->source = (uchar*)realloc(user_data->source, user_data->len);
	memcpy(user_data->source+user_data->pos, buffer, size);
	user_data->pos += size;
	return size;
}

static int vector_to_char(std::vector<uchar>& vec, void** data, int* len) {
	*len = vec.size();
	uchar *c = (uchar*)malloc(vec.size() * sizeof(uchar));
	memcpy(c, vec.data(), vec.size());
	*data = (void*)c;
	return 0;
}

/////////////////////// GifDecoder ///////////////////

GifDecoder::GifDecoder()
{
	m_signature = "GIF";
	m_state = 0;
	m_f = 0;
	m_buf_supported = true;
}

GifDecoder::~GifDecoder()
{
	close();
}

void GifDecoder::close()
{
	GifFileType *gif = (GifFileType*)m_state;
	if (gif) {
		DGifCloseFile(gif);
		m_state = 0;
	}

	if( m_f )
		fclose( m_f );

	m_width = m_height = 0;
	m_type = -1;
}

ImageDecoder GifDecoder::newDecoder() const
{
	return makePtr<GifDecoder>();
}

bool GifDecoder::readHeader()
{
	volatile bool result = false;
	close();

	int error = 0;
	GifFileType *gif_file;
	if ( !m_buf.empty() )
	{
		GifUserData* gif_data = new GifUserData;
		gif_data->source = m_buf.ptr();
		gif_data->len = m_buf.cols*m_buf.rows*m_buf.elemSize();
		gif_data->pos = 0;

		gif_file = DGifOpen(gif_data, gif_read_data);
		delete gif_data;

		result = true;
	}
	else
	{
		gif_file = DGifOpenFileName(m_filename.c_str());
		result = true;
	}

	if (!gif_file || error != 0)
	{
		result = false;
	}

	m_state = gif_file;

	int ret = DGifSlurp(gif_file);
	if ( ret != GIF_OK ) {
		close();
		result = false;
	}


	if ( gif_file->ImageCount <= 0 || gif_file->SWidth <= 0 || gif_file->SHeight <= 0 )
	{
		close();
		result = false;
	}

	m_width = gif_file->SWidth;
	m_height = gif_file->SHeight;
	m_type = CV_8UC3;

	if( !result )
		close();

	return result;
}

bool GifDecoder::readData( Mat& img )
{
	volatile bool result = false;

	if ( m_state && m_width && m_height )
	{
		GifFileType *gif_file = (GifFileType*)m_state;
		const ColorMapObject* ColorMap = (gif_file->Image.ColorMap ? gif_file->Image.ColorMap : gif_file->SColorMap);
		const uchar* src = gif_file->SavedImages[0].RasterBits;

		const int width = gif_file->Image.Width;
		const int height = gif_file->Image.Height;
		// TODO set to center
	//	const int top = gif_file->Image.Top;
	//	const int left = gif_file->Image.Left;
		if (width > m_width) m_width = width;
		if (height > m_height) m_height = height;

		int i, j;
		for (i = 0; i < height; i++) {
			int loc = width * i;
			uchar* dst = img.data + i * img.step[0];
			for (j = 0; j < width; j++) {
				GifColorType* color = &(ColorMap->Colors[*(src + loc + j)]);
				*dst++ = color->Blue;
				*dst++ = color->Green;
				*dst++ = color->Red;
			}
		}

		result = true;
	}

	img.quality = CV_DEFAULT_QUALITY;
	img.format = "GIF";

	close();
	return result;
}

/////////////////////// GifEncoder ///////////////////

GifEncoder::GifEncoder()
{
	m_description = "GIF files (*.gif)";
	m_buf_supported = true;
}

GifEncoder::~GifEncoder()
{
}

ImageEncoder GifEncoder::newEncoder() const
{
	return makePtr<GifEncoder>();
}

bool GifEncoder::write( const Mat& img, const std::vector<int>& )
{
    int width = img.cols, height = img.rows;

	GifFileType *gif;
	ColorMapObject *map;
	uchar *r, *g, *b, *frame;
	struct GifUserData *data;
	int palette_size = 256;
	int ret;

	map = GifMakeMapObject(256, NULL);
	if (!map) {
		return false;
	}

	if ( !m_buf )
	{
		gif = EGifOpenFileName(m_filename.c_str(), 0);
		if (!gif)
			return false;
	}
	else
	{
		data = (struct GifUserData*)calloc(1, sizeof(struct GifUserData));
		if (!data)
		{
			return false;
		}

		vector_to_char(*m_buf, (void**)&data->source, &data->len);
		gif = EGifOpen(data, gif_write_data);
		if (!gif) {
			free(data);
			return false;
		}
	}

	r = (uchar*)calloc(width*height, 1);
	g = (uchar*)calloc(width*height, 1);
	b = (uchar*)calloc(width*height, 1);
	frame = (uchar*)calloc(1, width*height*sizeof(GifByteType));
	memset(frame, 0xff, width*height*sizeof(GifByteType));

	int i, j;
	for (i = 0; i < height; i++) {
		for (j = 0; j < width; j++) {
			b[i*width+j] = img.data[img.channels()*(width*i+j)];
			g[i*width+j] = img.data[img.channels()*(width*i+j)+1];
			r[i*width+j] = img.data[img.channels()*(width*i+j)+2];
		}
	}

	GifQuantizeBuffer(width, height, &palette_size, r, g, b, frame, map->Colors);
	free(r);
	free(g);
	free(b);

	ret = EGifPutScreenDesc(gif, width, height, 8, 0, map);
	if (ret != GIF_OK) {
		GifFreeMapObject(map);
		EGifCloseFile(gif);
		free(frame);
		return false;
	}

	ret = EGifPutImageDesc(gif, 0, 0, width, height, 0, map);
	if (ret == GIF_ERROR) {
		GifFreeMapObject(map);
		EGifCloseFile(gif);
		free(frame);
		return false;
	}

	ret = EGifPutLine(gif, frame, width*height*sizeof(GifByteType));
	if (ret == GIF_ERROR) {
		GifFreeMapObject(map);
		EGifCloseFile(gif);
		free(frame);
		return false;
	}

	GifFreeMapObject(map);
	EGifCloseFile(gif);
	free(frame);
	return true;
}

}

#endif

/* End of file. */
