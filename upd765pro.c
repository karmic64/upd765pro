#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <ctype.h>
#include <errno.h>


#define min(a,b) ((a) < (b) ? (a) : (b))
#define max(a,b) ((a) > (b) ? (a) : (b))

/////////////////////////////////////////////////////////////////////////////
// .DSK defines

#define DSK_TRACKS 40
#define DSK_SIDES 1
#define DSK_TRACK_SECTORS 9
#define DSK_SECTOR_SIZE 0x200
#define DSK_SECTORS (DSK_TRACKS * DSK_TRACK_SECTORS)

#define DSK_HEADER_SIZE 0x100
#define DSK_MAGIC "MV - CPCEMU Disk-File\r\nDisk-Info\r\n"
#define DSK_MAGIC_SIZE 0x22
#define DSK_CREATOR "karmic20240306"
#define DSK_CREATOR_SIZE 0x0e
#define DSK_HEADER_PADDING_SIZE (DSK_HEADER_SIZE - 0x34)

#define DSK_TRACK_HEADER_SIZE 0x100
#define DSK_TRACK_MAGIC "Track-Info\r\n\0\0\0"
#define DSK_TRACK_MAGIC_SIZE 0x10
#define DSK_TRACK_SIZE (DSK_TRACK_HEADER_SIZE + (DSK_SECTOR_SIZE * DSK_TRACK_SECTORS))
#define DSK_TRACK_HEADER_SECTOR_SIZE 2
#define DSK_TRACK_GAP3_LENGTH 0x4e
#define DSK_TRACK_FILLER_BYTE 0xe5
#define DSK_TRACK_FIRST_SECTOR 0xc1
#define DSK_TRACK_HEADER_PADDING_SIZE (DSK_TRACK_HEADER_SIZE - 0x18 - (DSK_TRACK_SECTORS * 8))

////////////////////////////////////////////////////////////////////////////
// directory defines

#define DIR_BLOCK_SIZE 0x400
#define DIR_BLOCK_SECTORS (DIR_BLOCK_SIZE / DSK_SECTOR_SIZE)
#define DIR_BLOCKS 2
#define DIR_SECTORS (DIR_BLOCKS * DIR_BLOCK_SECTORS)
#define DIR_SIZE (DIR_BLOCKS * DIR_BLOCK_SIZE)
#define DIR_EXTENT_FILENAME_SIZE 8
#define DIR_EXTENT_EXTENSION_SIZE 3
#define DIR_EXTENT_SIZE 0x20
#define DIR_MAX_EXTENTS (DIR_SIZE / DIR_EXTENT_SIZE)
#define DIR_MAX_EXTENT_BLOCKS 0x10
#define DIR_MAX_EXTENT_DATA_SIZE (DIR_MAX_EXTENT_BLOCKS * DIR_BLOCK_SIZE)
#define DIR_RECORD_SIZE 0x80
#define DIR_MAX_EXTENT_RECORDS (DIR_MAX_EXTENT_BLOCKS * DIR_RECORD_SIZE)
#define DIR_DISK_BLOCKS (DSK_SECTORS * DSK_SECTOR_SIZE / DIR_BLOCK_SIZE)
#define DIR_INVALID_CHARACTERS "<>.,;:=?*[]"

typedef struct {
	uint8_t st;
	char name[DIR_EXTENT_FILENAME_SIZE];
	char ext[DIR_EXTENT_EXTENSION_SIZE];
	uint8_t xl;
	uint8_t bc;
	uint8_t xh;
	uint8_t rc;
	uint8_t al[DIR_MAX_EXTENT_BLOCKS];
} extent_t;

typedef extent_t directory_t[DIR_MAX_EXTENTS];


///////////////////////////////////////////////////////////////////////////
// .DSK functions

unsigned get_track_dsk_offset(unsigned track) {
	return DSK_HEADER_SIZE + (DSK_TRACK_SIZE * track);
}

unsigned get_track_sector_dsk_offset(unsigned track, unsigned sector) {
	return get_track_dsk_offset(track) + DSK_TRACK_HEADER_SIZE + (DSK_SECTOR_SIZE * sector);
}

unsigned get_sector_dsk_offset(unsigned sector) {
	unsigned track = sector / DSK_TRACK_SECTORS;
	sector %= DSK_TRACK_SECTORS;
	return get_track_sector_dsk_offset(track, sector);
}



//////////////////////////////////////////////////////////////////////////
// output functions

void fputc_times(int c, int times, FILE * f) {
	for (int i = 0; i < times; i++)
		fputc(c, f);
}

void fput16(unsigned v, FILE * f) {
	fputc(v >> 0, f);
	fputc(v >> 8, f);
}


///////////////////////////////////////////////////////////////////////////
// main

int main(int argc, char ** argv) {
	if (argc < 2) {
		puts(
			"args: out-dsk [in-file[,file-arg]...]..."
			"\nfilename args:"
			"\n\tro\tread-only"
			"\n\tsys\tsystem file (hidden)"
			"\n\tb__\tforce start at block __"
		);
		return EXIT_FAILURE;
	}
	const char * out_disk_name = argv[1];
	const int in_files = argc - 2;
	char ** in_file_names = &argv[2];
	
	
	//////////// write blank out disk
	
	FILE * diskf = fopen(out_disk_name, "wb");
	if (!diskf) {
		printf("%s: %s\n", out_disk_name, strerror(errno));
		return EXIT_FAILURE;
	}
	
	// dsk header
	fwrite(DSK_MAGIC, 1, DSK_MAGIC_SIZE, diskf);
	fwrite(DSK_CREATOR, 1, DSK_CREATOR_SIZE, diskf);
	fputc(DSK_TRACKS, diskf);
	fputc(DSK_SIDES, diskf);
	fput16(DSK_TRACK_SIZE, diskf);
	fputc_times(0, DSK_HEADER_PADDING_SIZE, diskf);
	
	// tracks and sectors
	for (unsigned track = 0; track < DSK_TRACKS; track++) {
		fwrite(DSK_TRACK_MAGIC, 1, DSK_TRACK_MAGIC_SIZE, diskf);
		fputc(track, diskf);
		fputc(0, diskf); // side number
		fput16(0, diskf); // unused
		fputc(DSK_TRACK_HEADER_SECTOR_SIZE, diskf);
		fputc(DSK_TRACK_SECTORS, diskf);
		fputc(DSK_TRACK_GAP3_LENGTH, diskf);
		fputc(DSK_TRACK_FILLER_BYTE, diskf);
		for (unsigned sector = 0; sector < DSK_TRACK_SECTORS; sector++) {
			fputc(track, diskf);
			fputc(0, diskf); // side number
			fputc(DSK_TRACK_FIRST_SECTOR + sector, diskf);
			fputc(DSK_TRACK_HEADER_SECTOR_SIZE, diskf);
			fputc(0, diskf); // FDC status 1
			fputc(0, diskf); // FDC status 2
			fput16(0, diskf); // unused
		}
		fputc_times(0, DSK_TRACK_HEADER_PADDING_SIZE, diskf);
		
		for (unsigned sector = 0; sector < DSK_TRACK_SECTORS; sector++) {
			fputc_times(DSK_TRACK_FILLER_BYTE, DSK_SECTOR_SIZE, diskf);
		}
	}
	
	
	//////////// make directory and read files
	
	directory_t directory;
	memset(&directory, DSK_TRACK_FILLER_BYTE, sizeof(directory));
	unsigned extents_on_disk = 0;
	
	uint8_t taken_blocks[DIR_DISK_BLOCKS];
	unsigned first_free_block = DIR_BLOCKS;
	unsigned blocks_free = DIR_DISK_BLOCKS - DIR_BLOCKS;
	memset(&taken_blocks[0], -1, first_free_block);
	memset(&taken_blocks[first_free_block], 0, blocks_free);
	
	int errors = 0;
	for (int in_file_id = 0; in_file_id < in_files; in_file_id++) {
		// parse argument
		int read_only = 0;
		int system_file = 0;
		int force_block = -1;
		
		char * src_filename = in_file_names[in_file_id];
		char * filename_token = strtok(src_filename, ",");
		printf("Reading %s...\n", src_filename);
		while (1) {
			filename_token = strtok(NULL, ",");
			if (!filename_token)
				break;
			
			if (!strcmp(filename_token, "ro")) {
				read_only = 1;
			} else if (!strcmp(filename_token, "sys")) {
				system_file = 1;
			} else if (filename_token[0] == 'b') {
				int fb = strtol(&filename_token[1], NULL, 0);
				if (fb < DIR_BLOCKS || fb >= DIR_DISK_BLOCKS) {
					printf("Invalid block number %s\n", &filename_token[1]);
					errors++;
				} else {
					force_block = fb;
				}
			} else {
				printf("Can't parse file option %s\n", filename_token);
				errors++;
			}
		}
		
		// read the file blocks
		extent_t * first_extent = NULL;
		extent_t * cur_extent = NULL;
		unsigned file_blocks = 0;
		unsigned file_extents = 0;
		unsigned cur_block = 0;
		
		FILE * inf = fopen(src_filename, "rb");
		if (!inf) {
			printf("Couldn't open: %s\n", strerror(errno));
			errors++;
		} else {
			while (1) {
				uint8_t block_data[DIR_BLOCK_SIZE];
				size_t bytes_read = fread(block_data, 1, DIR_BLOCK_SIZE, inf);
				if (ferror(inf)) {
					printf("Error while reading: %s\n", strerror(errno));
					errors++;
					break;
				}
				if (bytes_read == 0) {
					// no bytes read, no error, therefore EOF
					break;
				}
				
				if (file_blocks % DIR_MAX_EXTENT_BLOCKS == 0) {
					// too much data or first block, must allocate a new extent
					if (extents_on_disk == DIR_MAX_EXTENTS) {
						puts("Out of directory space");
						errors++;
						fclose(inf);
						goto fatal;
					}
					
					cur_extent = &directory[extents_on_disk++];
					if (!first_extent) {
						// this is the first extent, initialize it fully
						first_extent = cur_extent;
						
						if (bytes_read < 0x80) {
							// source file is not big enough to have a header
							puts("File too small");
							errors++;
							goto in_file_fatal;
						}
						
						int file_errors = 0;
						unsigned status = block_data[0];
						if (status >= 0x20) {
							printf("Invalid status %02X\n", status);
							file_errors++;
						}
						for (unsigned i = 0; i < DIR_EXTENT_FILENAME_SIZE + DIR_EXTENT_EXTENSION_SIZE; i++) {
							uint8_t b = block_data[i+1];
							if (b >= 0x7f || b < 0x20 || strchr(DIR_INVALID_CHARACTERS, b)) {
								printf("Illegal filename %.8s.%.3s\n", &block_data[1], &block_data[9]);
								file_errors++;
								break;
							}
							char c = toupper(b);
							if (i < DIR_EXTENT_FILENAME_SIZE)
								cur_extent->name[i] = c;
							else
								cur_extent->ext[i-DIR_EXTENT_FILENAME_SIZE] = c;
						}
						
						if (file_errors) {
							errors += file_errors;
							goto in_file_fatal;
						}
						
						cur_extent->st = status;
						if (read_only)
							cur_extent->ext[0] |= 0x80;
						if (system_file)
							cur_extent->ext[1] |= 0x80;
						
						// set up initial block
						if (force_block >= 0 && taken_blocks[force_block]) {
							printf("WARNING: Block %d is taken, can't start file here\n", force_block);
							force_block = -1;
							errors++;
						}
						if (force_block >= 0)
							cur_block = force_block;
						else
							cur_block = first_free_block;
						
						// correct checksum
						unsigned checksum = 0;
						for (unsigned i = 0; i <= 66; i++)
							checksum += block_data[i];
						block_data[67] = checksum >> 0;
						block_data[68] = checksum >> 8;
					} else {
						// copy some information from the old extent
						cur_extent->st = first_extent->st;
						memcpy(&cur_extent->name, &first_extent->name, DIR_EXTENT_FILENAME_SIZE);
						memcpy(&cur_extent->ext, &first_extent->ext, DIR_EXTENT_EXTENSION_SIZE);
					}
					
					// initialize non-continuous extent fields
					cur_extent->xl = file_extents++;
					cur_extent->bc = 0;
					cur_extent->xh = 0;
					cur_extent->rc = 0;
					memset(&cur_extent->al, 0, DIR_MAX_EXTENT_BLOCKS);
				}
				
				// write this block to disk
				if (!blocks_free) {
					puts("Out of free blocks");
					errors++;
					fclose(inf);
					goto fatal;
				}
				
				while (taken_blocks[cur_block]) {
					// this block is taken, must find a new one
					if (cur_block == DIR_DISK_BLOCKS) {
						first_free_block = DIR_BLOCKS;
						cur_block = DIR_BLOCKS;
					}
					if (cur_block == first_free_block)
						first_free_block++;
					cur_block++;
				}
				
				unsigned cur_sector = cur_block * DIR_BLOCK_SECTORS;
				size_t bytes_left = bytes_read;
				size_t block_index = 0;
				while (bytes_left) {
					unsigned sector_offset = get_sector_dsk_offset(cur_sector);
					fseek(diskf, sector_offset, SEEK_SET);
					
					size_t bytes_to_write = min(bytes_left, DSK_SECTOR_SIZE);
					fwrite(&block_data[block_index], 1, bytes_to_write, diskf);
					block_index += bytes_to_write;
					bytes_left -= bytes_to_write;
					cur_sector++;
				}
				
				cur_extent->al[(file_blocks++) % DIR_MAX_EXTENT_BLOCKS] = cur_block;
				cur_extent->rc += (bytes_read + (DIR_RECORD_SIZE - 1)) / DIR_RECORD_SIZE;
				taken_blocks[cur_block] = -1;
				blocks_free--;
				if (cur_block == DIR_DISK_BLOCKS) {
					first_free_block = DIR_BLOCKS;
					cur_block = DIR_BLOCKS;
				}
				if (cur_block == first_free_block)
					first_free_block++;
				cur_block++;
				
				// done?
				if (feof(inf))
					break;
			}
			
			puts("OK");
in_file_fatal:
			fclose(inf);
		}
		
		if (in_file_id < in_files-1)
			putchar('\n');
	}
	
	
	
	//////////// write directory to disk
	fseek(diskf, get_sector_dsk_offset(0), SEEK_SET);
	fwrite(&directory, sizeof(*directory), extents_on_disk, diskf);
	
	
	//////////// done
	
fatal:
	fclose(diskf);
	
	return errors;
}




