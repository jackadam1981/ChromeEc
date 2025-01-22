#include "lzav.h"

#include <stdio.h>
#include <stdlib.h>

int main() {
    FILE *fp;
    char *filename = "rts54xx.bin";
    long file_size;

    // Get the filename from the user.  You could also hardcode this.

    fp = fopen(filename, "rb"); // Open in binary read mode

    if (fp == NULL) {
        perror("Error opening file");
        return 1;
    }

    // Seek to the end of the file to determine its size
    if (fseek(fp, 0, SEEK_END) != 0) {
        perror("Error seeking to end of file");
        fclose(fp);
        return 1;
    }

    file_size = ftell(fp); // Get the current position (which is the end)

    if (file_size == -1) {
        perror("Error getting file size");
        fclose(fp);
        return 1;
    }

    //Rewind to the beginning of the file to read the content if needed later
    rewind(fp);


    printf("File size: %ld bytes\n", file_size);

    unsigned char *buffer = (unsigned char *)malloc(file_size * sizeof(unsigned char));
    if (buffer == NULL) {
        perror("Memory allocation failed");
        fclose(fp);
        return 1;
    }

    fread(buffer, 1, file_size, fp); // Read the entire file into the buffer

    int max_len = lzav_compress_bound_hi( file_size ); // Note another bound function!
    void* comp_buf = malloc( max_len );
    int comp_len = lzav_compress_hi( buffer, comp_buf, file_size, max_len );

    if( comp_len == 0 && file_size != 0 )
	perror("Compression failed\n");
    else
	printf("Compressed size %d [ratio: %f]\n", comp_len, (float)comp_len / (float)file_size);

    FILE *fp1;
    fp1 = fopen("rts_54xx_comp.bin", "wb");
    fwrite(comp_buf, 1, comp_len, fp1);
    unsigned char buf[128*1024];
    printf("Decompress size: %d, ", lzav_decompress(comp_buf, buf, comp_len, sizeof(buf)));
    printf("comparison returns %d\n", memcmp(buffer, buf, sizeof(buf)));

    fclose(fp1);
    free(buffer); // Free the buffer when done
    fclose(fp);
    return 0;

}
