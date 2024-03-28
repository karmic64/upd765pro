#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <errno.h>
#include <setjmp.h>

#include <png.h>


//////////////////////////////////////////////////////////////////////////////
// image data

#define H_DISPLAYED 50
#define V_DISPLAYED 38
	
#define IMAGE_WIDTH (H_DISPLAYED * 8 / 2)
#define IMAGE_HEIGHT (V_DISPLAYED * 8)

png_byte image[IMAGE_HEIGHT][IMAGE_WIDTH];
png_byte * image_rows[IMAGE_HEIGHT];

void init_image_rows() {
	for (unsigned i = 0; i < IMAGE_HEIGHT; i++)
		image_rows[i] = image[i];
}




#define BITS_PER_PIXEL 4
#define PIXELS_PER_BYTE 2
#define RASTERS 8
#define BYTES_PER_RASTER 0x800
#define BYTES_PER_CHUNK (RASTERS * BYTES_PER_RASTER)
#define PIXELS_PER_RASTER (BYTES_PER_RASTER * PIXELS_PER_BYTE)

unsigned calculate_bitmap_size(unsigned width, unsigned height, unsigned addr) {
	unsigned offset = addr % BYTES_PER_RASTER;
	unsigned raster_bytes = (width * (height / RASTERS)) / PIXELS_PER_BYTE;
	unsigned required_chunks = 0;
	unsigned accounted_bytes = 0;
	while (accounted_bytes < raster_bytes) {
		unsigned this_chunk_raster_bytes = required_chunks ? BYTES_PER_RASTER : BYTES_PER_RASTER - offset;
		accounted_bytes += this_chunk_raster_bytes;
		required_chunks++;
	}
	
	return (required_chunks * BYTES_PER_CHUNK) - (BYTES_PER_RASTER - (raster_bytes % BYTES_PER_RASTER));
}

unsigned get_cpc_bitmap_byte(unsigned c1, unsigned c2) {
	const uint8_t bit_tbl[8] = {0x83, 0x03, 0x81, 0x01, 0x82, 0x02, 0x80, 0x00};
	
	unsigned c = 0;
	for (unsigned b = 0; b < 8; b++) {
		unsigned bv = bit_tbl[b];
		unsigned cb = ((bv & 0x80) ? c2 : c1) & (1 << (bv & 3));
		if (cb)
			c |= 1 << b;
	}
	return c;
}



/////////////////////////////////////////////////////////////////////////////
// palette/colors

#define CPC_COLOR(r,g,b) (((r / 50) << 4) | ((g / 50) << 2) | ((b / 50) << 0))
#define CPC_PALETTE_SIZE 0x20
const uint8_t cpc_palette[CPC_PALETTE_SIZE] = {
	CPC_COLOR( 50, 50, 50), // 0x40 - white
	CPC_COLOR( 50, 50, 50), // 0x41 - white
	CPC_COLOR(  0,100, 50), // 0x42 - sea green
	CPC_COLOR(100,100, 50), // 0x43 - pastel yellow
	CPC_COLOR(  0,  0, 50), // 0x44 - blue
	CPC_COLOR(100,  0, 50), // 0x45 - purple
	CPC_COLOR(  0, 50, 50), // 0x46 - cyan
	CPC_COLOR(100, 50, 50), // 0x47 - pink
	CPC_COLOR(100,  0, 50), // 0x48 - purple
	CPC_COLOR(100,100, 50), // 0x49 - pastel yellow
	CPC_COLOR(100,100,  0), // 0x4a - bright yellow
	CPC_COLOR(100,100,100), // 0x4b - bright white
	CPC_COLOR(100,  0,  0), // 0x4c - bright red
	CPC_COLOR(100,  0,100), // 0x4d - bright magenta
	CPC_COLOR(100, 50,  0), // 0x4e - orange
	CPC_COLOR(100, 50,100), // 0x4f - pastel magenta
	CPC_COLOR(  0,  0, 50), // 0x50 - blue
	CPC_COLOR(  0,100, 50), // 0x51 - sea green
	CPC_COLOR(  0,100,  0), // 0x52 - bright green
	CPC_COLOR(  0,100,100), // 0x53 - bright cyan
	CPC_COLOR(  0,  0,  0), // 0x54 - black
	CPC_COLOR(  0,  0,100), // 0x55 - bright blue
	CPC_COLOR(  0, 50,  0), // 0x56 - green
	CPC_COLOR(  0, 50,100), // 0x57 - sky blue
	CPC_COLOR( 50,  0, 50), // 0x58 - magenta
	CPC_COLOR( 50,100, 50), // 0x59 - pastel green
	CPC_COLOR( 50,100,  0), // 0x5a - lime
	CPC_COLOR( 50,100,100), // 0x5b - pastel cyan
	CPC_COLOR( 50,  0,  0), // 0x5c - red
	CPC_COLOR( 50,  0,100), // 0x5d - mauve
	CPC_COLOR( 50, 50,  0), // 0x5e - yellow
	CPC_COLOR( 50, 50,100), // 0x5f - pastel blue
};
uint8_t cpc_color_lookup[3][3][3];

void make_cpc_color_lookup() {
	for (unsigned r = 0; r <= 2; r++) {
		for (unsigned g = 0; g <= 2; g++) {
			for (unsigned b = 0; b <= 2; b++) {
				unsigned color = (r << 4) | (g << 2) | (b << 0);
				for (unsigned i = 0; i < CPC_PALETTE_SIZE; i++) {
					if (cpc_palette[i] == color) {
						cpc_color_lookup[r][g][b] = i;
						break;
					}
				}
			}
		}
	}
}

void make_cpc_color_lookup_scr() {
	for (unsigned r = 0; r <= 2; r++) {
		for (unsigned g = 0; g <= 2; g++) {
			for (unsigned b = 0; b <= 2; b++) {
				cpc_color_lookup[r][g][b] = b + (r * 3) + (g * 3 * 3);
			}
		}
	}
}

unsigned get_color_component(unsigned c) {
	if (c <= 0x55)
		return 0;
	if (c >= 0xaa)
		return 2;
	return 1;
}

unsigned get_cpc_color(unsigned r, unsigned g, unsigned b) {
	r = get_color_component(r);
	g = get_color_component(g);
	b = get_color_component(b);
	
	return cpc_color_lookup[r][g][b];
}


#define OUT_PALETTE_SIZE 0x10
uint8_t out_palette[OUT_PALETTE_SIZE];




///////////////////////////////////////////////////////////////////////////
// output functions

void fputc_times(int c, int times, FILE * f) {
	for (int i = 0; i < times; i++)
		fputc(c, f);
}

void fput16(unsigned v, FILE * f) {
	fputc(v >> 0, f);
	fputc(v >> 8, f);
}

void fput24(unsigned v, FILE * f) {
	fputc(v >> 0, f);
	fputc(v >> 8, f);
	fputc(v >> 16, f);
}





////////////////////////////////////////////////////////////////////////////
// main


int main(int argc, char * argv[]) {
	if (argc != 4) {
		puts("usage: make-image in-png out-image start-addr");
		return EXIT_FAILURE;
	}
	const char * in_png_name = argv[1];
	const char * out_image_name = argv[2];
	const unsigned start_addr = strtoul(argv[3], NULL, 0);
	
	
	/////////////////////// read png
	
	FILE * f = fopen(in_png_name, "rb");
	if (!f) {
		printf("%s: %s\n", in_png_name, strerror(errno));
		return EXIT_FAILURE;
	}
	
	png_structp png_ptr = png_create_read_struct(
		PNG_LIBPNG_VER_STRING,NULL,NULL,NULL);
	if (!png_ptr) {
		fclose(f);
		puts("can't create png struct");
		return EXIT_FAILURE;
	}
	
	png_infop info_ptr = png_create_info_struct(png_ptr);
	if (!info_ptr) {
		fclose(f);
		png_destroy_read_struct(&png_ptr, NULL, NULL);
		puts("can't create png info struct");
		return EXIT_FAILURE;
	}
	
	if (setjmp(png_jmpbuf(png_ptr))) {
		png_destroy_read_struct(&png_ptr, &info_ptr, NULL);
		fclose(f);
		return EXIT_FAILURE;
	}
	png_init_io(png_ptr, f);
	
	png_read_info(png_ptr, info_ptr);
	unsigned width = png_get_image_width(png_ptr,info_ptr);
	unsigned height = png_get_image_height(png_ptr,info_ptr);
	unsigned color_type = png_get_color_type(png_ptr,info_ptr);
	png_colorp palette = NULL;
	int num_palette = 0;
	png_get_PLTE(png_ptr, info_ptr, &palette, &num_palette);
	
	int info_errors = 0;
	if (width != IMAGE_WIDTH) {
		printf("illegal image width %u\n", width);
		info_errors++;
	}
	if (height != IMAGE_HEIGHT) {
		printf("illegal image height %u\n", width);
		info_errors++;
	}
	if (color_type != PNG_COLOR_TYPE_PALETTE) {
		printf("illegal color type %u\n", color_type);
		info_errors++;
	}
	if (!palette) {
		puts("no palette present");
		info_errors++;
	} else if (num_palette == 0 || num_palette > OUT_PALETTE_SIZE) {
		printf("illegal palette size %d\n", num_palette);
		info_errors++;
	}
	if (info_errors)
		png_longjmp(png_ptr, 1);
	
	make_cpc_color_lookup_scr();
	for (int pi = 0; pi < OUT_PALETTE_SIZE; pi++) {
		if (pi < num_palette) {
			out_palette[pi] = get_cpc_color(
				palette[pi].red,
				palette[pi].green,
				palette[pi].blue
			) | 0x40;
		} else {
			out_palette[pi] = 0x40;
		}
	}
	
	png_set_strip_16(png_ptr);
	png_set_packing(png_ptr);
	
	init_image_rows();
	png_read_image(png_ptr, image_rows);
	
	png_read_end(png_ptr, NULL);
	png_destroy_read_struct(&png_ptr, &info_ptr, NULL);
	fclose(f);
	
	
	
	
	/////////////////////////// write output data
	
	f = fopen(out_image_name,"wb");
	if (!f) {
		printf("%s: %s\n", out_image_name, strerror(errno));
		return EXIT_FAILURE;
	}
	
	unsigned bitmap_size = calculate_bitmap_size(IMAGE_WIDTH, IMAGE_HEIGHT, start_addr);
	
	// AMSDOS header
	unsigned data_length = OUT_PALETTE_SIZE + bitmap_size;
	fputc(0, f);
	fwrite("IMAGE   BIN", 1, 8+3, f);
	fputc_times(0, 4, f);
	fputc(0, f);
	fputc(0, f);
	fputc(2, f);
	fput16(data_length, f);
	fput16(start_addr - OUT_PALETTE_SIZE, f);
	fputc(0, f);
	fput16(data_length, f);
	fput16(0, f);
	fputc_times(0, 64-28, f);
	
	fput24(data_length, f);
	fput16(0, f);
	fputc_times(0, 128-69, f);
	
	// palette
	fwrite(&out_palette, 1, OUT_PALETTE_SIZE, f);
	
	// image data
	unsigned offset = start_addr % BYTES_PER_RASTER;
	for (unsigned bi = 0; bi < bitmap_size; bi++) {
		unsigned addr = bi + offset;
		unsigned chunk = (addr & 0xc000) >> 14;
		unsigned raster = (addr & 0x3800) >> 11;
		unsigned raster_index = (addr & 0x7ff);
		unsigned full_raster_index = raster_index + (chunk * BYTES_PER_RASTER) - offset;
		
		unsigned x = (full_raster_index * PIXELS_PER_BYTE) % IMAGE_WIDTH;
		unsigned y = ((full_raster_index * PIXELS_PER_BYTE) / IMAGE_WIDTH * RASTERS) + raster;
		unsigned b;
		if (y >= IMAGE_HEIGHT)
			b = 0xe5;
		else
			b = get_cpc_bitmap_byte(image[y][x], image[y][x+1]);
		
		fputc(b, f);
	}
	
	
	fclose(f);
	
	
}


