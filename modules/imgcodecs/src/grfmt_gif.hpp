// Add by cvley <ley@hackcv.com>
//

#ifndef _GRFMT_GIF_H_
#define _GRFMT_GIF_H_

#include "grfmt_base.hpp"
#include "bitstrm.hpp"

#ifdef HAVE_GIF

namespace cv
{

class GifDecoder : public BaseImageDecoder
{
public:

	GifDecoder();
	virtual ~GifDecoder();

	bool readData( Mat& img );
	bool readHeader();
	void close();

	ImageDecoder newDecoder() const;

protected:

	FILE* m_f;
	void* m_state;

private:
	GifDecoder(const GifDecoder &); // copy disabled
	GifDecoder& operator=(const GifDecoder &); // assign disabled
};

class GifEncoder : public BaseImageEncoder
{
public:
	GifEncoder();
	virtual ~GifEncoder();

	bool write( const Mat& img, const std::vector<int>& params );
	ImageEncoder newEncoder() const;
};

}

#endif

#endif /*_GRFMT_GIF_H_*/
