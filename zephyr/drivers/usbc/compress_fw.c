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

    for(int i=0; i<2;i++) {
        int max_len = lzav_compress_bound_hi( 64*1024 ); // Note another bound function!
        void* comp_buf = malloc( max_len );
        int comp_len = lzav_compress_hi( buffer + (i * 64*1024), comp_buf, 64*1024, max_len );
        FILE *fp1;
        char output_name[20] = {0};
        if( comp_len == 0 && file_size != 0 )
	    perror("Compression failed\n");
        else
	    printf("Compressed idx %d, size %d [ratio: %f]\n", i, comp_len, (float)comp_len / ((float)file_size / 2));

        unsigned char buf[64*1024];
        printf("Decompress size: %d, ", lzav_decompress(comp_buf, buf, comp_len, sizeof(buf)));
        if(!memcmp(buffer + (i * 64*1024), buf, sizeof(buf)))
            printf("Verified, output to file\n");
        else {
            printf("Decompress validation failed.\n");
            return -1;
        }
        
        sprintf(output_name, "rts54xx_0%d", i);
        fp1 = fopen(output_name, "wb");
        if(!fp1)
            return -1;
        fwrite(comp_buf, 1, comp_len, fp1);
        fclose(fp1);
    }

    free(buffer); // Free the buffer when done
    fclose(fp);
    return 0;

}
