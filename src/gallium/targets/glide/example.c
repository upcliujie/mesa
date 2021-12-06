// Minimal Glide program which outputs one red frame before exiting.
//
// This can be used to test this frontend without one of the existing games.

void grGlideInit(void);
void grSstSelect(int card);
void grSstWinOpen(int hwnd, int res, int ref, int format, int origin, int buffers, int aux_buffers);
void grBufferClear(unsigned color, unsigned char alpha, unsigned short depth);
void grBufferSwap(int interval);

int main()
{
	grGlideInit();
	grSstSelect(0);
	grSstWinOpen(0, 7, 0, 1, 0, 2, 0);
	grBufferClear(0x0000ff, 0x00, 0xffff);
	grBufferSwap(1);
}
